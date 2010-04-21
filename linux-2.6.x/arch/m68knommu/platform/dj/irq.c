/*
 * dj/irq.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2000  Michael Leslie <mleslie@lineo.com>
 * Copyright (c) 1996 Roman Zippel
 * Copyright (c) 1999 D. Jeff Dionne <jeff@uclinux.org>
 * Copyright (C) 1999-2007, Greg Ungerer <gerg@snapgear.com>
 * Copyright (C) 2009, Brian S. Julin <bri@abrij.org>
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/dj/djio.h>
#include <asm/dj/irq.h>

/* assembler routines */
asmlinkage void system_call(void);
asmlinkage void buserr(void);
asmlinkage void trap(void);
asmlinkage void bad_interrupt(void);
asmlinkage void inthandler(void);

extern void **_ramvec;

/* The number of spurious interrupts */
volatile unsigned int num_spurious;

/*
 * This function should be called during kernel startup to initialize
 * the vector table.
 */
void init_vectors(void)
{
	int i;

        /*
         *      There is a common trap handler and common interrupt
         *      handler that handle almost every vector. We treat
         *      the system call and bus error special, they get their
         *      own first level handlers.
         */
        for (i = 3; (i <= 23); i++)
		_ramvec[i] = trap;
        _ramvec[33] = trap;
#if !defined(CONFIG_UCBOOTSTRAP)
        _ramvec[34] = trap;
#endif
        for (i = 35; (i <= 63); i++)
		_ramvec[i] = trap;
        for (i = 24; (i <= 31); i++)
		_ramvec[i] = inthandler;
        for (i = 64; (i < 255); i++)
		_ramvec[i] = inthandler;
        _ramvec[255] = 0;

        _ramvec[2] = buserr;
        _ramvec[32] = system_call;
}

char *foo[200];

void enable_vector(unsigned int irq)
{
	unsigned long flags;

	if (irq < DJIO_IRQ_BASE)
		return;
	if (irq > DJIO_IRQ_BASE + 0x1f)
		return; /* 0x1f is a guess */

	local_irq_save(flags);
	outb(DJIO_A_IRQ_GLOBAL_DISABLE, DJIO_A_IRQ_GLOBAL);
	outb(2, DJIO_A_IRQ_N(irq - DJIO_IRQ_BASE)); /* All are prio 2 for now */
	outb(DJIO_A_IRQ_GLOBAL_ENABLE, DJIO_A_IRQ_GLOBAL);
	local_irq_restore(flags);
}

void disable_vector(unsigned int irq)
{
	unsigned long flags;

	if (irq < DJIO_IRQ_BASE)
		return;
	if (irq > DJIO_IRQ_BASE + 0x1f)
		return; /* 0x1f is a guess */

	local_irq_save(flags);
	outb(DJIO_A_IRQ_GLOBAL_DISABLE, DJIO_A_IRQ_GLOBAL);
	outb(DJIO_A_IRQ_IRQN_DISABLE, DJIO_A_IRQ_N(irq - DJIO_IRQ_BASE));
	outb(DJIO_A_IRQ_GLOBAL_ENABLE, DJIO_A_IRQ_GLOBAL);
	local_irq_restore(flags);
}

void ack_vector(unsigned int irq)
{
	unsigned long flags;

	if (irq < DJIO_IRQ_BASE)
		return;
	if (irq > DJIO_IRQ_BASE + 0x1f)
		return; /* 0x1f is a guess */
	local_irq_save(flags);
	outb(DJIO_A_IRQ_GLOBAL_DISABLE, DJIO_A_IRQ_GLOBAL);
	outb(0, DJIO_A_IRQ_N(irq - DJIO_IRQ_BASE)); /* XXX prio is stomped */
	outb(DJIO_A_IRQ_GLOBAL_ENABLE, DJIO_A_IRQ_GLOBAL);
	local_irq_restore(flags);
}

void coldfire_reset(void)
{
}
