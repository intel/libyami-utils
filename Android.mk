LOCAL_PATH := $(call my-dir)
LIBYAMICODEC_PATH := $(LOCAL_PATH)
include $(CLEAR_VARS)
include $(LIBYAMICODEC_PATH)/common.mk

include $(LIBYAMICODEC_PATH)/tests/Android.mk
