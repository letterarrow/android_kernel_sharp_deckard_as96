/* drivers/sharp/shdisp_no_lcdc/shdisp_dbg.h  (Display Driver)
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

#ifndef SHDISP_DBG_H
#define SHDISP_DBG_H

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */

#define SHDISP_LOG_ENABLE


#ifdef SHDISP_LOG_ENABLE
    extern unsigned char shdisp_log_lv;
    #define SHDISP_SET_LOG_LV(lv) shdisp_log_lv = lv;
    #define SHDISP_PRINTK(lv, fmt, args...) if ((shdisp_log_lv & lv) != 0) printk(fmt, ## args);
#else   /* SHDISP_LOG_ENABLE */
    #define SHDISP_SET_LOG_LV(lv)
    #define SHDISP_PRINTK(lv, fmt, args...)
#endif   /* SHDISP_LOG_ENABLE */

#define SHDISP_LOG_LV_ERR       0x01
#define SHDISP_LOG_LV_TRACE     0x02
#define SHDISP_LOG_LV_DEBUG     0x04
#define SHDISP_LOG_LV_PERFORM   0x08

#define SHDISP_ERR(fmt, args...) \
        SHDISP_PRINTK(SHDISP_LOG_LV_ERR, KERN_ERR "[SHDISP_ERROR][%s] " fmt, __func__, ## args)

#define SHDISP_TRACE(fmt, args...) \
        SHDISP_PRINTK(SHDISP_LOG_LV_TRACE, KERN_INFO "[SHDISP_TRACE][%s] " fmt, __func__, ## args)

#define SHDISP_DEBUG(fmt, args...) \
        SHDISP_PRINTK(SHDISP_LOG_LV_DEBUG, KERN_DEBUG "[SHDISP_DEBUG][%s] " fmt, __func__, ## args)

#if defined (CONFIG_ANDROID_ENGINEERING)
    #define SHDISP_PERFORMANCE(fmt, args...) \
            SHDISP_PRINTK(SHDISP_LOG_LV_PERFORM, ",[SHDISP_PERFORM]" fmt, ## args)
#else /* CONFIG_ANDROID_ENGINEERING */
    #define SHDISP_PERFORMANCE(fmt, args...)
#endif /* CONFIG_ANDROID_ENGINEERING */


/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

#endif /* SHDISP_DBG_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */

