/* drivers/sharp/shfmtx/shfmtx_si4710-cmd.c
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
/* CONFIG_SH_AUDIO_DRIVER newly created */ /* 03-007 */

/* ------------------------------------------------------------------------- */
/* DEBUG MACRAOS                                                             */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <sharp/shfmtx_si4710.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include "shfmtx_si4710-cmd.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define MAX_ARGS 7

#define msb(x)                  ((u8)((u16) x >> 8))
#define lsb(x)                  ((u8)((u16) x &  0x00FF))
#define compose_u16(msb, lsb)   (((u16)msb << 8) | lsb)

#define PORT_GPO2  (69)

struct vregs_info {
    const char * const name;
    int state;
    const int nominal_min;
    const int low_power_min;
    const int max_voltage;
    const int uA_load;
    struct regulator *regulator;
};

struct si4710_revision_info {
    u8  part_num;
    u8  firm_major;
    u8  firm_minor;
    u16 patch;
    u8  comp_major;
    u8  comp_minor;
    u8  chip_rev; 
};

struct si4710_tune_status {
    u16 frequency;
    u8  power;
    u8  antcap;
};

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static struct i2c_client *this_client = NULL;
static struct shfmtx_platform_data *pdata;

static DEFINE_MUTEX(shfmtx_lock);
static int shfmtx_si4710_opened;

static u8 PoweredUp = 0;
static u8 Muted     = 0;

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/******************************************************************************
 * shfmtx_si4710_cmd_send_command
******************************************************************************/
static int shfmtx_si4710_cmd_send_command(const u8 command,
                const u8 args[], const int argn,
                u8 response[], const int respn, const int usecs)
{
    u8 data1[MAX_ARGS + 1];
    int ret = 0;
#if defined( SH_FMTX_DEBUG )
    int i = 0;
#endif

    if ((!this_client) || (!this_client->adapter)) {
        pr_err("%s: client is NULL.\n", __func__);
        return -ENXIO;
    }
    if ((argn < 0) || (MAX_ARGS < argn)) {
        pr_err("%s: arg size is invalid.\n", __func__);
        return -EINVAL;
    }
    if (respn < 0) {
        pr_err("%s: resp size is invalid.\n", __func__);
        return -EINVAL;
    }

    pr_debug("%s: -START-. command[0x%02X].\n", __func__, command);

    data1[0] = command;
    memcpy(data1 + 1, args, argn);

    ret = i2c_master_send(this_client, data1, argn + 1);
    if (ret != argn + 1) {
        pr_err("%s: command send failed.\n", __func__);
        if (ret > 0) {
            ret = -EIO;
        }
    }
    else {
        pr_debug("%s: wait response start.\n", __func__);
        usleep(usecs);
        pr_debug("%s: wait response end.\n", __func__);

        ret = i2c_master_recv(this_client, response, respn);
        if (ret != respn) {
            pr_err("%s: response recv failed.\n", __func__);
            if (ret > 0) {
                ret = -EIO;
            }
        }
        else {
            if ( !(response[0] & SI4710_CTS) ||
                  (response[0] & SI4710_ERR) ) {
                pr_err("%s: response status is invalid.\n", __func__);
                ret = -EIO;
            }
            else {
                ret = 0;
            }
        }
    }

#if defined( SH_FMTX_DEBUG )
    for(i = 0; i < respn; i++)
    {
        pr_debug("%s: response[%d] %d", __func__, i, response[i]);
    }
#endif

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_set_property
******************************************************************************/
static int shfmtx_si4710_cmd_set_property(u16 propNumber, u16 propValue)
{
    u8 args[SI4710_SET_PROP_NARGS];
    u8 resp[SI4710_SET_PROP_NRESP];
    int ret = 0;

    pr_debug("%s: -START-. property[0x%04X], value[0x%04X].\n", __func__, propNumber, propValue);
    
    args[0] = 0;
    args[1] = (u8)(propNumber >> 8);
    args[2] = (u8)(propNumber & 0x00FF);
    args[3] = (u8)(propValue >> 8);
    args[4] = (u8)(propValue & 0x00FF);

    ret = shfmtx_si4710_cmd_send_command(SI4710_CMD_SET_PROPERTY,
                                   args, ARRAY_SIZE(args),
                                   resp, ARRAY_SIZE(resp),
                                   TIMEOUT_SET_PROPERTY);

    if (ret != 0) {
        pr_err("%s: set property failed.\n", __func__);
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_get_property
******************************************************************************/
static int shfmtx_si4710_cmd_get_property(u16 propNumber, u16* pOutVal)
{
    u8 args[SI4710_GET_PROP_NARGS];
    u8 resp[SI4710_GET_PROP_NRESP];
    int ret = 0;

    if (!pOutVal) {
        pr_err("%s: pOutVal is NULL.\n", __func__);
        return -EINVAL;
    }

    pr_debug("%s: -START-. property[0x%04X].\n", __func__, propNumber);
    
    args[0] = 0;
    args[1] = (u8)(propNumber >> 8);
    args[2] = (u8)(propNumber & 0x00FF);

    ret = shfmtx_si4710_cmd_send_command(SI4710_CMD_GET_PROPERTY,
                                   args, ARRAY_SIZE(args),
                                   resp, ARRAY_SIZE(resp),
                                   DEFAULT_TIMEOUT);

    if (ret != 0) {
        pr_err("%s: get property failed.\n", __func__);
    }
    else {
        *pOutVal = compose_u16(resp[2], resp[3]);
        pr_debug("%s: get property success. val[0x%04X].\n", __func__, *pOutVal);
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_set_property
******************************************************************************/
static int shfmtx_si4710_set_property(u16 propNumber, u16 propValue)
{
    int ret = 0;
    u16 getVal = 0;

    pr_debug("%s: -START-. property[0x%04X], value[0x%04X].\n", __func__, propNumber, propValue);

    ret = shfmtx_si4710_cmd_set_property(propNumber, propValue);
    if (ret != 0) {
        pr_err("%s: set property failed.\n", __func__);
    }
    else {
        ret = shfmtx_si4710_cmd_get_property(propNumber, &getVal);
        if (ret != 0) {
            pr_err("%s: get property failed.\n", __func__);
        }
        else {
            if (propValue != getVal) {
                pr_err("%s: set param and get param are different.\n", __func__);
                ret = -EIO;
            }
        }
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_powerup
******************************************************************************/
static int shfmtx_si4710_cmd_powerup(void)
{
    u8 args[SI4710_PWUP_NARGS];
    u8 resp[SI4710_PWUP_NRESP];
    int ret = 0;

    pr_debug("%s: -START-.\n", __func__);

    if (!PoweredUp) {
        args[0] = SI4710_PWUP_GPO2OEN | SI4710_PWUP_FUNC_TX;
        args[1] = SI4710_PWUP_OPMOD_ANALOG;

        ret = shfmtx_si4710_cmd_send_command(SI4710_CMD_POWER_UP, 
                                   args, ARRAY_SIZE(args),
                                   resp, ARRAY_SIZE(resp),
                                   TIMEOUT_POWER_UP);

        if (ret != 0) {
            pr_err("%s: powerup failed.\n", __func__);
        }
        else {
            PoweredUp = 1;
        }
    }
    else {
        pr_err("%s: already power on.\n", __func__);
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_powerdown
******************************************************************************/
int shfmtx_si4710_cmd_powerdown(void)
{
    u8 resp[SI4710_PWDN_NRESP];
    int ret = 0;

#if 0
    struct vregs_info regulators;
#endif

    struct pm_gpio param = {
        .direction      = PM_GPIO_DIR_OUT,
        .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
        .output_value   = 0,
        .pull           = PM_GPIO_PULL_UP_30,
        .vin_sel        = PM_GPIO_VIN_S4,
        .out_strength   = PM_GPIO_STRENGTH_MED,
        .function       = PM_GPIO_FUNC_NORMAL,
    };

    pr_debug("%s: -START-.\n", __func__);
    mutex_lock(&shfmtx_lock);

    if(PoweredUp) {
        ret = shfmtx_si4710_cmd_send_command(SI4710_CMD_POWER_DOWN,
                                   NULL, 0,
                                   resp, ARRAY_SIZE(resp),
                                   DEFAULT_TIMEOUT);

        if (ret != 0) {
            pr_err("%s: powerdown failed.\n", __func__);
        }
        else {

            /* RSTB */
            usleep(1);

            ret = gpio_request(pdata->shfmtx_gpio_reset, "shfmtx");
            if (ret != 0)
            {
                pr_err("%s: Failed to request gpio %d\n", __func__,
                        pdata->shfmtx_gpio_reset);
            }

            ret = pm8xxx_gpio_config(pdata->shfmtx_gpio_reset, &param);
            if (ret != 0)
            {
                pr_err("%s: Failed to configure gpio %d\n", __func__,
                        pdata->shfmtx_gpio_reset);
            }
            else
            {
                pr_debug("%s: gpio_direction_output.\n", __func__);
                gpio_direction_output(pdata->shfmtx_gpio_reset, 0);
            }
            usleep(1);

            gpio_free(pdata->shfmtx_gpio_reset);

#if 0
            /* VDD off */
            regulators.regulator = regulator_get(NULL, "8921_l10");
            pr_debug("%s: regulator_get. %p\n", __func__, regulators.regulator);
            if (!IS_ERR(regulators.regulator))
            {
                ret = regulator_disable(regulators.regulator);
                if(ret)
                {
                    pr_err("%s: regulator_disable.\n", __func__);
                }
                regulator_put(regulators.regulator);
            }
            else
            {
                ret = -EIO;
                pr_err("%s: regulator_get failed.\n", __func__);
            }
#endif

            if (ret == 0) {
                PoweredUp = 0;
            }
        }
    }
    else {
        pr_err("%s: already power off.\n", __func__);
    }
    
    mutex_unlock(&shfmtx_lock);
    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_get_rev
******************************************************************************/
int shfmtx_si4710_cmd_get_rev(u32* revision)
{
    int ret = 0;
    u8 resp[SI4710_GETREV_NRESP];
    struct si4710_revision_info rev;

    pr_debug("%s: -START-.\n", __func__);
    mutex_lock(&shfmtx_lock);

    if(PoweredUp) {
        ret = shfmtx_si4710_cmd_send_command(SI4710_CMD_GET_REV,
                        NULL, 0,
                        resp, ARRAY_SIZE(resp),
                        DEFAULT_TIMEOUT);

        if (ret != 0) {
            pr_err("%s: get revision failed.\n", __func__);
        }
        else {
            rev.part_num   = resp[1];
            rev.firm_major = resp[2];
            rev.firm_minor = resp[3];
            rev.patch      = compose_u16(resp[4], resp[5]);
            rev.comp_major = resp[6];
            rev.comp_minor = resp[7];
            rev.chip_rev   = resp[8];
            pr_debug( "%s :PartNum[0x%02X], FirmwareMajor[0x%02X], FirmwareMinor[0x%02X], Patch[0x%04X].\n",
                             __func__, rev.part_num, rev.firm_major, rev.firm_minor, rev.patch);
            pr_debug( "    CompMajor[0x%02X], CompMinor[0x%02X], ChipRev[0x%02X].\n",
                             rev.comp_major, rev.comp_minor, rev.chip_rev);

            *revision = (rev.part_num << 16) | (rev.firm_major << 8) | rev.chip_rev;
        }
    }
    else {
        pr_err("%s: get revision failed. power is off.\n", __func__);
        ret = -EPERM;
    }

    mutex_unlock(&shfmtx_lock);
    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_set_preemphasis
******************************************************************************/
int shfmtx_si4710_cmd_set_preemphasis(u16 param)
{
    int ret = 0;

    pr_debug("%s: -START-.\n", __func__);
    mutex_lock(&shfmtx_lock);
    
    if(PoweredUp) {
        ret = shfmtx_si4710_set_property(SI4710_TX_PREEMPHASIS, param);
        if (ret != 0) {
            pr_err("%s: set preemphasis failed.\n", __func__);
        }
    }
    else {
        pr_err("%s: : set preemphasis failed. power is off.\n", __func__);
        ret = -EPERM;
    }
    
    mutex_unlock(&shfmtx_lock);
    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}


/******************************************************************************
 * shfmtx_si4710_setup
******************************************************************************/
static int shfmtx_si4710_setup(void)
{

#if 0
    struct vregs_info regulators;
#endif
    unsigned gpio_config_gpo2;
    int ret = 0;

    struct pm_gpio param = {
        .direction      = PM_GPIO_DIR_OUT,
        .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
        .output_value   = 0,
        .pull           = PM_GPIO_PULL_UP_30,
        .vin_sel        = PM_GPIO_VIN_S4,
        .out_strength   = PM_GPIO_STRENGTH_MED,
        .function       = PM_GPIO_FUNC_NORMAL,
    };

    pr_debug("%s: -START-.\n", __func__);

#if 0
    /* VDD */
    regulators.regulator = regulator_get(NULL, "8921_l10");
    if (IS_ERR(regulators.regulator))
    {
        pr_err("%s: regulator_get err \n", __func__);
        ret = -EIO;
        goto func_end;
    }
    pr_debug("%s: regulator_get.%p \n", __func__, regulators.regulator);
    ret = regulator_set_voltage(regulators.regulator, 2900000, 2900000);
    if (ret != 0) {
        pr_err("%s: regulator_set_voltage err.\n", __func__);
        goto err_regulator_L10;
    }

    ret = regulator_set_optimum_mode(regulators.regulator, 515000);
    if (ret < 0) {
        pr_err("%s: regulator_set_optimum_mode err. \n", __func__);
        goto err_regulator_L10;
    }

    ret = regulator_enable(regulators.regulator);
    if (ret != 0) {
        pr_err("%s: regulator_enable err. \n", __func__);
        goto err_regulator_L10;
    }
    usleep(50);
#endif

    /* gpo2 */
    ret = gpio_request(PORT_GPO2, "shfmgpo2");
    if (ret != 0) {
        pr_err("%s: Failed to request gpio gpo2 \n", __func__);
        goto func_end;
    }

    gpio_config_gpo2 = GPIO_CFG( PORT_GPO2, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA );
    ret = gpio_tlmm_config( gpio_config_gpo2, 0/*GPIO_ENABLE*/ );
    if (ret != 0) {
        pr_err("%s: Failed to gpio_tlmm_config gpo2 \n", __func__);
        goto err_free_gpo2;
    }
    usleep(10);
    
    /* RSTB */
    ret = gpio_request(pdata->shfmtx_gpio_reset, "shfmtx");
    if (ret != 0) {
        pr_err("%s: Failed to request gpio %d\n", __func__,
                pdata->shfmtx_gpio_reset);
        goto err_free_gpo2;
    }

    ret = pm8xxx_gpio_config(pdata->shfmtx_gpio_reset, &param);
    if (ret != 0){
        pr_err("%s: Failed to configure gpio %d\n", __func__,
                pdata->shfmtx_gpio_reset);
        goto err_free_rstb;
    }
    else{
        pr_debug("%s: gpio_direction_output.\n", __func__);
        gpio_direction_output(pdata->shfmtx_gpio_reset, 1);
    }
    usleep(1);

err_free_rstb:
    gpio_free(pdata->shfmtx_gpio_reset);
err_free_gpo2:
    gpio_free(PORT_GPO2);

#if 0
err_regulator_L10:
    regulator_put(regulators.regulator);
#endif
func_end:
    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_initialize
******************************************************************************/
int shfmtx_si4710_cmd_initialize(void)
{
    int ret = 0;

    if (!pdata) {
        pr_err("%s: no platform data!\n", __func__);
        return -EIO;
    }

    pr_debug("%s: -START-.\n", __func__);
    mutex_lock(&shfmtx_lock);

    if (!PoweredUp) {
        ret = shfmtx_si4710_setup();
        if (ret != 0) {
            pr_err("%s: si4710 setup error.\n", __func__);
            goto func_end;
        }

        ret = shfmtx_si4710_cmd_powerup();
        if (ret != 0) {
            pr_err("%s: power up failed.\n", __func__);
            goto func_end;
        }

        PoweredUp = 1;
        Muted = 0;
    }
    else {
        pr_err("%s: already power on.\n", __func__);
    }

func_end:
    mutex_unlock(&shfmtx_lock);
    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_tx_tune_status
******************************************************************************/
static int shfmtx_si4710_cmd_tx_tune_status(struct si4710_tune_status *pOutput)
{
    int ret = 0;
    u8 resp[SI4710_TXSTATUS_NRESP];
    const u8 args[SI4710_TXSTATUS_NARGS] = {
        0x00
    };
    
    if (!pOutput) {
        pr_err("%s: pOutput is NULL.\n", __func__);
        return -EINVAL;
    }

    pr_debug("%s: -START-.\n", __func__);

    ret = shfmtx_si4710_cmd_send_command(SI4710_CMD_TX_TUNE_STATUS,
                  args, ARRAY_SIZE(args), 
                  resp, ARRAY_SIZE(resp), 
                  DEFAULT_TIMEOUT);

    if (ret != 0) {
        pr_err("%s: get tune status  failed.\n", __func__);
    }
    else {
        pOutput->frequency = compose_u16(resp[2], resp[3]);
        pOutput->power     = resp[5];
        pOutput->antcap    = resp[6];
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_tx_tune_freq
******************************************************************************/
static int shfmtx_si4710_cmd_tx_tune_freq(u16 frequency)
{
    int ret = 0;
    u8 resp[SI4710_TXFREQ_NRESP];
    const u8 args[SI4710_TXFREQ_NARGS] = {
        0x00,
        msb(frequency),
        lsb(frequency)
    };

    pr_debug("%s: -START- frequency=%d.\n", __func__, frequency);

    ret = shfmtx_si4710_cmd_send_command(SI4710_CMD_TX_TUNE_FREQ,
                  args, ARRAY_SIZE(args), 
                  resp, ARRAY_SIZE(resp), 
                  TIMEOUT_TX_TUNE);

    if (ret != 0) {
        pr_err("%s: set frequency failed.\n", __func__);
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_tx_tune_power
******************************************************************************/
static int shfmtx_si4710_cmd_tx_tune_power(u8 power, u8 antcap)
{
    int ret = 0;
    u8 resp[SI4710_TXPWR_NRESP];
    const u8 args[SI4710_TXPWR_NARGS] = {
        0x00,
        0x00,
        power,
        antcap,
    };

    pr_debug("%s: -START- power=%d antcap=%d.\n",
        __func__, power, antcap);

    ret = shfmtx_si4710_cmd_send_command(SI4710_CMD_TX_TUNE_POWER,
                  args, ARRAY_SIZE(args), 
                  resp, ARRAY_SIZE(resp), 
                  TIMEOUT_TX_TUNE_POWER);

    if (ret != 0) {
        pr_err("%s: set power failed.\n", __func__);
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_set_power
******************************************************************************/
int shfmtx_si4710_cmd_set_power(u8 power, u8 antcap)
{
    int ret = 0;
    struct si4710_tune_status status;

    pr_debug("%s: -START- power=%d antcap=%d.\n",
        __func__, power, antcap);
    mutex_lock(&shfmtx_lock);

    if(PoweredUp) {
        ret = shfmtx_si4710_cmd_tx_tune_power(power, antcap);
        if (ret != 0) {
            pr_err("%s: set power failed.\n", __func__);
        }
        else {
            ret = shfmtx_si4710_cmd_tx_tune_status(&status);
            if (ret != 0) {
                pr_err("%s: get power failed.\n", __func__);
            }
            else {
                if (status.power != power) {
                    pr_err("%s: set power and get power are different.\n", __func__);
                    ret = -EIO;
                }
                
                // Set antcap zero is auto-setting.
                if ((antcap != 0) && (status.antcap != antcap)) {
                    pr_err("%s: set antcap and get antcap are different.\n", __func__);
                    ret = -EIO;
                }
            }
        }
    }
    else {
        pr_err("%s: : set power failed. power is off.\n", __func__);
        ret = -EPERM;
    }

    mutex_unlock(&shfmtx_lock);
    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_get_power
******************************************************************************/
int shfmtx_si4710_cmd_get_power(u8* pOutPower, u8* pOutAntcap)
{
    int ret = 0;
    struct si4710_tune_status status;

    pr_debug("%s: -START-.\n", __func__);
    mutex_lock(&shfmtx_lock);

    if(PoweredUp) {
        ret = shfmtx_si4710_cmd_tx_tune_status(&status);
        if (ret != 0) {
            pr_err("%s: get frequency failed.\n", __func__);
        }
        else {
            if (pOutPower) {
                *pOutPower = status.power;
            }
            if (pOutAntcap) {
                *pOutAntcap = status.antcap;
            }
        }
    }
    else {
        pr_err("%s: : get power failed. power is off.\n", __func__);
        ret = -EPERM;
    }

    mutex_unlock(&shfmtx_lock);
    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_set_freq
******************************************************************************/
int shfmtx_si4710_cmd_set_freq(u16 frequency)
{
    int ret = 0;
    struct si4710_tune_status status;

    pr_debug("%s: -START- frequency=%d.\n", __func__, frequency);
    mutex_lock(&shfmtx_lock);

    if(PoweredUp) {
        ret = shfmtx_si4710_cmd_tx_tune_freq(frequency);
        if (ret != 0) {
            pr_err("%s: set frequency failed.\n", __func__);
        }
        else {
            ret = shfmtx_si4710_cmd_tx_tune_status(&status);
            if (ret != 0) {
                pr_err("%s: get frequency failed.\n", __func__);
            }
            else {
                if (status.frequency != frequency) {
                    pr_err("%s: set frequency and get frequency are different.\n", __func__);
                    ret = -EIO;
                }
            }
        }
    }
    else {
        pr_err("%s: : set frequency failed. power is off.\n", __func__);
        ret = -EPERM;
    }

    mutex_unlock(&shfmtx_lock);
    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_get_freq
******************************************************************************/
int shfmtx_si4710_cmd_get_freq(u16* pOutVal)
{
    int ret = 0;
    struct si4710_tune_status status;

    pr_debug("%s: -START-.\n", __func__);
    mutex_lock(&shfmtx_lock);

    if(PoweredUp) {
        ret = shfmtx_si4710_cmd_tx_tune_status(&status);
        if (ret != 0) {
            pr_err("%s: get frequency failed.\n", __func__);
        }
        else {
            *pOutVal = status.frequency;
        }
    }
    else {
        pr_err("%s: : get frequency failed. power is off.\n", __func__);
        ret = -EPERM;
    }

    mutex_unlock(&shfmtx_lock);
    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_set_mute
******************************************************************************/
int shfmtx_si4710_cmd_set_mute(u16 mute)
{
    int ret = 0;

    pr_debug("%s: -START-. param = 0x%02x.\n", __func__, mute);
    mutex_lock(&shfmtx_lock);

    if(PoweredUp) {
        if (Muted != mute) {
            ret = shfmtx_si4710_set_property(SI4710_TX_LINE_INPUT_MUTE, mute);
            if (ret != 0) {
                pr_err("%s: set mute failed.\n", __func__);
            }
            else {
                Muted = mute;
            }
        }
        else {
            pr_err("%s: set mute skip. same setting.\n", __func__);
        }
    }
    else {
        pr_err("%s: set mute failed. power is off.\n", __func__);
        ret = -EPERM;
    }

    mutex_unlock(&shfmtx_lock);
    pr_debug("%s: -END-. ret[%d]\n", __func__, ret);
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_open
******************************************************************************/
static int shfmtx_si4710_cmd_open(struct inode *inode, struct file *file)
{
    int rc = 0;
    pr_debug("%s: -START-.\n", __func__);

    mutex_lock(&shfmtx_lock);

    if (shfmtx_si4710_opened) {
        pr_err("%s: busy\n", __func__);
        rc = -EBUSY;
        goto done;
    }

    shfmtx_si4710_opened = 1;
done:
    mutex_unlock(&shfmtx_lock);
    pr_debug("%s: -END-. ret[%d]\n", __func__, rc);
    return rc;
}

/******************************************************************************
 * shfmtx_si4710_cmd_release
******************************************************************************/
static int shfmtx_si4710_cmd_release(struct inode *inode, struct file *file)
{

    pr_debug("%s: -START-.\n", __func__);

    mutex_lock(&shfmtx_lock);
    shfmtx_si4710_opened = 0;
    mutex_unlock(&shfmtx_lock);
    
    pr_debug("%s: -END-.\n", __func__);
    return 0;
}
static struct file_operations shfmtx_si4710_fops = {
    .owner   = THIS_MODULE,
    .open    = shfmtx_si4710_cmd_open,
    .release = shfmtx_si4710_cmd_release,
};
static struct miscdevice shfmtx_si4710_cmd_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "shfmtx_si4710",
    .fops = &shfmtx_si4710_fops,
};

/******************************************************************************
 * shfmtx_si4710_cmd_probe
******************************************************************************/
static int shfmtx_si4710_cmd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    pdata = client->dev.platform_data;

    if (!pdata) {
        pr_err("%s: platform data is NULL.\n", __func__);
        return -EINVAL;
    }

    pr_debug("%s: -START-.\n", __func__);

    this_client = client;

    ret = misc_register(&shfmtx_si4710_cmd_device);
    if (ret != 0) {
        pr_err("%s: shfmtx_si4710_cmd_device register failed.\n", __func__);
    }

    pr_debug("%s: -END-. ret[%d]\n", __func__, ret);
    return ret;
}

/******************************************************************************
 * shfmtx_si4710_cmd_remove
******************************************************************************/
static int shfmtx_si4710_cmd_remove(struct i2c_client *client)
{
    pr_debug("%s: -START-.\n", __func__);
    this_client = i2c_get_clientdata(client);
    pr_debug("%s: -END-.\n", __func__);
    return 0;
}

static const struct i2c_device_id shfmtx_si4710_id[] = {
    { SHFMTX_I2C_NAME, 0 },
    { }
};

static struct i2c_driver shfmtx_si4710_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name  = SHFMTX_I2C_NAME,
    },
    .class    = I2C_CLASS_HWMON,
    .probe    = shfmtx_si4710_cmd_probe,
    .id_table = shfmtx_si4710_id,
    .remove   = shfmtx_si4710_cmd_remove,
};

/******************************************************************************
 * shfmtx_si4710_cmd_init
******************************************************************************/
static int __init shfmtx_si4710_cmd_init(void)
{
    pr_debug("%s: called.\n", __func__);
    return i2c_add_driver(&shfmtx_si4710_driver);
}

module_init(shfmtx_si4710_cmd_init);

MODULE_DESCRIPTION("shfmtx_si4710 driver");
MODULE_LICENSE("GPL");
