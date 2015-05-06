ifeq ($(wildcard $(KLIB_BUILD)/.config),)
# These will be ignored by compat autoconf
 export CONFIG_PCI=y
 export CONFIG_USB=y
 export CONFIG_PCMCIA=y
 export CONFIG_SSB=m
else
include $(KLIB_BUILD)/.config
endif

ifneq ($(wildcard $(KLIB_BUILD)/Makefile),)

KERNEL_VERSION := $(shell $(MAKE) -C $(KLIB_BUILD) kernelversion | sed -n 's/^\([0-9]\)\..*/\1/p')

ifneq ($(KERNEL_VERSION),2)
else
KERNEL_26SUBLEVEL := $(shell $(MAKE) -C $(KLIB_BUILD) kernelversion | sed -n 's/^2\.6\.\([0-9]\+\).*/\1/p')
endif

ifdef CONFIG_COMPAT_KERNEL_2_6_24
$(error "ERROR: compat-wireless by default supports kernels >= 2.6.24, try enabling only one driver though")
endif #CONFIG_COMPAT_KERNEL_2_6_24

ifeq ($(CONFIG_CFG80211),y)
$(error "ERROR: your kernel has CONFIG_CFG80211=y, you should have it CONFIG_CFG80211=m if you want to use this thing.")
endif


# 2.6.27 has FTRACE_DYNAMIC borked, so we will complain if
# you have it enabled, otherwise you will very likely run into
# a kernel panic.
# XXX: move this to compat_autoconf.h script generation
ifeq ($(KERNEL_VERSION),2)
ifeq ($(shell test $(KERNEL_VERSION) -eq 2 -a $(KERNEL_26SUBLEVEL) -eq 27 && echo yes),yes)
ifeq ($(CONFIG_DYNAMIC_FTRACE),y)
$(error "ERROR: Your 2.6.27 kernel has CONFIG_DYNAMIC_FTRACE, please upgrade your distribution kernel as newer ones should not have this enabled (and if so report a bug) or remove this warning if you know what you are doing")
endif
endif
endif

# This is because with CONFIG_MAC80211 include/linux/skbuff.h will
# enable on 2.6.27 a new attribute:
#
# skb->do_not_encrypt
#
# and on 2.6.28 another new attribute:
#
# skb->requeue
#
# In kernel 2.6.32 both attributes were removed.
#
# XXX: move this to compat_autoconf.h script generation
ifeq ($(KERNEL_VERSION),2)
ifeq ($(shell test $(KERNEL_VERSION) -eq 2 -a $(KERNEL_26SUBLEVEL) -ge 27 -a $(KERNEL_26SUBLEVEL) -le 31 && echo yes),yes)
ifeq ($(CONFIG_MAC80211),)
$(error "ERROR: Your >=2.6.27 and <= 2.6.31 kernel has CONFIG_MAC80211 disabled, you should have it CONFIG_MAC80211=m if you want to use this thing.")
endif
endif
endif

ifneq ($(KERNELRELEASE),) # This prevents a warning

ifneq ($(QOS_REQS_MISSING),) # Complain about our missing dependencies
$(warning "WARNING: You are running a kernel >= 2.6.23, you should enable in it $(QOS_REQS_MISSING) for 802.11[ne] support")
endif

endif # build check
endif # kernel Makefile check

# These both are needed by compat-wireless || compat-bluetooth so enable them
 export CONFIG_COMPAT_RFKILL=y

# The Bluetooth compatibility only builds on kernels >= 2.6.27 for now
ifndef CONFIG_COMPAT_KERNEL_2_6_27
ifeq ($(CONFIG_BT),y)
# we'll ignore compiling bluetooth
else
 export CONFIG_COMPAT_BLUETOOTH=y
 export CONFIG_COMPAT_BLUETOOTH_MODULES=m
 export CONFIG_HID_GENERIC=m
endif
endif #CONFIG_COMPAT_KERNEL_2_6_27

#
# CONFIG_COMPAT_FIRMWARE_CLASS definition has no leading whitespace,
# because it gets passed-on through compat_autoconf.h.
#
ifdef CONFIG_COMPAT_KERNEL_2_6_33
ifndef CONFIG_COMPAT_RHEL_6_1
ifdef CONFIG_FW_LOADER
export CONFIG_COMPAT_FIRMWARE_CLASS=m
endif #CONFIG_FW_LOADER
endif #CONFIG_COMPAT_RHEL_6_1
endif #CONFIG_COMPAT_KERNEL_2_6_33

ifdef CONFIG_COMPAT_KERNEL_2_6_36
ifndef CONFIG_COMPAT_RHEL_6_1
 export CONFIG_COMPAT_KFIFO=y
endif #CONFIG_COMPAT_RHEL_6_1
endif #CONFIG_COMPAT_KERNEL_2_6_36

#
# CONFIG_COMPAT_BT_SOCK_CREATE_NEEDS_KERN definitions have no leading
# whitespace, because they get passed-on through compat_autoconf.h.
#
ifndef CONFIG_COMPAT_KERNEL_2_6_33
export CONFIG_COMPAT_BT_SOCK_CREATE_NEEDS_KERN=y
endif #CONFIG_COMPAT_KERNEL_2_6_33
ifdef CONFIG_COMPAT_RHEL_6_0
export CONFIG_COMPAT_BT_SOCK_CREATE_NEEDS_KERN=y
endif #CONFIG_COMPAT_RHEL_6_0

export CONFIG_BT=m
export CONFIG_BT_RFCOMM=m
ifndef CONFIG_COMPAT_KERNEL_2_6_33
export CONFIG_COMPAT_BT_RFCOMM_TTY=y
endif #CONFIG_COMPAT_KERNEL_2_6_33
export CONFIG_BT_BNEP=m
export CONFIG_BT_BNEP_MC_FILTER=y
export CONFIG_BT_BNEP_PROTO_FILTER=y
# CONFIG_BT_CMTP depends on ISDN_CAPI
ifdef CONFIG_ISDN_CAPI
export CONFIG_BT_CMTP=m
endif #CONFIG_ISDN_CAPI
ifndef CONFIG_COMPAT_KERNEL_2_6_28
export CONFIG_COMPAT_BT_HIDP=m
endif #CONFIG_COMPAT_KERNEL_2_6_28

export CONFIG_BT_HCIUART=M
export CONFIG_BT_HCIUART_H4=y
export CONFIG_BT_HCIUART_BCSP=y
export CONFIG_BT_HCIUART_ATH3K=y
export CONFIG_BT_HCIUART_LL=y
export CONFIG_COMPAT_KERNEL_3_4=n
