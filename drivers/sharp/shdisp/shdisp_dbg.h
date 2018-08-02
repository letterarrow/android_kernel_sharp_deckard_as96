/* drivers/sharp/shdisp/shdisp_dbg.h  (Display Driver)
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


#include <linux/tty.h>

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */

#if defined (CONFIG_ANDROID_ENGINEERING)
    #define SHDISP_LOG_ENABLE
#endif /* CONFIG_ANDROID_ENGINEERING */


#ifdef SHDISP_LOG_ENABLE
    extern unsigned char shdisp_log_lv;
    #define SHDISP_SET_LOG_LV(lv) shdisp_log_lv = lv;
    #if defined (CONFIG_ANDROID_ENGINEERING)
      #define SHDISP_PRINTK(lv, fmt, args...) shdisp_printk(fmt, ## args); \
                                            if ((shdisp_log_lv & lv) != 0) printk(fmt, ## args);
      #define SHDISP_LOGDUMP    shdisp_logdump();
    #else
      #define SHDISP_PRINTK(lv, fmt, args...) if ((shdisp_log_lv & lv) != 0) printk(fmt, ## args);
      #define SHDISP_LOGDUMP
    #endif
#else   /* SHDISP_LOG_ENABLE */
    #define SHDISP_SET_LOG_LV(lv)
    #define SHDISP_LOGDUMP
    #define SHDISP_PRINTK(lv, fmt, args...)
#endif   /* SHDISP_LOG_ENABLE */

#define SHDISP_LOG_LV_ERR       0x01
#define SHDISP_LOG_LV_TRACE     0x02
#define SHDISP_LOG_LV_DEBUG     0x04
#define SHDISP_LOG_LV_PERFORM   0x08

#if defined (CONFIG_ANDROID_ENGINEERING)
#define SHDISP_ERR(fmt, args...) \
        SHDISP_PRINTK(SHDISP_LOG_LV_ERR, KERN_ERR "[SHDISP_ERROR][%s] " fmt, __func__, ## args)
#else
#define SHDISP_ERR(fmt, args...) \
        printk(KERN_ERR "[SHDISP_ERROR][%s] " fmt, __func__, ## args);
#endif /* CONFIG_ANDROID_ENGINEERING */

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


extern struct tty_struct *shdisp_tty;
#define SHDISP_DEBUG_CONSOLE(fmt, args...) \
        do { \
            int buflen = 0; \
            buflen = sprintf(&proc_buf[proc_buf_pos], fmt, ## args); \
            proc_buf_pos += (buflen > 0) ? buflen : 0; \
        } while(0)
/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
#ifdef SHDISP_LOG_ENABLE
#if defined (CONFIG_ANDROID_ENGINEERING)
extern int shdisp_printk(const char *fmt, ...);
extern void shdisp_logdump(void);
#endif /* CONFIG_ANDROID_ENGINEERING */
#endif

#endif /* SHDISP_DBG_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */

