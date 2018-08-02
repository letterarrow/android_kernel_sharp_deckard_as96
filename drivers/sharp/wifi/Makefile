#ifeq ($(CONFIG_WIFIPM),y)
 EXTRA_CFLAGS += -DWIFIPM_INI=0
 EXTRA_CFLAGS += -DWIFIPM_DEBUG
 obj-y	+= wifipm.o
#endif

ifneq ($(CONFIG_SHWLAN_BCM4334_1),n)
BCM_SRC_PATH := broadcom/1.28.11.5/bcmdhd/
endif

ifneq ($(CONFIG_SHWLAN_BCM4335_1),n)
BCM_SRC_PATH := broadcom/1.61.36/
endif

ifeq ($(CONFIG_BCM4335),y)
 EXTRA_CFLAGS += -DSH_CONFIG_WLAN_STATIC
endif

obj-$(CONFIG_WIFIPM) += $(BCM_SRC_PATH)

