/* drivers/sharp/shdisp/shdisp_dbg.c  (Display Driver)
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

/*---------------------------------------------------------------------------*/
/* INCLUDE FILES                                                             */
/*---------------------------------------------------------------------------*/
#include "shdisp_dbg.h"

/*---------------------------------------------------------------------------*/
/* MACROS                                                                    */
/*---------------------------------------------------------------------------*/
#ifdef SHDISP_LOG_ENABLE
#if defined (CONFIG_ANDROID_ENGINEERING)
#define LOG_BUF_SIZE    (4096)
#define LINE_BUF_SIZE   (1024)
#endif /* CONFIG_ANDROID_ENGINEERING */
#endif

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
#ifdef SHDISP_LOG_ENABLE
    unsigned char shdisp_log_lv = SHDISP_LOG_LV_ERR;
#if defined (CONFIG_ANDROID_ENGINEERING)
    static char printk_buf[LINE_BUF_SIZE];
    static int log_buf_offset = 0;
    static char log_buf[LOG_BUF_SIZE] = {0};
    static int ring = 0;
#endif /* CONFIG_ANDROID_ENGINEERING */
#endif

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
#ifdef SHDISP_LOG_ENABLE
#if defined (CONFIG_ANDROID_ENGINEERING)
int shdisp_vprintk(const char *fmt, va_list args);
#endif /* CONFIG_ANDROID_ENGINEERING */
#endif

/*---------------------------------------------------------------------------*/
/* FUNCTIONS                                                                 */
/*---------------------------------------------------------------------------*/
#ifdef SHDISP_LOG_ENABLE
#if defined (CONFIG_ANDROID_ENGINEERING)
int shdisp_printk(const char *fmt, ...)
{
    va_list args;
    int r;

    va_start(args, fmt);
    r = shdisp_vprintk(fmt, args);
    va_end(args);

	return r;
}

int shdisp_vprintk(const char *fmt, va_list args)
{
    int len;

    len = vscnprintf(printk_buf,
                     sizeof(printk_buf), fmt, args);

    if( (log_buf_offset + len) < LOG_BUF_SIZE) {
        memcpy(&log_buf[log_buf_offset], printk_buf, len);
        log_buf_offset = log_buf_offset + len;
    }
    else {
        ring = 1;
        memcpy(&log_buf[log_buf_offset], printk_buf, LOG_BUF_SIZE - log_buf_offset);
        memcpy(&log_buf[0], &printk_buf[LOG_BUF_SIZE - log_buf_offset], len - (LOG_BUF_SIZE - log_buf_offset));
        log_buf_offset = len - (LOG_BUF_SIZE - log_buf_offset);
    }
    log_buf[log_buf_offset] = '\0';
    log_buf_offset++;

    return len;
}


void shdisp_logdump(void)
{
    int l;
    int s;
    int len = 0;
    char *p;

    if(ring == 0) {
        s = 0;
    }
    else {
        s = log_buf_offset;
    }

    printk(KERN_ERR "[SHDISP] LOG DUMP -------------------->\n");

    while (len < LOG_BUF_SIZE) {

        p = printk_buf;
        l = 0;
        
        while (s < LOG_BUF_SIZE && l < LINE_BUF_SIZE - 1) {

            if(log_buf[s] == '\0' ) break;

            p[l] = log_buf[s];
            s++;
            l++;
        }
        
        if( s >= LOG_BUF_SIZE) {

            s = 0;

            while (s < LOG_BUF_SIZE && l < LINE_BUF_SIZE - 1) {

                if(log_buf[s] == '\0' ) break;

                p[l] = log_buf[s];
                s++;
                l++;
            }
        }
        
        if(l > 0) {
            p[l] = '\0';
            printk(KERN_ERR "%s", p);
        }

        s++;
        len += (l + 1);
        
    }
    printk(KERN_ERR "<-------------------- [SHDISP] LOG DUMP\n");
    
    printk(KERN_ERR "[SHDISP] STACK DUMP -------------------->\n");
    dump_stack();
    printk(KERN_ERR "<-------------------- [SHDISP] STACK DUMP\n");
    
    return;

}
#endif /* CONFIG_ANDROID_ENGINEERING */
#endif

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */

