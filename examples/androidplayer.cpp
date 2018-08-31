/*
 * Copyright (C) 2013-2014 Intel Corporation. All rights reserved.
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

#include "vppinputoutput.h"
#include "decodeinput.h"
#include <Yami.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <ui/DisplayInfo.h>
#include <android/native_window.h>
#include <system/window.h>
#include <ui/GraphicBufferMapper.h>
#include <hardware/hardware.h>
#include <hardware/gralloc1.h>
#include <va/va_android.h>
#include <va/va.h>
#include <va/va_drmcommon.h>
#include <map>
#include <vector>

using namespace YamiMediaCodec;
using namespace android;

#ifndef CHECK_EQ
#define CHECK_EQ(a, b) do {                     \
            if ((a) != (b)) {                   \
                assert(0 && "assert fails");    \
            }                                   \
    } while (0)
#endif

#define ANDROID_DISPLAY 0x18C34078

typedef int32_t (*GRALLOC1_PFN_SET_INTERLACE)(gralloc1_device_t *device, buffer_handle_t buffer, uint32_t interlace);
#define GRALLOC1_FUNCTION_SET_INTERLACE 104

typedef int32_t /*gralloc1_error_t*/ (*GRALLOC1_PFN_GET_PRIME)(
        gralloc1_device_t *device, buffer_handle_t buffer, uint32_t *prime);
#define GRALLOC1_FUNCTION_GET_PRIME 103


typedef int32_t /*gralloc1_error_t*/ (*GRALLOC1_PFN_GET_BYTE_STRIDE)(
        gralloc1_device_t *device, buffer_handle_t buffer, uint32_t *outStride, uint32_t size);
#define GRALLOC1_FUNCTION_GET_BYTE_STRIDE 102

class Gralloc1
{
public:
    static SharedPtr<Gralloc1> create()
    {
        SharedPtr<Gralloc1> g(new Gralloc1);
        if (!g->init())
            g.reset();
        return g;
    }

    bool getByteStride(buffer_handle_t handle, uint32_t *outStride, uint32_t size)
    {
        return m_getByteStride(m_device, handle, outStride, size) == 0;
    }

    bool getPrime(buffer_handle_t handle, uint32_t* prime)
    {
        return m_getPrime(m_device, handle, prime) == 0;
    }

    bool setInterlace(buffer_handle_t handle, bool interlace)
    {
        uint32_t i = interlace;
        return m_setInterlace(m_device, handle, i) == 0;
    }

    ~Gralloc1()
    {
        if (m_device)
             gralloc1_close(m_device);
        //how to close module?

    }

private:
    bool init()
    {
        CHECK_EQ(0,  hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &m_module));
        CHECK_EQ(0, gralloc1_open(m_module, &m_device));
        m_setInterlace = (GRALLOC1_PFN_SET_INTERLACE)m_device->getFunction(m_device, GRALLOC1_FUNCTION_SET_INTERLACE);
        m_getPrime = (GRALLOC1_PFN_GET_PRIME)m_device->getFunction(m_device, GRALLOC1_FUNCTION_GET_PRIME);
        m_getByteStride = (GRALLOC1_PFN_GET_BYTE_STRIDE)m_device->getFunction(m_device, GRALLOC1_FUNCTION_GET_BYTE_STRIDE);
        return m_setInterlace && m_getPrime && m_getByteStride;
    }
    const struct hw_module_t* m_module = NULL;
    gralloc1_device_t* m_device = NULL;
    GRALLOC1_PFN_SET_INTERLACE m_setInterlace = NULL;
    GRALLOC1_PFN_GET_PRIME m_getPrime = NULL;
    GRALLOC1_PFN_GET_BYTE_STRIDE  m_getByteStride = NULL;
};

class AndroidPlayer
{
public:
    bool init(int argc, char** argv)
    {
        if (argc != 2) {
            printf("usage: androidplayer xxx.264\n");
            return false;
        }

        if(!initWindow()) {
            fprintf(stderr, "failed to create android surface\n");
            return false;
        }

        m_input.reset(DecodeInput::create(argv[1]));
        if (!m_input) {
            fprintf(stderr, "failed to open %s", argv[1]);
            return false;
        }

        if (!initDisplay()) {
            return false;
        }

        if(!createVpp()) {
            fprintf(stderr, "create vpp failed\n");
            return false;
        }

        //init decoder
        m_decoder.reset(createVideoDecoder(m_input->getMimeType()), releaseVideoDecoder);
        if (!m_decoder) {
            fprintf(stderr, "failed create decoder for %s", m_input->getMimeType());
            return false;
        }

        //set native display
        m_decoder->setNativeDisplay(m_nativeDisplay.get());
        return true;
    }

    bool run()
    {
        VideoConfigBuffer configBuffer;
        memset(&configBuffer, 0, sizeof(configBuffer));
        configBuffer.profile = VAProfileNone;
        const string codecData = m_input->getCodecData();
        if (codecData.size()) {
            configBuffer.data = (uint8_t*)codecData.data();
            configBuffer.size = codecData.size();
        }

        Decode_Status status = m_decoder->start(&configBuffer);
        assert(status == DECODE_SUCCESS);

        VideoDecodeBuffer inputBuffer;
        memset(&inputBuffer, 0, sizeof(inputBuffer));

        while (m_input->getNextDecodeUnit(inputBuffer)) {
            status = m_decoder->decode(&inputBuffer);
            if (DECODE_FORMAT_CHANGE == status) {
                //drain old buffers
                renderOutputs();
                const VideoFormatInfo *formatInfo = m_decoder->getFormatInfo();
                //resend the buffer
                status = m_decoder->decode(&inputBuffer);
            }
            if(status == DECODE_SUCCESS) {
                renderOutputs();
            } else {
                fprintf(stderr, "decode error %d\n", status);
                break;
            }
        }
        //renderOutputs();
        m_decoder->stop();
        return true;
    }

    AndroidPlayer() : m_width(0), m_height(0)
    {
    }

    ~AndroidPlayer()
    {
    }
private:
    VADisplay m_vaDisplay;
    bool initDisplay()
    {
        unsigned int display = ANDROID_DISPLAY;
        m_vaDisplay = vaGetDisplay(&display);

        int major, minor;
        VAStatus status;
        status = vaInitialize(m_vaDisplay, &major, &minor);
        if (status != VA_STATUS_SUCCESS) {
            fprintf(stderr, "init vaDisplay failed\n");
            return false;
        }

        m_nativeDisplay.reset(new NativeDisplay);
        m_nativeDisplay->type = NATIVE_DISPLAY_VA;
        m_nativeDisplay->handle = (intptr_t)m_vaDisplay;
        m_gralloc = Gralloc1::create();
        return m_gralloc.get();
    }

    bool createVpp()
    {
        NativeDisplay nativeDisplay;
        nativeDisplay.type = NATIVE_DISPLAY_VA;
        nativeDisplay.handle = (intptr_t)m_vaDisplay;
        m_vpp.reset(createVideoPostProcess(YAMI_VPP_SCALER), releaseVideoPostProcess);
       return m_vpp->setNativeDisplay(nativeDisplay) == YAMI_SUCCESS;
    }

    void renderOutputs()
    {
        SharedPtr<VideoFrame> srcFrame;
        do {
            srcFrame = m_decoder->getOutput();
            if (!srcFrame)
                break;

            if(!displayOneFrame(srcFrame))
                break;
        } while (1);
    }

    bool initWindow()
    {
        const int HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL = 0x100;
        static sp<SurfaceComposerClient> client = new SurfaceComposerClient();
        //create surface
        static sp<SurfaceControl> surfaceCtl = client->createSurface(String8("testsurface"), 800, 600, HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL, 0);

        // configure surface
        SurfaceComposerClient::Transaction{}
             .setLayer(surfaceCtl, 100000)
             .setPosition(surfaceCtl, 100, 100)
             .setSize(surfaceCtl, 1920, 1080)
             .apply();

        m_surface = surfaceCtl->getSurface();

        static sp<ANativeWindow> mNativeWindow = m_surface;
        int bufWidth = 1920;
        int bufHeight = 1088;

        int consumerUsage = 0;
        CHECK_EQ(NO_ERROR, mNativeWindow->query(mNativeWindow.get(), NATIVE_WINDOW_CONSUMER_USAGE_BITS, &consumerUsage));
        CHECK_EQ(0,
                 native_window_set_usage(
                 mNativeWindow.get(),
                 consumerUsage));

        CHECK_EQ(0,
                 native_window_set_scaling_mode(
                 mNativeWindow.get(),
                 NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW));

        CHECK_EQ(0, native_window_set_buffers_geometry(
                    mNativeWindow.get(),
                    bufWidth,
                    bufHeight,
                    HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL));


        CHECK_EQ(0, native_window_api_connect(mNativeWindow.get(), NATIVE_WINDOW_API_MEDIA));

        status_t err;
        err = native_window_set_buffer_count(mNativeWindow.get(), 5);
        if (err != 0) {
            ALOGE("native_window_set_buffer_count failed: %s (%d)", strerror(-err), -err);
            return false;
        }

        return true;
    }

    SharedPtr<VideoFrame> createVaSurface(const ANativeWindowBuffer* buf)
    {
        SharedPtr<VideoFrame> frame;

        uint32_t pitch[2];
        if (!m_gralloc->getByteStride(buf->handle, pitch, 2))
            return frame;

        VASurfaceAttrib attrib;
        memset(&attrib, 0, sizeof(attrib));

        VASurfaceAttribExternalBuffers external;
        memset(&external, 0, sizeof(external));

        external.pixel_format = VA_FOURCC_NV12;
        external.width = buf->width;
        external.height = buf->height;
        external.pitches[0] = pitch[0];
        external.pitches[1] = pitch[1];
        external.offsets[0] = 0;
        external.offsets[1] = pitch[0] * buf->height;
        external.num_planes = 2;
        external.num_buffers = 1;
        uint32_t handle;
        if (!m_gralloc->getPrime(buf->handle, &handle)) {
            ERROR("get prime failed");
            return frame;
        }
        external.buffers = (long unsigned int*)&handle; //graphic handel
        external.flags = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

        attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
        attrib.type = (VASurfaceAttribType)VASurfaceAttribExternalBufferDescriptor;
        attrib.value.type = VAGenericValueTypePointer;
        attrib.value.value.p = &external;

        VASurfaceID id;
        VAStatus vaStatus = vaCreateSurfaces(m_vaDisplay, VA_RT_FORMAT_YUV420,
                            buf->width, buf->height, &id, 1, &attrib, 1);
        if (vaStatus != VA_STATUS_SUCCESS)
            return frame;

        frame.reset(new VideoFrame);
        memset(frame.get(), 0, sizeof(VideoFrame));

        frame->surface = static_cast<intptr_t>(id);
        frame->crop.width = buf->width;
        frame->crop.height = buf->height;
        ERROR("id = %x\r\n", id);
        return frame;
    }

    bool displayOneFrame(SharedPtr<VideoFrame>& srcFrame)
    {
        status_t err;
        SharedPtr<VideoFrame> dstFrame;
        sp<ANativeWindow> mNativeWindow = m_surface;
        ANativeWindowBuffer* buf;

        printf("+wait\n");
        err = native_window_dequeue_buffer_and_wait(mNativeWindow.get(), &buf);
        if (err != 0) {
            fprintf(stderr, "dequeueBuffer failed: %s (%d)\n", strerror(-err), -err);
            return false;
        }

        std::map< ANativeWindowBuffer*, SharedPtr<VideoFrame> >::const_iterator it;
        it = m_buff.find(buf);
        if (it != m_buff.end()) {
            dstFrame = it->second;
        } else {
            dstFrame = createVaSurface(buf);
            m_buff.insert(std::pair<ANativeWindowBuffer*, SharedPtr<VideoFrame> >(buf, dstFrame));
        }

        m_vpp->process(srcFrame, dstFrame);

        if (mNativeWindow->queueBuffer(mNativeWindow.get(), buf, -1) != 0) {
            fprintf(stderr, "queue buffer to native window failed\n");
            return false;
        }
        return true;
    }

    SharedPtr<NativeDisplay> m_nativeDisplay;

    SharedPtr<IVideoDecoder> m_decoder;
    SharedPtr<DecodeInput> m_input;
    int m_width, m_height;
    SharedPtr<Gralloc1> m_gralloc;

    sp<Surface> m_surface;
    std::map< ANativeWindowBuffer*, SharedPtr<VideoFrame> > m_buff;
    SharedPtr<IVideoPostProcess> m_vpp;
};

int main(int argc, char** argv)
{
    AndroidPlayer player;
    if (!player.init(argc, argv)) {
        ERROR("init player failed with %s", argv[1]);
        return -1;
    }
    if (!player.run()){
        ERROR("run simple player failed");
        return -1;
    }
    printf("play file done\n");

    return  0;

}
