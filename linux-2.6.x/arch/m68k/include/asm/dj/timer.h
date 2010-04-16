/****************************************************************************/

/*
 *	dj_timer.h -- HP Deskjet ASIC hardware timer hardware registers
 *
 *	(C) Copyright 2009, Brian S. Julin (bri@abrij.org)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef	dj_timer_h
#define	dj_timer_h

/*****************************************************************************/

/* Registers in DJIO_A_TIMER block */
#define DJIO_A_TIMER_A_PERIOD      0x00  /* word. # counter cycles per irq    */
#define DJIO_A_TIMER_B_PERIOD      0x02  /* word. # counter cycles per irq    */
#define DJIO_A_TIMER_C_PERIOD      0x04  /* word. # counter cycles per irq    */
#define DJIO_A_TIMER_D_PERIOD      0x06  /* word. # counter cycles per irq    */
#define DJIO_A_TIMER_E_PERIOD      0x08  /* word. # counter cycles per irq    */
#define DJIO_A_TIMER_A_SHOT        0x10  /* word. # counter cycles till irq   */
#define DJIO_A_TIMER_B_SHOT        0x12  /* word. # counter cycles till irq   */
#define DJIO_A_TIMER_C_SHOT        0x14  /* word. # counter cycles till irq   */
#define DJIO_A_TIMER_D_SHOT        0x16  /* word. # counter cycles till irq   */
#define DJIO_A_TIMER_E_SHOT        0x18  /* word. # counter cycles till irq   */
#define DJIO_A_TIMER_IRQ_ACK       0x22  /* IRQ source ack                    */
#define DJIO_A_TIMER_IRQ_ENAB      0x24  /* IRQ source enable/disable         */
#define DJIO_A_TIMER_COUNTER_DIV   0x27  /* divides input to counter 	      */
#define DJIO_A_TIMER_COUNTER       0x28  /* 4 bytes containing cycle counter  */

/*
 *  Bitfields in register DJIO_A_TIMER_COUNTER_DIV
 *
 *  Note that writing 0 to the divider field behaves as if 
 *  32 were written.  Bit 5 is not part of the numeric field.
 */
#define DJIO_A_TIMER_COUNTER_DIV_MASK   0x1f /* Put divider in here. 16=1MHZ  */
#define DJIO_A_TIMER_COUNTER_DIV_SET0   0xe0 /* Set these bits to 0           */


/* Bitfields in register DJIO_A_TIMER_IRQ_ACK and DJIO_A_TIMER_ENAB           */
#define DJIO_A_TIMER_IRQ_A         0x01
#define DJIO_A_TIMER_IRQ_B         0x02
#define DJIO_A_TIMER_IRQ_C         0x04
#define DJIO_A_TIMER_IRQ_D         0x08
#define DJIO_A_TIMER_IRQ_E         0x10

/*****************************************************************************/

/* Map of timers to IRQ LINES                                                 */
#define DJIO_A_TIMER_IRQ_A_LINE    0x07
#define DJIO_A_TIMER_IRQ_B_LINE    0x13
#define DJIO_A_TIMER_IRQ_C_LINE    0x06
#define DJIO_A_TIMER_IRQ_D_LINE    0x05
#define DJIO_A_TIMER_IRQ_E_LINE    0x04

/* Valid dividers are 4 to 32.  Below 4 can be unstable on some platforms.    */
#define DJ_COUNTER_FREQ       (16000000/CONFIG_DJ_COUNTER_DIV)
#define DJ_PER_JIFFY_CLICKS   (DJ_COUNTER_FREQ/HZ)

/* Difference between one-shot timer programmed expiry and resulting IRQ      */
#define DJ_TIMER_LAG_CLICKS   (1200/CONFIG_DJ_COUNTER_DIV)

/* Reasonable minimum one-shot delta that will result in accurate expiry      */
#define DJ_TIMER_MIN_DELTA_NS (150000)

#endif	/* dj_timer_h */
