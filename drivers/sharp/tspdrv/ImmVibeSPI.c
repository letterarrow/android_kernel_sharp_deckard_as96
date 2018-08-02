/*
** =============================================================================
**
** File: ImmVibeSPI.c
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
** $Revision: 35184 $
**
** Copyright (c) 2012 Immersion Corporation. All Rights Reserved.
**
** This file contains Original Code and/or Modifications of Original Code
** as defined in and that are subject to the GNU Public License v2 -
** (the 'License'). You may not use this file except in compliance with the
** License. You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
** TouchSenseSales@immersion.com.
**
** The Original Code and all software distributed under the License are
** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
** the License for the specific language governing rights and limitations
** under the License.
**
** =============================================================================
*/
#ifndef CONFIG_SCTSPDRV
#warning ********* Compiling SPI for DRV2604 using LRA actuator ************
#endif /* CONFIG_SCTSPDRV */

#ifdef IMMVIBESPIAPI
#undef IMMVIBESPIAPI
#endif
#define IMMVIBESPIAPI static

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/slab.h>
#include <linux/types.h>

#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/i2c.h>
#include <linux/semaphore.h>
#include <linux/device.h>

#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#include <linux/gpio.h>

#include <linux/workqueue.h>

#ifdef CONFIG_SCTSPDRV 
#include <sharp/sh_smem.h>

#ifdef CONFIG_SHTERM
#include <sharp/shterm_k.h>
#endif /* CONFIG_SHTERM */

#endif /* CONFIG_SCTSPDRV */

/*
** Copy ImmVibeSPI.c, autocal.seq, and init.seq from
** actuator subdirectory into the same directory as tspdrv.c
*/

/*
** Enable workqueue to allow DRV2604 time to brake
*/
#define GUARANTEE_AUTOTUNE_BRAKE_TIME  1

/*
** Enable to use DRV2604 EN pin to enter standby mode
*/
#ifdef CONFIG_SCTSPDRV 
#define USE_DRV2604_EN_PIN  1
#else /* CONFIG_SCTSPDRV */ 
#define USE_DRV2604_EN_PIN  0
#endif /* CONFIG_SCTSPDRV */

/*
** gpio connected to DRV2604 EN pin
*/
#ifdef CONFIG_SCTSPDRV 
#define GPIO_AMP_EN  0x3B
#else /* CONFIG_SCTSPDRV */ 
#define GPIO_AMP_EN  0x00
#endif /* CONFIG_SCTSPDRV */

/*
** Enable to use DRV2604 i2c command to enter standby mode
*/
#define USE_DRV2604_STANDBY 1

/*
** Address of our device
*/
#define DEVICE_ADDR 0x5A

/*
** i2c bus that it sits on
*/
#ifdef CONFIG_SCTSPDRV 
#define DEVICE_BUS  7 
#else /* CONFIG_SCTSPDRV */ 
#define DEVICE_BUS  2
#endif /* CONFIG_SCTSPDRV */

/*
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS 1

/*
** Name of the DRV2604 board
*/
#define DRV2604_BOARD_NAME   "DRV2604"

/*
** Go
*/
#define GO_REG 0x0C
#define GO     0x01
#define STOP   0x00

/*
** Status
*/
#define STATUS_REG          0x00
#define STATUS_DEFAULT      0x00

#define DIAG_RESULT_MASK    (1 << 3)
#define AUTO_CAL_PASSED     (0 << 3)
#define AUTO_CAL_FAILED     (1 << 3)
#define DIAG_GOOD           (0 << 3)
#define DIAG_BAD            (1 << 3)

#define DEV_ID_MASK (7 << 5)
#define DRV2605 (5 << 5)
#define DRV2604 (4 << 5)

/*
** Mode
*/
#define MODE_REG            0x01
#define MODE_STANDBY        0x40

#define DRV2604_MODE_MASK           0x07
#define MODE_INTERNAL_TRIGGER       0
#define MODE_REAL_TIME_PLAYBACK     5
#define MODE_DIAGNOSTICS            6
#define AUTO_CALIBRATION            7

#define MODE_STANDBY_MASK           0x40
#define MODE_READY                  1  /* default */
#define MODE_SOFT_STANDBY           0

#define MODE_RESET                  0x80

/*
** Real Time Playback
*/
#define REAL_TIME_PLAYBACK_REG      0x02

/*
** Library Selection
*/
#define LIBRARY_SELECTION_REG       0x03
#define LIBRARY_SELECTION_DEFAULT   0x00

#define LIBRARY_A 0x01
#define LIBRARY_B 0x02
#define LIBRARY_C 0x03
#define LIBRARY_D 0x04
#define LIBRARY_E 0x05
#define LIBRARY_F 0x06

/*
** Waveform Sequencer
*/
#define WAVEFORM_SEQUENCER_REG      0x04
#define WAVEFORM_SEQUENCER_REG2     0x05
#define WAVEFORM_SEQUENCER_REG3     0x06
#define WAVEFORM_SEQUENCER_REG4     0x07
#define WAVEFORM_SEQUENCER_REG5     0x08
#define WAVEFORM_SEQUENCER_REG6     0x09
#define WAVEFORM_SEQUENCER_REG7     0x0A
#define WAVEFORM_SEQUENCER_REG8     0x0B
#define WAVEFORM_SEQUENCER_MAX      8
#define WAVEFORM_SEQUENCER_DEFAULT  0x00

/*
** OverDrive Time Offset
*/
#define OVERDRIVE_TIME_OFFSET_REG  0x0D

/*
** Sustain Time Offset, postive
*/
#define SUSTAIN_TIME_OFFSET_POS_REG 0x0E

/*
** Sustain Time Offset, negative
*/
#define SUSTAIN_TIME_OFFSET_NEG_REG 0x0F

/*
** Brake Time Offset
*/
#define BRAKE_TIME_OFFSET_REG       0x10

/*
** Audio to Haptics Control
*/
#define AUDIO_HAPTICS_CONTROL_REG   0x11

#define AUDIO_HAPTICS_RECT_10MS     (0 << 2)
#define AUDIO_HAPTICS_RECT_20MS     (1 << 2)
#define AUDIO_HAPTICS_RECT_30MS     (2 << 2)
#define AUDIO_HAPTICS_RECT_40MS     (3 << 2)

#define AUDIO_HAPTICS_FILTER_100HZ  0
#define AUDIO_HAPTICS_FILTER_125HZ  1
#define AUDIO_HAPTICS_FILTER_150HZ  2
#define AUDIO_HAPTICS_FILTER_200HZ  3

/*
** Audio to Haptics Minimum Input Level
*/
#define AUDIO_HAPTICS_MIN_INPUT_REG 0x12

/*
** Audio to Haptics Maximum Input Level
*/
#define AUDIO_HAPTICS_MAX_INPUT_REG 0x13

/*
** Audio to Haptics Minimum Output Drive
*/
#define AUDIO_HAPTICS_MIN_OUTPUT_REG 0x14

/*
** Audio to Haptics Maximum Output Drive
*/
#define AUDIO_HAPTICS_MAX_OUTPUT_REG 0x15

/*
** Rated Voltage
*/
#define RATED_VOLTAGE_REG           0x16

/*
** Overdrive Clamp Voltage
*/
#define OVERDRIVE_CLAMP_VOLTAGE_REG 0x17

/*
** Auto Calibrationi Compensation Result
*/
#define AUTO_CALI_RESULT_REG        0x18

/*
** Auto Calibration Back-EMF Result
*/
#define AUTO_CALI_BACK_EMF_RESULT_REG 0x19

/*
** Feedback Control
*/
#define FEEDBACK_CONTROL_REG        0x1A

#define FEEDBACK_CONTROL_BEMF_LRA_GAIN0 0 /* 5x */
#define FEEDBACK_CONTROL_BEMF_LRA_GAIN1 1 /* 10x */
#define FEEDBACK_CONTROL_BEMF_LRA_GAIN2 2 /* 20x */
#define FEEDBACK_CONTROL_BEMF_LRA_GAIN3 3 /* 30x */

#define LOOP_RESPONSE_SLOW      (0 << 2)
#define LOOP_RESPONSE_MEDIUM    (1 << 2) /* default */
#define LOOP_RESPONSE_FAST      (2 << 2)
#define LOOP_RESPONSE_VERY_FAST (3 << 2)

#define FB_BRAKE_FACTOR_1X   (0 << 4) /* 1x */
#define FB_BRAKE_FACTOR_2X   (1 << 4) /* 2x */
#define FB_BRAKE_FACTOR_3X   (2 << 4) /* 3x (default) */
#define FB_BRAKE_FACTOR_4X   (3 << 4) /* 4x */
#define FB_BRAKE_FACTOR_6X   (4 << 4) /* 6x */
#define FB_BRAKE_FACTOR_8X   (5 << 4) /* 8x */
#define FB_BRAKE_FACTOR_16X  (6 << 4) /* 16x */
#define FB_BRAKE_DISABLED    (7 << 4)

#define FEEDBACK_CONTROL_MODE_ERM 0 /* default */
#define FEEDBACK_CONTROL_MODE_LRA (1 << 7)

/*
** Control1
*/
#define Control1_REG            0x1B

#define STARTUP_BOOST_ENABLED   (1 << 7)
#define STARTUP_BOOST_DISABLED  (0 << 7) /* default */

/*
** Control2
*/
#define Control2_REG            0x1C

#define IDISS_TIME_MASK         0x03
#define IDISS_TIME_VERY_SHORT   0
#define IDISS_TIME_SHORT        1
#define IDISS_TIME_MEDIUM       2 /* default */
#define IDISS_TIME_LONG         3

#define BLANKING_TIME_MASK          0x0C
#define BLANKING_TIME_VERY_SHORT    (0 << 2)
#define BLANKING_TIME_SHORT         (1 << 2)
#define BLANKING_TIME_MEDIUM        (2 << 2) /* default */
#define BLANKING_TIME_VERY_LONG     (3 << 2)

#define AUTO_RES_GAIN_MASK         0x30
#define AUTO_RES_GAIN_VERY_LOW     (0 << 4)
#define AUTO_RES_GAIN_LOW          (1 << 4)
#define AUTO_RES_GAIN_MEDIUM       (2 << 4) /* default */
#define AUTO_RES_GAIN_HIGH         (3 << 4)

#define SOFT_BRAKE_MASK            0x40
#define SOFT_BRAKE                 (1 << 6)

#define BIDIR_INPUT_MASK           0x80
#define UNIDIRECT_INPUT            (0 << 7)
#define BIDIRECT_INPUT             (1 << 7) /* default */

/*
** Control3
*/
#define Control3_REG 0x1D
#define NG_Thresh_1 (1 << 6)
#define NG_Thresh_2 (2 << 6)
#define NG_Thresh_3 (3 << 6)

/*
** Auto Calibration Memory Interface
*/
#define AUTOCAL_MEM_INTERFACE_REG   0x1E

#define AUTOCAL_TIME_150MS          (0 << 4)
#define AUTOCAL_TIME_250MS          (1 << 4)
#define AUTOCAL_TIME_500MS          (2 << 4)
#define AUTOCAL_TIME_1000MS         (3 << 4)

#define SILICON_REVISION_REG        0x3B
#define SILICON_REVISION_MASK       0x07

#define AUDIO_HAPTICS_MIN_INPUT_VOLTAGE     0x19
#define AUDIO_HAPTICS_MAX_INPUT_VOLTAGE     0x64
#define AUDIO_HAPTICS_MIN_OUTPUT_VOLTAGE    0x19
#define AUDIO_HAPTICS_MAX_OUTPUT_VOLTAGE    0xFF

#define DEFAULT_LRA_AUTOCAL_COMPENSATION    0x06
#define DEFAULT_LRA_AUTOCAL_BACKEMF         0xDE

#define DEFAULT_DRIVE_TIME      0x17
#define MAX_AUTOCALIBRATION_ATTEMPT 2

/*
** Rated Voltage:
** Calculated using the formula r = v * 255 / 5.6
** where r is what will be written to the register
** and v is the rated voltage of the actuator.

** Overdrive Clamp Voltage:
** Calculated using the formula o = oc * 255 / 5.6
** where o is what will be written to the register
** and oc is the overdrive clamp voltage of the actuator.
*/
#define SKIP_LRA_AUTOCAL        1

#define GO_BIT_POLL_INTERVAL    15

#define REAL_TIME_PLAYBACK_CALIBRATION_STRENGTH 0x7F /* 100% of rated voltage (closed loop) */
#define LRA_RATED_VOLTAGE                       0x38
#define LRA_OVERDRIVE_CLAMP_VOLTAGE             0x72
#define LRA_DRIVE_TIME                          0x13

#define MAX_REVISION_STRING_SIZE 10

#ifdef CONFIG_SCTSPDRV
#define GPIO_LEVEL_HIGH         1
#define GPIO_LEVEL_LOW          0
#define MODEL_TYPE_D1           0x01
#define MODEL_TYPE_S1           0x02
#endif /* CONFIG_SCTSPDRV */

static int g_nDeviceID = -1;
static struct i2c_client* g_pTheClient = NULL;
static bool g_bAmpEnabled = false;
static bool g_brake = false;

#ifdef CONFIG_SCTSPDRV
static int skip_i2c = 0;

static const unsigned char LRA_autocal_sequence[] = {
#if CONFIG_TSPDRV_MODEL_TYPE == MODEL_TYPE_D1
#include <autocal1030.seq>
#elif CONFIG_TSPDRV_MODEL_TYPE == MODEL_TYPE_S1
#include <autocal0934.seq>
#else
#include <autocal1030.seq>
#endif
};

static const unsigned char LRA_init_sequence[] = {
#if CONFIG_TSPDRV_MODEL_TYPE == MODEL_TYPE_D1
#include <init1030.seq>
#elif CONFIG_TSPDRV_MODEL_TYPE == MODEL_TYPE_S1
#include <init0934.seq>
#else
#include <init1030.seq>
#endif
};

IMMVIBESPIAPI VibeStatus ImmVibeSPI_Set_AutoCal_Data(void);
#else /* CONFIG_SCTSPDRV */

static const unsigned char LRA_autocal_sequence[] = {
#include <autocal.seq>
};

#if SKIP_LRA_AUTOCAL == 1
static const unsigned char LRA_init_sequence[] = {
#include <init.seq>
};
#endif

#endif /* CONFIG_SCTSPDRV */

#ifdef CONFIG_SCTSPDRV
/* adb debug_log */
extern int tspdrv_func_log;
extern int tspdrv_debug_log;
extern int tspdrv_force_log;
extern int tspdrv_error_log;

#define TSPDRV_SPI_FUNC_LOG(fmt, args...) \
	if( tspdrv_func_log == 1 ){ \
		printk( KERN_DEBUG "[TSPDRV][ImmVibeSPI.c][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}
#define TSPDRV_SPI_DBG_LOG(fmt, args...) \
	if( tspdrv_debug_log == 1 ){ \
		printk( KERN_DEBUG "[TSPDRV][ImmVibeSPI.c][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}
#define TSPDRV_SPI_FORCE_LOG(fmt, args...) \
	if( tspdrv_force_log == 1 ){ \
		printk( KERN_DEBUG "[TSPDRV]" fmt "\n",## args ); \
	}
#define TSPDRV_SPI_ERR_LOG(fmt, args...) \
	if( tspdrv_error_log == 1 ){ \
		printk( KERN_DEBUG "[TSPDRV]" fmt "\n",## args ); \
	} \
	else if( tspdrv_error_log == 2 ){ \
		printk( KERN_DEBUG "[TSPDRV][ImmVibeSPI.c][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}
#endif /* CONFIG_SCTSPDRV */


static void drv2604_write_reg_val(const unsigned char* data, unsigned int size)
{
    int i = 0;

    TSPDRV_SPI_FUNC_LOG("START");

#ifdef CONFIG_SCTSPDRV 
    if(skip_i2c == 1)
        return;
#endif /* CONFIG_SCTSPDRV */

    if (size % 2 != 0)
        return;

    while (i < size)
    {
        TSPDRV_SPI_DBG_LOG("i2c write  address = %02X  data = %02X",data[i], data[i+1]);
        i2c_smbus_write_byte_data(g_pTheClient, data[i], data[i+1]);
        i+=2;
    }
    TSPDRV_SPI_FUNC_LOG("END");
}

#ifndef CONFIG_SCTSPDRV
static void drv2604_set_go_bit(char val)
{
    char go[] =
    {
        GO_REG, val
    };
    drv2604_write_reg_val(go, sizeof(go));
}
#endif /* CONFIG_SCTSPDRV */

#ifdef CONFIG_SCTSPDRV
static unsigned char drv2604_i2c_read(struct i2c_client *client,
									int reg, u8 *data, int size)
{
	int rc;
	u8 buf[2];
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags= 0,
			.len  = 1,
			.buf  = buf,
		},
		{
			.addr = client->addr,
			.flags= I2C_M_RD,
			.len  = size,
			.buf  = data,
		}
	};

    TSPDRV_SPI_FUNC_LOG("START");
    
	buf[0] = reg;
	rc = i2c_transfer(client->adapter, msg, 2);
	if(rc != 2){
		TSPDRV_SPI_ERR_LOG("read of register %d", reg);
		rc = -1;
	}

    TSPDRV_SPI_DBG_LOG("i2c read  address = %02X  data = %02X  result = %d",reg,buf[1],rc);
    TSPDRV_SPI_FUNC_LOG("END");
	return rc;
}
#endif /* CONFIG_SCTSPDRV */

static unsigned char drv2604_read_reg(unsigned char reg)
{
    TSPDRV_SPI_FUNC_LOG("START");
    TSPDRV_SPI_DBG_LOG("i2c read  address = %02X",reg);
    TSPDRV_SPI_FUNC_LOG("END");

    return i2c_smbus_read_byte_data(g_pTheClient, reg);
}

static void drv2604_poll_go_bit(void)
{
    TSPDRV_SPI_FUNC_LOG("START");

    while (drv2604_read_reg(GO_REG) == GO)
      schedule_timeout_interruptible(msecs_to_jiffies(GO_BIT_POLL_INTERVAL));

    TSPDRV_SPI_FUNC_LOG("END");
}

static void drv2604_set_rtp_val(char value)
{
    char rtp_val[] =
    {
        REAL_TIME_PLAYBACK_REG, value
    };
    TSPDRV_SPI_FUNC_LOG("START");

    drv2604_write_reg_val(rtp_val, sizeof(rtp_val));

    TSPDRV_SPI_FUNC_LOG("END");
}

static void drv2604_change_mode(char mode)
{
    unsigned char tmp[] =
    {
        MODE_REG, mode
    };
    TSPDRV_SPI_FUNC_LOG("START");

    drv2604_write_reg_val(tmp, sizeof(tmp));

    TSPDRV_SPI_FUNC_LOG("END");
}

static void drv2604_set_en(bool enabled)
{
    TSPDRV_SPI_FUNC_LOG("START");

    gpio_direction_output(GPIO_AMP_EN, enabled ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);

    TSPDRV_SPI_FUNC_LOG("END");
}

static int drv2604_probe(struct i2c_client* client, const struct i2c_device_id* id);
static int drv2604_remove(struct i2c_client* client);
static const struct i2c_device_id drv2604_id[] =
{
    {DRV2604_BOARD_NAME, 0},
    {}
};

static struct i2c_board_info info = {
  I2C_BOARD_INFO(DRV2604_BOARD_NAME, DEVICE_ADDR),
};

static struct i2c_driver drv2604_driver =
{
    .probe = drv2604_probe,
    .remove = drv2604_remove,
    .id_table = drv2604_id,
    .driver =
    {
        .name = DRV2604_BOARD_NAME,
    },
};

static int drv2604_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
    char status;
#if SKIP_LRA_AUTOCAL == 0
    int nCalibrationCount = 0;
#endif

#ifdef CONFIG_SCTSPDRV
    int rc;
#endif /* CONFIG_SCTSPDRV */

    TSPDRV_SPI_FUNC_LOG("START");

  if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
  {
#ifdef CONFIG_SCTSPDRV
    DbgOut((DBL_ERROR, "drv2604 probe failed"));
#else /* CONFIG_SCTSPDRV */
    DbgOut((DBL_ERROR, "drv2605 probe failed"));
#endif /* CONFIG_SCTSPDRV */
    TSPDRV_SPI_FUNC_LOG("END");
    return -ENODEV;
  }

    /* Wait 30 us */
    udelay(30);
    g_pTheClient = client;

#if defined(USE_DRV2604_EN_PIN)
    drv2604_set_en(true);
#endif

#ifdef CONFIG_SCTSPDRV
    rc = drv2604_i2c_read(g_pTheClient,STATUS_REG,&status,1);
    if( rc < 0 ){
		skip_i2c = 1;
	}
#endif /* CONFIG_SCTSPDRV */

#if SKIP_LRA_AUTOCAL == 1

#ifdef CONFIG_SCTSPDRV
#ifdef TSPDRV_USE_SMEM
    ImmVibeSPI_Set_AutoCal_Data();
#else /* TSPDRV_USE_SMEM */
    drv2604_write_reg_val(LRA_init_sequence, sizeof(LRA_init_sequence));
#endif /* TSPDRV_USE_SMEM */
#else /* CONFIG_SCTSPDRV */
    drv2604_write_reg_val(LRA_init_sequence, sizeof(LRA_init_sequence));
#endif /* CONFIG_SCTSPDRV */

#ifndef CONFIG_SCTSPDRV
    status = drv2604_read_reg(STATUS_REG);
#endif /* CONFIG_SCTSPDRV */

#else
    /* Run auto-calibration */
    do{
        drv2604_write_reg_val(LRA_autocal_sequence, sizeof(LRA_autocal_sequence));
        /* Wait until the procedure is done */
        drv2604_poll_go_bit();

        /* Read status */
        status = drv2604_read_reg(STATUS_REG);

        nCalibrationCount++;

    } while (((status & DIAG_RESULT_MASK) == AUTO_CAL_FAILED) && (nCalibrationCount < MAX_AUTOCALIBRATION_ATTEMPT));

    /* Check result */
    if ((status & DIAG_RESULT_MASK) == AUTO_CAL_FAILED)
    {
      DbgOut((DBL_ERROR, "drv2604 auto-calibration failed after %d attempts.\n", nCalibrationCount));
    }
    else
    {
        /* Read calibration results */
        drv2604_read_reg(AUTO_CALI_RESULT_REG);
        drv2604_read_reg(AUTO_CALI_BACK_EMF_RESULT_REG);
        drv2604_read_reg(FEEDBACK_CONTROL_REG);
    }

#endif

    /* Read device ID */
    g_nDeviceID = (status & DEV_ID_MASK);
    switch (g_nDeviceID)
    {
        case DRV2605:
            DbgOut((DBL_INFO, "drv2604 driver found: drv2605.\n"));
            break;
        case DRV2604:
            DbgOut((DBL_INFO, "drv2604 driver found: drv2604.\n"));
            break;
        default:
            DbgOut((DBL_INFO, "drv2604 driver found: unknown.\n"));
            break;
    }

#if USE_DRV2604_STANDBY
    /* Put hardware in standby */
    drv2604_change_mode(MODE_STANDBY);
#elif USE_DRV2604_EN_PIN
    /* enable RTP mode that will be toggled on/off with EN pin */
    drv2604_change_mode(MODE_REAL_TIME_PLAYBACK);
#endif

#ifndef CONFIG_SCTSPDRV
#if USE_DRV2604_EN_PIN
    /* turn off chip */
    drv2604_set_en(false);
#endif
#endif /* CONFIG_SCTSPDRV */

    DbgOut((DBL_INFO, "drv2604 probe succeeded"));

    TSPDRV_SPI_FUNC_LOG("END");

  return 0;
}

static int drv2604_remove(struct i2c_client* client)
{
    TSPDRV_SPI_FUNC_LOG("START");
    DbgOut((DBL_VERBOSE, "drv2604_remove.\n"));
    TSPDRV_SPI_FUNC_LOG("END");
    return 0;
}

#ifdef GUARANTEE_AUTOTUNE_BRAKE_TIME

#define AUTOTUNE_BRAKE_TIME 25

static VibeInt8 g_lastForce = 0;

static void autotune_brake_complete(struct work_struct *work)
{
    TSPDRV_SPI_FUNC_LOG("START");

    /* new nForce value came in before workqueue terminated */
    if (g_lastForce > 0)
        return;

#if USE_DRV2604_STANDBY
    /* Put hardware in standby */
    drv2604_change_mode(MODE_STANDBY);
#endif

#if USE_DRV2604_EN_PIN
    drv2604_set_en(false);
#endif
    TSPDRV_SPI_FUNC_LOG("END");
}

DECLARE_DELAYED_WORK(g_brake_complete, autotune_brake_complete);

static struct workqueue_struct *g_workqueue;

#endif

/*
** Called to disable amp (disable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
    TSPDRV_SPI_FUNC_LOG("START");

    if (g_bAmpEnabled)
    {
        DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpDisable.\n"));

        /* Set the force to 0 */
        drv2604_set_rtp_val(0);

#ifdef GUARANTEE_AUTOTUNE_BRAKE_TIME
        /* if a brake signal arrived from daemon, let the chip stay on
         * extra time to allow it to brake */
        if (g_brake && g_workqueue)
        {
            queue_delayed_work(g_workqueue,
                               &g_brake_complete,
                               msecs_to_jiffies(AUTOTUNE_BRAKE_TIME));
        }
        else /* disable immediately (smooth effect style) */
#endif
        {
#if USE_DRV2604_STANDBY
            /* Put hardware in standby via i2c */
            drv2604_change_mode(MODE_STANDBY);
#endif

#if USE_DRV2604_EN_PIN
            /* Disable hardware via pin */
            drv2604_set_en(false);
#endif
        }

        g_bAmpEnabled = false;
    }
    TSPDRV_SPI_FUNC_LOG("END");

    return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
    TSPDRV_SPI_FUNC_LOG("START");

    if (!g_bAmpEnabled)
    {
        DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpEnable.\n"));

#ifdef GUARANTEE_AUTOTUNE_BRAKE_TIME
        cancel_delayed_work_sync(&g_brake_complete);
#endif

#if USE_DRV2604_EN_PIN
        drv2604_set_en(true);
#endif

#if USE_DRV2604_STANDBY
        drv2604_change_mode(MODE_REAL_TIME_PLAYBACK);
#endif

        g_bAmpEnabled = true;
    }

    TSPDRV_SPI_FUNC_LOG("END");

    return VIBE_S_SUCCESS;
}

/*
** Called at initialization time.
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
    struct i2c_adapter* adapter;
    struct i2c_client* client;

    TSPDRV_SPI_FUNC_LOG("START");

    DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_Initialize.\n"));

    g_bAmpEnabled = true;   /* to force ImmVibeSPI_ForceOut_AmpDisable disabling the amp */

    adapter = i2c_get_adapter(DEVICE_BUS);

#if defined(USE_DRV2604_EN_PIN)
    gpio_request(GPIO_AMP_EN, "vibrator-en");
#endif

    if (adapter) {
        client = i2c_new_device(adapter, &info);

        if (client) {
            int retVal = i2c_add_driver(&drv2604_driver);

            if (retVal) {
               TSPDRV_SPI_FUNC_LOG("END");
                return VIBE_E_FAIL;
            }

        } else {
            DbgOut((DBL_VERBOSE, "drv2605: Cannot create new device.\n"));
            TSPDRV_SPI_FUNC_LOG("END");
            return VIBE_E_FAIL;
        }

    } else {
        DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpDisable.\n"));
        TSPDRV_SPI_FUNC_LOG("END");

        return VIBE_E_FAIL;
    }

    ImmVibeSPI_ForceOut_AmpDisable(0);

#ifdef GUARANTEE_AUTOTUNE_BRAKE_TIME
    g_workqueue = create_workqueue("tspdrv_workqueue");
#endif

    TSPDRV_SPI_FUNC_LOG("END");
 
    return VIBE_S_SUCCESS;
}

/*
** Called at termination time to disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
    TSPDRV_SPI_FUNC_LOG("START");

    DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_Terminate.\n"));

#ifdef GUARANTEE_AUTOTUNE_BRAKE_TIME
    if (g_workqueue)
    {
        destroy_workqueue(g_workqueue);
        g_workqueue = 0;
    }
#endif

    ImmVibeSPI_ForceOut_AmpDisable(0);

    /* Remove TS5000 driver */
    i2c_del_driver(&drv2604_driver);

    /* Reverse i2c_new_device */
    i2c_unregister_device(g_pTheClient);

    TSPDRV_SPI_FUNC_LOG("END");

    return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set the force
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{

#ifdef GUARANTEE_AUTOTUNE_BRAKE_TIME
    VibeInt8 force = pForceOutputBuffer[0];

    TSPDRV_SPI_FUNC_LOG("START");
    TSPDRV_SPI_FORCE_LOG("force = %d",pForceOutputBuffer[0]);

    if (force > 0 && g_lastForce <= 0)
    {
        g_brake = false;

#ifdef CONFIG_SCTSPDRV
#ifdef CONFIG_SHTERM
        shterm_k_set_info( SHTERM_INFO_VIB, 1 );
#endif /* CONFIG_SHTERM */
#endif /* CONFIG_SCTSPDRV */

        ImmVibeSPI_ForceOut_AmpEnable(nActuatorIndex);
    }
    else if (force <= 0 && g_lastForce > 0)
    {
        g_brake = force < 0;

        ImmVibeSPI_ForceOut_AmpDisable(nActuatorIndex);

#ifdef CONFIG_SCTSPDRV
#ifdef CONFIG_SHTERM
        shterm_k_set_info( SHTERM_INFO_VIB, 0 );
#endif /* CONFIG_SHTERM */
#endif /* CONFIG_SCTSPDRV */

    }

    if (g_lastForce != force)
    {
        /* AmpDisable sets force to zero, so need to here */
        if (force > 0)
            drv2604_set_rtp_val(pForceOutputBuffer[0]);

        g_lastForce = force;
    }
#else
    TSPDRV_SPI_FUNC_LOG("START");
    TSPDRV_SPI_FORCE_LOG("force = %d",pForceOutputBuffer[0]);

    drv2604_set_rtp_val(pForceOutputBuffer[0]);
#endif

    TSPDRV_SPI_FUNC_LOG("END");

    return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
    TSPDRV_SPI_FUNC_LOG("START");

    if (nActuatorIndex != 0) return VIBE_S_SUCCESS;

    switch (nFrequencyParameterID)
    {
        case VIBE_KP_CFG_FREQUENCY_PARAM1:
            /* Update frequency parameter 1 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM2:
            /* Update frequency parameter 2 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM3:
            /* Update frequency parameter 3 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM4:
            /* Update frequency parameter 4 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM5:
            /* Update frequency parameter 5 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM6:
            /* Update frequency parameter 6 */
            break;
    }

    TSPDRV_SPI_FUNC_LOG("END");

    return VIBE_S_SUCCESS;
}

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
    char szRevision[MAX_REVISION_STRING_SIZE];

    TSPDRV_SPI_FUNC_LOG("START");

    if ((!szDevName) || (nSize < 1)) return VIBE_E_FAIL;

    DbgOut((DBL_VERBOSE, "ImmVibeSPI_Device_GetName.\n"));

    switch (g_nDeviceID)
    {
        case DRV2605:
            strncpy(szDevName, "DRV2605", nSize-1);
            break;
        case DRV2604:
            strncpy(szDevName, "DRV2604", nSize-1);
            break;
        default:
            strncpy(szDevName, "Unknown", nSize-1);
            break;
    }

    /* Append revision number to the device name */
    sprintf(szRevision, " Rev:%d", (drv2604_read_reg(SILICON_REVISION_REG) & SILICON_REVISION_MASK));
    if ((strlen(szRevision) + strlen(szDevName)) < nSize-1)
        strcat(szDevName, szRevision);

    szDevName[nSize - 1] = '\0'; /* make sure the string is NULL terminated */

    TSPDRV_SPI_FUNC_LOG("END");

    return VIBE_S_SUCCESS;
}

#ifdef CONFIG_SCTSPDRV

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_GetDeviceID(unsigned char *tspresult)
{
	u8 status;
	int rc;

    TSPDRV_SPI_FUNC_LOG("START");

    if (tspresult == NULL) {
        TSPDRV_SPI_ERR_LOG("tspresult NULL");
        TSPDRV_SPI_FUNC_LOG("END");
        return -1;
    }

	rc = drv2604_i2c_read(g_pTheClient,STATUS_REG,&status,1);

	if(rc < 0)
	{
        TSPDRV_SPI_FUNC_LOG("END");
		return VIBE_E_FAIL;
	}

	*tspresult = ( status >> 5 );

    TSPDRV_SPI_FUNC_LOG("END");

    return VIBE_S_SUCCESS;
}

IMMVIBESPIAPI VibeStatus ImmVibeSPI_AutoCalExec(struct tspdrv_autocal_presetting_data acal_setting_data)
{

    char status = 0;
    int nCalibrationCount = 0;
    unsigned char fbctl_reg_temp;

    unsigned char autocal_sequence[] = {
#if CONFIG_TSPDRV_MODEL_TYPE == MODEL_TYPE_D1
#include <autocal1030.seq>
#elif CONFIG_TSPDRV_MODEL_TYPE == MODEL_TYPE_S1
#include <autocal0934.seq>
#else
#include <autocal1030.seq>
#endif
    };

    TSPDRV_SPI_FUNC_LOG("START");

    /* auto-calibration time setting */
    autocal_sequence[11] = acal_setting_data.acal_time << 4;

    /* register setting */
    if( acal_setting_data.ratedvoltage != 0x00 || acal_setting_data.odclamp != 0x00 || acal_setting_data.fbctrl != 0x00 )
    {
        /* RATED_VOLTAGE_REG */
        autocal_sequence[3] = acal_setting_data.ratedvoltage;

        /* OVERDRIVE_CLAMP_VOLTAGE_REG */
        autocal_sequence[5] = acal_setting_data.odclamp;

        if(drv2604_i2c_read(g_pTheClient,STATUS_REG,&fbctl_reg_temp,1) < 0)
        {
            TSPDRV_SPI_ERR_LOG("drv2604 auto-calibration i2c error");
            TSPDRV_SPI_FUNC_LOG("END");
            return VIBE_E_FAIL;
        }

        fbctl_reg_temp = (fbctl_reg_temp & 0x03) | ( acal_setting_data.fbctrl & 0xFC );

        /* FEEDBACK_CONTROL_REG */
        autocal_sequence[7] = fbctl_reg_temp;
    }

    /* Run auto-calibration */
    do{
        drv2604_write_reg_val(autocal_sequence, sizeof(autocal_sequence));
        /* Wait until the procedure is done */
        drv2604_poll_go_bit();

        /* Read status */
        if(drv2604_i2c_read(g_pTheClient,STATUS_REG,&status,1) < 0)
        {
            TSPDRV_SPI_ERR_LOG("drv2604 auto-calibration i2c error");
            TSPDRV_SPI_FUNC_LOG("END");
            return VIBE_E_FAIL;
        }

        nCalibrationCount++;

    } while (((status & DIAG_RESULT_MASK) == AUTO_CAL_FAILED) && (nCalibrationCount < MAX_AUTOCALIBRATION_ATTEMPT));

    /* Check result */
    if ((status & DIAG_RESULT_MASK) == AUTO_CAL_FAILED)
    {
        TSPDRV_SPI_ERR_LOG("drv2604 auto-calibration failed after %d attempts. status = 0x%02X", nCalibrationCount,status);
        TSPDRV_SPI_FUNC_LOG("END");
        return VIBE_E_FAIL;
    }

    TSPDRV_SPI_FUNC_LOG("END");

	return VIBE_S_SUCCESS;

}

IMMVIBESPIAPI VibeStatus ImmVibeSPI_Get_AutoCal_Result(struct tspdrv_autocal_data *acal_data)
{
    TSPDRV_SPI_FUNC_LOG("START");

    if (acal_data == NULL) {
        TSPDRV_SPI_ERR_LOG("acal_data NULL");
        TSPDRV_SPI_FUNC_LOG("END");
        return VIBE_E_FAIL;
    }

	acal_data->acalcomp = drv2604_read_reg(AUTO_CALI_RESULT_REG);
	acal_data->acalbemf = drv2604_read_reg(AUTO_CALI_BACK_EMF_RESULT_REG);
	acal_data->bemfgain = drv2604_read_reg(FEEDBACK_CONTROL_REG) & 0x03;

    TSPDRV_SPI_FUNC_LOG("END");

    return VIBE_S_SUCCESS;
}


IMMVIBESPIAPI VibeStatus ImmVibeSPI_Set_AutoCal_Data(void)
{

    unsigned char tspdrv_acal_data[3];
    sharp_smem_common_type *p_sh_smem_common_type = NULL;

    unsigned char LRA_autocal_init_sequence[] = {
#if CONFIG_TSPDRV_MODEL_TYPE == MODEL_TYPE_D1
#include <init1030.seq>
#elif CONFIG_TSPDRV_MODEL_TYPE == MODEL_TYPE_S1
#include <init0934.seq>
#else
#include <init1030.seq>
#endif
    };

    TSPDRV_SPI_FUNC_LOG("START");

    memset((void*)tspdrv_acal_data, 0x00, sizeof(tspdrv_acal_data));
    p_sh_smem_common_type = sh_smem_get_common_address();
    if (p_sh_smem_common_type != NULL) {
        memcpy(tspdrv_acal_data, p_sh_smem_common_type->shdiag_tspdrv_acal_data, sizeof(tspdrv_acal_data));
        TSPDRV_SPI_ERR_LOG("smem read data = 0x%02X 0x%02X 0x%02X",tspdrv_acal_data[0],tspdrv_acal_data[1],tspdrv_acal_data[2]);
        if( ((tspdrv_acal_data[0] != 0x00) || (tspdrv_acal_data[1] != 0x00) || (tspdrv_acal_data[2] != 0x00))
        && ((tspdrv_acal_data[0] != 0xFF) || (tspdrv_acal_data[1] != 0xFF) || (tspdrv_acal_data[2] != 0xFF)))
        {
            LRA_autocal_init_sequence[9]  = tspdrv_acal_data[0];
            LRA_autocal_init_sequence[11] = tspdrv_acal_data[1];
            LRA_autocal_init_sequence[13] = LRA_autocal_init_sequence[13] | tspdrv_acal_data[2];
            drv2604_write_reg_val(LRA_autocal_init_sequence, sizeof(LRA_autocal_init_sequence));
        }
        else
        {
            drv2604_write_reg_val(LRA_init_sequence, sizeof(LRA_init_sequence));
        }
    }
    else
    {
        drv2604_write_reg_val(LRA_init_sequence, sizeof(LRA_init_sequence));
    }

    TSPDRV_SPI_FUNC_LOG("END");

    return VIBE_S_SUCCESS;
}


#endif /* CONFIG_SCTSPDRV */
