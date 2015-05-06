
ifndef PRODUCT_OUT
$(error PRODUCT_OUT undefined)
endif

TI_BLUETI_KO_INSTALL_PATH := $(PRODUCT_OUT)/system/lib/modules

LOCAL_MODULE := ti_bluetooth_ko
LOCAL_MODULE_CLASS := FAKE
LOCAL_MODULE_SUFFIX := -timestamp
LOCAL_BUILT_MODULE := $(PRODUCT_OUT)/obj/$(LOCAL_MODULE_CLASS)/$(LOCAL_MODULE)_intermediates/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)

KO_LIST := bluetooth.ko,bnep.ko,hidp.ko,rfcomm.ko,btwilink.ko

$(LOCAL_BUILT_MODULE):
	./tibluez.sh $(TI_COMPAT_ARGS)
	mkdir -p $(TI_BLUETI_KO_INSTALL_PATH)
	@pwd
	cp -f kos/{$(KO_LIST)} $(TI_BLUETI_KO_INSTALL_PATH)
	@mkdir -p $(dir $@)
	@touch $@

all: $(LOCAL_BUILT_MODULE)

clean:
	rm -rf $(TI_BLUETI_KO_INSTALL_PATH)/{$(KO_LIST)} kos $(LOCAL_BUILT_MODULE)
