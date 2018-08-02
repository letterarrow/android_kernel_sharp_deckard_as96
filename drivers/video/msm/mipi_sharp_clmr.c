/* drivers/video/msm/mipi_sharp_clmr.c  (Display Driver)
 *
 * Copyright (C) 2011-2012 SHARP CORPORATION
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

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mdp.h"
#include "mdp4.h"
#include "mipi_sharp.h"
#include "mipi_sharp_clmr.h"
#include "../../sharp/shdisp/shdisp_dbg.h"
//#include "../../sharp/shdisp/shdisp_clmr.h"
//#include "../../sharp/shdisp/shdisp_pm.h"
#include <sharp/shdisp_kerl.h>
#include <linux/fb.h>
#include <linux/hrtimer.h>

/* ------------------------------------------------------------------------- */
/* DEBUG MACRAOS                                                             */
/* ------------------------------------------------------------------------- */

#define MIPI_SHARP_RW_MAX_SIZE          SHDISP_LCDDR_BUF_MAX
#define MIPI_SHARP_R_SIZE               10
#define LCD_RECOVERY_LOOP_MAX            3
//#define MIPI_SHARP_CLMR_1HZ_BLACK_ON    1
//#define MIPI_SHARP_CLMR_AUTO_PAT_OFF    0

enum {
    LCD_RECOVERY_STOP,
    LCD_RECOVERY_ACTIVE,
    NUM_RECOVERY_STATUS
};

static int lcd_recovery_fail_flg = 0;
static int lcd_recovering = LCD_RECOVERY_STOP;
static int mipi_sh_clmr_panel_ch_used[3];

#if defined(CONFIG_SHDISP_PANEL_IGZO)
enum { 
    START_DISPLAY_OFF,
    START_DISPLAY_ON, 
    NUM_START_DISPLAY 
};
static int mipi_sh_start_display = START_DISPLAY_OFF;
#endif

//#ifndef SHDISP_NOT_SUPPORT_DET
//#endif

static int mipi_sharp_clmr_panel_lcd_on(struct platform_device *pdev);
static int mipi_sharp_clmr_panel_lcd_off(struct platform_device *pdev);
static void mipi_sharp_clmr_panel_lcd_set_backlight(struct msm_fb_data_type *mfd);

static int mipi_sharp_clmr_panel_cmd_sqe_cabc_init(struct msm_fb_data_type *mfd, struct mipi_panel_info *mipi);
static int mipi_sharp_clmr_panel_cmd_sqe_cabc_indoor_on(struct msm_fb_data_type *mfd, struct mipi_panel_info *mipi);
static int mipi_sharp_clmr_panel_cmd_sqe_cabc_outdoor_on(struct msm_fb_data_type *mfd, struct mipi_panel_info *mipi, int lut_level);
static int mipi_sharp_clmr_panel_cmd_sqe_cabc_off(struct msm_fb_data_type *mfd, struct mipi_panel_info *mipi, int wait_on, int pwm_disable);
static int mipi_sharp_clmr_panel_cmd_sqe_cabc_outdoor_move(struct msm_fb_data_type *mfd, struct mipi_panel_info *mipi, int lut_level);

static int mipi_sharp_clmr_panel_cabc_ctrl(struct platform_device *pdev);
static int mipi_sharp_clmr_panel_start_disp(void);

#if 1  /* CUST_ID_00204 */ 
void msm_fb_perflock_ctrl(int on);
#endif


void msm_lcd_recovery_subscribe(void)
{
#ifndef SHDISP_NOT_SUPPORT_DET
    struct shdisp_subscribe subscribe;

    if (lcd_recovering != LCD_RECOVERY_STOP) {
        return;
    }

    subscribe.irq_type = SHDISP_IRQ_TYPE_DET;
    subscribe.callback = msm_lcd_recovery;
    shdisp_api_event_subscribe(&subscribe);
#endif
}

void msm_lcd_recovery_unsubscribe(void)
{
#ifndef SHDISP_NOT_SUPPORT_DET
    if (lcd_recovering != LCD_RECOVERY_STOP) {
        return;
    }

    shdisp_api_event_unsubscribe(SHDISP_IRQ_TYPE_DET);
#endif
}

DEFINE_MUTEX(recovery_lock);
void lock_recovery(void)
{
    mutex_lock(&recovery_lock);
}

void unlock_recovery(void)
{
    mutex_unlock(&recovery_lock);
}

#if 1
void msm_lcd_recovery(void)
{
#ifndef SHDISP_NOT_SUPPORT_DET
    int i;
    struct msm_fb_data_type *mfd;
    struct msm_fb_panel_data *pdata = NULL;
    struct fb_info *info = registered_fb[0];
    if (!info){
        return;
    }

    printk("msm_lcd_recovery start\n");

    lock_recovery();
#ifdef CONFIG_SHLCDC_BOARD /* CUST_ID_00024 */
    if (!((struct msm_fb_data_type *)info->par)->panel_power_on && fb_suspend_cond != FB_SUSPEND_COND_1HZ) {
        unlock_recovery();
        printk(KERN_INFO"msm_lcd_recovery do not recovery because lcd is off\n");
        return;
    }
#endif	/* CONFIG_SHLCDC_BOARD */
    lcd_recovering = LCD_RECOVERY_ACTIVE;

    for (i=0; i<LCD_RECOVERY_LOOP_MAX; i++) {

        mfd = (struct msm_fb_data_type *)info->par;
        if (mfd->panel_power_on) {
            shdisp_api_do_recovery(0);
        } else {
            shdisp_api_do_recovery(1);
#if defined(CONFIG_SHDISP_PANEL_IGZO)
            mipi_sh_start_display = START_DISPLAY_OFF;
#endif
            fb_suspend_cond = FB_SUSPEND_COND_NORMAL_ON;
            lcd_recovery_fail_flg = 0;
            lcd_recovering = LCD_RECOVERY_STOP;
            unlock_recovery();
            printk("msm_lcd_recovery power off\n");
            return;
        }
        if( mfd->panel_power_on ){
            pdata = (struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;
            if(pdata && pdata->start_display ){
#if defined(CONFIG_SHDISP_PANEL_IGZO)
                mipi_sh_start_display = START_DISPLAY_OFF;
#endif
                pdata->start_display(mfd);
                shdisp_api_do_recovery_set_bklmode_luxmode();
            }
        }

        shdisp_api_main_disp_on();


        if (shdisp_api_check_recovery() == SHDISP_RESULT_SUCCESS) {
            lcd_recovering = LCD_RECOVERY_STOP;
            lcd_recovery_fail_flg = 0;
            unlock_recovery();
            printk("msm_lcd_recovery completed\n");

            return;
        }

    }

    lcd_recovery_fail_flg = 1;
    lcd_recovering = LCD_RECOVERY_STOP;

    unlock_recovery();
    printk("msm_lcd_recovery retry over\n");
#endif
}
#else /* #if 1 */
//#ifndef SHDISP_NOT_SUPPORT_DET
//#endif
}
#endif /* #if 1 */

void mipi_dsi_det_recovery(void)
{
#ifndef SHDISP_NOT_SUPPORT_DET
    shdisp_api_do_mipi_dsi_det_recovery();
#endif
}

int mipi_sharp_is_recovery(void)
{
    return lcd_recovering;
}

int mipi_sharp_is_recovery_faild(void)
{
    return lcd_recovery_fail_flg;
}

int mipi_sharp_is_fw_timeout(void)
{
#if defined(CONFIG_SHDISP_PANEL_IGZO)
    return shdisp_api_main_mipi_cmd_is_fw_timeout();
#else
    return 0;
#endif
}

void mipi_dsi_det_recovery_flag_clr(void)
{
#ifndef SHDISP_NOT_SUPPORT_DET
#endif
}


//#ifndef SHDISP_NOT_SUPPORT_DET
//#endif


int mipi_sharp_api_set_lpf(unsigned char *param)
{
    return 0;
}

void mipi_sharp_delay_us(unsigned long usec)
{
    struct timespec tu;

    if (usec >= 1000*1000) {
        tu.tv_sec  = usec / 1000000;
        tu.tv_nsec = (usec % 1000000) * 1000;
    }
    else
    {
        tu.tv_sec  = 0;
        tu.tv_nsec = usec * 1000;
    }

    hrtimer_nanosleep(&tu, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC);

    return;
}














static int mipi_sharp_clmr_panel_lcd_on(struct platform_device *pdev)
{
#ifdef CONFIG_USES_SHLCDC
    SHDISP_DEBUG("in\n");

    SHDISP_VIDEO_PERFORMANCE("RESUME PANEL LCD-ON 0010 START\n");
#if defined(CONFIG_SHDISP_PANEL_IGZO)
    if(fb_suspend_cond == FB_SUSPEND_COND_1HZ){
        ;
    }
    else {
        shdisp_api_main_mipi_cmd_lcd_on();
    }
#else
    shdisp_api_main_mipi_cmd_lcd_on();
#endif
    SHDISP_VIDEO_PERFORMANCE("RESUME PANEL LCD-ON 0010 END\n");
    
    SHDISP_DEBUG("out\n");
#endif
    return 0;
}

static int mipi_sharp_clmr_panel_lcd_off(struct platform_device *pdev)
{
#ifdef CONFIG_USES_SHLCDC
    SHDISP_DEBUG("in\n");
    
    SHDISP_VIDEO_PERFORMANCE("SUSPEND PANEL LCD-OFF 0010 START\n");
#if defined(CONFIG_SHDISP_PANEL_IGZO)
    if(fb_suspend_cond == FB_SUSPEND_COND_1HZ){
        shdisp_api_main_mipi_cmd_lcd_off_black_screen_on();
        if(mipi_sh_start_display == START_DISPLAY_OFF)
        {
            mipi_sharp_clmr_panel_start_disp();
        }
    }
    else {
        shdisp_api_main_mipi_cmd_lcd_stop_prepare();
        shdisp_api_main_mipi_cmd_lcd_off();
        SHDISP_VIDEO_PERFORMANCE("SUSPEND PANEL POWER-OFF 0010 START\n");
        shdisp_api_main_lcd_power_off();
        SHDISP_VIDEO_PERFORMANCE("SUSPEND PANEL POWER-OFF 0010 END\n");
        mipi_sh_start_display = START_DISPLAY_OFF;
    }
#else
    shdisp_api_main_mipi_cmd_lcd_stop_prepare();
    shdisp_api_main_mipi_cmd_lcd_off();
    SHDISP_VIDEO_PERFORMANCE("SUSPEND PANEL POWER-OFF 0010 START\n");
    shdisp_api_main_lcd_power_off();
    SHDISP_VIDEO_PERFORMANCE("SUSPEND PANEL POWER-OFF 0010 END\n");
#endif
    SHDISP_VIDEO_PERFORMANCE("SUSPEND PANEL LCD-OFF 0010 END\n");
    
    SHDISP_DEBUG("out\n");
#endif

    lcd_recovery_fail_flg = 0;

    return 0;
}


static int mipi_sharp_clmr_panel_start_disp(void)
{
    SHDISP_VIDEO_PERFORMANCE("RESUME PANEL START-DISP 0010 START\n");
    shdisp_api_main_mipi_cmd_lcd_start_display();
    SHDISP_VIDEO_PERFORMANCE("RESUME PANEL START-DISP 0010 END\n");
//#if defined(CONFIG_SHDISP_PANEL_ANDY)
#if defined(CONFIG_SHDISP_PANEL_IGZO)
    mipi_sh_start_display = START_DISPLAY_ON;
#endif
    if( lcd_recovering != LCD_RECOVERY_ACTIVE ){
        if( shdisp_api_check_det() != SHDISP_RESULT_SUCCESS ){
            shdisp_api_main_disp_init_2nd();
            shdisp_api_main_disp_on();
            msm_lcd_recovery_subscribe();
            shdisp_api_requestrecovery();
        }
    }
    return 0;
}
static int mipi_sharp_clmr_panel_lcd_start_display(struct msm_fb_data_type *mfd)
{
#ifdef CONFIG_USES_SHLCDC

#if defined(CONFIG_SHDISP_PANEL_IGZO)
    SHDISP_DEBUG("in. fb_suspend_cond=%d mipi_sh_start_display=%d\n", fb_suspend_cond, mipi_sh_start_display);
    if(mipi_sh_start_display == START_DISPLAY_OFF)
    {
        mipi_sharp_clmr_panel_start_disp();
    }
    else
    {
        shdisp_api_main_mipi_cmd_lcd_on_after_black_screen();
    }
#else
    SHDISP_DEBUG("in\n");
    mipi_sharp_clmr_panel_start_disp();

#endif

	SHDISP_DEBUG("out\n");
#endif
    return 0;
}

static void mipi_sharp_clmr_panel_lcd_set_backlight(struct msm_fb_data_type *mfd)
{
    struct shdisp_main_bkl_ctl bkl;
    SHDISP_DEBUG("in\n");
    SHDISP_DEBUG("[SHDISP]%s in %d\n", __func__, mfd->bl_level);

    if (mfd->bl_level == 0) {

        SHDISP_VIDEO_PERFORMANCE("SUSPEND PANEL BACKLIGHT-OFF 0010 START\n");
        shdisp_api_main_bkl_off();
        SHDISP_VIDEO_PERFORMANCE("SUSPEND PANEL BACKLIGHT-OFF 0010 END\n");

    }else{
        bkl.mode = SHDISP_MAIN_BKL_MODE_FIX;
        bkl.param = mfd->bl_level;

        SHDISP_VIDEO_PERFORMANCE("RESUME PANEL BACKLIGHT-ON 0010 START\n");
        shdisp_api_main_bkl_on(&bkl);
        SHDISP_VIDEO_PERFORMANCE("RESUME PANEL BACKLIGHT-ON 0010 END\n");
#if 1  /* CUST_ID_00204 */ 
        msm_fb_perflock_ctrl(0);
#endif

    }
    SHDISP_DEBUG("out\n");
}


static int mipi_sharp_clmr_panel_cmd_sqe_cabc_init(struct msm_fb_data_type *mfd, struct mipi_panel_info *mipi)
{
    return 0;
}

static int mipi_sharp_clmr_panel_cmd_sqe_cabc_indoor_on(struct msm_fb_data_type *mfd, struct mipi_panel_info *mipi)
{
    return 0;
}


static int mipi_sharp_clmr_panel_cmd_sqe_cabc_outdoor_on(struct msm_fb_data_type *mfd, struct mipi_panel_info *mipi, int lut_level)
{
    return 0;
}

static int mipi_sharp_clmr_panel_cmd_sqe_cabc_off(struct msm_fb_data_type *mfd, struct mipi_panel_info *mipi, int wait_on, int pwm_disable)
{
    return 0;
}

static int mipi_sharp_clmr_panel_cmd_sqe_cabc_outdoor_move(struct msm_fb_data_type *mfd, struct mipi_panel_info *mipi, int lut_level)
{
    return 0;
}

static int mipi_sharp_clmr_panel_cabc_ctrl(struct platform_device *pdev)
{
    return 0;
}


static int __devinit mipi_sharp_clmr_panel_lcd_probe(struct platform_device *pdev)
{
    struct mipi_dsi_panel_platform_data *platform_data;
    struct msm_fb_panel_data            *pdata;
    struct msm_panel_info               *pinfo;

    static u32 width, height;

    SHDISP_DEBUG("in pdev->id=%d\n", pdev->id);
    if (pdev->id == 0) {
        platform_data = pdev->dev.platform_data;

        if (platform_data) {
            width  = (platform_data->width_in_mm);
            height = (platform_data->height_in_mm);
        }

        SHDISP_DEBUG("out1\n");
        return 0;
    }

    pdata = (struct msm_fb_panel_data *)pdev->dev.platform_data;
    pinfo = &(pdata->panel_info);

    pinfo->width_in_mm  = width;
    pinfo->height_in_mm = height;

    msm_fb_add_device(pdev);

    SHDISP_DEBUG("out\n");
    return 0;
}

static struct platform_driver this_driver = {
    .probe  = mipi_sharp_clmr_panel_lcd_probe,
    .driver = {
        .name   = "mipi_sharp_clmr_panel",
    },
};

static struct msm_fb_panel_data sharp_clmr_panel_panel_data = {
    .on     = mipi_sharp_clmr_panel_lcd_on,
    .off    = mipi_sharp_clmr_panel_lcd_off,
    .set_backlight = mipi_sharp_clmr_panel_lcd_set_backlight,
    .start_display = mipi_sharp_clmr_panel_lcd_start_display,

    .cabc_init          = mipi_sharp_clmr_panel_cmd_sqe_cabc_init,
    .cabc_indoor_on     = mipi_sharp_clmr_panel_cmd_sqe_cabc_indoor_on,
    .cabc_outdoor_on    = mipi_sharp_clmr_panel_cmd_sqe_cabc_outdoor_on,
    .cabc_off           = mipi_sharp_clmr_panel_cmd_sqe_cabc_off,
    .cabc_outdoor_move  = mipi_sharp_clmr_panel_cmd_sqe_cabc_outdoor_move,
    .cabc_ctrl          = mipi_sharp_clmr_panel_cabc_ctrl,
};

int mipi_sharp_device_register(struct msm_panel_info *pinfo, u32 channel, u32 panel)
{
    struct platform_device *pdev = NULL;
    int ret;

    SHDISP_DEBUG("in\n");
    if ((channel >= 3) || mipi_sh_clmr_panel_ch_used[channel]){
        SHDISP_DEBUG("out1\n");
        return -ENODEV;
    }

    mipi_sh_clmr_panel_ch_used[channel] = TRUE;

    pdev = platform_device_alloc("mipi_sharp_clmr_panel", (panel << 8)|channel);
    if (!pdev){
        SHDISP_DEBUG("out2\n");
        return -ENOMEM;
    }

    sharp_clmr_panel_panel_data.panel_info = *pinfo;

    ret = platform_device_add_data(pdev, &sharp_clmr_panel_panel_data,
        sizeof(sharp_clmr_panel_panel_data));
    if (ret) {
        printk(KERN_ERR
          "%s: platform_device_add_data failed!\n", __func__);
        goto err_device_put;
    }

    ret = platform_device_add(pdev);
    if (ret) {
        printk(KERN_ERR
          "%s: platform_device_register failed!\n", __func__);
        goto err_device_put;
    }
    ret = shdisp_api_main_mipi_cmd_lcd_probe();
    if (ret) {
        printk(KERN_ERR
          "%s: shdisp_api_main_mipi_cmd_lcd_probe failed!\n", __func__);
        goto err_device_put;
    }
    SHDISP_DEBUG("out\n");
    return 0;

err_device_put:
    platform_device_put(pdev);
    return ret;
}

static int __init mipi_sharp_clmr_panel_lcd_init(void)
{
    int ret;

    SHDISP_DEBUG("[SHDISP]%s in\n", __func__);
    ret = platform_driver_register(&this_driver);
    if( ret < 0 )
    {
        goto mipi_init_err_1;
    }
    return 0;

mipi_init_err_1:
    return -1;
}
module_init(mipi_sharp_clmr_panel_lcd_init);

static void mipi_sharp_clmr_panel_lcd_exit(void)
{
    platform_driver_unregister(&this_driver);
    return;
}
module_exit(mipi_sharp_clmr_panel_lcd_exit);
