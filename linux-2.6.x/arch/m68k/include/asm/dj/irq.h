/* dj_irq.h -- Deskjet ASIC IRQ controller registers and bitfields
 *
 *	(C) Copyright 2009, Brian S. Julin (bri@abrij.org)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

/****************************************************************************/
#ifndef	dj_irq_h
#define	dj_irq_h
/****************************************************************************/

/* Registers in DJIO_A_IRQ interrupt control register block                   */
#define DJIO_A_IRQ_GLOBAL         (DJIO_A_IRQ + 0x00) /* Global control       */
#define DJIO_A_IRQ_IRQN           (DJIO_A_IRQ + 0x02) /* Per-line registers   */

/* Values for register DJIO_A_IRQ_GLOBAL                                      */
#define DJIO_A_IRQ_GLOBAL_DISABLE   0x01
#define DJIO_A_IRQ_GLOBAL_ENABLE    0x00
#define DJIO_A_IRQ_GLOBAL_UNKNOWN   0x78 /* These bits change during use      */

/* Bitfields in register DJIO_A_IRQ_IRQN and subsequent per-line registers    */
#define DJIO_A_IRQ_IRQN_PRIO_MASK   0x07  /* IRQ priority + enable            */
#define DJIO_A_IRQ_IRQN_DISABLE     0x00  /* Priority 0 turns IRQ off         */
#define DJIO_A_IRQ_IRQN_ACK         0x80  /* set by hw, clear to ack          */

/* Access of per-line registers */
#define DJIO_A_IRQ_NIRQ             0x13  /* there may be more ???            */
#define DJIO_A_IRQ_N(N)             (DJIO_A_IRQ_IRQN + (N))

/* Offset of IRQ lines numbers to system vector numbers */
#define DJIO_IRQ_BASE		    0x40

/****************************************************************************/
#endif	/* dj_irq_h */
