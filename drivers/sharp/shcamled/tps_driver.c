/* drivers/sharp/shcamled/tps_driver.c
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

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/leds-pmic8058.h>
#include <linux/leds-pm8xxx.h>
#include <linux/err.h>
#include <sharp/tps_driver.h>

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
static int tps_driver_control_port(int isEnable);
extern int shcamled_pmic_req_torch_led_current_off(void);
extern int shcamled_pmic_get_led_conflict(void);

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define FALSE 0
#define TRUE 1

//#define TPS_DRIVER_ENABLE_DEBUG

#ifdef TPS_DRIVER_ENABLE_DEBUG
#define TPS_DRIVER_INFO(x...)   printk(KERN_INFO "[TPS_DRIVER] " x)
#define TPS_DRIVER_TRACE(x...)  printk(KERN_DEBUG "[TPS_DRIVER] " x)
#define TPS_DRIVER_ERROR(x...)  printk(KERN_ERR "[TPS_DRIVER] " x)
#else
#define TPS_DRIVER_INFO(x...)   do {} while(0)
#define TPS_DRIVER_TRACE(x...)  do {} while(0)
#define TPS_DRIVER_ERROR(x...)  printk(KERN_ERR "[TPS_DRIVER] " x)
#endif

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static int tps_driver_use_usb = FALSE;
static int tps_driver_use_camled = FALSE;
static int tps_driver_use_hml = FALSE;
static struct regulator *ext_5v = NULL;
DEFINE_MUTEX(tps_driver_mut);

/* ------------------------------------------------------------------------- */
/* tps_driver_control_port                                                   */
/* ------------------------------------------------------------------------- */
static int tps_driver_control_port(int isEnable)
{
    TPS_DRIVER_TRACE("%s req:%d \n", __func__, isEnable);

    if(isEnable == TRUE) {
        if(ext_5v == NULL) {
            ext_5v = regulator_get(NULL, "ext_5v");
            if (IS_ERR(ext_5v)) {
                pr_err("%s: ext_5v get failed\n", __func__);
                ext_5v = NULL;
                return TPS_DRIVER_FAILURE;
            }
            if(regulator_enable(ext_5v)) {
                TPS_DRIVER_ERROR("%s: ext_5v enable failed\n", __func__);
                return TPS_DRIVER_FAILURE;
            }
        }
    }
    else {
        regulator_disable(ext_5v);
        regulator_put(ext_5v);
        ext_5v = NULL;
    }

    TPS_DRIVER_TRACE("%s req:%d proc done\n", __func__, isEnable);

    return TPS_DRIVER_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* tps_driver_register_user                                                  */
/* ------------------------------------------------------------------------- */
int tps_driver_register_user(int user)
{
    int rc = TPS_DRIVER_FAILURE;
    int ctrl_rc = FALSE;

    if(user < TPS_DRIVER_USER_USB || user > TPS_DRIVER_USER_CAMLED) {
        TPS_DRIVER_ERROR("[E] %s L.%d\n", __func__, __LINE__);
        return TPS_DRIVER_FAILURE;
    }

    mutex_lock(&tps_driver_mut);

    if((user == TPS_DRIVER_USER_USB && tps_driver_use_usb == TRUE)
    || (user == TPS_DRIVER_USER_HML && tps_driver_use_hml == TRUE)
    || (user == TPS_DRIVER_USER_CAMLED && tps_driver_use_camled == TRUE)) {
        TPS_DRIVER_INFO("%s %d user registered already\n", __func__, user);
        mutex_unlock(&tps_driver_mut);
        return TPS_DRIVER_SUCCESS;
    }

    if(user == TPS_DRIVER_USER_CAMLED && tps_driver_use_usb == TRUE &&
       shcamled_pmic_get_led_conflict() == TRUE) {
        TPS_DRIVER_INFO("%s USB is in use. %d user\n", __func__, user);
        mutex_unlock(&tps_driver_mut);
        return TPS_DRIVER_FAILURE;
    }

    TPS_DRIVER_TRACE("%s %d user U:%d C:%d\n",
                   __func__, user,tps_driver_use_usb, tps_driver_use_camled);

    switch (user) {
    case TPS_DRIVER_USER_USB:
        if(tps_driver_use_usb == FALSE) {
            rc = TPS_DRIVER_SUCCESS;

            if(tps_driver_use_hml == FALSE &&
               tps_driver_use_camled == FALSE) {
                rc = tps_driver_control_port(TRUE);
            }
            else if(tps_driver_use_camled == TRUE) {
                ctrl_rc = shcamled_pmic_req_torch_led_current_off();
                TPS_DRIVER_TRACE("%s proc turning off LED L.%d\n",
                                                     __func__, __LINE__);
                if (ctrl_rc == TRUE) {
                    tps_driver_use_camled = FALSE;
                }
            }
            if(rc == TPS_DRIVER_SUCCESS) {
                tps_driver_use_usb = TRUE;
            }
        }
        else {
            TPS_DRIVER_ERROR("[E] %s L.%d\n", __func__, __LINE__);
        }
        break;

    case TPS_DRIVER_USER_CAMLED:
        if(tps_driver_use_camled == FALSE) {
            rc = TPS_DRIVER_SUCCESS;

            if(tps_driver_use_usb == FALSE &&
               tps_driver_use_hml == FALSE) {
                rc = tps_driver_control_port(TRUE);
            }
            if(rc == TPS_DRIVER_SUCCESS) {
                tps_driver_use_camled = TRUE;
            }
        }
        else {
            TPS_DRIVER_ERROR("[E] %s L.%d\n", __func__, __LINE__);
        }
        break;

    case TPS_DRIVER_USER_HML:
        if(tps_driver_use_hml == FALSE) {
            rc = TPS_DRIVER_SUCCESS;

            if(tps_driver_use_usb == FALSE &&
               tps_driver_use_camled == FALSE) {
                rc = tps_driver_control_port(TRUE);
            }
            if(rc == TPS_DRIVER_SUCCESS) {
                tps_driver_use_hml = TRUE;
            }
        }
        else {
            TPS_DRIVER_ERROR("[E] %s L.%d\n", __func__, __LINE__);
        }
        break;

    default:
        break;
    }

    TPS_DRIVER_TRACE("%s done\n", __func__);

    mutex_unlock(&tps_driver_mut);

    return rc;
}
EXPORT_SYMBOL(tps_driver_register_user);

/* ------------------------------------------------------------------------- */
/* tps_driver_unregister_user                                                */
/* ------------------------------------------------------------------------- */
int tps_driver_unregister_user(int user)
{
    int rc = TPS_DRIVER_FAILURE;

    if(user < TPS_DRIVER_USER_USB || user > TPS_DRIVER_USER_CAMLED) {
        TPS_DRIVER_ERROR("[E] %s L.%d\n", __func__, __LINE__);
        return TPS_DRIVER_FAILURE;
    }

    mutex_lock(&tps_driver_mut);

    if((user == TPS_DRIVER_USER_USB && tps_driver_use_usb == FALSE)
    || (user == TPS_DRIVER_USER_HML &&  tps_driver_use_hml == FALSE)
    || (user == TPS_DRIVER_USER_CAMLED && tps_driver_use_camled == FALSE)) {
        TPS_DRIVER_TRACE("%s %d user unregistered already\n", __func__, user);
        mutex_unlock(&tps_driver_mut);
        return TPS_DRIVER_SUCCESS;
    }

    TPS_DRIVER_TRACE("%s %d user U:%d C:%d\n",
                   __func__, user,tps_driver_use_usb, tps_driver_use_camled);

    switch (user) {
    case TPS_DRIVER_USER_USB:
        if(tps_driver_use_usb == TRUE) {
            rc = TPS_DRIVER_SUCCESS;

            if(tps_driver_use_hml == FALSE &&
               tps_driver_use_camled == FALSE) {
                rc = tps_driver_control_port(FALSE);
            }
            tps_driver_use_usb = FALSE;
        }
        break;

    case TPS_DRIVER_USER_CAMLED:
        if(tps_driver_use_camled == TRUE) {
            rc = TPS_DRIVER_SUCCESS;

            if(tps_driver_use_usb == FALSE &&
               tps_driver_use_hml == FALSE) {
                rc = tps_driver_control_port(FALSE);
            }
            tps_driver_use_camled = FALSE;
        }
        break;

    case TPS_DRIVER_USER_HML:
        if(tps_driver_use_hml == TRUE) {
            rc = TPS_DRIVER_SUCCESS;

            if(tps_driver_use_usb == FALSE &&
               tps_driver_use_camled == FALSE) {
                rc = tps_driver_control_port(FALSE);
            }
            tps_driver_use_hml = FALSE;
        }
        break;

    default:
        break;
    }

    TPS_DRIVER_TRACE("%s done\n", __func__);

    mutex_unlock(&tps_driver_mut);

    return rc;
}
EXPORT_SYMBOL(tps_driver_unregister_user);

/* ------------------------------------------------------------------------- */
/* tps_driver_get_user                                                       */
/* ------------------------------------------------------------------------- */
int tps_driver_get_user(void)
{

    if(tps_driver_use_usb == TRUE) {
        if(tps_driver_use_camled == FALSE)
            return TPS_DRIVER_USER_USB;
        else
            return TPS_DRIVER_USER_BOTH;
    }
    else if(tps_driver_use_hml == TRUE) {
        if(tps_driver_use_camled == FALSE)
            return TPS_DRIVER_USER_HML;
        else
            return TPS_DRIVER_USER_BOTH;
    }
    else {
        if(tps_driver_use_camled == FALSE)
            return TPS_DRIVER_USER_NONE;
        else
            return TPS_DRIVER_USER_CAMLED;
    }
}
EXPORT_SYMBOL(tps_driver_get_user);

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
