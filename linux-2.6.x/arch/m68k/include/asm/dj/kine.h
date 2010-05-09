
/*****************************************************************************/

/*
 *	dj_kine.h -- Definitions for HP Deskjet kinetic devices
 *
 * 	(C) Copyright 2010 Brian S. Julin (bri@abrij.org)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef	dj_kine_h
#define	dj_kine_h

/* 
 * Registers in DJIO_A_KINE block (plus a few from DJIO_A_DGHT... stay sharp!)
 */

/* Motors */
#define DJIO_A_KINE_BDC0_ENAB	0x22 /* Byte. 1 enable motor/timer           */
#define DJIO_A_KINE_BDC0_OOMF	0x20 /* Byte. motor drive strength           */
#define DJIO_A_KINE_BDC0_DXON	0x23 /* Byte. motor direction                */

#define DJIO_A_KINE_BDC1_ENAB	0x00 /* Byte. 1 enable motor/timer           */
#define DJIO_A_KINE_BDC1_OOMF	0x01 /* Byte. motor drive strength           */
#define DJIO_A_KINE_BDC1_DXON	0x02 /* Byte. motor direction                */

#define DJIO_A_KINE_STP0_ENAB	0x06 /* Byte. 1 enables stepper #0 drive     */
#define DJIO_A_KINE_STP0_OOMF	0x08 /* Word. stepper 0 drive strengths      */
#define DJIO_A_KINE_STP0_DXON	0x07 /* Byte. stepper 0 Coil/dir selects     */

/* Kinetics-dedicated timers */
#define DJIO_A_KINE_STP0_WAIT	0x0a /* Word. Stepper wait timer period      */
#define DJIO_A_KINE_BDC1_WAIT	0x04 /* Word. brushed DC 1 wait timer period */
#define DJIO_A_KINE_BDC0_WAIT	0x24 /* Word. brushed DC 0 wait timer period */

/* Encoders */
#define DJIO_A_KINE_ENC0_ENAB	0x03 /* byte. 1 enables encoder 0 (LED??)    */
#define DJIO_A_KINE_ENC0_SNS0	0x16 /* word. immediate read encoder sensors */
#define DJIO_A_KINE_ENC0_SNS1	0x18 /* word. immediate read encoder sensors */
#define DJIO_A_KINE_ENC0_PULS	0x1a /* byte. run encoder. (4, poll bit 1)   */
#define DJIO_A_KINE_ENC0_CNTL	0x1b /* byte. activates various circuits     */
#define DJIO_A_KINE_ENC0_WTF1	0x13 /* byte. (?)                            */

#define DJIO_A_DGHT_ENC1_WPOS	0x00 /* Word. Write value to regauge RPOS    */
#define DJIO_A_DGHT_ENC1_RPOS	0x02 /* Word. Read postprocessed position    */
#define DJIO_A_DGHT_ENC1_UNK	0x06 /* Word. Do not know what this does     */

/* Encoders differential amp calibration */
#define DJIO_A_KINE_ENC0_RCAL	0x10  /* word. calibration readback          */
#define DJIO_A_KINE_ENC0_CCAL	0x15  /* byte. calibration r/w latching      */
#define DJIO_A_KINE_ENC0_DCAL	0x1c  /* byte. calibration write data        */
#define DJIO_A_KINE_ENC0_ACAL	0x1d  /* byte. calibration write address     */
#define DJIO_A_KINE_ENC0_WTF2	0x21  /* byte. unknown function              */
#define DJIO_A_KINE_ENC0_WTF3	0x31  /* byte. unknown function              */
#define DJIO_A_KINE_E0ZX_ENAB	0x36  /* byte. 1 enables zero-cross circuit  */
#define DJIO_A_KINE_E0ZX_WTF1	0x38  /* byte. unknown, write same as 0x36   */

/* Interrupt sources */
#define DJIO_A_KINE_IRQ_ENAB	0x33  /* byte. bitflags. IRQ source enables. */
#define DJIO_A_KINE_IRQ_ACKN	0x34  /* byte. write-only. IRQ source acks.  */

/* Unknown but essential-to-init registers */
#define DJIO_A_KINE_INIT1_WTF1	0x12  /* byte unknown. initialized to 1      */
#define DJIO_A_KINE_INIT1_WTF2	0x30  /* byte unknown. initialized to 1      */


/*****************************************************************************/

/* Register flags/fields/values                                              */

/* Values for DJIO_A_KINE_BDC0_DXON and DJIO_A_KINE_BDC1_DXON                */
#define DJIO_A_KINE_BDC_DXON_OFF  0x00    /* Coil off                        */
/* NOTE: never drive forward and reverse at once                             */
#define DJIO_A_KINE_BDC_DXON_F    0x01    /* Forward drive                   */
#define DJIO_A_KINE_BDC_DXON_R    0x02    /* Reverse drive                   */

/* 
 * NOTE: sustained stepper drive use may overdrain current and reboot system
 * drive in short bursts 1/4 duty cycle.  But maybe it is just my current test
 * system.  It did sit in the rain for weeks.
 */

/* Values for DJIO_A_KINE_STP0_DXON stepper 0 Coil/dir selects               */
#define DJIO_A_KINE_STP0_DXON_OFF  0x00   /* Drive no coils                  */
/* NOTE: never drive forward and reverse of the same bifilar pair at once    */
#define DJIO_A_KINE_STP0_DXON_B0F  0x01   /* Forward bifilar of pair 0       */
#define DJIO_A_KINE_STP0_DXON_B0R  0x02   /* Reverse bifilar of pair 0       */
#define DJIO_A_KINE_STP0_DXON_B1F  0x04   /* Forward bifilar of pair 1       */
#define DJIO_A_KINE_STP0_DXON_B1R  0x08   /* Reverse bifilar of pair 1       */

/* Values for DJIO_A_KINE_STP0_OOMF Stepper 0 coil drive strengths           */
#define DJIO_A_KINE_STP0_OOMF_B0   0xff00 /* Mask for bifilar 0 level        */
#define DJIO_A_KINE_STP0_OOMF_B1   0x00ff /* Mask for bifilar 1 level        */

/* Values for DJIO_A_KINE_ENC0_CNTL encoder 0 miscellaneous controls         */
#define DJIO_A_KINE_ENC0_CNTL_SNS1 0x04   /* Turns on SNS1 output register   */
#define DJIO_A_KINE_ENC0_CNTL_CH0  0x01   /* Decides what FA13 writes affect */
#define DJIO_A_KINE_ENC0_CNTL_CH1  0x02   /* Decides what FA13 writes affect */

/* Values for DJIO_A_KINE_IRQ_ENAB and DJIO_A_KINE_IRQ_ACKN                  */
#define DJIO_A_KINE_IRQ_BDC0  0x01   /* Brushed DC 0 wait timer tick         */
#define DJIO_A_KINE_IRQ_STP0  0x02   /* Stepper 0 wait timer tick            */
#define DJIO_A_KINE_IRQ_BDC1  0x04   /* Brushed DC 1 wait timer tick         */
#define DJIO_A_KINE_IRQ_E0U2  0x08   /* Encoder 0 unknown function           */
#define DJIO_A_KINE_IRQ_E0ZX  0x20   /* Encoder 0 Zero-crossing detector     */

#define DJIO_A_KINE_IRQ_DISABLE(bits) \
  do {  						      \
    outb(inb(DJIO_A_KINE | DJIO_A_KINE_IRQ_ENAB) & ~(bits),   \
	 DJIO_A_KINE | DJIO_A_KINE_IRQ_ENAB);		      \
  } while (0)

#define DJIO_A_KINE_IRQ_ENABLE(bits) \
  do {  						      \
    outb(inb(DJIO_A_KINE | DJIO_A_KINE_IRQ_ENAB) | (bits),    \
	 DJIO_A_KINE | DJIO_A_KINE_IRQ_ENAB);		      \
  } while (0)

#define DJIO_A_KINE_IRQ_ACK(bits) \
  do {  						      \
    outb((bits), DJIO_A_KINE | DJIO_A_KINE_IRQ_ACKN);	      \
  } while (0)

/* Map of above sources to IRQ lines                                         */
#define DJIO_A_KINE_BDC0_IRQ_LINE  1
#define DJIO_A_KINE_STP0_IRQ_LINE  0
#define DJIO_A_KINE_E0U2_IRQ_LINE  1
#define DJIO_A_KINE_E0ZX_IRQ_LINE  1
#define DJIO_A_KINE_BDC1_IRQ_LINE  2

/* Bitfields in register DJIO_A_KINE_ENC0_PULS                               */
#define DJIO_A_KINE_ENCS_PULS_DO   0x04  /* write. starts an encoder cycle   */
#define DJIO_A_KINE_ENCS_PULS_DONE 0x02  /* read. cycle done, data valid     */
#define DJIO_A_KINE_ENCS_PULS_WTF  0x08  /* do not know what this bit does   */

/* Bitfields in register DJIO_A_KINE_ENC0_SNS0 */
#define DJIO_A_KINE_ENC0_SNS0_DIGI 0xc000 /* Simple on/off values            */
#define DJIO_A_KINE_ENC0_SNS0_UNK0 0x3800 /* do not know what this is        */
#define DJIO_A_KINE_ENC0_SNS0_ANLG 0x01fe /* hw-derived "analog" position    */
#define DJIO_A_KINE_ENC0_SNS0_UNK1 0x0001 /* do not know what this bit does  */


/**
 * DOC: Encoder 0 opamp calibration
 *
 * Each sensor in the encoder is preprocessed by analog circuitry, ADC,
 * and a zero-crossing impulse generator.
 *
 * This circuitry has two gains and two offsets. The procedure to change
 * these values is as follows.
 *
 * 1) Write index to DJIO_A_KINE_ENC0_ACAL to designate which setting to alter.
 * 2) Write the value to assign at the given index to DJIO_A_KINE_ENC0_DCAL
 * 3) Write 2 to DJIO_A_KINE_ENC0_CCAL
 * 4) Wait 5ms before trying to utilize the encoder or change other settings
 *
 * In addition, the (yet to be named) register 0x13 writes an additional
 * configuration value or strobe to one of the two opamps.  Which one it
 * writes to is determined by DJIO_A_KINE_ENC0_CNTL.  The procedure is as
 * follows.
 *
 * 1) Write DJIO_A_KINE_ENC0_CNTL_CH0 or DJIO_A_KINE_ENC0_CNTL_CH1 bits
 *    into DJIO_A_KINE_ENC0_CNTL
 * 2) sleep 5ms before trying to utilize the encoder or change other settings
 * 3) Write two subsequent values to (yet to be named) register 0x13
 *
 * The total effect of these calibrations is has not been determined.
 * The effects of the first set can be seen on immediate reads of the encoder.
 * The effects of the second set seem to be invisible thus far.
 */

/* Indices written to DJIO_A_KINE_ENCO_ACAL to choose settings.              */
#define DJIO_A_KINE_ENC0_ACAL_GAIN0   0
#define DJIO_A_KINE_ENC0_ACAL_GAIN1   2
#define DJIO_A_KINE_ENC0_ACAL_OFFSET0 1
#define DJIO_A_KINE_ENC0_ACAL_OFFSET1 3


#endif	/* dj_kine_h */
