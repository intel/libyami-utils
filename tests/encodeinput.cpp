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
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include "common/log.h"
#include "common/utils.h"

#include "encodeinput.h"
#include "encodeInputDecoder.h"

using namespace YamiMediaCodec;

#define MAX_WIDTH  8192
#define MAX_HEIGHT 4320
EncodeInput * EncodeInput::create(const char* inputFileName, uint32_t fourcc, int width, int height)
{
    EncodeInput *input = NULL;
    if (!inputFileName) {
#if ANDROID
        input = new EncodeInputSurface();
#else
        return NULL;
#endif
    }
    else {
#ifndef ANDROID // temp disable transcoding and camera support on android
        DecodeInput* decodeInput = DecodeInput::create(inputFileName);
        if (decodeInput) {
            input = new EncodeInputDecoder(decodeInput);
        }
        else if (!strncmp(inputFileName, "/dev/video", strlen("/dev/video"))) {
            input = new EncodeInputCamera;
        }
        else
#endif
        {
            input = new EncodeInputFile;
        }
    }

    if (!input)
        return NULL;

    if (!(input->init(inputFileName, fourcc, width, height))) {
        delete input;
        return NULL;
    }

    return input;
}

EncodeInputFile::EncodeInputFile()
    : m_fp(NULL)
    , m_buffer(NULL)
    , m_readToEOS(false)
{
}

bool EncodeInputFile::init(const char* inputFileName, uint32_t fourcc, int width, int height)
{
    if (!width || !height) {
        if (!guessResolution(inputFileName, width, height)) {
            fprintf(stderr, "failed to guess input width and height\n");
            return false;
        }
    }
    m_width = width;
    m_height = height;
    m_fourcc = fourcc;

    if ((m_width <= 0) ||(m_height <= 0) ||(m_width > MAX_WIDTH) || (m_height > MAX_HEIGHT)) {
        fprintf(stderr, "input width and height is invalid\n");
        return false;
    }

    if (!m_fourcc)
        m_fourcc = guessFourcc(inputFileName);

    if (!m_fourcc)
        m_fourcc = VA_FOURCC('I', '4', '2', '0');

    switch (m_fourcc) {
    case VA_FOURCC_NV12:
    case VA_FOURCC('I', '4', '2', '0'):
    case VA_FOURCC_YV12:
        m_frameSize = m_width * m_height * 3 / 2;
    break;
    case VA_FOURCC_YUY2:
        m_frameSize = m_width * m_height * 2;
    break;
    default:
        ASSERT(0);
    break;
    }

    m_fp = fopen(inputFileName, "r");
    if (!m_fp) {
        fprintf(stderr, "fail to open input file: %s", inputFileName);
        return false;
    }

    m_buffer = static_cast<uint8_t*>(malloc(m_frameSize));
    return true;
}

bool EncodeInputFile::getOneFrameInput(VideoFrameRawData &inputBuffer)
{
    if (m_readToEOS)
        return false;

    uint8_t *buffer = m_buffer;
    if (inputBuffer.handle)
        buffer = reinterpret_cast<uint8_t*>(inputBuffer.handle);

    size_t ret = fread(buffer, sizeof(uint8_t), m_frameSize, m_fp);

    if (ret <= 0) {
        m_readToEOS = true;
        return false;
    }

    if (ret < m_frameSize) {
        fprintf (stderr, "data is not enough to read(read size: %zu, m_frameSize: %zu), maybe resolution is wrong\n", ret, m_frameSize);
        return false;
    }

    return fillFrameRawData(&inputBuffer, m_fourcc, m_width, m_height, buffer);
}

EncodeInputFile::~EncodeInputFile()
{
    if(m_fp)
        fclose(m_fp);

    if(m_buffer)
        free(m_buffer);
}

EncodeOutput::EncodeOutput():m_fp(NULL)
{
}

EncodeOutput::~EncodeOutput()
{
    if (m_fp)
        fclose(m_fp);
}

EncodeOutput* EncodeOutput::create(const char* outputFileName, int width,
                                   int height, const char* codecName)
{
    EncodeOutput* output = NULL;
    if (outputFileName == NULL)
        return NULL;
    const char* ext = strrchr(outputFileName, '.');
    std::string codec(""); // needed for strcasecmp to work
    if (!ext && !codecName)
        return NULL;
    ext++;

    if (codecName)
        codec.assign(codecName);

    if (!strcasecmp("AVC", codec.c_str()) || !strcasecmp(ext, "h264")
        || !strcasecmp(ext, "264") || !strcasecmp(ext, "jsv")
        || !strcasecmp(ext, "avc") || !strcasecmp(ext, "26l")
        || !strcasecmp(ext, "jvt")) {
        output = new EncodeOutputH264();
    }
    else if (!strcasecmp("VP8", codec.c_str())
             || (!strcasecmp("VP8", codec.c_str()) && !strcasecmp(ext, "ivf"))
             || !strcasecmp(ext, "vp8")) {
        output = new EncodeOutputVP8();
    }
    else if (!strcasecmp("VP9", codec.c_str())
             || (!strcasecmp("VP9", codec.c_str()) && !strcasecmp(ext, "ivf"))
             || !strcasecmp(ext, "vp9")) {
        output = new EncodeOutputVP9();
    }
    else if (!strcasecmp("JPEG", codec.c_str()) || !strcasecmp(ext, "jpg")
             || !strcasecmp(ext, "jpeg")) {
        output = new EncodeStreamOutputJpeg();
    }
    else if (!strcasecmp("HEVC", codec.c_str()) || !strcasecmp(ext, "h265")
             || !strcasecmp(ext, "265") || !strcasecmp(ext, "hevc")) {
        output = new EncodeOutputHEVC();
    }
    else
        return NULL;

    if (!output->init(outputFileName, width, height)) {
        delete output;
        return NULL;
    }
    return output;
}

bool EncodeOutput::init(const char* outputFileName, int width , int height)
{
    m_fp = fopen(outputFileName, "w+");
    if (!m_fp) {
        fprintf(stderr, "fail to open output file: %s\n", outputFileName);
        return false;
    }
    return true;
}

bool EncodeOutput::write(void* data, int size)
{
    return fwrite(data, 1, size, m_fp) == (size_t)size;
}

const char* EncodeOutputH264::getMimeType()
{
    return YAMI_MIME_H264;
}

const char* EncodeStreamOutputJpeg::getMimeType()
{
    return "image/jpeg";
}

const char* EncodeOutputVP8::getMimeType()
{
    return YAMI_MIME_VP8;
}

const char* EncodeOutputVP9::getMimeType()
{
    return YAMI_MIME_VP9;
}

const char* EncodeOutputHEVC::getMimeType()
{
    return YAMI_MIME_HEVC;
}

void setUint32(uint8_t* header, uint32_t value)
{
    uint32_t* h = (uint32_t*)header;
    *h = value;
}

void setUint16(uint8_t* header, uint16_t value)
{
    uint16_t* h = (uint16_t*)header;
    *h = value;
}

void EncodeOutputVPX::getIVFFileHeader(uint8_t *header, int width, int height)
{
    setUint32(header, YAMI_FOURCC('D','K','I','F'));
    setUint16(header+4, 0);                     // version
    setUint16(header+6,  32);                   // headersize
    setUint32(header+8,  m_fourcc);             // fourcc
    setUint16(header+12, width);                // width
    setUint16(header+14, height);               // height
    setUint32(header+16, 30);                   // rate
    setUint32(header+20, 1);                    // scale
    setUint32(header+24, m_frameCount);         // framecount
    setUint32(header+28, 0);                    // unused
}

bool EncodeOutputVPX::init(const char* outputFileName, int width , int height)
{
    if (!EncodeOutput::init(outputFileName, width, height))
        return false;
    uint8_t header[32];
    getIVFFileHeader(header, width, height);
    return EncodeOutput::write(header, sizeof(header));
}

bool EncodeOutputVPX::write(void* data, int size)
{
    uint8_t header[12];
    memset(header, 0, sizeof(header));
    setUint32(header, size);
    if (!EncodeOutput::write(&header, sizeof(header)))
        return false;
    if (!EncodeOutput::write(data, size))
        return false;
    m_frameCount++;
    return true;
}
EncodeOutputVPX::EncodeOutputVPX()
    : m_frameCount(0)
    , m_fourcc(0)
{
}

EncodeOutputVPX::~EncodeOutputVPX()
{
    if (m_fp && !fseek(m_fp, 24,SEEK_SET)) {
        EncodeOutput::write(&m_frameCount, sizeof(m_frameCount));
    }
}

EncodeOutputVP8::EncodeOutputVP8()
{
    EncodeOutputVPX::setFourcc(YAMI_FOURCC('V','P','8','0'));
}

EncodeOutputVP9::EncodeOutputVP9()
{
    EncodeOutputVPX::setFourcc(YAMI_FOURCC('V','P','9','0'));
}

bool createOutputBuffer(VideoEncOutputBuffer* outputBuffer, int maxOutSize)
{
    outputBuffer->data = static_cast<uint8_t*>(malloc(maxOutSize));
    if (!outputBuffer->data)
        return false;
    outputBuffer->bufferSize = maxOutSize;
    outputBuffer->format = OUTPUT_EVERYTHING;
    return true;
}

#ifdef __BUILD_GET_MV__
bool createMVBuffer(VideoEncMVBuffer* MVBuffer, int Size)
{
    MVBuffer->data = static_cast<uint8_t*>(malloc(Size));
    if (!MVBuffer->data)
        return false;
    MVBuffer->bufferSize = Size;
    return true;
}
#endif
