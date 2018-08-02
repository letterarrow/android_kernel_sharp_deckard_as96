/* drivers/sharp/shfmtx/shfmtx_si4710-cmd.h
 *
 * Copyright (C) 2012 SHARP CORPORATION
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/* CONFIG_SH_AUDIO_DRIVER newly created */

#ifndef __ASM_ARM_ARCH_SHFMTX_SI4710_CMD_H
#define __ASM_ARM_ARCH_SHFMTX_SI4710_CMD_H

#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define SHFMTX_CMD_LEN              (8)

/********************************************************
 * Command Timeouts 
********************************************************/
#define DEFAULT_TIMEOUT             (500)
#define TIMEOUT_SET_PROPERTY        (11000)
#define TIMEOUT_TX_TUNE_POWER       (30000)
#define TIMEOUT_TX_TUNE             (110000)
#define TIMEOUT_POWER_UP            (150000)

/********************************************************
 * Command and its arguments definitions
********************************************************/
#define SI4710_PWUP_CTSIEN          (1<<7)
#define SI4710_PWUP_GPO2OEN         (1<<6)
#define SI4710_PWUP_PATCH           (1<<5)
#define SI4710_PWUP_XOSCEN          (1<<4)
#define SI4710_PWUP_FUNC_TX         (0x02)
#define SI4710_PWUP_FUNC_PATCH      (0x0F)
#define SI4710_PWUP_OPMOD_ANALOG    (0x50)
#define SI4710_PWUP_OPMOD_DIGITAL   (0x0F)
#define SI4710_PWUP_NARGS           (2)
#define SI4710_PWUP_NRESP           (1)
#define SI4710_CMD_POWER_UP         (0x01)

#define SI4710_GETREV_NRESP         (9)
#define SI4710_CMD_GET_REV          (0x10)

#define SI4710_PWDN_NRESP           (1)
#define SI4710_CMD_POWER_DOWN       (0x11)

#define SI4710_SET_PROP_NARGS       (5)
#define SI4710_SET_PROP_NRESP       (1)
#define SI4710_CMD_SET_PROPERTY     (0x12)

#define SI4710_GET_PROP_NARGS       (3)
#define SI4710_GET_PROP_NRESP       (4)
#define SI4710_CMD_GET_PROPERTY     (0x13)

#define SI4710_GET_STATUS_NRESP     (1)
#define SI4710_CMD_GET_INT_STATUS   (0x14)

#define SI4710_CMD_PATCH_ARGS       (0x15)
#define SI4710_CMD_PATCH_DATA       (0x16)

#define SI4710_MAX_FREQ             (10800)
#define SI4710_MIN_FREQ             (7600)
#define SI4710_TXFREQ_NARGS         (3)
#define SI4710_TXFREQ_NRESP         (1)
#define SI4710_CMD_TX_TUNE_FREQ     (0x30)

#define SI4710_MAX_POWER            (118)
#define SI4710_MIN_POWER            (88)
#define SI4710_MAX_ANTCAP           (191)
#define SI4710_MIN_ANTCAP           (0)
#define SI4710_TXPWR_NARGS          (4)
#define SI4710_TXPWR_NRESP          (1)
#define SI4710_CMD_TX_TUNE_POWER    (0x31)

#define SI4710_TXMEA_NARGS          (4)
#define SI4710_TXMEA_NRESP          (1)
#define SI4710_CMD_TX_TUNE_MEASURE  (0x32)

#define SI4710_INTACK_MASK          (0x01)
#define SI4710_TXSTATUS_NARGS       (1)
#define SI4710_TXSTATUS_NRESP       (8)
#define SI4710_CMD_TX_TUNE_STATUS   (0x33)

#define SI4710_OVERMOD_BIT          (1<<2)
#define SI4710_IALH_BIT             (1<<1)
#define SI4710_IALL_BIT             (1<<0)
#define SI4710_ASQSTATUS_NARGS      (1)
#define SI4710_ASQSTATUS_NRESP      (5)
#define SI4710_CMD_TX_ASQ_STATUS    (0x34)

#define SI4710_RDSBUFF_MODE_MASK    (0x87)
#define SI4710_RDSBUFF_NARGS        (7)
#define SI4710_RDSBUFF_NRESP        (6)
#define SI4710_CMD_TX_RDS_BUFF      (0x35)

#define SI4710_RDSPS_PSID_MASK      (0x1F)
#define SI4710_RDSPS_NARGS          (5)
#define SI4710_RDSPS_NRESP          (1)
#define SI4710_CMD_TX_RDS_PS        (0x36)

#define SI4710_CMD_GPO_CTL          (0x80)
#define SI4710_CMD_GPO_SET          (0x81)

/********************************************************
 * GPO_IEN
********************************************************/
#define GPO_IEN                     (0x0001)
#define GPO_IEN_STCIEN_MASK         (0x0001)
#define GPO_IEN_ASQIEN_MASK         (0x0002)
#define GPO_IEN_RDSIEN_MASK         (0x0004)
#define GPO_IEN_SAMEIEN_MASK        (0x0004)
#define GPO_IEN_RSQIEN_MASK         (0x0008)
#define GPO_IEN_ERRIEN_MASK         (0x0040)
#define GPO_IEN_CTSIEN_MASK         (0x0080)
#define GPO_IEN_STCREP_MASK         (0x0100)
#define GPO_IEN_ASQREP_MASK         (0x0200)
#define GPO_IEN_RDSREP_MASK         (0x0400)
#define GPO_IEN_RSQREP_MASK         (0x0800)
#define GPO_IEN_STCIEN_SHFT         (0)
#define GPO_IEN_ASQIEN_SHFT         (1)
#define GPO_IEN_RDSIEN_SHFT         (2)
#define GPO_IEN_SAMEIEN_SHFT        (2)
#define GPO_IEN_RSQIEN_SHFT         (3)
#define GPO_IEN_ERRIEN_SHFT         (6)
#define GPO_IEN_CTSIEN_SHFT         (7)
#define GPO_IEN_STCREP_SHFT         (8)
#define GPO_IEN_ASQREP_SHFT         (9)
#define GPO_IEN_RDSREP_SHFT         (10)
#define GPO_IEN_RSQREP_SHFT         (11)

/********************************************************
 * Bits from status response
********************************************************/
#define SI4710_CTS                  (1<<7)
#define SI4710_ERR                  (1<<6)
#define SI4710_RDS_INT              (1<<2)
#define SI4710_ASQ_INT              (1<<1)
#define SI4710_STC_INT              (1<<0)

/********************************************************
 * Property definitions
********************************************************/
#define SI4710_GPO_IEN                  (0x0001)
#define SI4710_DIG_INPUT_FORMAT         (0x0101)
#define SI4710_DIG_INPUT_SAMPLE_RATE    (0x0103)
#define SI4710_REFCLK_FREQ              (0x0201)
#define SI4710_REFCLK_PRESCALE          (0x0202)
#define SI4710_TX_COMPONENT_ENABLE      (0x2100)
#define SI4710_TX_AUDIO_DEVIATION       (0x2101)
#define SI4710_TX_PILOT_DEVIATION       (0x2102)
#define SI4710_TX_RDS_DEVIATION         (0x2103)
#define SI4710_TX_LINE_INPUT_LEVEL      (0x2104)
#define SI4710_TX_LINE_INPUT_MUTE       (0x2105)
#define SI4710_TX_PREEMPHASIS           (0x2106)
#define SI4710_TX_PILOT_FREQUENCY       (0x2107)
#define SI4710_TX_ACOMP_ENABLE          (0x2200)
#define SI4710_TX_ACOMP_THRESHOLD       (0x2201)
#define SI4710_TX_ACOMP_ATTACK_TIME     (0x2202)
#define SI4710_TX_ACOMP_RELEASE_TIME    (0x2203)
#define SI4710_TX_ACOMP_GAIN            (0x2204)
#define SI4710_TX_LIMITER_RELEASE_TIME  (0x2205)
#define SI4710_TX_ASQ_INTERRUPT_SOURCE  (0x2300)
#define SI4710_TX_ASQ_LEVEL_LOW         (0x2301)
#define SI4710_TX_ASQ_DURATION_LOW      (0x2302)
#define SI4710_TX_ASQ_LEVEL_HIGH        (0x2303)
#define SI4710_TX_ASQ_DURATION_HIGH     (0x2304)
#define SI4710_TX_RDS_INTERRUPT_SOURCE  (0x2C00)
#define SI4710_TX_RDS_PI                (0x2C01)
#define SI4710_TX_RDS_PS_MIX            (0x2C02)
#define SI4710_TX_RDS_PS_MISC           (0x2C03)
#define SI4710_TX_RDS_PS_REPEAT_COUNT   (0x2C04)
#define SI4710_TX_RDS_PS_MESSAGE_COUNT  (0x2C05)
#define SI4710_TX_RDS_PS_AF             (0x2C06)
#define SI4710_TX_RDS_FIFO_SIZE         (0x2C07)

#define MAX_RDS_PS_NAME                 (96)

#define TX_PREEMPHASIS_75               (0x00)
#define TX_PREEMPHASIS_50               (0x01)

#define TX_UNMUTE                       (0x00)
#define TX_RIMUTE                       (0x01)
#define TX_LIMUTE                       (0x02)
#define TX_MUTE                         (0x03)

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
extern int shfmtx_si4710_cmd_powerdown(void);
extern int shfmtx_si4710_cmd_get_rev(u32* revision);
extern int shfmtx_si4710_cmd_initialize(void);
extern int shfmtx_si4710_cmd_set_freq(u16 frequency);
extern int shfmtx_si4710_cmd_get_freq(u16* pOutVal);
extern int shfmtx_si4710_cmd_set_power(u8 power, u8 antcap);
extern int shfmtx_si4710_cmd_get_power(u8* pOutPower, u8* pOutAntcap);
extern int shfmtx_si4710_cmd_set_mute(u16 mute);
extern int shfmtx_si4710_cmd_set_preemphasis(u16 param);

#endif
