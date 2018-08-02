/* drivers/sharp/shdisp/shdisp_panel.c  (Display Driver)
 *
 * Copyright (C) 2012-2013 SHARP CORPORATION
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

/* ------------------------------------------------------------------------- */
/* SHARP DISPLAY DRIVER FOR KERNEL MODE                                      */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */


#include "../../video/msm/msm_fb.h"
//#include "../../video/msm/mipi_dsi.h"
#include "../../video/msm/msm_fb_panel.h"
#include "../../video/msm/mdp.h"
#include "../../video/msm/mdp4.h"
#include "../../video/msm/mipi_sharp_clmr.h"
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/idr.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/fb.h>
#ifdef CONFIG_TOUCHSCREEN_SHTPS
#include <sharp/shtps_dev.h>
#endif /* CONFIG_TOUCHSCREEN_SHTPS */
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/wakelock.h>

#ifdef CONFIG_SHTERM
#include <sharp/shterm_k.h>
#endif /* CONFIG_SHTERM */
#include <sharp/sh_smem.h>
#include <sharp/shdisp_kerl.h>
#include "shdisp_system.h"
#include "shdisp_bdic.h"
#include "shdisp_type.h"
#include "shdisp_pm.h"
#include "shdisp_dbg.h"

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/delay.h>
#include <asm/param.h>

#include "../../video/msm/mipi_sharp.h"

//#include "../../video/msm/msm_fb.h"


#include "shdisp_panel.h"
#if defined(CONFIG_SHDISP_PANEL_MARCO)
#include "shdisp_marco.h"
#elif defined(CONFIG_SHDISP_PANEL_ANDY)
#include "shdisp_andy.h"
#elif defined(CONFIG_SHDISP_PANEL_RYOMA)
#include "shdisp_ryoma.h"
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
#include "shdisp_gemini.h"
#else
#include "shdisp_null_panel.h"
#endif
#include <sharp/sh_boot_manager.h>

#define SHDISP_BYTE0_SHIFT                   (0)
#define SHDISP_BYTE1_SHIFT                   (8)
#define SHDISP_BYTE2_SHIFT                   (16)
#define SHDISP_BYTE3_SHIFT                   (24)

#define SHDISP_DSI_HDR_DTYPE(dtype)          (((dtype) & 0x0000003F) << SHDISP_BYTE0_SHIFT)
#define SHDISP_DSI_HDR_DATA1(data)           (((data)  & 0x000000FF) << SHDISP_BYTE1_SHIFT)
#define SHDISP_DSI_HDR_DATA2(data)           (((data)  & 0x000000FF) << SHDISP_BYTE2_SHIFT)
#define SHDISP_DSI_HDR_DATACNT1(data)        (((data)  & 0x000000FF) << SHDISP_BYTE2_SHIFT)
#define SHDISP_DSI_HDR_DATACNT2(data)        ((((data) >> 8) & 0x000000FF) << SHDISP_BYTE3_SHIFT)
#define SHDISP_DSI_HDR_WC(wc)                ((wc) & 0xFF00) <<  | (wc) & 0xFF
#define SHDISP_DSI_HDR_VC(vc)                (((vc) & 0x03) << 22)

#define MIPI_DSI_SHORT_PACKET_SIZE           (4)
#define MIPI_DSI_SHORT_PACKET_LEN            (8)
#define MIPI_DSI_READ_RESPONSE_LEN           (8)

static struct shdisp_panel_operations *shdisp_panel_fops = NULL;

static struct shdisp_panel_operations shdisp_def_fops = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static char max_pktsize[2] = {0x01, 0x00};
static struct dsi_cmd_desc pkt_size_cmd[] = {
    {DTYPE_MAX_PKTSIZE, 1, 0, 0, 0, sizeof(max_pktsize), max_pktsize}
};
static int  dsi_transfer_mode;
static int  test_dtype = 0;
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
static char max_pktsize2[2] = {0x01, 0x00};
static struct dsi_cmd_desc pkt_size_cmd2[] = {
    {DTYPE_MAX_PKTSIZE, 1, 0, 0, 0, sizeof(max_pktsize2), max_pktsize2}
};
#endif

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
static char *shdisp_panel_mipi_dsi_buf_reserve(struct dsi_buf *dp, int len);
static char *shdisp_panel_mipi_dsi_buf_push(struct dsi_buf *dp, int len);
static char *shdisp_panel_mipi_dsi_buf_reserve_hdr(struct dsi_buf *dp, int hlen);
static char *shdisp_panel_mipi_dsi_buf_init(struct dsi_buf *dp);
static int shdisp_panel_mipi_dsi_generic_lwrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm);
static int shdisp_panel_mipi_dsi_generic_swrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm);
static int shdisp_panel_mipi_dsi_generic_read(struct dsi_buf *dp, struct dsi_cmd_desc *cm);
static int shdisp_panel_mipi_dsi_dcs_swrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm);
static int shdisp_panel_mipi_dsi_dcs_swrite1(struct dsi_buf *dp, struct dsi_cmd_desc *cm);
#ifndef SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
static int shdisp_panel_mipi_dsi_dcs_swrite1_multi(struct dsi_buf *dp, struct dsi_cmd_desc *cm);
#endif
static int shdisp_panel_mipi_dsi_cmd_dma_add(struct dsi_buf *dp, struct dsi_cmd_desc *cm);
static int shdisp_panel_mipi_dsi_cmd_dma_tx(struct dsi_buf *tp);
static int shdisp_panel_mipi_dsi_cmd_dma_rx(struct dsi_buf *tp, unsigned char *rp, int rlen);
static int shdisp_panel_mipi_dsi_cmd_dma_post_rx(void);
static unsigned char shdisp_panel_API_get_fw_transfer_mode_w(int dtype, int dsi_mode);
static unsigned char shdisp_panel_API_get_fw_transfer_mode_r(int dsi_mode);


/* ------------------------------------------------------------------------- */
/* DEBUG MACROS                                                              */
/* ------------------------------------------------------------------------- */
//#define SHDISP_SW_DISABLE_SMEM_INFO

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define MIPI_SHARP_RW_MAX_SIZE          SHDISP_LCDDR_BUF_MAX
#define MIPI_SHARP_R_SIZE               10

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_check_panel_info                                         */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_check_panel_info(void)
{
    int revision = 0;

    SHDISP_DEBUG("in\n");
#if defined(CONFIG_SHDISP_PANEL_SWITCH)
    switch (shdisp_kerl_ctx.hw_revision) {
    case SHDISP_HW_REV_ES0:
    case SHDISP_HW_REV_ES1:
    case SHDISP_HW_REV_PP1:
    case SHDISP_HW_REV_PP2:
        revision = 1;
        break;
    case SHDISP_HW_REV_PP25:
    case SHDISP_HW_REV_MP:
    default:
        revision = 0;
        break;
    }
#endif /* CONFIG_SHDISP_PANEL_SWITCH */

    SHDISP_DEBUG("out revision=%04x\n", revision);
    return revision;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SQE_panel_write_reg                                                */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_SQE_write_reg(struct shdisp_lcddr_reg *panel_reg)
{
    int ret;

    SHDISP_DEBUG("in\n");
    ret = shdisp_panel_API_diag_write_reg(panel_reg->cog, panel_reg->address, panel_reg->buf, panel_reg->size);

    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_diag_write_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SQE_panel_read_reg                                                 */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_SQE_read_reg(struct shdisp_lcddr_reg *panel_reg)
{
    int ret;

    SHDISP_DEBUG("in\n");
    ret = shdisp_panel_API_diag_read_reg(panel_reg->cog, panel_reg->address, panel_reg->buf, panel_reg->size);

    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_create                                                   */
/* ------------------------------------------------------------------------- */

void shdisp_panel_API_create(void)
{
    shdisp_panel_fops = &shdisp_def_fops;

#if defined(CONFIG_SHDISP_PANEL_MARCO)
    shdisp_panel_fops = shdisp_marco_API_create();
#elif defined(CONFIG_SHDISP_PANEL_ANDY)
    shdisp_panel_fops = shdisp_andy_API_create();
#elif defined(CONFIG_SHDISP_PANEL_RYOMA)
    shdisp_panel_fops = shdisp_ryoma_API_create();
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    shdisp_panel_fops = shdisp_gemini_API_create();
#else
    shdisp_panel_fops = shdisp_null_panel_API_create();
#endif
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_init_io                                                  */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_init_io(void)
{
    if (shdisp_panel_fops->init_io) {
        return shdisp_panel_fops->init_io();
    }
    dsi_transfer_mode = SHDISP_DSI_LOW_POWER_MODE;
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_exit_io                                                  */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_exit_io(void)
{
    if (shdisp_panel_fops->exit_io) {
        return shdisp_panel_fops->exit_io();
    }
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_init_isr                                                 */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_init_isr(void)
{
    if (shdisp_panel_fops->init_isr) {
        return shdisp_panel_fops->init_isr();
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_set_param                                                */
/* ------------------------------------------------------------------------- */

void shdisp_panel_API_set_param(struct shdisp_panel_param_str *param_str)
{
    if (shdisp_panel_fops->set_param) {
        shdisp_panel_fops->set_param(param_str);
    }
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_power_on                                                 */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_power_on(void)
{
    SHDISP_DEBUG("in\n");
    if (shdisp_panel_fops->power_on) {
        SHDISP_DEBUG("out1\n");
        return shdisp_panel_fops->power_on();
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_power_off                                                */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_power_off(void)
{
    SHDISP_DEBUG("in\n");
    if (shdisp_panel_fops->power_off) {
        SHDISP_DEBUG("out1\n");
        return shdisp_panel_fops->power_off();
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_disp_init_1st                                            */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_disp_init_1st(void)
{
    if (shdisp_panel_fops->init_1st) {
        return shdisp_panel_fops->init_1st();
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_disp_init_2nd                                            */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_disp_init_2nd(void)
{
    if (shdisp_panel_fops->init_2nd) {
        return shdisp_panel_fops->init_2nd();
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_disp_on                                                  */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_disp_on(void)
{
    SHDISP_DEBUG("in\n");
    if (shdisp_panel_fops->disp_on) {
        SHDISP_DEBUG("out1\n");
        return shdisp_panel_fops->disp_on();
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_sleep                                                    */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_sleep(void)
{
    SHDISP_DEBUG("in\n");
    if (shdisp_panel_fops->sleep) {
        SHDISP_DEBUG("out1\n");
        return shdisp_panel_fops->sleep();
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_deep_standby                                             */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_deep_standby(void)
{
    SHDISP_DEBUG("in\n");
    if (shdisp_panel_fops->deep_standby) {
        SHDISP_DEBUG("out1\n");
        return shdisp_panel_fops->deep_standby();
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_check_upper_unit                                         */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_check_upper_unit(void)
{
    if (shdisp_panel_fops->check_upper_unit) {
        return shdisp_panel_fops->check_upper_unit();
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_check_flicker_param                                      */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_check_flicker_param(unsigned short alpha_in, unsigned short *alpha_out)
{
    if (shdisp_panel_fops->check_flicker) {
        return shdisp_panel_fops->check_flicker(alpha_in, alpha_out);
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_disp_update                                              */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_disp_update(struct shdisp_main_update *update)
{
    if (shdisp_panel_fops->disp_update) {
        return shdisp_panel_fops->disp_update(update);
    }
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_disp_clear_screen                                        */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_disp_clear_screen(struct shdisp_main_clear *clear)
{
    if (shdisp_panel_fops->clear_screen) {
        return shdisp_panel_fops->clear_screen(clear);
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_diag_write_reg                                           */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_diag_write_reg(int cog, unsigned char addr, unsigned char *write_data, unsigned char size)
{
    if (shdisp_panel_fops->write_reg) {
        return shdisp_panel_fops->write_reg(cog, addr, write_data, size);
    }
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_diag_read_reg                                            */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_diag_read_reg(int cog, unsigned char addr, unsigned char *read_data, unsigned char size)
{
    if (shdisp_panel_fops->read_reg) {
        return shdisp_panel_fops->read_reg(cog, addr, read_data, size);
    }
    return SHDISP_RESULT_SUCCESS;
}

#ifndef SHDISP_NOT_SUPPORT_FLICKER

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_diag_set_flicker_param                                   */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_diag_set_flicker_param(unsigned short alpha)
{
    if (shdisp_panel_fops->set_flicker) {
        return shdisp_panel_fops->set_flicker(alpha);
    }
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_diag_get_flicker_param                                   */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_diag_get_flicker_param(unsigned short *alpha)
{
    if (shdisp_panel_fops->get_flicker) {
        return shdisp_panel_fops->get_flicker(alpha);
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_diag_get_flicker_low_param                               */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_diag_get_flicker_low_param(unsigned short *alpha)
{
    if (shdisp_panel_fops->get_flicker_low) {
        return shdisp_panel_fops->get_flicker_low(alpha);
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_diag_set_flicker_param_multi_cog                         */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_diag_set_flicker_param_multi_cog(struct shdisp_diag_flicker_param *flicker_param)
{
    if (shdisp_panel_fops->set_flicker_multi_cog) {
        return shdisp_panel_fops->set_flicker_multi_cog(flicker_param);
    }
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_diag_get_flicker_param_multi_cog                         */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_diag_get_flicker_param_multi_cog(struct shdisp_diag_flicker_param *flicker_param)
{
    if (shdisp_panel_fops->get_flicker_multi_cog) {
        return shdisp_panel_fops->get_flicker_multi_cog(flicker_param);
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_diag_get_flicker_low_param_multi_cog                     */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_diag_get_flicker_low_param_multi_cog(struct shdisp_diag_flicker_param *flicker_param)
{
    if (shdisp_panel_fops->get_flicker_low_multi_cog) {
        return shdisp_panel_fops->get_flicker_low_multi_cog(flicker_param);
    }
    return SHDISP_RESULT_SUCCESS;
}
#endif /* SHDISP_NOT_SUPPORT_FLICKER */


#if defined(CONFIG_SHDISP_PANEL_GEMINI)
/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_diag_write_reg_multi_cog                            */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_mipi_diag_write_reg_multi_cog(struct dsi_buf *tp, unsigned char addr, unsigned char *write_data, unsigned char size, int mode)
{
    struct msm_fb_data_type *mfd;
    struct mipi_panel_info *mipi;
    struct fb_info *info = NULL;
    struct dsi_cmd_desc cmd[1];
    char cmd_buf[MIPI_SHARP_RW_MAX_SIZE+1];

    if (size > MIPI_SHARP_RW_MAX_SIZE) {
        SHDISP_ERR("size over, -EINVAL\n");
        return -EINVAL;
    }
    if (mode > SHDISP_CLMR_FWCMD_DSI_DSI_TXC) {
        SHDISP_ERR("mode (%d), -EINVAL\n", mode);
        return -EINVAL;
    }

    if (!num_registered_fb){
        pr_err("%s: num_registered_fb == NULL, -ENODEV\n", __func__);
        return -ENODEV;
    }

    info = registered_fb[0];
    if (!info){
        pr_err("%s: registered_fb[0] == NULL, -ENODEV\n", __func__);
        return -ENODEV;
    }

    mfd = (struct msm_fb_data_type *)info->par;
    if (!mfd) {
        pr_err("%s: mfd == NULL, -ENODEV\n", __func__);
        return -ENODEV;
    }

    if (!mfd->panel_power_on) {
        pr_err("%s: power off, -ENODEV\n", __func__);
        return -ENODEV;
    }

    mipi = &mfd->panel_info.mipi;
    if (mipi->mode != DSI_CMD_MODE) {
        pr_err("%s: Not command mode, -ENODEV\n", __func__);
        return -ENODEV;
    }
    cmd_buf[0] = addr;
    cmd_buf[1] = 0x00;
    memcpy(&cmd_buf[1], write_data, size);

    if (addr != 0xDC) {
        if (size == 0) {
            cmd[0].dtype = DTYPE_GEN_WRITE;
        } else if(size == 1) {
            cmd[0].dtype = DTYPE_GEN_WRITE1;
        } else {
            cmd[0].dtype = DTYPE_GEN_LWRITE;
        }
    }
    else {
        if (size == 0) {
            cmd[0].dtype = DTYPE_DCS_WRITE;
        } else if(size == 1) {
            cmd[0].dtype = DTYPE_DCS_WRITE1;
        } else {
            cmd[0].dtype = DTYPE_DCS_LWRITE;
        }
    }

    cmd[0].dtype |= mode;

    cmd[0].last = 0x01;
    cmd[0].vc   = 0x00;
    cmd[0].ack  = 0x00;
    cmd[0].wait = 0x00;
    cmd[0].dlen = size + 1;
    cmd[0].payload = cmd_buf;

    if (shdisp_panel_API_mipi_dsi_cmds_tx(tp, cmd, ARRAY_SIZE(cmd)) != SHDISP_RESULT_SUCCESS){
        SHDISP_ERR("mipi_dsi_cmds_tx error\n");
        return -ENODEV;
    }

    return SHDISP_RESULT_SUCCESS;
}
#endif

int shdisp_panel_API_mipi_diag_write_reg(struct dsi_buf *tp, unsigned char addr, unsigned char *write_data, unsigned char size)
{
    struct msm_fb_data_type *mfd;
    struct mipi_panel_info *mipi;
    struct fb_info *info = NULL;
    struct dsi_cmd_desc cmd[1];
    char cmd_buf[MIPI_SHARP_RW_MAX_SIZE+1];

    if (size > MIPI_SHARP_RW_MAX_SIZE) {
        SHDISP_ERR("size over, -EINVAL\n");
        return -EINVAL;
    }

    if (!num_registered_fb){
        pr_err("%s: num_registered_fb == NULL, -ENODEV\n", __func__);
        return -ENODEV;
    }

    info = registered_fb[0];
    if (!info){
        pr_err("%s: registered_fb[0] == NULL, -ENODEV\n", __func__);
        return -ENODEV;
    }

    mfd = (struct msm_fb_data_type *)info->par;
    if (!mfd) {
        pr_err("%s: mfd == NULL, -ENODEV\n", __func__);
        return -ENODEV;
    }

    if (!mfd->panel_power_on) {
        pr_err("%s: power off, -ENODEV\n", __func__);
        return -ENODEV;
    }

    mipi = &mfd->panel_info.mipi;
    if (mipi->mode != DSI_CMD_MODE) {
        pr_err("%s: Not command mode, -ENODEV\n", __func__);
        return -ENODEV;
    }
    cmd_buf[0] = addr;
    cmd_buf[1] = 0x00;
    memcpy(&cmd_buf[1], write_data, size);

#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    if (addr != 0xDC) {
        if (size == 0) {
            cmd[0].dtype = DTYPE_GEN_WRITE;
        } else if(size == 1) {
            cmd[0].dtype = DTYPE_GEN_WRITE1;
        } else {
            cmd[0].dtype = DTYPE_GEN_LWRITE;
        }
    } else {
        if (size == 0) {
            cmd[0].dtype = DTYPE_DCS_WRITE;
        } else if(size == 1) {
            cmd[0].dtype = DTYPE_DCS_WRITE1;
        } else {
            cmd[0].dtype = DTYPE_DCS_LWRITE;
        }
    }
#else
    if ((addr >= 0xB0) && (addr != 0xDA) && (addr != 0xDB) && (addr != 0xDC)) {
        if (size == 0) {
            cmd[0].dtype = DTYPE_GEN_WRITE;
        } else if(size == 1) {
            cmd[0].dtype = DTYPE_GEN_WRITE1;
        } else {
            cmd[0].dtype = DTYPE_GEN_LWRITE;
        }
    }
    else {
        if (size == 0) {
            cmd[0].dtype = DTYPE_DCS_WRITE;
        } else if(size == 1) {
            cmd[0].dtype = DTYPE_DCS_WRITE1;
        } else {
            cmd[0].dtype = DTYPE_DCS_LWRITE;
        }
    }
#endif

    cmd[0].last = 0x01;
    cmd[0].vc   = 0x00;
    cmd[0].ack  = 0x00;
    cmd[0].wait = 0x00;
    cmd[0].dlen = size + 1;
    cmd[0].payload = cmd_buf;

    if (shdisp_panel_API_mipi_dsi_cmds_tx(tp, cmd, ARRAY_SIZE(cmd)) != SHDISP_RESULT_SUCCESS){
        SHDISP_ERR("mipi_dsi_cmds_tx error\n");
        return -ENODEV;
    }

    return SHDISP_RESULT_SUCCESS;
}

#ifndef SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_diag_mltshortpkt_write_reg                          */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_mipi_diag_mltshortpkt_write_reg(struct dsi_buf *tp, unsigned char * addrs, unsigned char * write_data, unsigned int size)
{
    struct dsi_cmd_desc cmd;
    char cmd_buf[MIPI_SHARP_RW_MAX_SIZE+1];
    int i;
    char * head;
    int ret;
    int len = 0;


    if( size < 2 ) {
        SHDISP_ERR("shorthead count < 2...\n");
        return SHDISP_RESULT_FAILURE;
    }

    if( dsi_transfer_mode != SHDISP_DSI_HIGH_SPEED_MODE ) {
        SHDISP_ERR("DSI power mode is LP...\n");
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_panel_mipi_dsi_buf_init(tp);
    head = tp->data;

    cmd.last = 0x01;
    cmd.vc   = 0x00;
    cmd.ack  = 0x00;
    cmd.wait = 0x00;
    cmd.dlen = 2;
    cmd.payload = cmd_buf;

    for(i=0 ; i < size; i++ ){

        if( len+MIPI_DSI_SHORT_PACKET_SIZE > 64 ){
            ret = shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_HS_MULTI_SWRITE, tp->len, head);
            shdisp_panel_mipi_dsi_buf_init(tp);
            head = tp->data;
        }

        cmd_buf[0] = *addrs;
        cmd_buf[1] = *write_data;

        len = shdisp_panel_mipi_dsi_dcs_swrite1_multi(tp,&cmd);

        addrs++;
        write_data++;
    }

    ret = shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_HS_MULTI_SWRITE, tp->len, head);
    shdisp_panel_mipi_dsi_cmd_dma_tx(tp);

    return ret;
}
#endif


#if defined(CONFIG_SHDISP_PANEL_GEMINI)
/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_diag_read_reg_multi_cog                             */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_mipi_diag_read_reg_multi_cog(struct dsi_buf *tp, struct dsi_buf *rp, unsigned char addr, unsigned char *read_data, unsigned char size, int mode)
{
    struct msm_fb_data_type *mfd;
    struct mipi_panel_info *mipi;
    struct fb_info *info = NULL;
#ifdef SHDISP_LOG_ENABLE
    int i;
    unsigned char rlen;
#endif  /* SHDISP_LOG_ENABLE */
    struct dsi_cmd_desc cmd[1];
    char cmd_buf[2+1];

    SHDISP_DEBUG("[S] address:%02X, buf:0x%08X, size:%d\n", addr, (int)read_data, size);
    if ((size > MIPI_SHARP_RW_MAX_SIZE) || (size == 0)) {
        SHDISP_ERR("size over, -EINVAL\n");
        return -EINVAL;
    }

    if (!num_registered_fb){
        pr_err("%s: num_registered_fb == NULL, -ENODEV\n", __func__);
        return -ENODEV;
    }

    info = registered_fb[0];
    if (!info){
        pr_err("%s: registered_fb[0] == NULL, -ENODEV\n", __func__);
        return -ENODEV;
    }

    mfd = (struct msm_fb_data_type *)info->par;
    if (!mfd) {
        pr_err("%s: mfd == NULL, -ENODEV\n", __func__);
        return -ENODEV;
    }

    if (!mfd->panel_power_on) {
        pr_err("%s: power off, -ENODEV\n", __func__);
        return -ENODEV;
    }

    mipi = &mfd->panel_info.mipi;
    if (mipi->mode != DSI_CMD_MODE) {
        pr_err("%s: Not command mode, -ENODEV\n", __func__);
        return -ENODEV;
    }

    cmd_buf[0] = addr;
    cmd_buf[1] = 0x00;

    cmd[0].dtype = DTYPE_DCS_READ;
    cmd[0].dtype |= mode;

    cmd[0].last     = 0x01;
    cmd[0].vc       = 0x00;
    cmd[0].ack      = 0x01;
    cmd[0].wait     = 0x00;
    cmd[0].dlen     = 2;
    cmd[0].payload  = cmd_buf;

    memset(rp->data, 0, sizeof(rp->data));

    if (shdisp_panel_API_mipi_dsi_cmds_rx2(tp, rp, cmd, size) != SHDISP_RESULT_SUCCESS){
        SHDISP_ERR("mipi_dsi_cmds_rx error\n");
        return -ENODEV;
    }

#ifdef SHDISP_LOG_ENABLE
    if (size <= 2) {
        rlen = MIPI_DSI_SHORT_PACKET_LEN;
    }
    else {
        rlen = size + MIPI_DSI_READ_RESPONSE_LEN;
    }
    for (i = 0; i < rlen; i++) {
        if ((i % MIPI_DSI_SHORT_PACKET_LEN) == 0) {
            SHDISP_PRINTK(SHDISP_LOG_LV_DEBUG, "\n[SHDISP_DEBUG][%s] Data    : ",__func__);
        }
        SHDISP_PRINTK(SHDISP_LOG_LV_DEBUG, "%02X ", rp->data[i]);
    }
    SHDISP_PRINTK(SHDISP_LOG_LV_DEBUG, "\n");
#endif  /* SHDISP_LOG_ENABLE */

    if (size == 1) {
        read_data[0] = rp->data[5];
    } else if (size == 2) {
        read_data[0] = rp->data[5];
        read_data[1] = rp->data[6];
    } else {
        memcpy(&read_data[0], &rp->data[8], size);
    }

    SHDISP_DEBUG("[E] SHDISP_RESULT_SUCCESS\n");
    return SHDISP_RESULT_SUCCESS;
}
#endif

int shdisp_panel_API_mipi_diag_read_reg(struct dsi_buf *tp, struct dsi_buf *rp, unsigned char addr, unsigned char *read_data, unsigned char size)
{
    struct msm_fb_data_type *mfd;
    struct mipi_panel_info *mipi;
    struct fb_info *info = NULL;
#ifdef SHDISP_LOG_ENABLE
    int i;
    unsigned char rlen;
#endif  /* SHDISP_LOG_ENABLE */
    struct dsi_cmd_desc cmd[1];
    char cmd_buf[2+1];

    SHDISP_DEBUG("[S] address:%02X, buf:0x%08X, size:%d\n", addr, (int)read_data, size);
    if ((size > MIPI_SHARP_RW_MAX_SIZE) || (size == 0)) {
        SHDISP_ERR("size over, -EINVAL\n");
        return -EINVAL;
    }

    if (!num_registered_fb){
        pr_err("%s: num_registered_fb == NULL, -ENODEV\n", __func__);
        return -ENODEV;
    }

    info = registered_fb[0];
    if (!info){
        pr_err("%s: registered_fb[0] == NULL, -ENODEV\n", __func__);
        return -ENODEV;
    }

    mfd = (struct msm_fb_data_type *)info->par;
    if (!mfd) {
        pr_err("%s: mfd == NULL, -ENODEV\n", __func__);
        return -ENODEV;
    }

    if (!mfd->panel_power_on) {
        pr_err("%s: power off, -ENODEV\n", __func__);
        return -ENODEV;
    }

    mipi = &mfd->panel_info.mipi;
    if (mipi->mode != DSI_CMD_MODE) {
        pr_err("%s: Not command mode, -ENODEV\n", __func__);
        return -ENODEV;
    }

    cmd_buf[0] = addr;
    cmd_buf[1] = 0x00;

#if defined(CONFIG_SHDISP_PANEL_RYOMA) || defined(CONFIG_SHDISP_PANEL_MARCO)
    if((addr >= 0xB0) && (addr != 0xDA) && (addr != 0xDB) && (addr != 0xDC)) {
        cmd[0].dtype = DTYPE_GEN_READ2;
    } else {
        cmd[0].dtype = DTYPE_DCS_READ;
    }
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    cmd[0].dtype = DTYPE_DCS_READ;
#else
    cmd[0].dtype = DTYPE_GEN_READ2;
#endif

    cmd[0].last     = 0x01;
    cmd[0].vc       = 0x00;
    cmd[0].ack      = 0x01;
    cmd[0].wait     = 0x00;
    cmd[0].dlen     = 2;
    cmd[0].payload  = cmd_buf;

    memset(rp->data, 0, sizeof(rp->data));

    if (shdisp_panel_API_mipi_dsi_cmds_rx(tp, rp, cmd, size) != SHDISP_RESULT_SUCCESS){
        SHDISP_ERR("mipi_dsi_cmds_rx error\n");
        return -ENODEV;
    }

#ifdef SHDISP_LOG_ENABLE
    if (size <= 2) {
        rlen = MIPI_DSI_SHORT_PACKET_LEN;
    }
    else {
        rlen = size + MIPI_DSI_READ_RESPONSE_LEN;
    }
    for (i = 0; i < rlen; i++) {
        if ((i % MIPI_DSI_SHORT_PACKET_LEN) == 0) {
            SHDISP_PRINTK(SHDISP_LOG_LV_DEBUG, "\n[SHDISP_DEBUG][%s] Data    : ",__func__);
        }
        SHDISP_PRINTK(SHDISP_LOG_LV_DEBUG, "%02X ", rp->data[i]);
    }
    SHDISP_PRINTK(SHDISP_LOG_LV_DEBUG, "\n");
#endif  /* SHDISP_LOG_ENABLE */

    if (size == 1) {
        read_data[0] = rp->data[5];
    } else if (size == 2) {
        read_data[0] = rp->data[5];
        read_data[1] = rp->data[6];
    } else {
        memcpy(&read_data[0], &rp->data[8], size);
    }

    SHDISP_DEBUG("[E] SHDISP_RESULT_SUCCESS\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_cmd_lcd_on                                          */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_mipi_cmd_lcd_on(void)
{
    int ret;

    SHDISP_DEBUG("in\n");
#if defined(CONFIG_SHDISP_PANEL_MARCO)
    ret = shdisp_marco_API_mipi_lcd_on();
#elif defined(CONFIG_SHDISP_PANEL_ANDY)
    ret = shdisp_andy_API_mipi_lcd_on();
#elif defined(CONFIG_SHDISP_PANEL_RYOMA)
    ret = shdisp_ryoma_API_mipi_lcd_on();
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    ret = shdisp_gemini_API_mipi_lcd_on();
#else
    ret = shdisp_null_panel_API_mipi_lcd_on();
#endif

    SHDISP_DEBUG("out\n");
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_cmd_lcd_off                                         */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_mipi_cmd_lcd_off(void)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");
#if defined(CONFIG_SHDISP_PANEL_MARCO)
    ret = shdisp_marco_API_mipi_lcd_off();
#elif defined(CONFIG_SHDISP_PANEL_ANDY)
    ret = shdisp_andy_API_mipi_lcd_off();
#elif defined(CONFIG_SHDISP_PANEL_RYOMA)
    ret = shdisp_ryoma_API_mipi_lcd_off();
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    ret = shdisp_gemini_API_mipi_lcd_off();
#else
    ret = shdisp_null_panel_API_mipi_lcd_off();
#endif

    SHDISP_DEBUG("out\n");
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_cmd_lcd_on_after_black_screen                       */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_mipi_cmd_lcd_on_after_black_screen(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
#if defined(CONFIG_SHDISP_PANEL_ANDY)
    ret = shdisp_andy_API_mipi_lcd_on_after_black_screen();
#elif defined(CONFIG_SHDISP_PANEL_RYOMA)
    ret = shdisp_ryoma_API_mipi_lcd_on_after_black_screen();
#endif
    SHDISP_DEBUG("out\n");
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_cmd_lcd_off_black_screen_on                         */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_mipi_cmd_lcd_off_black_screen_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
#if defined(CONFIG_SHDISP_PANEL_ANDY)
    ret = shdisp_andy_API_mipi_lcd_off_black_screen_on();
#elif defined(CONFIG_SHDISP_PANEL_RYOMA)
    ret = shdisp_ryoma_API_mipi_lcd_off_black_screen_on();
#endif
    SHDISP_DEBUG("out\n");
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_cmd_start_display                                   */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_mipi_cmd_start_display(void)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");
#if defined(CONFIG_SHDISP_PANEL_MARCO)
    ret = shdisp_marco_API_mipi_start_display();
#elif defined(CONFIG_SHDISP_PANEL_ANDY)
    ret = shdisp_andy_API_mipi_start_display();
#elif defined(CONFIG_SHDISP_PANEL_RYOMA)
    ret = shdisp_ryoma_API_mipi_start_display();
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    ret = shdisp_gemini_API_mipi_start_display();
#else
    ret = shdisp_null_panel_API_mipi_start_display();
#endif

//#if !defined(CONFIG_SHDISP_PANEL_ANDY) && !defined(CONFIG_SHDISP_PANEL_MARCO) && !defined(CONFIG_SHDISP_PANEL_GEMINI)
//#endif

    SHDISP_DEBUG("out\n");
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_cmd_stop_prepare                                    */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_mipi_cmd_stop_prepare(void)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");

    SHDISP_PERFORMANCE("SUSPEND LCDC STOP-PREPARE 0010 START\n");

    shdisp_panel_API_request_RateCtrl(0,0,0);

    SHDISP_PERFORMANCE("SUSPEND LCDC STOP-PREPARE 0010 END\n");

    SHDISP_DEBUG("out\n");
    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_request_RateCtrl                                         */
/* ------------------------------------------------------------------------- */
void shdisp_panel_API_request_RateCtrl( int ctrl, unsigned char maxFR, unsigned char minFR )
{
#if defined(CONFIG_SHDISP_PANEL_MARCO)
    unsigned char param[2]    = { 0x00, 0x20 };
#elif defined(CONFIG_SHDISP_PANEL_ANDY)
    unsigned char param[2]    = { 0x0D, 0x11 };
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    unsigned char param[2]    = { 0x0D, 0x31 };
#elif defined(CONFIG_SHDISP_PANEL_RYOMA)
    unsigned char param[2]    = { 0x0D, 0x41 };
#else
    unsigned char param[2]    = { 0x00, 0x00 };
#endif

    SHDISP_DEBUG("in\n");

    if( ctrl ){
#if 1
#if defined(CONFIG_SHDISP_PANEL_ANDY) || defined(CONFIG_SHDISP_PANEL_RYOMA)
        if (sh_boot_get_bootmode() == SH_BOOT_F_T || sh_boot_get_bootmode() == SH_BOOT_H_C) {
            param[0] = 0x00;
        }
        else {
            param[0] = ( ((maxFR&0x0f) << 4) | (minFR&0x0f) );
        }
#else /* CONFIG_SHDISP_PANEL_ANDY */
        param[0] = 0x00;
#endif /* CONFIG_SHDISP_PANEL_ANDY */
#else
#if defined(CONFIG_SHDISP_PANEL_ANDY) || defined(CONFIG_SHDISP_PANEL_RYOMA)
        param[0] = 0x00;
#else
        param[0] = 0x00;
#endif /* CONFIG_SHDISP_PANEL_ANDY || CONFIG_SHDISP_PANEL_RYOMA */
#endif
    }
    else {
         param[0] = param[1] = 0;
    }

    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_SET_DEVCODE, sizeof(param), param );
    shdisp_FWCMD_buf_finish();
    shdisp_FWCMD_doKick( 1, 0, 0 );
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    SHDISP_DEBUG("out\n");
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_check_recovery                                           */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_check_recovery(void)
{
    if (shdisp_panel_fops->check_recovery) {
        return shdisp_panel_fops->check_recovery();
    }
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_get_recovery_type                                        */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_get_recovery_type(int *type)
{
    if (shdisp_panel_fops->recovery_type) {
        return shdisp_panel_fops->recovery_type(type);
    }
    return SHDISP_RESULT_SUCCESS;
}


int shdisp_panel_API_cabc_init(void)
{
    return 0;
}

int shdisp_panel_API_cabc_indoor_on(void)
{
    return 0;
}

int shdisp_panel_API_cabc_outdoor_on(int lut_level)
{
    return 0;
}

int shdisp_panel_API_cabc_off(int wait_on, int pwm_disable)
{
    return 0;
}

int shdisp_panel_API_cabc_outdoor_move(int lut_level)
{
    return 0;
}


int shdisp_panel_API_diag_set_gamma_info(struct shdisp_diag_gamma_info *gamma_info)
{
    if (shdisp_panel_fops->set_gamma_info) {
        return shdisp_panel_fops->set_gamma_info(gamma_info);
    }
    return SHDISP_RESULT_SUCCESS;
}

int shdisp_panel_API_diag_get_gamma_info(struct shdisp_diag_gamma_info *gamma_info)
{
    if (shdisp_panel_fops->get_gamma_info) {
        return shdisp_panel_fops->get_gamma_info(gamma_info);
    }
    return SHDISP_RESULT_SUCCESS;
}

int shdisp_panel_API_diag_set_gamma(struct shdisp_diag_gamma *gamma)
{
    if (shdisp_panel_fops->set_gamma) {
        return shdisp_panel_fops->set_gamma(gamma);
    }
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_power_on_recovery                                        */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_power_on_recovery(void)
{
    SHDISP_DEBUG("in\n");
    if ( shdisp_panel_fops->power_on_recovery ){
        shdisp_panel_fops->power_on_recovery();
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_power_off_recovery                                       */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_power_off_recovery(void)
{
    SHDISP_DEBUG("in\n");
    if ( shdisp_panel_fops->power_off_recovery ){
        shdisp_panel_fops->power_off_recovery();
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_set_drive_freq                                           */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_set_drive_freq(int type)
{
    SHDISP_DEBUG("in\n");
    if ( shdisp_panel_fops->set_drive_freq ){
        shdisp_panel_fops->set_drive_freq(type);
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_get_drive_freq                                           */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_get_drive_freq(void)
{
    SHDISP_DEBUG("in\n");
    if ( shdisp_panel_fops->get_drive_freq ){
        return shdisp_panel_fops->get_drive_freq();
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_A;
}

#if 0
/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_cmd_lcd_on_recovery                                 */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_mipi_cmd_lcd_on_recovery(void)
{
    int ret;
    SHDISP_DEBUG("in\n");
#if defined(CONFIG_SHDISP_PANEL_MARCO)
    ret = shdisp_marco_API_mipi_lcd_on_recovery();
#elif defined(CONFIG_SHDISP_PANEL_ANDY)
    ret = shdisp_andy_API_mipi_lcd_on_recovery();
#else
    ret = SHDISP_RESULT_SUCCESS;
#endif
    SHDISP_DEBUG("out\n");
    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_cmd_lcd_off_recovery                                */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_mipi_cmd_lcd_off_recovery(void)
{
    int ret;
    SHDISP_DEBUG("out\n");
#if defined(CONFIG_SHDISP_PANEL_MARCO)
    ret = shdisp_marco_API_mipi_lcd_off_recovery();
#elif defined(CONFIG_SHDISP_PANEL_ANDY)
    ret = shdisp_andy_API_mipi_lcd_off_recovery();
#else
    ret = SHDISP_RESULT_SUCCESS;
#endif
    SHDISP_DEBUG("out\n");
    return ret;
}
#endif

/*
 * mipi dsi buf mechanism
 */
static char *shdisp_panel_mipi_dsi_buf_reserve(struct dsi_buf *dp, int len)
{
    dp->data += len;
    return dp->data;
}
static char *shdisp_panel_mipi_dsi_buf_push(struct dsi_buf *dp, int len)
{
    dp->data -= len;
    dp->len += len;
    return dp->data;
}

static char *shdisp_panel_mipi_dsi_buf_reserve_hdr(struct dsi_buf *dp, int hlen)
{
    dp->hdr = (uint32 *)dp->data;
    return shdisp_panel_mipi_dsi_buf_reserve(dp, hlen);
}

static char *shdisp_panel_mipi_dsi_buf_init(struct dsi_buf *dp)
{
    int off;

    dp->data = dp->start;
    off = (int)dp->data;
    /* 8 byte align */
    off &= 0x07;
    if (off)
        off = 8 - off;
    dp->data += off;
    dp->len = 0;
    return dp->data;
}

/*
 * mipi dsi gerneric long write
 */
static int shdisp_panel_mipi_dsi_generic_lwrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
    unsigned short fwcmd_len;
    unsigned char fwcmd;
    char *bp;
    uint32 *hp;

    if (cm->dlen && cm->payload == 0) {
        SHDISP_ERR("NO payload error\n");
        return 0;
    }

    bp = shdisp_panel_mipi_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);

    /* copy payload */
    if(cm->payload) {
        memcpy( bp, cm->payload, cm->dlen );
        dp->len += cm->dlen;
    }

    /* fill up header */
    hp = dp->hdr;

    if (dsi_transfer_mode == SHDISP_DSI_HIGH_SPEED_MODE) {
        fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_LWRITE;
    }
    else {
        fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_LWRITE;
    }
    fwcmd_len = 4 + dp->len;

    *hp = 0x00000000;
    *hp = SHDISP_DSI_HDR_DTYPE(DTYPE_GEN_LWRITE);
    *hp |= (uint32)(SHDISP_DSI_HDR_DATA2(cm->dlen >> 8));
    *hp |= (uint32)(SHDISP_DSI_HDR_DATA1(cm->dlen & 0x00FF));

    shdisp_panel_mipi_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
    shdisp_FWCMD_buf_add( fwcmd, fwcmd_len, (unsigned char*)(hp) );

    return dp->len;
}

/*
 * mipi dsi gerneric short write with 0, 1 2 parameters
 */
static int shdisp_panel_mipi_dsi_generic_swrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
    uint32 *hp;
    int len;
    unsigned char fwcmd;
    unsigned short fwcmd_len;

    if (cm->dlen && cm->payload == 0) {
        SHDISP_ERR("NO payload error\n");
        return 0;
    }

    shdisp_panel_mipi_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
    hp = dp->hdr;

    fwcmd = shdisp_panel_API_get_fw_transfer_mode_w(cm->dtype, dsi_transfer_mode);
    fwcmd_len = 4;

    *hp = 0x00000000;

    len = (cm->dlen > 2) ? 2 : cm->dlen;

    if (len == 1) {
        *hp |= SHDISP_DSI_HDR_DTYPE(DTYPE_GEN_WRITE1);
        *hp |= SHDISP_DSI_HDR_DATA1(cm->payload[0]);
        *hp |= SHDISP_DSI_HDR_DATA2(0);
    } else if (len == 2) {
        *hp |= SHDISP_DSI_HDR_DTYPE(DTYPE_GEN_WRITE2);
        *hp |= SHDISP_DSI_HDR_DATA1(cm->payload[0]);
        *hp |= SHDISP_DSI_HDR_DATA2(cm->payload[1]);
    } else {
        SHDISP_ERR("len = %d", len);
        *hp |= SHDISP_DSI_HDR_DTYPE(DTYPE_GEN_WRITE);
        *hp |= SHDISP_DSI_HDR_DATA1(0);
        *hp |= SHDISP_DSI_HDR_DATA2(0);
    }
    shdisp_panel_mipi_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
    shdisp_FWCMD_buf_add( fwcmd, fwcmd_len, (unsigned char*)(hp) );
    return dp->len; /* 4 bytes */
}

/*
 * mipi dsi gerneric read with 0, 1 2 parameters
 */
static int shdisp_panel_mipi_dsi_generic_read(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
    uint32 *hp;
    int len;
    unsigned char fwcmd;
    unsigned short fwcmd_len;

    if (cm->dlen && cm->payload == 0) {
        SHDISP_ERR("NO payload error\n");
        return 0;
    }

    shdisp_panel_mipi_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
    hp = dp->hdr;

#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    test_dtype = cm->dtype & SHDISP_CLMR_FWCMD_DSI_DSI_TXC;
#else
    test_dtype = 0;
#endif
    fwcmd = shdisp_panel_API_get_fw_transfer_mode_r(dsi_transfer_mode);
    fwcmd_len = 4;
    test_dtype = 0;

    *hp = 0x00000000;

    len = (cm->dlen > 2) ? 2 : cm->dlen;

    if (len == 1) {
        *hp |= SHDISP_DSI_HDR_DTYPE(DTYPE_GEN_READ1);
        *hp |= SHDISP_DSI_HDR_DATA1(cm->payload[0]);
        *hp |= SHDISP_DSI_HDR_DATA2(0);
    } else if (len == 2) {
        *hp |= SHDISP_DSI_HDR_DTYPE(DTYPE_GEN_READ2);
        *hp |= SHDISP_DSI_HDR_DATA1(cm->payload[0]);
        *hp |= SHDISP_DSI_HDR_DATA2(cm->payload[1]);
    } else {
        *hp |= SHDISP_DSI_HDR_DTYPE(DTYPE_GEN_READ);
        *hp |= SHDISP_DSI_HDR_DATA1(0);
        *hp |= SHDISP_DSI_HDR_DATA2(0);
    }

    shdisp_panel_mipi_dsi_buf_push(dp,  DSI_HOST_HDR_SIZE);
    shdisp_FWCMD_buf_add( fwcmd, fwcmd_len, (unsigned char*)(hp) );
    return dp->len; /* 4 bytes */
}

/*
 * mipi dsi dcs long write
 */
static int shdisp_panel_mipi_dsi_dcs_lwrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
    char *bp;
    uint32 *hp;
    unsigned char fwcmd;
    unsigned short fwcmd_len;

    SHDISP_DEBUG("in\n");
    if (cm->dlen && cm->payload == 0) {
        SHDISP_ERR("NO payload error\n");
        return 0;
    }

    bp = shdisp_panel_mipi_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);

    /* copy payload */
    if(cm->payload) {
        memcpy( bp, cm->payload, cm->dlen );
        dp->len += cm->dlen;
    }

    /* fill up header */
    hp = dp->hdr;

    if (dsi_transfer_mode == SHDISP_DSI_HIGH_SPEED_MODE) {
        fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_LWRITE;
    }
    else {
        fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_LWRITE;
    }
    fwcmd_len = 4 + dp->len;

    *hp = 0x00000000;
    *hp = SHDISP_DSI_HDR_DTYPE(DTYPE_DCS_LWRITE);
    *hp |= (uint32)(SHDISP_DSI_HDR_DATA2(cm->dlen >> 8));
    *hp |= (uint32)(SHDISP_DSI_HDR_DATA1(cm->dlen & 0x00FF));

    shdisp_panel_mipi_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
    shdisp_FWCMD_buf_add( fwcmd, fwcmd_len, (unsigned char*)(hp) );

    SHDISP_DEBUG("out dp->len=%d\n", dp->len);
    return dp->len;
}


/*
 * mipi dsi dcs read
 */
static int shdisp_panel_mipi_dsi_dcs_read(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
    uint32 *hp;
    unsigned char fwcmd;
    unsigned short fwcmd_len;

    SHDISP_DEBUG("in\n");
    if (cm->dlen && cm->payload == 0) {
        SHDISP_ERR("NO payload error\n");
        return 0;
    }

    shdisp_panel_mipi_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
    hp = dp->hdr;

#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    test_dtype = cm->dtype & SHDISP_CLMR_FWCMD_DSI_DSI_TXC;
    fwcmd = shdisp_panel_API_get_fw_transfer_mode_r(dsi_transfer_mode);
    test_dtype = 0;
#else
    if (dsi_transfer_mode == SHDISP_DSI_HIGH_SPEED_MODE) {
        fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_BTA;
    }
    else {
        fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_BTA;
    }
#endif
    fwcmd_len = 4;
    *hp = 0x00000000;

    *hp |= SHDISP_DSI_HDR_DTYPE(DTYPE_DCS_READ);
    *hp |= SHDISP_DSI_HDR_DATA1(cm->payload[0]);   /* dcs command byte */
    *hp |= SHDISP_DSI_HDR_DATA2(0);

    shdisp_panel_mipi_dsi_buf_push(dp,  DSI_HOST_HDR_SIZE);
    shdisp_FWCMD_buf_add( fwcmd, fwcmd_len, (unsigned char*)(hp) );
    SHDISP_DEBUG("out dp->len=%d\n", dp->len);
    return dp->len; /* 4 bytes */
}


/*
 * mipi dsi dcs short write with 0 parameters
 */
static int shdisp_panel_mipi_dsi_dcs_swrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
    uint32 *hp;
    unsigned char fwcmd;
    unsigned short fwcmd_len;

    if (cm->payload == 0) {
        SHDISP_ERR("NO payload error\n");
        return -EINVAL;
    }

    shdisp_panel_mipi_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
    hp = dp->hdr;

    fwcmd = shdisp_panel_API_get_fw_transfer_mode_w(cm->dtype, dsi_transfer_mode);
    fwcmd_len = 4;
    *hp = 0x00000000;

    *hp |= SHDISP_DSI_HDR_DTYPE(DTYPE_DCS_WRITE);
    *hp |= SHDISP_DSI_HDR_DATA1(cm->payload[0]);   /* dcs command byte */
    *hp |= SHDISP_DSI_HDR_DATA2(0);

    shdisp_panel_mipi_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
    shdisp_FWCMD_buf_add( fwcmd, fwcmd_len, (unsigned char*)(hp) );
    return dp->len;
}

/*
 * mipi dsi dcs short write with 1 parameters
 */
static int shdisp_panel_mipi_dsi_dcs_swrite1(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
    uint32 *hp;
    unsigned char fwcmd;
    unsigned short fwcmd_len;

    if (cm->dlen < 2 || cm->payload == 0) {
        SHDISP_ERR("NO payload error\n");
        return -EINVAL;
    }

    shdisp_panel_mipi_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
    hp = dp->hdr;

    fwcmd = shdisp_panel_API_get_fw_transfer_mode_w(cm->dtype, dsi_transfer_mode);
    fwcmd_len = 4;

    *hp = 0x00000000;

    *hp |= SHDISP_DSI_HDR_DTYPE(DTYPE_DCS_WRITE1);
    *hp |= SHDISP_DSI_HDR_DATA1(cm->payload[0]);   /* dcs comamnd byte */
    *hp |= SHDISP_DSI_HDR_DATA2(cm->payload[1]);   /* parameter */

    shdisp_panel_mipi_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
    shdisp_FWCMD_buf_add( fwcmd, fwcmd_len, (unsigned char*)(hp) );
    return dp->len;
}


/*
 * mipi dsi dcs short write with 1 parameters
 */
#ifndef SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
static int shdisp_panel_mipi_dsi_dcs_swrite1_multi(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
    uint32 *hp;

    if (cm->dlen < 2 || cm->payload == 0) {
        SHDISP_ERR("NO payload error\n");
        return -EINVAL;
    }

    hp = (uint32*)dp->data;
    shdisp_panel_mipi_dsi_buf_reserve(dp, DSI_HOST_HDR_SIZE);

    *hp = 0x00000000;

    *hp |= SHDISP_DSI_HDR_DTYPE(DTYPE_DCS_WRITE1);
    *hp |= SHDISP_DSI_HDR_DATA1(cm->payload[0]);   /* dcs comamnd byte */
    *hp |= SHDISP_DSI_HDR_DATA2(cm->payload[1]);   /* parameter */
    dp->len += DSI_HOST_HDR_SIZE;
    return dp->len;
}
#endif

static int shdisp_panel_mipi_dsi_set_max_pktsize(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
    uint32 *hp;
    unsigned char fwcmd;
    unsigned short fwcmd_len;

    if (cm->payload == 0) {
        SHDISP_ERR("NO payload error\n");
        return -EINVAL;
    }

    shdisp_panel_mipi_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
    hp = dp->hdr;

    fwcmd = shdisp_panel_API_get_fw_transfer_mode_w(cm->dtype, dsi_transfer_mode);
    fwcmd_len = 4;
    *hp = 0x00000000;

    *hp |= SHDISP_DSI_HDR_DTYPE(DTYPE_MAX_PKTSIZE);
    *hp |= SHDISP_DSI_HDR_DATA1(cm->payload[0]);
    *hp |= SHDISP_DSI_HDR_DATA2(cm->payload[1]);

    shdisp_panel_mipi_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
    shdisp_FWCMD_buf_add( fwcmd, fwcmd_len, (unsigned char*)(hp) );
    return dp->len;
}

static int shdisp_panel_mipi_dsi_cmd_dma_add(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
    int len = 0;

    switch (cm->dtype & 0xff) {
    case DTYPE_GEN_WRITE:
    case DTYPE_GEN_WRITE1:
    case DTYPE_GEN_WRITE2:
        len = shdisp_panel_mipi_dsi_generic_swrite(dp, cm);
        break;
    case DTYPE_GEN_LWRITE:
        len = shdisp_panel_mipi_dsi_generic_lwrite(dp, cm);
        break;
    case DTYPE_GEN_READ:
    case DTYPE_GEN_READ1:
    case DTYPE_GEN_READ2:
        len = shdisp_panel_mipi_dsi_generic_read(dp, cm);
        break;
    case DTYPE_DCS_WRITE:
        len = shdisp_panel_mipi_dsi_dcs_swrite(dp, cm);
        break;
    case DTYPE_DCS_WRITE1:
        len = shdisp_panel_mipi_dsi_dcs_swrite1(dp, cm);
        break;
    case DTYPE_DCS_LWRITE:
        len = shdisp_panel_mipi_dsi_dcs_lwrite(dp, cm);
        break;
    case DTYPE_DCS_READ:
        len = shdisp_panel_mipi_dsi_dcs_read(dp, cm);
        break;
    case DTYPE_MAX_PKTSIZE:
        len = shdisp_panel_mipi_dsi_set_max_pktsize(dp, cm);
        break;
    case DTYPE_NULL_PKT:
    case DTYPE_BLANK_PKT:
    case DTYPE_CM_ON:
    case DTYPE_CM_OFF:
    case DTYPE_PERIPHERAL_ON:
    case DTYPE_PERIPHERAL_OFF:
    default:
        SHDISP_DEBUG("dtype=%x NOT supported\n", cm->dtype);
        break;
    }
    return len;
}

static int shdisp_panel_mipi_dsi_cmd_dma_tx(struct dsi_buf *tp)
{
    int ret = SHDISP_RESULT_SUCCESS;

    if (!shdisp_FWCMD_buf_get_nokick())
    {
        ret = shdisp_FWCMD_buf_finish();
        if (ret != SHDISP_RESULT_SUCCESS)
            return ret;
        ret = shdisp_FWCMD_doKick(1, 0, NULL);
    }
    return ret;
}

static int shdisp_panel_mipi_dsi_cmd_dma_rx(struct dsi_buf *tp, unsigned char *rp, int rlen)
{
    int ret = SHDISP_RESULT_SUCCESS;

    ret = shdisp_FWCMD_buf_finish();
    if (ret != SHDISP_RESULT_SUCCESS)
        return ret;

    ret = shdisp_FWCMD_doKick(1, rlen, rp);

    return ret;
}

static int shdisp_panel_mipi_dsi_cmd_dma_post_rx(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
#ifdef SHDISP_PANEL_OUT_BTA_AFTER_READ
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_MIPICMD);
    ret = shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_DSI_BTA, 0, NULL);
    if (ret != SHDISP_RESULT_SUCCESS) {
        return ret;
    }

    ret = shdisp_FWCMD_buf_finish();
    if (ret != SHDISP_RESULT_SUCCESS) {
        return ret;
    }

    ret = shdisp_FWCMD_doKick(1, 0, NULL);
#endif  /* SHDISP_PANEL_OUT_BTA_AFTER_READ */
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_dsi_cmds_tx                                         */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_mipi_dsi_cmds_tx(struct dsi_buf *tp, struct dsi_cmd_desc *cmds, int cnt)
{
    struct dsi_cmd_desc *cm;
    int i;
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in cnt=%d\n", cnt);
    /* turn on cmd mode
    * for video mode, do not send cmds more than
    * one pixel line, since it only transmit it
    * during BLLP.
    */
    cm = cmds;

    for (i = 0; i < cnt; i++) {
        shdisp_panel_mipi_dsi_buf_init(tp);
        shdisp_panel_mipi_dsi_cmd_dma_add(tp, cm);
        ret = shdisp_panel_mipi_dsi_cmd_dma_tx(tp);
        if (ret != SHDISP_RESULT_SUCCESS) {
            return ret;
        }
        if(cm->wait)
        {
            SHDISP_DEBUG("wait start=%d(ms)\n", cm->wait);
            shdisp_SYS_cmd_delay_us(cm->wait);
        }
        cm++;
    }
    SHDISP_DEBUG("out ret=%d\n", ret);
    return ret;
}

#ifndef SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
int shdisp_panel_API_mipi_dsi_cmds_mltshortpkt_tx(struct dsi_buf *tp, struct dsi_cmd_desc *cmds, int cnt)
{
    struct dsi_cmd_desc *cm;
    int i;
    char * head;
    int ret;
    int len = 0;

    if( cnt < 2 ) {
        SHDISP_ERR("shorthead count < 2...\n");
        return SHDISP_RESULT_FAILURE;
    }

    if( dsi_transfer_mode != SHDISP_DSI_HIGH_SPEED_MODE ) {
        SHDISP_ERR("DSI power mode is LP...\n");
        return SHDISP_RESULT_FAILURE;
    }

    head = shdisp_panel_mipi_dsi_buf_init(tp);

    for(i=0, cm=cmds; i < cnt; i++ ){
        if( cm->wait ){
            SHDISP_ERR("wait time but notwait..\n");
        }

        if( len+MIPI_DSI_SHORT_PACKET_SIZE > 64 ){
            ret = shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_HS_MULTI_SWRITE, tp->len ,head);
            head = shdisp_panel_mipi_dsi_buf_init(tp);
        }

        switch (cm->dtype) {
            case DTYPE_DCS_WRITE1:

                len = shdisp_panel_mipi_dsi_dcs_swrite1_multi(tp,cm);

                break;
            default:
                SHDISP_ERR("not shortpacket found...\n");
                break;
        }

        cm++;
    }

    ret = shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_HS_MULTI_SWRITE, tp->len ,head);

    return ret;
}
#endif

void shdisp_panel_API_mipi_dsi_cmds_tx_dummy(struct dsi_cmd_desc *cmds)
{
    struct dsi_cmd_desc *cm = NULL;
    cm = cmds;
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_dsi_cmds_rx                                         */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_mipi_dsi_cmds_rx(struct dsi_buf *tp, struct dsi_buf *rp, struct dsi_cmd_desc *cmds, unsigned char size)
{
    unsigned char rlen;
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("[S] size:%d\n", size);

    if (size <= 2) {
        rlen = MIPI_DSI_SHORT_PACKET_LEN;
    }
    else {
        rlen = size + MIPI_DSI_READ_RESPONSE_LEN;
    }

    /* packet size need to be set at every read */
    shdisp_panel_mipi_dsi_buf_init(tp);
    max_pktsize[0] = size;
    shdisp_panel_mipi_dsi_cmd_dma_add(tp, pkt_size_cmd);

    ret = shdisp_panel_mipi_dsi_cmd_dma_tx(tp);
    if (ret != SHDISP_RESULT_SUCCESS) {
        return ret;
    }

    shdisp_panel_mipi_dsi_buf_init(tp);
    shdisp_panel_mipi_dsi_cmd_dma_add(tp, cmds);

    ret = shdisp_panel_mipi_dsi_cmd_dma_rx(tp, (unsigned char*)rp->data, (int)rlen);
    if (ret != SHDISP_RESULT_SUCCESS) {
        return ret;
    }

    ret = shdisp_panel_mipi_dsi_cmd_dma_post_rx();
    if (ret != SHDISP_RESULT_SUCCESS) {
        return ret;
    }

    SHDISP_DEBUG("[E] rp->len:%d, rlen:%d\n", rp->len, rlen);

    return SHDISP_RESULT_SUCCESS;
}

#if defined(CONFIG_SHDISP_PANEL_GEMINI)
/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_dsi_cmds_rx2                                        */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_mipi_dsi_cmds_rx2(struct dsi_buf *tp, struct dsi_buf *rp, struct dsi_cmd_desc *cmds, unsigned char size)
{
    unsigned char rlen;
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("[S] size:%d\n", size);

    if (size <= 2) {
        rlen = MIPI_DSI_SHORT_PACKET_LEN;
    }
    else {
        rlen = size + MIPI_DSI_READ_RESPONSE_LEN;
    }

    /* packet size need to be set at every read */
    shdisp_panel_mipi_dsi_buf_init(tp);
    max_pktsize2[0] = size;
    pkt_size_cmd2[0].dtype = (pkt_size_cmd2[0].dtype & 0xFF) | (cmds->dtype & SHDISP_CLMR_FWCMD_DSI_DSI_TXC);
    shdisp_panel_mipi_dsi_cmd_dma_add(tp, pkt_size_cmd2);

    ret = shdisp_panel_mipi_dsi_cmd_dma_tx(tp);
    if (ret != SHDISP_RESULT_SUCCESS) {
        return ret;
    }

    shdisp_panel_mipi_dsi_buf_init(tp);
    shdisp_panel_mipi_dsi_cmd_dma_add(tp, cmds);

    ret = shdisp_panel_mipi_dsi_cmd_dma_rx(tp, (unsigned char*)rp->data, (int)rlen);
    if (ret != SHDISP_RESULT_SUCCESS) {
        return ret;
    }

    ret = shdisp_panel_mipi_dsi_cmd_dma_post_rx();
    if (ret != SHDISP_RESULT_SUCCESS) {
        return ret;
    }

    SHDISP_DEBUG("[E] rp->len:%d, rlen:%d\n", rp->len, rlen);

    return SHDISP_RESULT_SUCCESS;
}
#endif

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_set_transfer_mode                                   */
/* ------------------------------------------------------------------------- */
int shdisp_panel_API_mipi_set_transfer_mode(int mode)
{
    if (mode >= SHDISP_DSI_TRANSFER_MODE_MAX) {
        return SHDISP_RESULT_FAILURE;
    }
    dsi_transfer_mode = mode;
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_mipi_get_transfer_mode_w                                 */
/* ------------------------------------------------------------------------- */
unsigned char shdisp_panel_API_get_fw_transfer_mode_w(int dtype, int dsi_mode)
{
    unsigned char fwcmd = 0;
    switch(dsi_mode){
      case SHDISP_DSI_HIGH_SPEED_MODE:
        switch(dtype & SHDISP_CLMR_FWCMD_DSI_DSI_TXC){
        case SHDISP_CLMR_FWCMD_DSI_DSI_TX0:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_A;
            break;
        case SHDISP_CLMR_FWCMD_DSI_DSI_TX1:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_B;
            break;
        case SHDISP_CLMR_FWCMD_DSI_DSI_TXC:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_C;
            break;
        default:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE;
            break;
        }
        break;
      case SHDISP_DSI_LOW_POWER_MODE:
        switch(dtype & SHDISP_CLMR_FWCMD_DSI_DSI_TXC){
        case SHDISP_CLMR_FWCMD_DSI_DSI_TX0:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_A;
            break;
        case SHDISP_CLMR_FWCMD_DSI_DSI_TX1:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_B;
            break;
        case SHDISP_CLMR_FWCMD_DSI_DSI_TXC:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_C;
            break;
        default:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE;
            break;
        }
        break;
      case SHDISP_DSI_HIGH_SPEED_MODE_MS:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_A;
        break;
      case SHDISP_DSI_LOW_POWER_MODE_MS:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_A;
        break;
      case SHDISP_DSI_HIGH_SPEED_MODE_SL:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_B;
        break;
      case SHDISP_DSI_LOW_POWER_MODE_SL:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_B;
        break;
      case SHDISP_DSI_HIGH_SPEED_MODE_BOTH:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_C;
        break;
      case SHDISP_DSI_LOW_POWER_MODE_BOTH:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_C;
        break;
      default:
        fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE;
        SHDISP_ERR("unexpeted dsi_transfer_mode=%x\n", dsi_mode);
        break;
    }
    return fwcmd;
}

/* ------------------------------------------------------------------------- */
/* shdisp_panel_API_get_fw_transfer_mode_r                                 */
/* ------------------------------------------------------------------------- */

unsigned char shdisp_panel_API_get_fw_transfer_mode_r(int dsi_mode)
{
    unsigned char fwcmd = 0;
    switch(dsi_transfer_mode){
      case SHDISP_DSI_HIGH_SPEED_MODE:
        switch(test_dtype & SHDISP_CLMR_FWCMD_DSI_DSI_TXC){
        case SHDISP_CLMR_FWCMD_DSI_DSI_TX0:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_BTA_A;
            break;
        case SHDISP_CLMR_FWCMD_DSI_DSI_TX1:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_BTA_B;
            break;
        default:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_BTA;
            break;
        }
        break;
      case SHDISP_DSI_LOW_POWER_MODE:
        switch(test_dtype & SHDISP_CLMR_FWCMD_DSI_DSI_TXC){
        case SHDISP_CLMR_FWCMD_DSI_DSI_TX0:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_BTA_A;
            break;
        case SHDISP_CLMR_FWCMD_DSI_DSI_TX1:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_BTA_B;
            break;
        default:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_BTA;
            break;
        }
        break;
      case SHDISP_DSI_HIGH_SPEED_MODE_MS:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_BTA_A;
        break;
      case SHDISP_DSI_LOW_POWER_MODE_MS:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_BTA_A;
        break;
      case SHDISP_DSI_HIGH_SPEED_MODE_SL:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_BTA_B;
        break;
      case SHDISP_DSI_LOW_POWER_MODE_SL:
            fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_BTA_B;
        break;
      default:
        fwcmd = SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_BTA;
        SHDISP_ERR("unexpeted dsi_transfer_mode=%x\n", dsi_mode);
        break;
    }
    return fwcmd;
}

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
