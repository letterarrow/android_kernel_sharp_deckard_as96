/*
** =========================================================================
** File:
**     tspdrv.c
**
** Description: 
**     TouchSense Kernel Module main entry-point.
**
** Portions Copyright (c) 2008-2012 Immersion Corporation. All Rights Reserved. 
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
** =========================================================================
*/

#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>

#ifdef CONFIG_SCTSPDRV
#include <sharp/tspdrv.h>
#include <linux/regulator/consumer.h>
#else /* CONFIG_SCTSPDRV */
#include <tspdrv.h>
#endif /* CONFIG_SCTSPDRV */

static int g_nTimerPeriodMs = 5; /* 5ms timer by default. This variable could be used by the SPI.*/

#include <ImmVibeSPI.c>
#if defined(VIBE_DEBUG) && defined(VIBE_RECORD)
#include <tspdrvRecorder.c>
#endif

/* Device name and version information */
#define VERSION_STR " v3.6.12.1\n"                  /* DO NOT CHANGE - this is auto-generated */
#define VERSION_STR_LEN 16                          /* account extra space for future extra digits in version number */
static char g_szDeviceName[  (VIBE_MAX_DEVICE_NAME_LENGTH 
                            + VERSION_STR_LEN)
                            * NUM_ACTUATORS];       /* initialized in init_module */
static size_t g_cchDeviceName;                      /* initialized in init_module */

/* Flag indicating whether the driver is in use */
static char g_bIsPlaying = false;

/* Flag indicating whether the debug level*/
static atomic_t g_nDebugLevel;


/* Buffer to store data sent to SPI */
#define MAX_SPI_BUFFER_SIZE (NUM_ACTUATORS * (VIBE_OUTPUT_SAMPLE_SIZE + SPI_HEADER_SIZE))

static char g_cWriteBuffer[MAX_SPI_BUFFER_SIZE];


#if ((LINUX_VERSION_CODE & 0xFFFF00) < KERNEL_VERSION(2,6,0))
#error Unsupported Kernel version
#endif

#ifndef HAVE_UNLOCKED_IOCTL
#define HAVE_UNLOCKED_IOCTL 0
#endif

#ifdef IMPLEMENT_AS_CHAR_DRIVER
static int g_nMajor = 0;
#endif



/* Needs to be included after the global variables because they use them */
#include <tspdrvOutputDataHandler.c>
#ifdef CONFIG_HIGH_RES_TIMERS
    #include <VibeOSKernelLinuxHRTime.c>
#else
    #include <VibeOSKernelLinuxTime.c>
#endif



#ifdef CONFIG_SCTSPDRV

#define DEFAULT_TSPDRV_DBG_LEVEL DBL_ERROR
/* adb debug_log */
int tspdrv_func_log  = 0;
int tspdrv_debug_log = 0;
int tspdrv_force_log = 0;
int tspdrv_error_log = 1;

int tspdrv_dbg_level = DEFAULT_TSPDRV_DBG_LEVEL;
int tspdrv_dbg_level_change = 0 ;

#ifdef CONFIG_ANDROID_ENGINEERING
    module_param(tspdrv_func_log,  int, 0600);
    module_param(tspdrv_debug_log, int, 0600);
    module_param(tspdrv_force_log, int, 0600);
    module_param(tspdrv_error_log, int, 0600);
    module_param(tspdrv_dbg_level, int, 0600);
#endif  /* CONFIG_ANDROID_ENGINEERING */

#define TSPDRV_FUNC_LOG(fmt, args...) \
	if( tspdrv_func_log == 1 ){ \
		printk( KERN_DEBUG "[TSPDRV][tspdrv.c][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}
#define TSPDRV_DBG_LOG(fmt, args...) \
	if( tspdrv_debug_log == 1 ){ \
		printk( KERN_DEBUG "[TSPDRV][tspdrv.c][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}
#define TSPDRV_FORCE_LOG(fmt, args...) \
	if( tspdrv_force_log == 1 ){ \
		printk( KERN_DEBUG "[TSPDRV]" fmt "\n",## args ); \
	}
#define TSPDRV_ERR_LOG(fmt, args...) \
	if( tspdrv_error_log == 1 ){ \
		printk( KERN_DEBUG "[TSPDRV]" fmt "\n",## args ); \
	} \
	else if( tspdrv_error_log == 2 ){ \
		printk( KERN_DEBUG "[TSPDRV][tspdrv.c][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}

static struct regulator *reg_l10;
static const char *id_l10 = "8921_l10";
static int min_uV_l10 = 3000000, max_uV_l10 = 3000000;

static struct regulator *reg_l29;
static const char *id_l29 = "8921_l29";
static int min_uV_l29 = 1800000, max_uV_l29 = 1800000;

#endif /* CONFIG_SCTSPDRV */


asmlinkage void _DbgOut(int level, const char *fmt,...)
{
    static char printk_buf[MAX_DEBUG_BUFFER_LENGTH];
    static char prefix[6][4] = 
        {" * ", " ! ", " ? ", " I ", " V", " O "};

    int nDbgLevel = atomic_read(&g_nDebugLevel);

    TSPDRV_FUNC_LOG("START");

#ifdef CONFIG_SCTSPDRV
    if( tspdrv_dbg_level != DEFAULT_TSPDRV_DBG_LEVEL )
        tspdrv_dbg_level_change = 1;

    if( tspdrv_dbg_level_change == 1 )
        nDbgLevel = tspdrv_dbg_level;
#endif /* CONFIG_SCTSPDRV */

    if (0 <= level && level <= nDbgLevel) {
        va_list args;
        int ret;
        size_t size = sizeof(printk_buf);

        va_start(args, fmt);

        ret = scnprintf(printk_buf, size, KERN_EMERG "%s:%s %s",
             MODULE_NAME, prefix[level], fmt);
        if (ret >= size) return;

        vprintk(printk_buf, args);
        va_end(args);
    }

    TSPDRV_FUNC_LOG("END");
}

/* File IO */
static int open(struct inode *inode, struct file *file);
static int release(struct inode *inode, struct file *file);
static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos);
static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *ppos);
#if HAVE_UNLOCKED_IOCTL
static long unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#else
static int ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
#endif
static struct file_operations fops = 
{
    .owner =            THIS_MODULE,
    .read =             read,
    .write =            write,
#if HAVE_UNLOCKED_IOCTL
    .unlocked_ioctl =   unlocked_ioctl,
#else
    .ioctl =            ioctl,
#endif
    .open =             open,
    .release =          release,
    .llseek =           default_llseek    /* using default implementation as declared in linux/fs.h */
};

#ifndef IMPLEMENT_AS_CHAR_DRIVER
static struct miscdevice miscdev = 
{
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     MODULE_NAME,
	.fops =     &fops
};
#endif

static int suspend(struct platform_device *pdev, pm_message_t state);
static int resume(struct platform_device *pdev);
static struct platform_driver platdrv = 
{
    .suspend =  suspend,	
    .resume =   resume,	
    .driver = 
    {		
        .name = MODULE_NAME,	
    },	
};

static void platform_release(struct device *dev);
static struct platform_device platdev = 
{	
	.name =     MODULE_NAME,	
	.id =       -1,                     /* means that there is only one device */
	.dev = 
    {
		.platform_data = NULL, 		
		.release = platform_release,    /* a warning is thrown during rmmod if this is absent */
	},
};

/* Module info */
MODULE_AUTHOR("Immersion Corporation");
MODULE_DESCRIPTION("TouchSense Kernel Module");
MODULE_LICENSE("GPL v2");

static int __init tspdrv_init(void)
{
    int nRet, i;   /* initialized below */

    TSPDRV_FUNC_LOG("START");

#ifndef CONFIG_SCTSPDRV
    atomic_set(&g_nDebugLevel, DBL_ERROR);
#else /* CONFIG_SCTSPDRV */
    atomic_set(&g_nDebugLevel, tspdrv_dbg_level);
#endif /* CONFIG_SCTSPDRV */


    DbgOut((DBL_INFO, "tspdrv: init_module.\n"));

#ifdef IMPLEMENT_AS_CHAR_DRIVER
    g_nMajor = register_chrdev(0, MODULE_NAME, &fops);
    if (g_nMajor < 0) 
    {
        DbgOut((DBL_ERROR, "tspdrv: can't get major number.\n"));
        TSPDRV_FUNC_LOG("END");
        return g_nMajor;
    }
#else
    nRet = misc_register(&miscdev);
	if (nRet) 
    {
        DbgOut((DBL_ERROR, "tspdrv: misc_register failed.\n"));
        TSPDRV_FUNC_LOG("END");
		return nRet;
	}
#endif

	nRet = platform_device_register(&platdev);
	if (nRet) 
    {
        DbgOut((DBL_ERROR, "tspdrv: platform_device_register failed.\n"));
    }

	nRet = platform_driver_register(&platdrv);
	if (nRet) 
    {
        DbgOut((DBL_ERROR, "tspdrv: platform_driver_register failed.\n"));
    }

    DbgRecorderInit(());

#ifdef CONFIG_SCTSPDRV
	reg_l10= regulator_get(miscdev.this_device, id_l10);
	if (IS_ERR(reg_l10)) {
        TSPDRV_SPI_ERR_LOG("L10 regulator_get error");
        TSPDRV_SPI_FUNC_LOG("END");
		return VIBE_E_FAIL;
	}

	reg_l29= regulator_get(miscdev.this_device, id_l29);
	if (IS_ERR(reg_l29)) {
        TSPDRV_SPI_ERR_LOG("L29 regulator_get error");
        TSPDRV_SPI_FUNC_LOG("END");
		return VIBE_E_FAIL;
	}
    regulator_set_voltage(reg_l10, min_uV_l10, max_uV_l10);
    regulator_set_voltage(reg_l29, min_uV_l29, max_uV_l29);

    if (!regulator_is_enabled(reg_l10)) {
        if(regulator_enable(reg_l10) == 0){
            TSPDRV_SPI_DBG_LOG("vreg_L10 enabled.");
        }else{
            TSPDRV_SPI_ERR_LOG("vreg_L10 enable error.");
        }
    }
    if (!regulator_is_enabled(reg_l29)) {
        if(regulator_enable(reg_l29) == 0){
            TSPDRV_SPI_DBG_LOG("vreg_L29 enabled.");
        }else{
            TSPDRV_SPI_ERR_LOG("vreg_L29 enable error.");
        }
    }

    usleep(300);

#endif /* CONFIG_SCTSPDRV */

    ImmVibeSPI_ForceOut_Initialize();
    VibeOSKernelLinuxInitTimer();
    ResetOutputData();

    /* Get and concatenate device name and initialize data buffer */
    g_cchDeviceName = 0;
    for (i=0; i<NUM_ACTUATORS; i++)
    {
        char *szName = g_szDeviceName + g_cchDeviceName;
        ImmVibeSPI_Device_GetName(i, szName, VIBE_MAX_DEVICE_NAME_LENGTH);

        /* Append version information and get buffer length */
        strcat(szName, VERSION_STR);
        g_cchDeviceName += strlen(szName);

    }

    TSPDRV_FUNC_LOG("END");

    return 0;
}

static void __exit tspdrv_exit(void)
{
    TSPDRV_FUNC_LOG("START");

    DbgOut((DBL_INFO, "tspdrv: cleanup_module.\n"));

    DbgRecorderTerminate(());

    VibeOSKernelLinuxTerminateTimer();
    ImmVibeSPI_ForceOut_Terminate();

#ifdef CONFIG_SCTSPDRV
    regulator_put(reg_l10);
    regulator_put(reg_l29);
#endif /* CONFIG_SCTSPDRV */

	platform_driver_unregister(&platdrv);
	platform_device_unregister(&platdev);

#ifdef IMPLEMENT_AS_CHAR_DRIVER
    unregister_chrdev(g_nMajor, MODULE_NAME);
#else
    misc_deregister(&miscdev);
#endif

    TSPDRV_FUNC_LOG("END");
}

static int open(struct inode *inode, struct file *file) 
{
    TSPDRV_FUNC_LOG("START");

    DbgOut((DBL_INFO, "tspdrv: open.\n"));

    if (!try_module_get(THIS_MODULE)) return -ENODEV;

    TSPDRV_FUNC_LOG("END");

    return 0; 
}

static int release(struct inode *inode, struct file *file) 
{
    TSPDRV_FUNC_LOG("START");

    DbgOut((DBL_INFO, "tspdrv: release.\n"));

    /* 
    ** Reset force and stop timer when the driver is closed, to make sure
    ** no dangling semaphore remains in the system, especially when the
    ** driver is run outside of immvibed for testing purposes.
    */
    VibeOSKernelLinuxStopTimer();

    /* 
    ** Clear the variable used to store the magic number to prevent 
    ** unauthorized caller to write data. TouchSense service is the only 
    ** valid caller.
    */
    file->private_data = (void*)NULL;

    module_put(THIS_MODULE);

    TSPDRV_FUNC_LOG("END");

    return 0; 
}

static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    const size_t nBufSize = (g_cchDeviceName > (size_t)(*ppos)) ? min(count, g_cchDeviceName - (size_t)(*ppos)) : 0;

    TSPDRV_FUNC_LOG("START");

    TSPDRV_DBG_LOG("read() called  count = %d",count);

    /* End of buffer, exit */
    if (0 == nBufSize) return 0;

    if (0 != copy_to_user(buf, g_szDeviceName + (*ppos), nBufSize)) 
    {
        /* Failed to copy all the data, exit */
        DbgOut((DBL_ERROR, "tspdrv: copy_to_user failed.\n"));
        return 0;
    }

    /* Update file position and return copied buffer size */
    *ppos += nBufSize;

    TSPDRV_FUNC_LOG("END");

    return nBufSize;
}

static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    *ppos = 0;  /* file position not used, always set to 0 */

    TSPDRV_FUNC_LOG("START");
    /* 
    ** Prevent unauthorized caller to write data. 
    ** TouchSense service is the only valid caller.
    */
    if (file->private_data != (void*)TSPDRV_MAGIC_NUMBER) 
    {
        DbgOut((DBL_ERROR, "tspdrv: unauthorized write.\n"));
        TSPDRV_FUNC_LOG("END");
        return 0;
    }

    /* 
    ** Ignore packets that have size smaller than SPI_HEADER_SIZE or bigger than MAX_SPI_BUFFER_SIZE.
    ** Please note that the daemon may send an empty buffer (count == SPI_HEADER_SIZE)
    ** during quiet time between effects while playing a Timeline effect in order to maintain
    ** correct timing: if "count" is equal to SPI_HEADER_SIZE, the call to VibeOSKernelLinuxStartTimer()
    ** will just wait for the next timer tick.
    */
    if ((count < SPI_HEADER_SIZE) || (count > MAX_SPI_BUFFER_SIZE))
    {
        DbgOut((DBL_ERROR, "tspdrv: invalid buffer size.\n"));
        TSPDRV_FUNC_LOG("END");
        return 0;
    }

    /* Copy immediately the input buffer */
    if (0 != copy_from_user(g_cWriteBuffer, buf, count))
    {
        /* Failed to copy all the data, exit */
        DbgOut((DBL_ERROR, "tspdrv: copy_from_user failed.\n"));
        TSPDRV_FUNC_LOG("END");
        return 0;
    }

    /* Extract force output samples and save them in an internal buffer */
    if (!SaveOutputData(g_cWriteBuffer, count))
    {
        DbgOut((DBL_ERROR, "tspdrv: SaveOutputData failed.\n"));
        TSPDRV_FUNC_LOG("END");
        return 0;
    }

    TSPDRV_DBG_LOG("write() called  force = %d  count = %d",g_cWriteBuffer[3],count);

    /* Start the timer after receiving new output force */
    g_bIsPlaying = true;

    VibeOSKernelLinuxStartTimer();

    TSPDRV_FUNC_LOG("END");

    return count;
}

#if HAVE_UNLOCKED_IOCTL
static long unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
#else
static int ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
#endif
{

#ifdef CONFIG_SCTSPDRV
	void __user *argp = (void __user *)arg;
	unsigned char tspresult;
	struct tspdrv_autocal_presetting_data acal_setting_data;
	struct tspdrv_autocal_data acal_data;
#endif /* CONFIG_SCTSPDRV */

    TSPDRV_FUNC_LOG("START");

    TSPDRV_DBG_LOG("ioctl() called  cmd = %d",cmd);

    switch (cmd)
    {
        case TSPDRV_SET_MAGIC_NUMBER:
            file->private_data = (void*)TSPDRV_MAGIC_NUMBER;
            break;

        case TSPDRV_ENABLE_AMP:
            ImmVibeSPI_ForceOut_AmpEnable(arg);
            DbgRecorderReset((arg));
            DbgRecord((arg,";------- TSPDRV_ENABLE_AMP ---------\n"));
            break;

        case TSPDRV_DISABLE_AMP:
            ImmVibeSPI_ForceOut_AmpDisable(arg);
            break;

        case TSPDRV_GET_NUM_ACTUATORS:
            TSPDRV_FUNC_LOG("END");
            return NUM_ACTUATORS;

        case TSPDRV_SET_DBG_LEVEL:
            {
                long nDbgLevel;
                if (0 != copy_from_user((void *)&nDbgLevel, (const void __user *)arg, sizeof(long))) {
                    /* Error copying the data */
                    DbgOut((DBL_ERROR, "copy_from_user failed to copy debug level data.\n"));
                    TSPDRV_FUNC_LOG("END");
                    return -1;
                }

                if (DBL_TEMP <= nDbgLevel &&  nDbgLevel <= DBL_OVERKILL) {
                    atomic_set(&g_nDebugLevel, nDbgLevel);
                } else {
                    DbgOut((DBL_ERROR, "Invalid debug level requested, ignored."));
                }

                break;
            }

        case TSPDRV_GET_DBG_LEVEL:
            TSPDRV_FUNC_LOG("END");
            return atomic_read(&g_nDebugLevel);

        case TSPDRV_SET_DEVICE_PARAMETER:
            {
                device_parameter deviceParam;

                if (0 != copy_from_user((void *)&deviceParam, (const void __user *)arg, sizeof(deviceParam)))
                {
                    /* Error copying the data */
                    DbgOut((DBL_ERROR, "tspdrv: copy_from_user failed to copy kernel parameter data.\n"));
                    TSPDRV_FUNC_LOG("END");
                    return -1;
                }

                switch (deviceParam.nDeviceParamID)
                {
                    case VIBE_KP_CFG_UPDATE_RATE_MS:
                        /* Update the timer period */
                        g_nTimerPeriodMs = deviceParam.nDeviceParamValue;



#ifdef CONFIG_HIGH_RES_TIMERS
                        /* For devices using high resolution timer we need to update the ktime period value */
                        g_ktTimerPeriod = ktime_set(0, g_nTimerPeriodMs * 1000000);
#endif
                        break;

                    case VIBE_KP_CFG_FREQUENCY_PARAM1:
                    case VIBE_KP_CFG_FREQUENCY_PARAM2:
                    case VIBE_KP_CFG_FREQUENCY_PARAM3:
                    case VIBE_KP_CFG_FREQUENCY_PARAM4:
                    case VIBE_KP_CFG_FREQUENCY_PARAM5:
                    case VIBE_KP_CFG_FREQUENCY_PARAM6:
                        if (0 > ImmVibeSPI_ForceOut_SetFrequency(deviceParam.nDeviceIndex, deviceParam.nDeviceParamID, deviceParam.nDeviceParamValue))
                        {
                            DbgOut((DBL_ERROR, "tspdrv: cannot set device frequency parameter.\n"));
                            TSPDRV_FUNC_LOG("END");
                            return -1;
                        }
                        break;
                }
            }
#ifdef CONFIG_SCTSPDRV
		case TSPDRV_GET_DEVICE_ID:
	        if (copy_from_user(&tspresult, argp, sizeof(tspresult))) {
	            TSPDRV_ERR_LOG("copy_from_user failed.");
                TSPDRV_FUNC_LOG("END");
	            return -1;
	        }
	        if(ImmVibeSPI_ForceOut_GetDeviceID(&tspresult) < 0) {
	            TSPDRV_ERR_LOG("get device id error.");
                TSPDRV_FUNC_LOG("END");
				return -1;
			}
		
	        if (copy_to_user(argp, &tspresult, sizeof(tspresult))) {
	            TSPDRV_ERR_LOG("copy_to_user failed.");
                TSPDRV_FUNC_LOG("END");
	            return -1;
			}
	        break;

		case TSPDRV_AUTOCAL_EXEC:
	        if (copy_from_user(&acal_setting_data, argp, sizeof(acal_setting_data))) {
	            TSPDRV_ERR_LOG("copy_from_user failed.");
                TSPDRV_FUNC_LOG("END");
	            return -1;
	        }
	        if(ImmVibeSPI_AutoCalExec(acal_setting_data) < 0) {
	            TSPDRV_ERR_LOG("exec auto calibration error.");
                TSPDRV_FUNC_LOG("END");
				return -1;
			}
			break;

		case TSPDRV_AUTOCAL_DATA_GET:
	        if (copy_from_user(&acal_data, argp, sizeof(acal_data))) {
	            TSPDRV_ERR_LOG("copy_from_user failed.");
                TSPDRV_FUNC_LOG("END");
	            return -1;
	        }
	        if(ImmVibeSPI_Get_AutoCal_Result(&acal_data) < 0) {
	            TSPDRV_ERR_LOG("autocal check error.");
                TSPDRV_FUNC_LOG("END");
				return -1;
			}

	        if (copy_to_user(argp, &acal_data, sizeof(acal_data))) {
	            TSPDRV_ERR_LOG("copy_to_user failed.");
                TSPDRV_FUNC_LOG("END");
	            return -1;
			}
	        break;

		case TSPDRV_AUTOCAL_DATA_SET:
	        if(ImmVibeSPI_Set_AutoCal_Data() < 0) {
	            TSPDRV_ERR_LOG("autocal check error.");
                TSPDRV_FUNC_LOG("END");
				return -1;
			}
	        break;

#endif /* CONFIG_SCTSPDRV */

        }
    return 0;
}

static int suspend(struct platform_device *pdev, pm_message_t state) 
{
    TSPDRV_FUNC_LOG("START");

    if (g_bIsPlaying)
    {
        DbgOut((DBL_INFO, "tspdrv: can't suspend, still playing effects.\n"));
        TSPDRV_FUNC_LOG("END");
        return -EBUSY;
    }
    else
    {
#ifdef CONFIG_SCTSPDRV
        if (regulator_is_enabled(reg_l29)) {
            if(regulator_set_mode(reg_l29, REGULATOR_MODE_STANDBY) == 0){
                TSPDRV_DBG_LOG("vreg_L29 to LOW_POWER_MODE.");
            }else{
                TSPDRV_ERR_LOG("vreg_L29 change LOW_POWER_MODE error.");
            }
        }
        if (regulator_is_enabled(reg_l10)) {
            if(regulator_set_mode(reg_l10, REGULATOR_MODE_STANDBY) == 0){
                TSPDRV_DBG_LOG("vreg_L10 to LOW_POWER_MODE.");
            }else{
                TSPDRV_ERR_LOG("vreg_L10 change LOW_POWER_MODE error.");
            }
        }
#endif /* CONFIG_SCTSPDRV */

        DbgOut((DBL_INFO, "tspdrv: suspend.\n"));
        TSPDRV_FUNC_LOG("END");
        return 0;
    }
}

static int resume(struct platform_device *pdev) 
{	
    TSPDRV_FUNC_LOG("START");

#ifdef CONFIG_SCTSPDRV
    DbgOut((DBL_INFO, "tspdrv: resume.\n"));
#else /* CONFIG_SCTSPDRV */
    DbgOut((DBL_ERROR, "tspdrv: resume.\n"));
#endif /* CONFIG_SCTSPDRV */

#ifdef CONFIG_SCTSPDRV
    if (regulator_is_enabled(reg_l10)) {
        if(regulator_set_mode(reg_l10, REGULATOR_MODE_NORMAL) == 0){
            TSPDRV_DBG_LOG("vreg_L10 to NORMAL_MODE.");
        }else{
            TSPDRV_ERR_LOG("vreg_L10 change NORMAL_MODE error.");
        }
    }
    if (regulator_is_enabled(reg_l29)) {
        if(regulator_set_mode(reg_l29, REGULATOR_MODE_NORMAL) == 0){
            TSPDRV_DBG_LOG("vreg_L29 to NORMAL_MODE.");
        }else{
            TSPDRV_ERR_LOG("vreg_L29 change NORMAL_MODE error.");
        }
    }
#endif /* CONFIG_SCTSPDRV */

    TSPDRV_FUNC_LOG("END");

	return 0;   /* can resume */
}

static void platform_release(struct device *dev) 
{	
    TSPDRV_FUNC_LOG("START");

#ifdef CONFIG_SCTSPDRV
    DbgOut((DBL_INFO, "tspdrv: platform_release.\n"));
#else /* CONFIG_SCTSPDRV */
    DbgOut((DBL_ERROR, "tspdrv: platform_release.\n"));
#endif /* CONFIG_SCTSPDRV */

    TSPDRV_FUNC_LOG("END");
}

module_init(tspdrv_init);
module_exit(tspdrv_exit);
