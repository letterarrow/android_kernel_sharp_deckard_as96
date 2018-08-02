obj-$(CONFIG_SHLCDC_BOARD) += shdisp_kerl.o

obj-$(CONFIG_SHLCDC_BOARD) += shdisp_system.o

obj-$(CONFIG_SHLCDC_BOARD) += shdisp_pwm.o

obj-$(CONFIG_SHLCDC_BOARD) += shdisp_bl69y6.o

obj-$(CONFIG_SHDISP_PANEL_KITE) += shdisp_kite.o
obj-$(CONFIG_SHDISP_PANEL_RYOMA) += shdisp_ryoma.o

ifdef CONFIG_SHLCDC_LED_BD2802GU
obj-$(CONFIG_SHLCDC_LED_BD2802GU) += shdisp_bd2802gu.o
endif

obj-$(CONFIG_SHLCDC_BOARD) += shlcdc_eventlog.o
obj-$(CONFIG_SHLCDC_BOARD) += shdisp_dbg.o
EXTRA_CFLAGS	+= -DSH_BUILD_ID=\"$(SH_BUILD_ID)\"
