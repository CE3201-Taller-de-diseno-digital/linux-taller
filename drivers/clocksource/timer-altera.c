// SPDX-License-Identifier: GPL-2.0-only
/*
 * drivers/clocksource/timer-altera.c
 *
 * Copyright (C) 2022 Alejandro Soto <alejandro@34project.org>
 * Copyright (C) 2013-2014 Altera Corporation
 * Copyright (C) 2010 Tobias Klauser <tklauser@distanz.ch>
 * Copyright (C) 2004 Microtronix Datacom Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/interrupt.h>
#include <linux/clocksource.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/slab.h>

#include "timer-of.h"

#define ALTR_TIMER_COMPATIBLE		"altr,timer-1.0"

#define ALTERA_TIMER_STATUS_REG	0
#define ALTERA_TIMER_CONTROL_REG	4
#define ALTERA_TIMER_PERIODL_REG	8
#define ALTERA_TIMER_PERIODH_REG	12
#define ALTERA_TIMER_SNAPL_REG		16
#define ALTERA_TIMER_SNAPH_REG		20

#define ALTERA_TIMER_CONTROL_ITO_MSK	(0x1)
#define ALTERA_TIMER_CONTROL_CONT_MSK	(0x2)
#define ALTERA_TIMER_CONTROL_START_MSK	(0x4)
#define ALTERA_TIMER_CONTROL_STOP_MSK	(0x8)

struct altera_clocksource {
	struct timer_of *to;
	struct clocksource cs;
};

static u16 altera_timer_readw(struct timer_of *to, u32 offs)
{
	return readw(to->of_base.base + offs);
}

static void altera_timer_writew(struct timer_of *to, u16 val, u32 offs)
{
	writew(val, to->of_base.base + offs);
}

static inline unsigned long altera_clocksource_snapshot(struct altera_clocksource *altr_cs)
{
	unsigned long count;
	struct timer_of *to = altr_cs->to;

	altera_timer_writew(to, 0, ALTERA_TIMER_SNAPL_REG);
	count = altera_timer_readw(to, ALTERA_TIMER_SNAPH_REG) << 16 |
		altera_timer_readw(to, ALTERA_TIMER_SNAPL_REG);

	return count;
}

static u64 altera_clocksource_read(struct clocksource *cs)
{
	struct altera_clocksource *altr_cs;
	unsigned long flags;
	u32 count;

	altr_cs = container_of(cs, struct altera_clocksource, cs);

	local_irq_save(flags);
	count = altera_clocksource_snapshot(altr_cs);
	local_irq_restore(flags);

	/* Counter is counting down */
	return ~count;
}

static void altera_timer_start(struct timer_of *to)
{
	u16 ctrl;

	ctrl = altera_timer_readw(to, ALTERA_TIMER_CONTROL_REG);
	ctrl |= ALTERA_TIMER_CONTROL_START_MSK;
	altera_timer_writew(to, ctrl, ALTERA_TIMER_CONTROL_REG);
}

static void altera_timer_stop(struct timer_of *to)
{
	u16 ctrl;

	ctrl = altera_timer_readw(to, ALTERA_TIMER_CONTROL_REG);
	ctrl |= ALTERA_TIMER_CONTROL_STOP_MSK;
	altera_timer_writew(to, ctrl, ALTERA_TIMER_CONTROL_REG);
}

static void altera_timer_config(struct timer_of *to, unsigned long period,
			       bool periodic)
{
	u16 ctrl;

	/* The timer's actual period is one cycle greater than the value
	 * stored in the period register. */
	period--;

	ctrl = altera_timer_readw(to, ALTERA_TIMER_CONTROL_REG);
	/* stop counter */
	altera_timer_writew(to, ctrl | ALTERA_TIMER_CONTROL_STOP_MSK,
		ALTERA_TIMER_CONTROL_REG);

	/* write new count */
	altera_timer_writew(to, period, ALTERA_TIMER_PERIODL_REG);
	altera_timer_writew(to, period >> 16, ALTERA_TIMER_PERIODH_REG);

	ctrl |= ALTERA_TIMER_CONTROL_START_MSK | ALTERA_TIMER_CONTROL_ITO_MSK;
	if (periodic)
		ctrl |= ALTERA_TIMER_CONTROL_CONT_MSK;
	else
		ctrl &= ~ALTERA_TIMER_CONTROL_CONT_MSK;
	altera_timer_writew(to, ctrl, ALTERA_TIMER_CONTROL_REG);
}

static int altera_clockevent_set_next_event(unsigned long delta,
	struct clock_event_device *evt)
{
	struct timer_of *to = to_timer_of(evt);

	altera_timer_config(to, delta, false);
	return 0;
}

static int altera_clockevent_shutdown(struct clock_event_device *evt)
{
	struct timer_of *to = to_timer_of(evt);

	altera_timer_stop(to);
	return 0;
}

static int altera_clockevent_set_periodic(struct clock_event_device *evt)
{
	unsigned long period;
	struct timer_of *to = to_timer_of(evt);

	period = DIV_ROUND_UP(timer_of_rate(to), HZ);
	altera_timer_config(to, period, true);
	return 0;
}

static int altera_clockevent_resume(struct clock_event_device *evt)
{
	struct timer_of *to = to_timer_of(evt);

	altera_timer_start(to);
	return 0;
}

irqreturn_t altera_clockevent_handler(int irq, void *dev_id)
{
	struct clock_event_device *evt = (struct clock_event_device *) dev_id;
	struct timer_of *to = to_timer_of(evt);

	/* Clear the interrupt condition */
	altera_timer_writew(to, 0, ALTERA_TIMER_STATUS_REG);
	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static __init void altera_clockevent_init(struct timer_of *to)
{
	struct clock_event_device *evt = &to->clkevt;

	evt->features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT;
	evt->rating = 250;
	evt->shift = 32;
	evt->set_next_event = altera_clockevent_set_next_event;
	evt->set_state_shutdown = altera_clockevent_shutdown;
	evt->set_state_periodic = altera_clockevent_set_periodic;
	evt->set_state_oneshot = altera_clockevent_shutdown;
	evt->tick_resume = altera_clockevent_resume;
	//TODO: cpumask

	altera_timer_stop(to);
	/* clear pending interrupt */
	altera_timer_writew(to, 0, ALTERA_TIMER_STATUS_REG);

	clockevents_config_and_register(evt, timer_of_rate(to), 1, ULONG_MAX);
}

static __init int altera_clocksource_init(struct altera_clocksource *altr_cs)
{
	struct timer_of *to = altr_cs->to;
	struct clocksource *cs = &altr_cs->cs;

	unsigned int ctrl;
	int ret;

	cs->rating	= 250;
	cs->read = altera_clocksource_read;
	cs->mask = CLOCKSOURCE_MASK(32);
	cs->flags = CLOCK_SOURCE_IS_CONTINUOUS;

	ret = clocksource_register_hz(cs, timer_of_rate(to));
	if (ret)
		return ret;

	altera_timer_writew(to, USHRT_MAX, ALTERA_TIMER_PERIODL_REG);
	altera_timer_writew(to, USHRT_MAX, ALTERA_TIMER_PERIODH_REG);

	/* interrupt disable + continuous + start */
	ctrl = ALTERA_TIMER_CONTROL_CONT_MSK | ALTERA_TIMER_CONTROL_START_MSK;
	altera_timer_writew(to, ctrl, ALTERA_TIMER_CONTROL_REG);

	return 0;
}

static int __init altera_timer_init(struct device_node *np)
{
	int ret;
	bool clocksource;
	struct timer_of *to;
	struct altera_clocksource *altr_cs;

	clocksource = of_property_read_bool(np, "altr,timer-clocksource");

	to = kzalloc(sizeof(*to), GFP_KERNEL);
	if (!to)
		return -ENOMEM;

	to->flags = TIMER_OF_IRQ | TIMER_OF_CLOCK | TIMER_OF_BASE;
	to->of_irq.handler = altera_clockevent_handler;

	ret = timer_of_init(np, to);
	if (ret)
		goto err_free_to;

	if (clocksource) {
		altr_cs = kzalloc(sizeof(*altr_cs), GFP_KERNEL);
		if (!altr_cs) {
			ret = -ENOMEM;
			goto err_timer_of;
		}

		to->private_data = altr_cs;
		altr_cs->to = to;

		ret = altera_clocksource_init(altr_cs);
		if (ret)
			goto err_free_cs;
	} else
		altera_clockevent_init(to);

	return 0;

err_free_cs:
	kfree(altr_cs);
err_timer_of:
	timer_of_cleanup(to);
err_free_to:
	kfree(to);
	return ret;
}

TIMER_OF_DECLARE(altera_timer, ALTR_TIMER_COMPATIBLE, altera_timer_init);
