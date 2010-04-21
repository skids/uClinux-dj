/***************************************************************************/

/*
 *	dj/timer.c -- HP Deskjet ASIC hardware timer support.
 *
 *	Copyright (C) 2009-2010, Brian S. Julin <bri@abrij.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 */

/***************************************************************************/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/param.h>
#include <linux/clockchips.h>
#include <asm/io.h>
#include <asm/traps.h>
#include <asm/machdep.h>

#include <asm/dj/djio.h>
#include <asm/dj/timer.h>
#include <asm/dj/irq.h>


/***************************************************************************/

/* Convenience functions */

/**
 * dj_timer_irq_to_idx: map an IRQ line back to a timer index
 * @irq: valid irq line (vector) which the timer is assigned to.
 *
 * In case the hrtimer code ever decides to use more than one countdown 
 * unit (e.g. the profile code is integrated to use clock_event_device) 
 * all below code is written to work on any of the countdown timers.  
 *
 * We can tell them apart by the IRQ line stored in clock_event_device.irq,
 * as they are statically mapped 1:1 on this platform.
 *
 * This inline provides the reverse map from a received IRQ number to 
 * the timer index.
 */
static inline int dj_timer_irq_to_idx(int irq) {
	switch(irq) {
	case DJIO_IRQ_BASE + DJIO_A_TIMER_IRQ_A_LINE:
		return 0;
	case DJIO_IRQ_BASE + DJIO_A_TIMER_IRQ_B_LINE:
		return 1;
	case DJIO_IRQ_BASE + DJIO_A_TIMER_IRQ_C_LINE:
		return 2;
	case DJIO_IRQ_BASE + DJIO_A_TIMER_IRQ_D_LINE:
		return 3;
	case DJIO_IRQ_BASE + DJIO_A_TIMER_IRQ_E_LINE:
		return 4;
	}
	BUG();
	return -1;
}

/**
 * dj_timer_frob_enab: enable or disable IRQ source from a timer
 * @tidx: The index of the corresponding timer, 0 to 4.
 * @enab: Zero to disable, nonzero to enable
 *
 */
static inline void dj_timer_frob_enab(int tidx, int enab) {
	u16 tmp;

	tmp = readw(DJIO_A_TIMER + DJIO_A_TIMER_IRQ_ENAB);
	if (enab) {
		tmp |=  (1 << tidx);
	} else {
		tmp &= ~(1 << tidx);
	}
	writew(tmp, DJIO_A_TIMER + DJIO_A_TIMER_IRQ_ENAB);
}


/***************************************************************************/

/* Generic timer system glue */

/**
 * init_dj_timer: callback for the generic timer core to configure a timer
 * @mode: the CLK_EVT_MODE_* mode desired, see clockchip.h
 * @evt: the registered clock_event_device of the desired timer
 *
 */
static void dj_timer_init(enum clock_event_mode mode,
                          struct clock_event_device *evt)
{
	int tidx = dj_timer_irq_to_idx(evt->irq);
	u16 *sreg = ((u16 *)(DJIO_A_TIMER + DJIO_A_TIMER_A_SHOT)) + tidx;
	u16 *preg = ((u16 *)(DJIO_A_TIMER + DJIO_A_TIMER_A_PERIOD)) + tidx;

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		writew(0, sreg);
		writew(DJ_PER_JIFFY_CLICKS, preg);	
		dj_timer_frob_enab(tidx, 1);
		break;
		
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		writew(0, sreg);
		writew(0, preg);
		dj_timer_frob_enab(tidx, 0);
		break;
		
	case CLOCK_EVT_MODE_ONESHOT:
		writew(0, sreg);
		writew(0, preg);
		dj_timer_frob_enab(tidx, 1);
		break;

	case CLOCK_EVT_MODE_RESUME:
		/* Nothing to do here */
		break;
	}
}

/**
 * dj_timer_next_event: called from the hrtimer core to reprogram a timer
 * @delta: the amount of time (in hardware counter ticks) to count down.
 * @evt: the registered clock_event_device of the desired timer
 *
 */
static int dj_timer_next_event(unsigned long delta,
			       struct clock_event_device *evt)
{
	int tidx = dj_timer_irq_to_idx(evt->irq);
	u16 *sreg = ((u16 *)(DJIO_A_TIMER + DJIO_A_TIMER_A_SHOT)) + tidx;

	if (delta < DJ_TIMER_LAG_CLICKS) {
		/* This should not happen as DJ_TIMER_LAG < min_delta */
		delta = DJ_TIMER_LAG_CLICKS + 1;
	}
	delta -= DJ_TIMER_LAG_CLICKS;

	writew(((u16)1) << tidx, DJIO_A_TIMER + DJIO_A_TIMER_IRQ_ACK);
	writew(delta, sreg);
	dj_timer_frob_enab(tidx, 1);
	return 0;
}

/* For now we just define one source using timer A */

struct clock_event_device dj_timera_clock_event = {
	.name		= "timerA",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.set_mode	= dj_timer_init,
	.set_next_event	= dj_timer_next_event,
	.shift		= 0,
	.irq		= DJIO_IRQ_BASE + DJIO_A_TIMER_IRQ_A_LINE,
};

struct clock_event_device *dj_clock_event_devices[5] = { 
	&dj_timera_clock_event, NULL, NULL, NULL, NULL
};


/***************************************************************************/

/* IRQ handler */

static irqreturn_t dj_timer_tick(int irq, void *dummy)
{
	struct clock_event_device *evt = NULL;
	int tidx = dj_timer_irq_to_idx(irq);

#if 0
	/* Since we are IRQF_DISABLED we don't need to do this: */
	writeb(DJIO_A_IRQ_GLOBAL_DISABLE, DJIO_A_IRQ_GLOBAL);
	writeb(0, DJIO_A_IRQ_N(irq - DJIO_IRQ_BASE));
	writeb(DJIO_A_IRQ_GLOBAL_ENABLE,  DJIO_A_IRQ_GLOBAL);
#endif	

	/* No clue here.  Should we clear ACKs before or after handler? */
	writew(((u16)1) << tidx, DJIO_A_TIMER + DJIO_A_TIMER_IRQ_ACK);
		
	evt = dj_clock_event_devices[tidx];
	if (evt)
		evt->event_handler(evt);

	writeb(DJIO_A_IRQ_GLOBAL_DISABLE, DJIO_A_IRQ_GLOBAL);
	writeb(2, DJIO_A_IRQ_N(irq - DJIO_IRQ_BASE));
	writeb(DJIO_A_IRQ_GLOBAL_ENABLE,  DJIO_A_IRQ_GLOBAL);

	return IRQ_HANDLED;
}


/***************************************************************************/

static struct irqaction dj_timer_irqs[5] = {
	{
	 	.name	 = "timerA",
		.flags	 = IRQF_DISABLED | IRQF_TIMER,
		.handler = dj_timer_tick,
	},
	{
	 	.name	 = "timerB",
		.flags	 = IRQF_DISABLED | IRQF_TIMER,
		.handler = dj_timer_tick,
	},
	{
	 	.name	 = "timerC",
		.flags	 = IRQF_DISABLED | IRQF_TIMER,
		.handler = dj_timer_tick,
	},
	{
	 	.name	 = "timerD",
		.flags	 = IRQF_DISABLED | IRQF_TIMER,
		.handler = dj_timer_tick,
	},
	{
	 	.name	 = "timerE",
		.flags	 = IRQF_DISABLED | IRQF_TIMER,
		.handler = dj_timer_tick,
	},
};

/***************************************************************************/

/* Timekeeping glue */

static cycle_t dj_timer_read_clk(struct clocksource *cs)
{
	unsigned long flags;
	u32 cycles;

	local_irq_save(flags);
	cycles = readl(DJIO_A_TIMER + DJIO_A_TIMER_COUNTER);
	local_irq_restore(flags);

	return cycles;
}

static struct clocksource dj_timer_clk = {
	.name	= "djclock",
	.rating	= 250,
	.read	= dj_timer_read_clk,
	.shift	= 0,
	.mult   = (DJ_COUNTER_FREQ/1000),
	.mask	= CLOCKSOURCE_MASK(32),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};


/***************************************************************************/

void hw_timer_init(void)
{
	int i;
	struct clock_event_device *cevent;

	writeb(CONFIG_DJ_COUNTER_DIV & DJIO_A_TIMER_COUNTER_DIV_MASK, 
	       DJIO_A_TIMER + DJIO_A_TIMER_COUNTER_DIV);
	clocksource_register(&dj_timer_clk);
	dj_timer_clk.mult = clocksource_hz2mult(DJ_COUNTER_FREQ, 
						dj_timer_clk.shift);

	cevent = &dj_timera_clock_event; /* Until we need more than 1 */
	for (i = 0; i < 1; i++) {        /* Until we need more than 1 */

		cevent->cpumask = cpumask_of(smp_processor_id());
		cevent->mult = div_sc(DJ_COUNTER_FREQ, 
				      NSEC_PER_SEC, cevent->shift);
		cevent->max_delta_ns = clockevent_delta2ns(0xFFFF, cevent);
		cevent->min_delta_ns = DJ_TIMER_MIN_DELTA_NS;

		dj_timer_init(CLOCK_EVT_MODE_UNUSED, cevent);
		setup_irq(cevent->irq, &(dj_timer_irqs[i]));
		clockevents_register_device(cevent);
	}
}

