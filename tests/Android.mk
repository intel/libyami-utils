LOCAL_PATH := $(call my-dir)

#libinputoutput
include $(CLEAR_VARS)
include $(LOCAL_PATH)/../common.mk

LOCAL_SRC_FILES := \
    decodeinput.cpp \
    decodeoutput.cpp \
    encodeinput.cpp \
    vppinputdecode.cpp \
    vppinputdecodecapi.cpp \
    vppinputoutput.cpp \
    vppoutputencode.cpp \
    vppinputasync.cpp \
    md5.c \

LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/.. \
        $(LOCAL_PATH)/../interface \
        $(LOCAL_PATH)/../tests \
        external/libcxx/include \
        $(TARGET_OUT_HEADERS)/libva

LOCAL_SHARED_LIBRARIES := \
        libutils \
        liblog \
        libc++ \
        libva \
        libva-android \
        libgui \
        libhardware \
        libyami \

LOCAL_CPPFLAGS += \
        -frtti


LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE := libinputoutput
include $(BUILD_STATIC_LIBRARY)


###yamidecode
include $(CLEAR_VARS)
include $(LOCAL_PATH)/../common.mk
LOCAL_SRC_FILES := \
    decodehelp.cpp \
    decode.cpp \

LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/.. \
        $(LOCAL_PATH)/../interface \
        $(LOCAL_PATH)/../tests \
        external/libcxx/include \
        $(TARGET_OUT_HEADERS)/libva

LOCAL_SHARED_LIBRARIES := \
        libutils \
        liblog \
        libc++ \
        libva \
        libva-android \
        libgui \
        libhardware \
        libyami \

LOCAL_CPPFLAGS += \
        -frtti

LOCAL_MULTILIB := both
LOCAL_STATIC_LIBRARIES := libinputoutput
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE := yamidecode
LOCAL_MODULE_STEM_32 := $(LOCAL_MODULE)32
LOCAL_MODULE_STEM_64 := $(LOCAL_MODULE)64
include $(BUILD_EXECUTABLE)


###yamivpp
include $(CLEAR_VARS)
include $(LOCAL_PATH)/../common.mk
LOCAL_SRC_FILES := \
    vpp.cpp \

LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/.. \
        $(LOCAL_PATH)/../interface \
        $(LOCAL_PATH)/../tests \
        external/libcxx/include \
        $(TARGET_OUT_HEADERS)/libva

LOCAL_SHARED_LIBRARIES := \
        libutils \
        liblog \
        libc++ \
        libva \
        libva-android \
        libgui \
        libhardware \
        libyami \

LOCAL_CPPFLAGS += \
        -frtti

LOCAL_MULTILIB := both
LOCAL_STATIC_LIBRARIES := libinputoutput
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE := yamivpp
LOCAL_MODULE_STEM_32 := $(LOCAL_MODULE)32
LOCAL_MODULE_STEM_64 := $(LOCAL_MODULE)64
include $(BUILD_EXECUTABLE)


###yamiencode
include $(CLEAR_VARS)
include $(LOCAL_PATH)/../common.mk
LOCAL_SRC_FILES := \
    encode.cpp \

LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/.. \
        $(LOCAL_PATH)/../interface \
        $(LOCAL_PATH)/../tests \
        external/libcxx/include \
        $(TARGET_OUT_HEADERS)/libva

LOCAL_SHARED_LIBRARIES := \
        libutils \
        liblog \
        libc++ \
        libva \
        libva-android \
        libgui \
        libhardware \
        libyami \

LOCAL_CPPFLAGS += \
        -frtti

LOCAL_MULTILIB := both
LOCAL_STATIC_LIBRARIES := libinputoutput
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE := yamiencode
LOCAL_MODULE_STEM_32 := $(LOCAL_MODULE)32
LOCAL_MODULE_STEM_64 := $(LOCAL_MODULE)64

include $(BUILD_EXECUTABLE)


###yamitranscode
include $(CLEAR_VARS)
include $(LOCAL_PATH)/../common.mk
LOCAL_SRC_FILES := \
    yamitranscode.cpp \

LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/.. \
        $(LOCAL_PATH)/../interface \
        $(LOCAL_PATH)/../tests \
        external/libcxx/include \
        $(TARGET_OUT_HEADERS)/libva

LOCAL_SHARED_LIBRARIES := \
        libutils \
        liblog \
        libc++ \
        libva \
        libva-android \
        libgui \
        libhardware \
        libyami \

LOCAL_CPPFLAGS += \
        -frtti

LOCAL_MULTILIB := both
LOCAL_STATIC_LIBRARIES := libinputoutput
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE := yamitranscode
LOCAL_MODULE_STEM_32 := $(LOCAL_MODULE)32
LOCAL_MODULE_STEM_64 := $(LOCAL_MODULE)64

include $(BUILD_EXECUTABLE)
