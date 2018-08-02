GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)
EXTRA_CFLAGS += -Idrivers/media/video/msm
EXTRA_CFLAGS += -Idrivers/media/video/msm/io
obj-$(CONFIG_MSM_ACTUATOR) += msm_actuator.o
obj-$(CONFIG_BU63164GWL_ACT) += bu63164gwl_act.o
obj-$(CONFIG_IMX111_ACT) += imx111_act.o
obj-$(CONFIG_BU64291GWZ_ACT) += bu64291gwz_act.o
