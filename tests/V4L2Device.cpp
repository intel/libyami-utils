/*
 * Copyright (C) 2016 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "V4L2Device.h"

#include "common/log.h"
#include "common/common_def.h"
#include <string.h>
#include <inttypes.h>

#ifdef __ENABLE_V4L2_OPS__

#include "v4l2codec_device_ops.h"
#include <dlfcn.h>
#include <fcntl.h>

#define SIMULATE_V4L2_OP(OP, ...)                      \
    do {                                               \
        if (!m_ops.m##OP##Func)                        \
            return -1;                                 \
        return m_ops.m##OP##Func(m_fd, ##__VA_ARGS__); \
    } while (0)

struct TypeEntry {
    VideoDataMemoryType type;
    const char* str;
};

const static TypeEntry g_entrys[] = {
    { VIDEO_DATA_MEMORY_TYPE_DRM_NAME, "drm-name" },
    { VIDEO_DATA_MEMORY_TYPE_DMA_BUF, "dma-buf" },
    { VIDEO_DATA_MEMORY_TYPE_ANDROID_BUFFER_HANDLE, "android-buffer-handle" },
    { VIDEO_DATA_MEMORY_TYPE_EXTERNAL_DMA_BUF, "external-dma-buf" }

};

const char* frameTypeToString(VideoDataMemoryType type)
{
    for (size_t i = 0; i < N_ELEMENTS(g_entrys); i++) {
        if (type == g_entrys[i].type)
            return g_entrys[i].str;
    }
    ASSERT(0 && "not support yet");
    return NULL;
}

class V4L2DeviceOps : public V4L2Device {
public:
    bool open(const char* name, int32_t flags)
    {
        if (!m_ops.mOpenFunc)
            return false;
        m_fd = m_ops.mOpenFunc(name, flags);
        return m_fd != -1;
    }

    int32_t close()
    {
        SIMULATE_V4L2_OP(Close);
    }

    int32_t ioctl(int32_t cmd, void* arg)
    {
        SIMULATE_V4L2_OP(Ioctl, cmd, arg);
    }

    int32_t poll(bool pollDevice, bool* eventPending)
    {
        SIMULATE_V4L2_OP(Poll, pollDevice, eventPending);
    }

    int32_t setDevicePollInterrupt()
    {
        SIMULATE_V4L2_OP(SetDevicePollInterrupt);
    }

    int32_t clearDevicePollInterrupt()
    {
        SIMULATE_V4L2_OP(ClearDevicePollInterrupt);
    }

    void* mmap(void* addr, size_t length, int32_t prot,
        int32_t flags, unsigned int offset)
    {
        if (!m_ops.mMmapFunc)
            return NULL;

        return m_ops.mMmapFunc(addr, length, prot, flags, m_fd, offset);
    }

    int32_t munmap(void* addr, size_t length)
    {
        if (!m_ops.mMunmapFunc)
            return -1;

        return m_ops.mMunmapFunc(addr, length);
    }

    int32_t setFrameMemoryType(VideoDataMemoryType type)
    {
        const char* str = frameTypeToString(type);
        if (str)
            SIMULATE_V4L2_OP(SetParameter, "frame-memory-type", str);
        return -1;
    }

#if __ENABLE_TESTS_GLES__
    int32_t useEglImage(/*EGLDisplay*/ void* eglDisplay, /*EGLContext*/ void* eglContext,
        uint32_t bufferIndex, void* eglImage)
    {
        SIMULATE_V4L2_OP(UseEglImage, eglDisplay, eglContext, bufferIndex, eglImage);
    }

    int32_t setDrmFd(int drmFd)
    {
        ASSERT(0 && "not supported yet");
        return -1;
    }
#endif

#if __ENABLE_WAYLAND__
    int32_t setWaylandDisplay(struct wl_display* wlDisplay)
    {
        char displayStr[32];
        sprintf(displayStr, "%" PRIu64 "", (uint64_t)(wlDisplay));
        SIMULATE_V4L2_OP(SetParameter, "wayland-display", displayStr);
    }
#endif

#if __ENABLE_X11__
    /// it should be called before driver initialization (immediate after _Open()).
    int32_t setXDisplay(Display* x11Display)
    {
        char displayStr[32];
        sprintf(displayStr, "%" PRIu64 "", (uint64_t)x11Display);
        SIMULATE_V4L2_OP(SetParameter, "x11-display", displayStr);
    }
#endif

    V4L2DeviceOps()
        : m_handle(NULL)
    {
        memset(&m_ops, 0, sizeof(m_ops));
    }

    ~V4L2DeviceOps()
    {
        if (m_handle)
            dlclose(m_handle);
    }

protected:
    bool init()
    {
        const char* libName = "libyami_v4l2.so";
        m_handle = dlopen(libName, RTLD_NOW | RTLD_GLOBAL);
        if (!m_handle) {
            ERROR("dlopen failed for %s", libName);
            return false;
        }

        V4l2codecOperationInitFunc initFunc = NULL;
        initFunc = (V4l2codecOperationInitFunc)dlsym(RTLD_DEFAULT, "v4l2codecOperationInit");

        if (!initFunc) {
            ERROR("fail to dlsym v4l2codecOperationInit\n");
            return false;
        }

        INIT_V4L2CODEC_OPS_SIZE_VERSION(&m_ops);
        if (!initFunc(&m_ops)) {
            ERROR("fail to init v4l2 device operation func pointers\n");
            return false;
        }

        int isVersionMatch = 0;
        IS_V4L2CODEC_OPS_VERSION_MATCH(m_ops.mVersion, isVersionMatch);
        if (!isVersionMatch) {
            ERROR("V4l2CodecOps interface version doesn't match\n");
            return false;
        }
        if (m_ops.mSize != sizeof(V4l2CodecOps)) {
            ERROR("V4l2CodecOps interface data structure size doesn't match\n");
            return false;
        }
        return true;
    }

private:
    void* m_handle;
    V4l2CodecOps m_ops;
};
#undef SIMULATE_V4L2_OP

#else

#include <v4l2_wrapper.h>

class V4L2DeviceYami : public V4L2Device {
public:
    bool open(const char* name, int32_t flags)
    {
        m_fd = YamiV4L2_Open(name, flags);
        return m_fd != -1;
    }
    int32_t close()
    {
        return YamiV4L2_Close(m_fd);
    }
    int32_t ioctl(int32_t cmd, void* arg)
    {
        return YamiV4L2_Ioctl(m_fd, cmd, arg);
    }
    int32_t poll(bool pollDevice, bool* eventPending)
    {
        return YamiV4L2_Poll(m_fd, pollDevice, eventPending);
    }
    int32_t setDevicePollInterrupt()
    {
        return YamiV4L2_SetDevicePollInterrupt(m_fd);
    }
    int32_t clearDevicePollInterrupt()
    {
        return YamiV4L2_ClearDevicePollInterrupt(m_fd);
    }
    void* mmap(void* addr, size_t length, int32_t prot,
        int32_t flags, unsigned int offset)
    {
        return YamiV4L2_Mmap(addr, length, prot, flags, m_fd, offset);
    }
    int32_t munmap(void* addr, size_t length)
    {
        return YamiV4L2_Munmap(addr, length);
    }
    int32_t setFrameMemoryType(VideoDataMemoryType type)
    {
        return YamiV4L2_FrameMemoryType(m_fd, type);
    }

#if __ENABLE_TESTS_GLES__
    int32_t useEglImage(/*EGLDisplay*/ void* eglDisplay, /*EGLContext*/ void* eglContext,
        uint32_t bufferIndex, void* eglImage)
    {
        return YamiV4L2_UseEglImage(m_fd, eglDisplay, eglContext, bufferIndex, eglImage);
    }
    int32_t setDrmFd(int drmFd)
    {
        return YamiV4L2_SetDrmFd(m_fd, drmFd);
    }
#endif

#if __ENABLE_WAYLAND__
    int32_t setWaylandDisplay(struct wl_display* wlDisplay)
    {
        return YamiV4L2_SetWaylandDisplay(m_fd, wlDisplay);
    }
#endif

#if __ENABLE_X11__
    /// it should be called before driver initialization (immediate after _Open()).
    int32_t setXDisplay(Display* x11Display)
    {
        return YamiV4L2_SetXDisplay(m_fd, x11Display);
    }
#endif

protected:
    bool init()
    {
        return true;
    }
};
#endif //__ENABLE_V4L2_OPS__

SharedPtr<V4L2Device> V4L2Device::Create()
{
    SharedPtr<V4L2Device> device;
#ifdef __ENABLE_V4L2_OPS__
    device.reset(new V4L2DeviceOps);
#else
    device.reset(new V4L2DeviceYami);
#endif
    if (!device->init())
        device.reset();
    return device;
}
