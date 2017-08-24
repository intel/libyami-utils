/*
 * Copyright (C) 2011-2014 Intel Corporation. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <errno.h>
#include  <sys/mman.h>
#include <vector>
#include <stdint.h>

#include "common/log.h"
#include "common/utils.h"
#include "decodeinput.h"
#include "decodehelp.h"
#include "V4L2Device.h"
#include "V4L2Renderer.h"

#include <Yami.h>

#ifndef V4L2_EVENT_RESOLUTION_CHANGE
    #define V4L2_EVENT_RESOLUTION_CHANGE 5
#endif

uint32_t videoWidth = 0;
uint32_t videoHeight = 0;
static enum v4l2_memory inputMemoryType = V4L2_MEMORY_MMAP;
static VideoDataMemoryType memoryType = VIDEO_DATA_MEMORY_TYPE_DRM_NAME;
static enum v4l2_memory outputMemoryType = V4L2_MEMORY_MMAP;

struct RawFrameData {
    uint32_t width;
    uint32_t height;
    uint32_t pitch[3];
    uint32_t offset[3];
    uint32_t fourcc;            //NV12
    uint8_t *data;
};

const uint32_t k_maxInputBufferSize = 1024*1024;
const int k_inputPlaneCount = 1;
const int k_maxOutputPlaneCount = 3;
int outputPlaneCount = 2;

uint32_t inputQueueCapacity = 0;
uint32_t outputQueueCapacity = 0;
uint32_t k_extraOutputFrameCount = 2;
static std::vector<uint8_t*> inputFrames;
static std::vector<struct RawFrameData> rawOutputFrames;

static bool isReadEOS=false;
static int32_t stagingBufferInDevice = 0;
static uint32_t renderFrameCount = 0;

static DecodeParameter params;

bool feedOneInputFrame(const SharedPtr<DecodeInput>& input, const SharedPtr<V4L2Device>& device, int index = -1 /* if index is not -1, simple enque it*/)
{

    VideoDecodeBuffer inputBuffer;
    struct v4l2_buffer buf;
    struct v4l2_plane planes[k_inputPlaneCount];
    int ioctlRet = -1;

    memset(&buf, 0, sizeof(buf));
    memset(&planes, 0, sizeof(planes));
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; // it indicates input buffer(raw frame) type
    buf.memory = inputMemoryType;
    buf.m.planes = planes;
    buf.length = k_inputPlaneCount;

    if (index == -1) {
        ioctlRet = device->ioctl(VIDIOC_DQBUF, &buf);
        if (ioctlRet == -1)
            return true;
        stagingBufferInDevice --;
    } else {
        buf.index = index;
    }

    if (isReadEOS)
        return false;

    if (!input->getNextDecodeUnit(inputBuffer)) {
        // send empty buffer for EOS
        buf.m.planes[0].bytesused = 0;
        isReadEOS = true;
    } else {
        ASSERT(inputBuffer.size <= k_maxInputBufferSize);
        memcpy(inputFrames[buf.index], inputBuffer.data, inputBuffer.size);
        buf.m.planes[0].bytesused = inputBuffer.size;
        buf.m.planes[0].m.mem_offset = 0;
        buf.flags = inputBuffer.flag;
    }

    ioctlRet = device->ioctl(VIDIOC_QBUF, &buf);
    ASSERT(ioctlRet != -1);

    stagingBufferInDevice ++;
    return true;
}


bool handleResolutionChange(const SharedPtr<V4L2Device>& device,
    const SharedPtr<V4L2Renderer>& renderer)
{
    DEBUG("+handle resolution change");
    bool resolutionChanged = false;
    // check resolution change
    struct v4l2_event ev;
    memset(&ev, 0, sizeof(ev));

    while (device->ioctl(VIDIOC_DQEVENT, &ev) == 0) {
        if (ev.type == V4L2_EVENT_RESOLUTION_CHANGE) {
            resolutionChanged = true;
            break;
        }
    }
    if (!resolutionChanged) {
        WARNING("no resolution change");
        return false;
    }

    bool ret =  renderer->onFormatChanged();
    ERROR("-handle resolution change");
    return ret;
}

extern uint32_t v4l2PixelFormatFromMime(const char* mime);

int main(int argc, char** argv)
{
    SharedPtr<DecodeInput> input;
    uint32_t i = 0;
    int32_t ioctlRet = -1;
    YamiMediaCodec::CalcFps calcFps;

    if (!processCmdLine(argc, argv, &params))
        return -1;

    switch (params.renderMode) {
    case 0:
        memoryType = VIDEO_DATA_MEMORY_TYPE_RAW_COPY;
    break;
    case 3:
        memoryType = VIDEO_DATA_MEMORY_TYPE_DRM_NAME;
    break;
    case 4:
        memoryType = VIDEO_DATA_MEMORY_TYPE_DMA_BUF;
        break;
    case 6:
        memoryType = VIDEO_DATA_MEMORY_TYPE_EXTERNAL_DMA_BUF;
    break;
    default:
        ASSERT(0 && "unsupported render mode, -m [0,3,4, 6] are supported");
    break;
    }
    input.reset(DecodeInput::create(params.inputFile));
    if (!input) {
        ERROR("fail to init input stream\n");
        return -1;
    }

    SharedPtr<V4L2Device> device = V4L2Device::Create();
    if (!device) {
        ERROR("failed to create v4l2 device");
        return -1;
    }
    SharedPtr<V4L2Renderer> renderer = V4L2Renderer::create(device, memoryType);
    if (!renderer) {
        ERROR("unsupported render mode %d, please check your build configuration", memoryType);
        return -1;
    }

    renderFrameCount = 0;
    calcFps.setAnchor();
    // open device
    if (!device->open("decoder", 0)) {
        ERROR("open decode failed");
        return -1;
    }

    ioctlRet = device->setFrameMemoryType(memoryType);

    if (!renderer->setDisplay()) {
        ERROR("set display failed");
        return -1;
    }

    // query hw capability
    struct v4l2_capability caps;
    memset(&caps, 0, sizeof(caps));
    caps.capabilities = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING;
    ioctlRet = device->ioctl(VIDIOC_QUERYCAP, &caps);
    ASSERT(ioctlRet != -1);

    // set input/output data format
    uint32_t codecFormat = v4l2PixelFormatFromMime(input->getMimeType());
    if (!codecFormat) {
        ERROR("unsupported mimetype, %s", input->getMimeType());
        return -1;
    }

    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    format.fmt.pix_mp.pixelformat = codecFormat;
    format.fmt.pix_mp.width = input->getWidth();
    format.fmt.pix_mp.height = input->getHeight();
    format.fmt.pix_mp.num_planes = 1;
    format.fmt.pix_mp.plane_fmt[0].sizeimage = k_maxInputBufferSize;
    ioctlRet = device->ioctl(VIDIOC_S_FMT, &format);
    ASSERT(ioctlRet != -1);

    // set preferred output format
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    uint8_t* data = (uint8_t*)input->getCodecData().data();
    uint32_t size = input->getCodecData().size();
    //save codecdata, size+data, the type of format.fmt.raw_data is __u8[200]
    //we must make sure enough space (>=sizeof(uint32_t) + size) to store codecdata
    memcpy(format.fmt.raw_data, &size, sizeof(uint32_t));
    if(sizeof(format.fmt.raw_data) >= size + sizeof(uint32_t))
        memcpy(format.fmt.raw_data + sizeof(uint32_t), data, size);
    else {
        ERROR("No enough space to store codec data");
        return -1;
    }
    ioctlRet = device->ioctl(VIDIOC_S_FMT, &format);
    ASSERT(ioctlRet != -1);
    // input port starts as early as possible to decide output frame format
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    ioctlRet = device->ioctl(VIDIOC_STREAMON, &type);
    ASSERT(ioctlRet != -1);

    // setup input buffers
    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = inputMemoryType;
    reqbufs.count = 8;
    ioctlRet = device->ioctl(VIDIOC_REQBUFS, &reqbufs);
    ASSERT(ioctlRet != -1);
    ASSERT(reqbufs.count>0);
    inputQueueCapacity = reqbufs.count;
    inputFrames.resize(inputQueueCapacity);

    for (i=0; i<inputQueueCapacity; i++) {
        struct v4l2_plane planes[k_inputPlaneCount];
        struct v4l2_buffer buffer;
        memset(&buffer, 0, sizeof(buffer));
        memset(planes, 0, sizeof(planes));
        buffer.index = i;
        buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buffer.memory = inputMemoryType;
        buffer.m.planes = planes;
        buffer.length = k_inputPlaneCount;
        ioctlRet = device->ioctl(VIDIOC_QUERYBUF, &buffer);
        ASSERT(ioctlRet != -1);

        // length and mem_offset should be filled by VIDIOC_QUERYBUF above
        void* address = device->mmap(NULL,
            buffer.m.planes[0].length,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            buffer.m.planes[0].m.mem_offset);
        ASSERT(address);
        inputFrames[i] = static_cast<uint8_t*>(address);
        DEBUG("inputFrames[%d] = %p", i, inputFrames[i]);
    }

    // feed input frames first
    for (i=0; i<inputQueueCapacity; i++) {
        if (!feedOneInputFrame(input, device, i)) {
            break;
        }
    }

    // query video resolution
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    while (1) {
        if (device->ioctl(VIDIOC_G_FMT, &format) != 0) {
            if (errno != EINVAL) {
                // EINVAL means we haven't seen sufficient stream to decode the format.
                INFO("ioctl() failed: VIDIOC_G_FMT, haven't get video resolution during start yet, waiting");
            }
        } else {
            break;
        }
        usleep(50);
    }
    outputPlaneCount = format.fmt.pix_mp.num_planes;
    ASSERT(outputPlaneCount == 2);
    videoWidth = format.fmt.pix_mp.width;
    videoHeight = format.fmt.pix_mp.height;
    ASSERT(videoWidth && videoHeight);
    bool ret = renderer->setupOutputBuffers(videoWidth, videoHeight);
    ASSERT(ret && "setupOutputBuffers failed");

    // output port starts as late as possible to adopt user provide output buffer
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctlRet = device->ioctl(VIDIOC_STREAMON, &type);
    ASSERT(ioctlRet != -1);
//#define SEEK_POS  1300
#ifdef SEEK_POS
    uint32_t frames = 0;
    uint32_t seekPos = rand() % SEEK_POS;
#endif

    bool event_pending=true; // try to get video resolution.
    uint32_t dqCountAfterEOS = 0;
    do {
        if (event_pending) {
            handleResolutionChange(device, renderer);
        }

        renderer->renderOneFrame();
        if (!feedOneInputFrame(input, device)) {
            if (stagingBufferInDevice == 0)
                break;
            dqCountAfterEOS++;
        }
        if (dqCountAfterEOS == inputQueueCapacity)  // input drain
            break;

#ifdef SEEK_POS
        frames++;
        if (frames == seekPos) {
            ERROR("Seek from %d to pos 0", seekPos);
            frames = 0;
            seekPos = rand() % SEEK_POS;
            input.reset(DecodeInput::create(params.inputFile));
            if (!input) {
                ERROR("fail to init input stream\n");
                return -1;
            }

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            ioctlRet = device->ioctl(VIDIOC_STREAMOFF, &type);
            ASSERT(ioctlRet != -1);

            type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            ioctlRet = device->ioctl(VIDIOC_STREAMOFF, &type);
            ASSERT(ioctlRet != -1);
            stagingBufferInDevice = 0;

            type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            ioctlRet = device->ioctl(VIDIOC_STREAMON, &type);
            ASSERT(ioctlRet != -1);

            for (i = 0; i < inputQueueCapacity; i++) {
                if (!feedOneInputFrame(input, device, i)) {
                    break;
                }
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            ioctlRet = device->ioctl(VIDIOC_STREAMON, &type);
            renderer->queueOutputBuffers();
        }
#endif
    } while (device->poll(true, &event_pending) == 0);

    // drain output buffer
    int retry = 3;
    while (renderer->renderOneFrame() || (--retry) > 0) { // output drain
        usleep(10000);
    }

    calcFps.fps(renderFrameCount);
    // SIMULATE_V4L2_OP(Munmap)(void* addr, size_t length)
    possibleWait(input->getMimeType(), &params);

    // release queued input/output buffer
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = inputMemoryType;
    reqbufs.count = 0;
    ioctlRet = device->ioctl(VIDIOC_REQBUFS, &reqbufs);
    ASSERT(ioctlRet != -1);

    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = outputMemoryType;
    reqbufs.count = 0;
    ioctlRet = device->ioctl(VIDIOC_REQBUFS, &reqbufs);
    ASSERT(ioctlRet != -1);

    // stop input port
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    ioctlRet = device->ioctl(VIDIOC_STREAMOFF, &type);
    ASSERT(ioctlRet != -1);

    // stop output port
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctlRet = device->ioctl(VIDIOC_STREAMOFF, &type);
    ASSERT(ioctlRet != -1);

    // close device
    ioctlRet = device->close();
    ASSERT(ioctlRet != -1);

    fprintf(stdout, "decode done\n");
    return 0;
}

