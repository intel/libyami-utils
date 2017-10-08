/*
 * Copyright (C) 2011-2016 Intel Corporation. All rights reserved.
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
#include "vaapi/vaapidisplay.h"
#include "common/VaapiUtils.h"

#include <vector>
#include <deque>
#include <algorithm>
#include <string.h>
#include <linux/videodev2.h>

#include "V4L2Renderer.h"

const static uint32_t kExtraOutputFrameCount = 2;
const static uint32_t kOutputPlaneCount = 2;
const static uint32_t kMaxOutputPlaneCount = 3;

using namespace YamiMediaCodec;

V4L2Renderer::V4L2Renderer(const SharedPtr<V4L2Device>& device, VideoDataMemoryType memoryType)
    : m_device(device)
    , m_memoryType(memoryType)
{
}

bool V4L2Renderer::renderOneFrame()
{
    uint32_t index;
    bool ret;
    ret = dequeBuffer(index);
    if (!ret)
        return false;
    ret = render(index);
    ASSERT(ret && "render failed");
    ret = queueBuffer(index);
    ASSERT(ret && "queue buffer failed");
    return true;
}

bool V4L2Renderer::queueBuffer(uint32_t index, unsigned long userptr)
{
    struct v4l2_buffer buf;
    struct v4l2_plane planes[kMaxOutputPlaneCount]; // YUV output, in fact, we use NV12 of 2 planes
    int ioctlRet = -1;

    memset(&buf, 0, sizeof(buf));
    memset(&planes, 0, sizeof(planes));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; //it indicates output buffer type
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    buf.m.planes = planes;
    buf.m.userptr = userptr;
    buf.length = kOutputPlaneCount;
    ioctlRet = m_device->ioctl(VIDIOC_QBUF, &buf);
    ASSERT(ioctlRet != -1);
    return true;
}

bool V4L2Renderer::dequeBuffer(uint32_t& index)
{
    struct v4l2_buffer buf;
    struct v4l2_plane planes[kMaxOutputPlaneCount]; // YUV output, in fact, we use NV12 of 2 planes
    int ioctlRet = -1;

    memset(&buf, 0, sizeof(buf));
    memset(&planes, 0, sizeof(planes));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; //it indicates output buffer type
    buf.memory = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length = kOutputPlaneCount;

    ioctlRet = m_device->ioctl(VIDIOC_DQBUF, &buf);
    if (ioctlRet == -1)
        return false;
    index = buf.index;
    return true;
}

bool V4L2Renderer::getDpbSize(uint32_t& dpbSize)
{
    // setup output buffers
    // Number of output buffers we need.
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
    int32_t ioctlRet = m_device->ioctl(VIDIOC_G_CTRL, &ctrl);
    ASSERT(ioctlRet != -1);
    dpbSize = ctrl.value;
    return true;
}

bool V4L2Renderer::requestBuffers(uint32_t& count)
{
    struct v4l2_requestbuffers reqbufs;

    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    reqbufs.count = count;
    int32_t ioctlRet = m_device->ioctl(VIDIOC_REQBUFS, &reqbufs);
    ASSERT(ioctlRet != -1);
    ASSERT(reqbufs.count > 0);
    count = reqbufs.count;
    return true;
}

#ifdef __ENABLE_X11__
#include <X11/Xlib.h>
class X11Renderer : public V4L2Renderer {
public:
    X11Renderer(const SharedPtr<V4L2Device>& device, VideoDataMemoryType memoryType)
        : V4L2Renderer(device, memoryType)
        , m_x11Display(NULL)
        , m_x11Window(0)
    {
    }
    bool setDisplay()
    {
        XInitThreads();
        m_x11Display = XOpenDisplay(NULL);
        ASSERT(m_x11Display);
        DEBUG("x11display: %p", m_x11Display);
        int32_t ioctlRet = m_device->setXDisplay(m_x11Display);
        return ioctlRet != -1;
    }
    bool queueOutputBuffersAtStart(uint32_t count)
    {
        for (uint32_t i = 0; i < count; i++) {
            if (!queueBuffer(i)) {
                ASSERT(0);
            }
        }
        return true;
    }

protected:
    bool createWindow(uint32_t width, uint32_t height)
    {
        if (m_x11Window)
            return true;
        m_x11Window = XCreateSimpleWindow(m_x11Display, DefaultRootWindow(m_x11Display), 0, 0, width, height, 0, 0, WhitePixel(m_x11Display, 0));
        if (m_x11Window <= 0)
            return false;
        XMapWindow(m_x11Display, m_x11Window);
        return true;
    }
    Display* m_x11Display;
    Window m_x11Window;
};

#include <va/va.h>
#include <va/va_drmcommon.h>
class ExternalDmaBufRenderer : public X11Renderer {
    const static uint32_t kFrontSize = 3;

public:
    ExternalDmaBufRenderer(const SharedPtr<V4L2Device>& device, VideoDataMemoryType memoryType)
        : X11Renderer(device, memoryType)
        , m_width(0)
        , m_height(0)
    {
    }
    bool setDisplay()
    {
        XInitThreads();
        m_x11Display = XOpenDisplay(NULL);
        ASSERT(m_x11Display);
        DEBUG("x11display: %p", m_x11Display);

        NativeDisplay native;
        memset(&native, 0, sizeof(native));
        native.handle = (intptr_t)m_x11Display;
        native.type = NATIVE_DISPLAY_X11;
        m_display = VaapiDisplay::create(native);
        ASSERT(bool(m_display));
        return true;
    }

    bool setupOutputBuffers(uint32_t width, uint32_t height)
    {
        if (!createWindow(width, height)) {
            ERROR("Create window failed");
            return false;
        }
        uint32_t dpbSize;
        if (!getDpbSize(dpbSize)) {
            ERROR("get dpb size failed");
            return false;
        }
        uint32_t count = dpbSize + kExtraOutputFrameCount + kFrontSize;
        if (!requestBuffers(count)) {
            ERROR("requestBuffers failed");
            return false;
        }
        m_width = width;
        m_height = height;
        return setupOutputBuffers(width, height, count) && queueOutputBuffersAtStart(count);
    }

    bool queueOutputBuffersAtStart(uint32_t count)
    {
        for (uint32_t i = 0; i < count; i++) {
            if (!queueBuffer(i, (unsigned long)m_dmabuf[i])) {
                ASSERT(0);
            }
            releaseBufferHandle(m_images[i]);
        }
        for (uint32_t i = 0; i < kFrontSize; i++) {
            uint32_t index;
            if (!dequeBuffer(index)) {
                ASSERT(0);
            }
            m_front.push_back(index);
        }
        return true;
    }

    bool queueOutputBuffers()
    {
        for (size_t i = 0; i < m_dmabuf.size(); i++) {
            if (std::find(m_front.begin(), m_front.end(), i) == m_front.end()) {
                if (!queueBuffer(i)) {
                    ASSERT(0);
                }
            }
        }
        return true;
    }
    virtual ~ExternalDmaBufRenderer()
    {
        if (m_surfaces.size()) {
            vaDestroySurfaces(m_display->getID(), &m_surfaces[0], m_surfaces.size());
        }
    }

private:
    bool acquireBufferHandle(uintptr_t& handle, VAImage& image, VASurfaceID surface)
    {
        if (!checkVaapiStatus(vaDeriveImage(m_display->getID(), surface, &image), "DeriveImage"))
            return false;
        VABufferInfo bufferInfo;
        bufferInfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
        if (!checkVaapiStatus(vaAcquireBufferHandle(m_display->getID(), image.buf, &bufferInfo), "AcquireBufferHandle")) {
            checkVaapiStatus(vaDestroyImage(m_display->getID(), image.image_id), "vaDestroyImage");
            return false;
        }
        handle = bufferInfo.handle;
        return true;
    }
    bool releaseBufferHandle(VAImage& image)
    {
        checkVaapiStatus(vaReleaseBufferHandle(m_display->getID(), image.buf), "ReleaseBufferHandle");
        checkVaapiStatus(vaDestroyImage(m_display->getID(), image.image_id), "vaDestroyImage");
        return true;
    }
    bool setSurfaceInfo(VASurfaceID surface)
    {
        VAImage image;
        if (!checkVaapiStatus(vaDeriveImage(m_display->getID(), surface, &image), "DeriveImage"))
            return false;
        struct v4l2_create_buffers createBuffers;
        memset(&createBuffers, 0, sizeof(createBuffers));
        v4l2_pix_format_mplane& format = createBuffers.format.fmt.pix_mp;
        format.pixelformat = image.format.fourcc;
        format.width = image.width;
        format.height = image.height;
        format.num_planes = image.num_planes;
        for (uint32_t i = 0; i < format.num_planes; i++) {
            format.plane_fmt[i].bytesperline = image.pitches[i];
            //not really right, but we use sizeimage to deliver offset
            format.plane_fmt[i].sizeimage = image.offsets[i];
        }
        int32_t ioctlRet = m_device->ioctl(VIDIOC_CREATE_BUFS, &createBuffers);
        checkVaapiStatus(vaDestroyImage(m_display->getID(), image.image_id), "vaDestroyImage");
        return ioctlRet != -1;
    }
    bool setupOutputBuffers(uint32_t width, uint32_t height, uint32_t count)
    {
        m_surfaces.resize(count);

        VASurfaceAttrib attrib;
        uint32_t rtFormat = VA_RT_FORMAT_YUV420;
        int pixelFormat = VA_FOURCC_NV12;
        attrib.type = VASurfaceAttribPixelFormat;
        attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
        attrib.value.type = VAGenericValueTypeInteger;
        attrib.value.value.i = pixelFormat;
        VAStatus status;
        status = vaCreateSurfaces(m_display->getID(), rtFormat,
            width, height, &m_surfaces[0], count, &attrib, 1);
        ASSERT(status == VA_STATUS_SUCCESS);
        ASSERT(setSurfaceInfo(m_surfaces[0]));
        m_dmabuf.resize(count);
        m_images.resize(count);
        for (uint32_t i = 0; i < count; i++) {
            if (!acquireBufferHandle(m_dmabuf[i], m_images[i], m_surfaces[i]))
                ASSERT(0);
        }
        return true;
    }
    void addAndPopFront(uint32_t& index)
    {
        m_front.push_back(index);
        index = m_front.front();
        m_front.pop_front();
    }
    bool render(uint32_t& index)
    {
        ASSERT(index < m_surfaces.size());
        VASurfaceID s = m_surfaces[index];
        VAStatus status = vaPutSurface(m_display->getID(), s,
            m_x11Window, 0, 0, m_width, m_height,
            0, 0, m_width, m_height,
            NULL, 0, 0);
        bool ret = checkVaapiStatus(status, "vaPutSurface");

        addAndPopFront(index);
        return ret;
    }
    DisplayPtr m_display;
    std::vector<VASurfaceID> m_surfaces;
    std::vector<uintptr_t> m_dmabuf;
    std::vector<VAImage> m_images;
    std::deque<uint32_t> m_front;
    uint32_t m_width;
    uint32_t m_height;
};

#ifdef __ENABLE_EGL__ //our egl application need x11 for output

#include "./egl/gles2_help.h"

class EglRenderer : public X11Renderer {
public:
    EglRenderer(const SharedPtr<V4L2Device>& device, VideoDataMemoryType memoryType)
        : X11Renderer(device, memoryType)
        , m_eglContext(NULL)
    {
    }
    bool setupOutputBuffers(uint32_t width, uint32_t height)
    {
        if (!createWindow(width, height)) {
            ERROR("Create window failed");
            return false;
        }
        uint32_t dpbSize;
        if (!getDpbSize(dpbSize)) {
            ERROR("get dpb size failed");
            return false;
        }
        uint32_t count = dpbSize + kExtraOutputFrameCount;
        if (!requestBuffers(count)) {
            ERROR("requestBuffers failed");
            return false;
        }
        return setupOutputBuffers(count) && queueOutputBuffersAtStart(count);
    }
    bool render(uint32_t& idx)
    {
        const uint32_t index = idx;
        ASSERT(m_eglContext && m_textureIds.size());
        ASSERT(index >= 0 && index < m_textureIds.size());
        DEBUG("textureIds[%d] = 0x%x", index, m_textureIds[index]);
        GLenum target = GL_TEXTURE_2D;
        if (isDmaBuf())
            target = GL_TEXTURE_EXTERNAL_OES;
        int ret = drawTextures(m_eglContext, target, &m_textureIds[index], 1);

        return ret == 0;
    }
    virtual ~EglRenderer()
    {
        if (m_textureIds.size())
            glDeleteTextures(m_textureIds.size(), &m_textureIds[0]);
        ASSERT(glGetError() == GL_NO_ERROR);
        for (size_t i = 0; i < m_eglImages.size(); i++) {
            destroyImage(m_eglContext->eglContext.display, m_eglImages[i]);
        }
        /*
        there is still randomly fail in mesa; no good idea for it. seems mesa bug
        0  0x00007ffff079c343 in _mesa_symbol_table_dtor () from /usr/lib/x86_64-linux-gnu/libdricore9.2.1.so.1
        1  0x00007ffff073c55d in glsl_symbol_table::~glsl_symbol_table() () from /usr/lib/x86_64-linux-gnu/libdricore9.2.1.so.1
        2  0x00007ffff072a4d5 in ?? () from /usr/lib/x86_64-linux-gnu/libdricore9.2.1.so.1
        3  0x00007ffff072a4bd in ?? () from /usr/lib/x86_64-linux-gnu/libdricore9.2.1.so.1
        4  0x00007ffff064b48f in _mesa_reference_shader () from /usr/lib/x86_64-linux-gnu/libdricore9.2.1.so.1
        5  0x00007ffff0649397 in ?? () from /usr/lib/x86_64-linux-gnu/libdricore9.2.1.so.1
        6  0x000000000040624d in releaseShader (program=0x77cd90) at ./egl/gles2_help.c:158
        7  eglRelease (context=0x615920) at ./egl/gles2_help.c:310
        8  0x0000000000402ca8 in main (argc=<optimized out>, argv=<optimized out>) at v4l2decode.cpp:531
        */
        if (m_eglContext)
            eglRelease(m_eglContext);
        if (m_x11Window)
            XDestroyWindow(m_x11Display, m_x11Window);
    }

private:
    bool setupOutputBuffers(uint32_t count)
    {
        m_textureIds.resize(count);
        // setup all textures and eglImages

        if (!m_eglContext) {
            m_eglContext = eglInit(m_x11Display, m_x11Window, 0 /*VA_FOURCC_RGBA*/, isDmaBuf());
            if (!m_eglContext)
                return false;
        }
        m_eglImages.resize(count);
        glGenTextures(count, &m_textureIds[0]);
        for (uint32_t i = 0; i < count; i++) {
            int ret = 0;
            ret = m_device->useEglImage(m_eglContext->eglContext.display, m_eglContext->eglContext.context, i, &m_eglImages[i]);
            ASSERT(ret == 0);

            GLenum target = GL_TEXTURE_2D;
            if (isDmaBuf())
                target = GL_TEXTURE_EXTERNAL_OES;
            glBindTexture(target, m_textureIds[i]);
            imageTargetTexture2D(target, m_eglImages[i]);

            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            DEBUG("textureIds[%d]: 0x%x, eglImages[%d]: 0x%p", i, m_textureIds[i], i, m_eglImages[i]);
        }
        return true;
    }

    bool isDmaBuf()
    {
        return m_memoryType == VIDEO_DATA_MEMORY_TYPE_DMA_BUF;
    }

    bool queueOutputBuffers()
    {
        return queueOutputBuffersAtStart((uint32_t)m_eglImages.size());
    }

    EGLContextType* m_eglContext;
    std::vector<EGLImageKHR> m_eglImages;
    std::vector<GLuint> m_textureIds;
};

#endif

#endif

SharedPtr<V4L2Renderer> V4L2Renderer::create(const SharedPtr<V4L2Device>& device, VideoDataMemoryType memoryType)
{
    SharedPtr<V4L2Renderer> renderer;
#ifdef __ENABLE_X11__
    if (memoryType == VIDEO_DATA_MEMORY_TYPE_EXTERNAL_DMA_BUF)
        renderer.reset(new ExternalDmaBufRenderer(device, memoryType));
#ifdef __ENABLE_EGL__
    if (memoryType == VIDEO_DATA_MEMORY_TYPE_DRM_NAME || memoryType == VIDEO_DATA_MEMORY_TYPE_DMA_BUF)
        renderer.reset(new EglRenderer(device, memoryType));
#endif
#endif

    return renderer;
}
