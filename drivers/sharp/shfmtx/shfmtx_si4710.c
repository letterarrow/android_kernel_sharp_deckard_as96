/* drivers/sharp/shfmtx/shfmtx_si4710.c
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

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include <linux/version.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <linux/param.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/atomic.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <asm/unaligned.h>
#include <sharp/shfmtx_si4710.h>
#include "shfmtx_si4710-cmd.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define DRIVER_NAME "shfmtx_si4710"
#define TUNE_PARAM  (160)

#define TUNE_EVENT           (1) /* Tune completion event  */
#define EVENT_LISTEN         (1)
#define CAL_DATA_BUF         (10)

/*#define V4L2_CID_PRIVATE_BASE 0x08000000*/
#define V4L2_CID_PRIVATE_SHFMTX_SRCHMODE (V4L2_CID_PRIVATE_BASE + 0x001)
#define V4L2_CID_PRIVATE_SHFMTX_SCANDWELL (V4L2_CID_PRIVATE_BASE + 0x002)
#define V4L2_CID_PRIVATE_SHFMTX_SRCHON (V4L2_CID_PRIVATE_BASE + 0x003)
#define V4L2_CID_PRIVATE_SHFMTX_STATE (V4L2_CID_PRIVATE_BASE + 0x004)
#define V4L2_CID_PRIVATE_SHFMTX_TRANSMIT_MODE (V4L2_CID_PRIVATE_BASE + 0x005)
#define V4L2_CID_PRIVATE_SHFMTX_RDSGROUP_MASK (V4L2_CID_PRIVATE_BASE + 0x006)
#define V4L2_CID_PRIVATE_SHFMTX_REGION (V4L2_CID_PRIVATE_BASE + 0x007)
#define V4L2_CID_PRIVATE_SHFMTX_SIGNAL_TH (V4L2_CID_PRIVATE_BASE + 0x008)
#define V4L2_CID_PRIVATE_SHFMTX_SRCH_PTY (V4L2_CID_PRIVATE_BASE + 0x009)
#define V4L2_CID_PRIVATE_SHFMTX_SRCH_PI (V4L2_CID_PRIVATE_BASE + 0x00A)
#define V4L2_CID_PRIVATE_SHFMTX_SRCH_CNT (V4L2_CID_PRIVATE_BASE + 0x00B)
#define V4L2_CID_PRIVATE_SHFMTX_EMPHASIS (V4L2_CID_PRIVATE_BASE + 0x00C)
#define V4L2_CID_PRIVATE_SHFMTX_RDS_STD (V4L2_CID_PRIVATE_BASE + 0x00D)
#define V4L2_CID_PRIVATE_SHFMTX_SPACING (V4L2_CID_PRIVATE_BASE + 0x00E)
#define V4L2_CID_PRIVATE_SHFMTX_RDSON (V4L2_CID_PRIVATE_BASE + 0x00F)
#define V4L2_CID_PRIVATE_SHFMTX_RDSGROUP_PROC (V4L2_CID_PRIVATE_BASE + 0x010)
#define V4L2_CID_PRIVATE_SHFMTX_LP_MODE (V4L2_CID_PRIVATE_BASE + 0x011)
#define V4L2_CID_PRIVATE_SHFMTX_ANTENNA (V4L2_CID_PRIVATE_BASE + 0x012)
#define V4L2_CID_PRIVATE_SHFMTX_RDSD_BUF (V4L2_CID_PRIVATE_BASE + 0x013)
#define V4L2_CID_PRIVATE_SHFMTX_PSALL (V4L2_CID_PRIVATE_BASE + 0x014)
#define V4L2_CID_PRIVATE_SHFMTX_TX_SETPSREPEATCOUNT (V4L2_CID_PRIVATE_BASE + 0x015)
#define V4L2_CID_PRIVATE_SHFMTX_STOP_RDS_TX_PS_NAME (V4L2_CID_PRIVATE_BASE + 0x016)
#define V4L2_CID_PRIVATE_SHFMTX_STOP_RDS_TX_RT (V4L2_CID_PRIVATE_BASE + 0x017)
#define V4L2_CID_PRIVATE_SHFMTX_IOVERC (V4L2_CID_PRIVATE_BASE + 0x018)
#define V4L2_CID_PRIVATE_SHFMTX_INTDET (V4L2_CID_PRIVATE_BASE + 0x019)
#define V4L2_CID_PRIVATE_SHFMTX_MPX_DCC (V4L2_CID_PRIVATE_BASE + 0x01A)
#define V4L2_CID_PRIVATE_SHFMTX_AF_JUMP (V4L2_CID_PRIVATE_BASE + 0x01B)
#define V4L2_CID_PRIVATE_SHFMTX_RSSI_DELTA (V4L2_CID_PRIVATE_BASE + 0x01C)
#define V4L2_CID_PRIVATE_SHFMTX_HLSI (V4L2_CID_PRIVATE_BASE + 0x01D)
#define V4L2_CID_PRIVATE_SHFMTX_SOFT_MUTE (V4L2_CID_PRIVATE_BASE + 0x01E)
#define V4L2_CID_PRIVATE_SHFMTX_RIVA_ACCS_ADDR (V4L2_CID_PRIVATE_BASE + 0x01F)
#define V4L2_CID_PRIVATE_SHFMTX_RIVA_ACCS_LEN (V4L2_CID_PRIVATE_BASE + 0x020)
#define V4L2_CID_PRIVATE_SHFMTX_RIVA_PEEK (V4L2_CID_PRIVATE_BASE + 0x021)
#define V4L2_CID_PRIVATE_SHFMTX_RIVA_POKE (V4L2_CID_PRIVATE_BASE + 0x022)
#define V4L2_CID_PRIVATE_SHFMTX_SSBI_ACCS_ADDR (V4L2_CID_PRIVATE_BASE + 0x023)
#define V4L2_CID_PRIVATE_SHFMTX_SSBI_PEEK (V4L2_CID_PRIVATE_BASE + 0x024)
#define V4L2_CID_PRIVATE_SHFMTX_SSBI_POKE (V4L2_CID_PRIVATE_BASE + 0x025)
#define V4L2_CID_PRIVATE_SHFMTX_TX_TONE (V4L2_CID_PRIVATE_BASE + 0x026)
#define V4L2_CID_PRIVATE_SHFMTX_RDS_GRP_COUNTERS (V4L2_CID_PRIVATE_BASE + 0x027)
#define V4L2_CID_PRIVATE_SHFMTX_SET_NOTCH_FILTER (V4L2_CID_PRIVATE_BASE + 0x028)
#define V4L2_CID_PRIVATE_SHFMTX_SET_AUDIO_PATH (V4L2_CID_PRIVATE_BASE + 0x029)
#define V4L2_CID_PRIVATE_SHFMTX_DO_CALIBRATION (V4L2_CID_PRIVATE_BASE + 0x02A)
#define V4L2_CID_PRIVATE_SHFMTX_SRCH_ALGORITHM (V4L2_CID_PRIVATE_BASE + 0x02B)
#define V4L2_CID_PRIVATE_SHFMTX_GET_SINR (V4L2_CID_PRIVATE_BASE + 0x02C)

enum radio_state_t {
    FM_OFF = 0,
    FM_RECV,
    FM_TRANS,
    FM_RESET,
};

struct antcap_table {
    int frequency;
    u8  antcap;
};

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static unsigned int rds_buf = 100;
module_param(rds_buf, uint, 0);
MODULE_PARM_DESC(rds_buf, "RDS buffer entries: *100*");

static u8 tune_completion = 0;

struct antcap_table antcap_setting_table[] = {
#ifdef CONFIG_SH_AUDIO_DRIVER /* 03-006 */
    {7740, 0x50},
    {7830, 0x4b},
    {7900, 0x49},
    {8410, 0x34},
    {8500, 0x30}
#else
    {7740, 0x28},
    {7830, 0x24},
    {7900, 0x1e},
    {8410, 0x06},
    {8500, 0x04}
#endif /* CONFIG_SH_AUDIO_DRIVER */ /* 03-006 */
};

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/*********************************************************************
 * shfmtx_si4710_vidioc_getAntcap
*********************************************************************/
static u8 shfmtx_si4710_vidioc_getAntcap(int frequency) {
    int i;
    int table_size;
    u8 antcap = SI4710_MIN_ANTCAP;
    
    pr_debug( "%s: -START-. freqency %d.\n", __func__, frequency );
    
    table_size = sizeof(antcap_setting_table) / sizeof(struct antcap_table);
    
    for(i=0; i<table_size; i++) {
        if (antcap_setting_table[i].frequency <= frequency) {
            antcap = antcap_setting_table[i].antcap;
        }
        else {
            break;
        }
    }
    
    pr_debug( "%s: -END-. return %d.\n", __func__, antcap );
    return antcap;
}

/*********************************************************************
 * shfmtx_si4710_vidioc_queryctrl
*********************************************************************/
static int shfmtx_si4710_vidioc_queryctrl(struct file *file, void *priv,
        struct v4l2_queryctrl *qc)
{   
    pr_debug( "%s: -START-.\n", __func__ );
    pr_debug( "%s: -END-.\n", __func__ );
    return 0;
}

/*********************************************************************
 * shfmtx_si4710_vidioc_s_ext_ctrls
*********************************************************************/
static int shfmtx_si4710_vidioc_s_ext_ctrls(struct file *file, void *priv,
            struct v4l2_ext_controls *ctrl)
{
    pr_debug( "%s: -START-.\n", __func__ );
    pr_debug( "%s: -END-.\n", __func__ );
    return 0;
}

/*********************************************************************
 * shfmtx_si4710_vidioc_s_tune_power
*********************************************************************/
static int shfmtx_si4710_vidioc_s_tune_power(const int power)
{
    int ret = 0;
    int settingFreq;
    u8 antcap;

    pr_debug("%s: -START-. power=%d.\n", __func__, power);

    if ((power < SI4710_MIN_POWER) || (SI4710_MAX_POWER < power)) {
        pr_err( "%s: error. tune power is out of range.\n", __func__ );
        ret = -EINVAL;
    }
    else {
        ret = shfmtx_si4710_cmd_get_freq((u16*)&settingFreq);
        if (ret != 0) {
            pr_err( "%s: error. Si4710 get frequency failed.\n", __func__ );
        }
        else {
            antcap = shfmtx_si4710_vidioc_getAntcap(settingFreq);
            ret = shfmtx_si4710_cmd_set_power(power, antcap);
            if (ret != 0) {
                pr_err( "%s: error. Si4710 set tune power failed.\n", __func__ );
            }
        }
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/*********************************************************************
 * shfmtx_si4710_vidioc_set_mute
*********************************************************************/
static int shfmtx_si4710_vidioc_set_mute(const u16 mute)
{
    int ret = 0;

    pr_debug("%s: -START-. mute=%d.\n", __func__, mute);

    if (TX_MUTE < mute) {
        pr_err( "%s: error. mute param is out of range.\n", __func__ );
        ret = -EINVAL;
    }
    else {
        ret = shfmtx_si4710_cmd_set_mute(mute);
        if (ret != 0) {
            pr_err( "%s: error. Si4710 set mute failed.\n", __func__ );
        }
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/*********************************************************************
 * shfmtx_si4710_vidioc_set_state
*********************************************************************/
static int shfmtx_si4710_vidioc_set_state(const int state) 
{
    int ret = 0;

    pr_debug("%s: -START-, state=%d.\n", __func__, state );

    switch(state) {
    case FM_OFF:
        ret = shfmtx_si4710_cmd_powerdown();
        if (ret != 0) {
            pr_err( "%s: error. Si4710 powerdown failed.\n", __func__ );
        }
        break;
    case FM_RECV:
    case FM_TRANS:
    case FM_RESET:
        break;
    default:
        break;
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/*********************************************************************
 * shfmtx_si4710_vidioc_set_preemphasis
*********************************************************************/
static int shfmtx_si4710_vidioc_set_preemphasis(const u16 param) 
{
    int ret = 0;

    pr_debug("%s: -START-, param=%d.\n", __func__, param );

    if ( (param != TX_PREEMPHASIS_50) && (param != TX_PREEMPHASIS_75) ) {
        pr_err( "%s: invalid param.\n", __func__ );
        ret = -EINVAL;
    }
    else {
        ret = shfmtx_si4710_cmd_set_preemphasis(param);
        if (ret != 0) {
            pr_err( "%s: error. Si4710 set preemphasis failed.\n", __func__ );
        }
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/*********************************************************************
 * shfmtx_si4710_vidioc_s_ctrl
*********************************************************************/
static int shfmtx_si4710_vidioc_s_ctrl(struct file *file, void *priv,
        struct v4l2_control *ctrl)
{
    int ret = 0;

    if (!ctrl) {
        pr_err("%s: error. ctrl is NULL.\n", __func__ );
        return -EINVAL;
    }

    pr_debug("%s: -START-, id=0x%x.\n", __func__, ctrl->id );

    switch (ctrl->id) {
    case V4L2_CID_TUNE_POWER_LEVEL:
        pr_debug("  V4L2_CID_TUNE_POWER_LEVEL.\n");
        ret = shfmtx_si4710_vidioc_s_tune_power(ctrl->value);
        break;
    case V4L2_CID_AUDIO_MUTE:
        pr_debug("  V4L2_CID_AUDIO_MUTE.\n");
        ret = shfmtx_si4710_vidioc_set_mute(ctrl->value);
        break;
    case V4L2_CID_PRIVATE_SHFMTX_STATE:
        pr_debug("  V4L2_CID_PRIVATE_SHFMTX_STATE.\n");
        ret = shfmtx_si4710_vidioc_set_state(ctrl->value);
        break;
    case V4L2_CID_PRIVATE_SHFMTX_EMPHASIS:
        pr_debug("  V4L2_CID_PRIVATE_SHFMTX_EMPHASIS.\n");
        ret = shfmtx_si4710_vidioc_set_preemphasis(ctrl->value);
        break;
    default:
        break;
    }
    
    if (ret != 0) {
        pr_err( "%s: error. Si4710 set ctrl failed.\n", __func__ );
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/*********************************************************************
 * shfmtx_si4710_vidioc_s_tuner
*********************************************************************/
static int shfmtx_si4710_vidioc_s_tuner(struct file *file, void *priv,
        struct v4l2_tuner *tuner)
{
    pr_debug( "%s: -START-.\n", __func__ );
    pr_debug( "%s: -END-.\n", __func__ );
    return 0;
}

/*********************************************************************
 * shfmtx_si4710_vidioc_s_tune_frequency
*********************************************************************/
static int shfmtx_si4710_vidioc_s_tune_frequency(struct file *file, void *priv,
                    struct v4l2_frequency *freq)
{
    int ret = 0;
    int ret2;
    u16 frequency = 0;
    u8 power, antcap;

    if (!freq) {
        pr_err( "%s: error. freq is NULL.\n", __func__ );
        return -EINVAL;
    }

    pr_debug( "%s: -START-, frequency=%d.\n", __func__, freq->frequency );

    frequency = freq->frequency / TUNE_PARAM;

    if ((frequency < SI4710_MIN_FREQ) || (SI4710_MAX_FREQ < frequency)) {
        pr_err( "%s: error. frequency is out of range.\n", __func__ );
        ret = -EINVAL;
    }
    else {
        ret = shfmtx_si4710_cmd_set_freq(frequency);
        if (ret != 0) {
            pr_err( "%s: error. Si4710 set frequency failed.\n", __func__ );
        }
        else {
            tune_completion = 1;
        }
    }
    
    if (tune_completion == 1) {
        ret2 = shfmtx_si4710_cmd_get_power(&power, NULL);
        if (ret2 != 0) {
            pr_err( "%s: error. Si4710 get power failed.\n", __func__ );
        }
        else {
            antcap = shfmtx_si4710_vidioc_getAntcap(frequency);
            ret2 = shfmtx_si4710_cmd_set_power(power, antcap);
            if (ret2 != 0) {
                pr_err( "%s: error. Si4710 set antcap failed.\n", __func__ );
            }
        }
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/*********************************************************************
 * shfmtx_si4710_vidioc_g_tune_frequency
*********************************************************************/
static int shfmtx_si4710_vidioc_g_tune_frequency(struct file *file, void *priv,
        struct v4l2_frequency *freq)
{
    int ret = 0;
    u16 frequency = 0;

    if (!freq) {
        pr_err( "%s: error. freq is NULL.\n", __func__ );
        return -EINVAL;
    }

    pr_debug( "%s: -START-.\n", __func__ );

    ret = shfmtx_si4710_cmd_get_freq(&frequency);
    if (ret != 0) {
        pr_err( "%s: error. Si4710 get frequency failed.\n", __func__ );
    }
    else {
        pr_debug("%s: get frequency is %d.\n", __func__, frequency * TUNE_PARAM);
        freq->frequency = frequency * TUNE_PARAM;
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/*********************************************************************
 * shfmtx_si4710_init
*********************************************************************/
static void shfmtx_si4710_init(void) 
{
    pr_debug("%s: -START-.\n", __func__ );

    tune_completion = 0;

    pr_debug("%s: -END-.\n", __func__ );
}

/*********************************************************************
 * shfmtx_si4710_vidioc_querycap
*********************************************************************/
static int shfmtx_si4710_vidioc_querycap(struct file *file, void *priv,
    struct v4l2_capability *capability)
{
    int ret = 0;
    int retval = 0;
    u32 rev = 0;

    if (!capability) {
        pr_err("%s: error. capability is NULL.\n", __func__ );
        return -EINVAL;
    }

    pr_debug("%s: -START-.\n", __func__ );
    ret = shfmtx_si4710_cmd_initialize();
    if (ret == 0) {
        retval = shfmtx_si4710_cmd_get_rev(&rev);
        if (retval == 0) {
            capability->version = rev;
        }
        else {
            pr_err("%s: error. Si4710 get revision failed.\n", __func__ );
            capability->version = 0;
        }
        shfmtx_si4710_init();
    }
    else {
        pr_err("%s: error. Si4710 initialize failed.\n", __func__ );
    }

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/*********************************************************************
 * shfmtx_si4710_vidioc_dqbuf
*********************************************************************/
static int shfmtx_si4710_vidioc_dqbuf(struct file *file, void *priv,
                struct v4l2_buffer *buffer)
{
    int ret = 0;
    int bytesused = 0;
    unsigned char *buf = NULL;

    if (!buffer) {
        pr_err("%s: error. buffer is NULL.\n", __func__);
        return -EINVAL;
    }

    buf = (unsigned char *)buffer->m.userptr;
    if (!buf) {
        pr_err("%s: error. userptr is NULL.\n", __func__);
        return -EINVAL;
    }

    pr_debug("%s: -START- index[%d].\n", __func__, buffer->index );

    switch(buffer->index) {
    case EVENT_LISTEN:
        if (tune_completion){
            pr_debug("    %s Set TUNE_EVENT.\n", __func__);
            
            buf[bytesused] = TUNE_EVENT;
            bytesused++;
            tune_completion = 0;
        }
        break;
    case CAL_DATA_BUF:
        break;
    default:
        break;
    }

    buffer->bytesused = bytesused;

    pr_debug("%s: -END-. ret[%d].\n", __func__, ret );
    return ret;
}

/*********************************************************************
 * shfmtx_si4710_vidioc_g_fmt_type_private
*********************************************************************/
static int shfmtx_si4710_vidioc_g_fmt_type_private(struct file *file, void *priv,
                        struct v4l2_format *f)
{
    return 0;
}

static const struct v4l2_ioctl_ops shfmtx_si4710_ioctl_ops = {
    .vidioc_querycap              = shfmtx_si4710_vidioc_querycap,
    .vidioc_queryctrl             = shfmtx_si4710_vidioc_queryctrl,
    .vidioc_s_ctrl                = shfmtx_si4710_vidioc_s_ctrl,
    .vidioc_s_tuner               = shfmtx_si4710_vidioc_s_tuner,
    .vidioc_s_frequency           = shfmtx_si4710_vidioc_s_tune_frequency,
    .vidioc_g_frequency           = shfmtx_si4710_vidioc_g_tune_frequency,
    .vidioc_s_ext_ctrls           = shfmtx_si4710_vidioc_s_ext_ctrls,
    .vidioc_dqbuf                 = shfmtx_si4710_vidioc_dqbuf,
    .vidioc_g_fmt_type_private    = shfmtx_si4710_vidioc_g_fmt_type_private,
};

static const struct v4l2_file_operations shfmtx_si4710_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = video_ioctl2,
};

static struct video_device shfmtx_si4710_viddev_template = {
    .fops                   = &shfmtx_si4710_fops,
    .ioctl_ops              = &shfmtx_si4710_ioctl_ops,
    .name                   = DRIVER_NAME,
    .release                = video_device_release,
};

/*********************************************************************
 * shfmtx_si4710_probe
*********************************************************************/
static int __init shfmtx_si4710_probe(struct platform_device *pdev)
{
    struct video_device *video = NULL;
    int retval = 0;
    int video_nr = -1;

    if (!pdev) {
        pr_err( "%s: error. pdev is NULL.\n", __func__);
        return -EINVAL;
    }

    pr_debug( "%s: -START-.\n", __func__ );

    video = video_device_alloc();
    if (!video) {
        pr_err( "%s: error. Could not allocate video device.\n", __func__);
        retval =  -ENOMEM;
    }
    else {
        memcpy(&video->dev, &pdev->dev, sizeof(struct device));
        platform_set_drvdata(pdev, video);

        memcpy(video, &shfmtx_si4710_viddev_template,
          sizeof(shfmtx_si4710_viddev_template));

        retval = video_register_device(video, VFL_TYPE_RADIO,
                                       video_nr);
        if (retval != 0) {
            pr_err( "%s: error. Could not register video device.\n", __func__);
            video_device_release(video);
        }
    }

    pr_debug( "%s: -END-. ret[%d].\n", __func__, retval );
    return retval;
}

/*********************************************************************
 * shfmtx_si4710_remove
*********************************************************************/
static int __devexit shfmtx_si4710_remove(struct platform_device *pdev)
{
    struct video_device *video = platform_get_drvdata(pdev);

    pr_debug( "%s: -START-.\n", __func__ );

    video_unregister_device(video);

    video_device_release(video);

    platform_set_drvdata(pdev, NULL);
    
    pr_debug( "%s: -END-.\n", __func__ );
    return 0;
}

static struct platform_driver shfmtx_si4710_driver = {
    .driver = {
        .owner  = THIS_MODULE,
        .name   = "iris_fm",
    },
    .remove = __devexit_p(shfmtx_si4710_remove),
};

/*********************************************************************
 * shfmtx_si4710_video_init
*********************************************************************/
static int __init shfmtx_si4710_video_init(void)
{
    pr_debug( "%s: called.\n", __func__ );
    return platform_driver_probe(&shfmtx_si4710_driver, shfmtx_si4710_probe);
}
module_init(shfmtx_si4710_video_init);

/*********************************************************************
 * shfmtx_si4710_video_exit
*********************************************************************/
static void __exit shfmtx_si4710_video_exit(void)
{
    pr_debug( "%s: called.\n", __func__ );
    platform_driver_unregister(&shfmtx_si4710_driver);
}
module_exit(shfmtx_si4710_video_exit);

MODULE_LICENSE("GPL v2");
//MODULE_AUTHOR(DRIVER_AUTHOR);
//MODULE_DESCRIPTION(DRIVER_DESC);
