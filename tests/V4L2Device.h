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
#ifndef V4L2Device_h
#define V4L2Device_h

#include <VideoCommonDefs.h>
#if __ENABLE_X11__
#include <X11/Xlib.h>
#endif

#if __ENABLE_EGL__
#include <EGL/egl.h>
#endif

class V4L2Device {
public:
    static SharedPtr<V4L2Device> Create();
    virtual bool open(const char* name, int32_t flags) = 0;
    virtual int32_t close() = 0;
    virtual int32_t ioctl(int32_t cmd, void* arg) = 0;
    virtual int32_t poll(bool pollDevice, bool* eventPending) = 0;
    virtual int32_t setDevicePollInterrupt() = 0;
    virtual int32_t clearDevicePollInterrupt() = 0;
    virtual void* mmap(void* addr, size_t length, int32_t prot,
        int32_t flags, unsigned int offset)
        = 0;
    virtual int32_t munmap(void* addr, size_t length) = 0;
    virtual int32_t setFrameMemoryType(VideoDataMemoryType memory_type) = 0;

#if __ENABLE_EGL__
    virtual int32_t useEglImage(/*EGLDisplay*/ void* eglDisplay, /*EGLContext*/ void* eglContext,
        uint32_t bufferIndex, void* eglImage)
        = 0;
    virtual int32_t setDrmFd(int drmFd) = 0;
#endif

#if __ENABLE_WAYLAND__
    virtual int32_t setWaylandDisplay(struct wl_display* wlDisplay) = 0;
#endif

#if __ENABLE_X11__
    /// it should be called before driver initialization (immediate after _Open()).
    virtual int32_t setXDisplay(Display* x11Display) = 0;
#endif
protected:
    virtual bool init() = 0;
    int m_fd;
};

#endif
