/* include/sharp/sh_smem.h
 *
 * Copyright (C) 2012 Sharp Corporation
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

typedef struct 
{
    unsigned long       shdisp_data_buf[544];         /* Buffer for shdisp */
    unsigned char       shusb_softupdate_mode_flag;   /* softupdate mode flag */
    unsigned long       sh_filesystem_init;           /* file system innitialize flag */
    unsigned long       sh_hw_revision;               /* hardware revision number */
    unsigned long       sh_boot_mode;                 /* power up mode information */
    unsigned long       sh_pwr_on_status;             /* power on status information from pmic */
    unsigned long       shdiag_FlagData;              /* shdiag flag information */
    unsigned short      shdiag_BootMode;              /* shdiag Powerup mode */
    unsigned char       shdiag_FirstBoot;             /* shdiag FirstBoot information */
    unsigned char       shdiag_AdjChashe[16];         /* shdiag Adj chashe information */
    int                 sh_sleep_test_mode;
    unsigned char       shsecure_PassPhrase[32];
    unsigned long       fota_boot_mode;               /* FOTA mode information */
    unsigned char       pImeiData[16];                /* W-CDMA Imei data */
    unsigned long       sh_boot_key;                  /* key(s) ditected OSBL */
    unsigned char       sh_camver[4];                 /* Version information */
    unsigned char       sh_touchver[4];               /* Version information */
    unsigned char       sh_miconver[4];               /* Version information */
    unsigned char       shdarea_QRData[128];          /* QRdata */
    unsigned char       sh_swicdev;                   /* USB SWIC exist information */
    unsigned char       shdarea_WlanMacAddress[6];    /* WLAN Mac Address */
    unsigned char       sh_camImgAdjustData[102];     /* Camera Image Adjust Data */
    unsigned char       conf_clrvari[4];              /* Color Variations information */
    unsigned char       sh_camOtpData[10240];          /* Camera Production adjustment Data */
    unsigned char       shusb_qxdm_ena_flag;          /* QXDM enable flag */
    unsigned char       shusb_usb_charge_ena_flag;    /* USB charge enable flag */
    unsigned short      shdiag_TpsBaseLineTbl[1000];  /* Touch adjustment */
    unsigned short      shdiag_TpsBaseLineTbl_bk[1000]; /* Touch adjustment(Back side) */
    int                 shpwr_battery_present;        /* Battery presen */
    int                 shpwr_battery_voltage;        /* Battery voltage */
    int                 shpwr_battery_temperature;    /* Battery temperature */
    int                 shpwr_xo_temperature;         /* XO temperature */
    int                 shpwr_cable_status;           /* Cable status */
    unsigned char       sh_100hflg;                   /* 100 hours test flag */
    unsigned short      shdiag_proxadj[2];            /* Proximity sensor adjust */
    unsigned char       shtps_fwup_flag;              /* Touch panel firmware update flag */
    int                 shpwr_fuel_data[4];           /* Fuel gauge correction value */
    int                 shpwr_vbat_data[4];           /* Battery A/D converter correction value */
    unsigned char       sh_pvs_flg;                   /* PVS flag */
    unsigned char       shdarea_WlanCalPAData[30];    /* Output adjustment of WLAN */
    unsigned char       shdarea_WlanCal5GPAData[100]; /* Output adjustment for 5GHz of WLAN */
    unsigned char       sh_diagenc_gk[16];            /* Diag Area Key */
    unsigned char       shdiag_fullchgflg;            /* Full charge FLG(F Only) */
    char                sh_sbl_version[8];            /* sbl Version */
    char                shdiag_debugflg;              /* Debug FLG */
    char                shdiag_factoryflg;            /* Factory FLG */
    char                shdiag_nv_imeidata[10];       /* Diag NV IMEI */
    char                shdiag_nv_vbatt[16];          /* Diag NV VBATT */
    char                shdiag_nv_meid[7];            /* Diag NV MEID */
    char                shdiag_nv_mnf[8];             /* Diag NV MNF */
    char                shdiag_nv_bd_addr[6];         /* Diag NV BD_ADDR */
    char                shdiag_nv_wlan_mac_addr[6];   /* Diag NV WLAN_Mac_Addr */
    char                shdiag_nv_rtre_config;        /* Diag NV RTRE_Config */
    char                shdiag_nv_ftm_mode;           /* Diag NV FTM_Mode */
    char                shdiag_nv_dcm_simlock;        /* Diag NV DCM_SIMLOCK */
    char                shdiag_nv_kddi_uim_ms_maintenance_bit; /* Diag NV KDDI_UIM_MS_Maintenance_bit */
    char                shdiag_nv_ue_imeisv_svn;      /* Diag NV IMEI_SV */
    unsigned char       sh_hw_handset;                /* Handset FLG */
    unsigned char       sh_fuse_stat_APQ;             /* APQ FuseStatusFlag */
    unsigned char       shdiag_DoPrepend;             /* DoPrepend() Complete FLG */
    unsigned long long  shsys_timestamp[32];          /* System Timestamp */
    unsigned char       shdiag_tspdrv_acal_data[3];   /* Haptics Auto Calibration Data */
    unsigned short      shdiag_cprx_offset_hl[12][2]; /* Cprx high/low Data */
    unsigned short      shdiag_cprx_cal_offset[12];   /* Cprx Calibration offset Data */
    unsigned char       shdiag_charge_th_high[8];     /* ChageLimitMax */
    unsigned char       shdiag_charge_th_low[8];      /* ChageLimitMin */
    unsigned char       shpwr_batauthflg;             /* Battery Authentication FLG */
} sharp_smem_common_type;

#define SH_SMEM_COMMON_SIZE 153600

/*=============================================================================

FUNCTION sh_smem_get_common_address

=============================================================================*/
sharp_smem_common_type *sh_smem_get_common_address( void );

/*=============================================================================

FUNCTION sh_smem_get_sleep_power_collapse_disabled_address

=============================================================================*/
unsigned long *sh_smem_get_sleep_power_collapse_disabled_address( void );

/*=============================================================================

FUNCTION sh_smem_get_100hflg

=============================================================================*/
unsigned char sh_smem_get_100hflg( void );

/*=============================================================================

FUNCTION sh_smem_get_softupdate_flg

=============================================================================*/
unsigned char sh_smem_get_softupdate_flg( void );

/*=============================================================================

FUNCTION sh_smem_get_battery_voltage

=============================================================================*/
int sh_smem_get_battery_voltage( void );

/*=============================================================================

FUNCTION sh_smem_get_fota_boot_mode

=============================================================================*/
unsigned long sh_smem_get_fota_boot_mode( void );

/*=============================================================================

FUNCTION sh_smem_get_pvs_flg

=============================================================================*/
unsigned char sh_smem_get_pvs_flg( void );
