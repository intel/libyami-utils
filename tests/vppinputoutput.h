/*
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
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
#ifndef vppinputoutput_h
#define vppinputoutput_h

#include "common/log.h"
#include "common/utils.h"
#include "common/VaapiUtils.h"
#include "common/PooledFrameAllocator.h"
#include <Yami.h>

#include <va/va.h>
#ifndef ANDROID
#include <va/va_drm.h>
#endif
#include <vector>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>

using namespace YamiMediaCodec;

struct VADisplayDeleter
{
    VADisplayDeleter(int fd):m_fd(fd) {}
    void operator()(VADisplay* display)
    {
        vaTerminate(*display);
        delete display;
        close(m_fd);
    }
private:
    int m_fd;
};

SharedPtr<VADisplay> createVADisplay();


//virtual bool setFormat(uint32_t fourcc, int width, int height) = 0;
class FrameReader
{
public:
    virtual bool read(std::ifstream&, const SharedPtr<VideoFrame>& frame) = 0;
    virtual ~FrameReader() {}
};

class FrameWriter
{
public:
    virtual bool write(std::ofstream&, const SharedPtr<VideoFrame>& frame) = 0;
    virtual ~FrameWriter() {}
};

template <typename T>
class VaapiFrameIO
{
public:
    typedef bool (*FileIoFunc)(char* ptr, int size, T& fs);
    VaapiFrameIO(const SharedPtr<VADisplay>& display, FileIoFunc io)
        :m_display(display), m_io(io)
    {

    };
    bool doIO(T& fs, const SharedPtr<VideoFrame>& frame)
    {
        if (!fs || !fs.is_open() || !frame) {
            ERROR("invalid param");
            return false;
        }
        VASurfaceID surface = (VASurfaceID)frame->surface;
        VAImage image;

        VAStatus status = vaDeriveImage(*m_display,surface,&image);
        if (status != VA_STATUS_SUCCESS) {
            ERROR("vaDeriveImage failed = %d", status);
            return false;
        }
        uint32_t byteWidth[3], byteHeight[3], planes;
        uint32_t byteX[3], byteY[3];
        //image.width is not equal to frame->crop.width.
        //for supporting VPG Driver, use YV12 to replace I420
        if (!getPlaneResolution(frame->fourcc, frame->crop.width, frame->crop.height, byteWidth, byteHeight, planes)) {
            ERROR("get plane reoslution failed for %x, %dx%d", frame->fourcc, frame->crop.width, frame->crop.height);
            return false;
        }
        if (!getPlaneResolution(frame->fourcc, frame->crop.x, frame->crop.y, byteX, byteY, planes)) {
            ERROR("get left-top coordinate(%d,%d) failed", frame->crop.x, frame->crop.y);
            return false;
        }
        char* buf;
        status = vaMapBuffer(*m_display, image.buf, (void**)&buf);
        if (status != VA_STATUS_SUCCESS) {
            vaDestroyImage(*m_display, image.image_id);
            ERROR("vaMapBuffer failed = %d", status);
            return false;
        }
        bool ret = true;
        for (uint32_t i = 0; i < planes; i++) {
            char* ptr = buf + image.offsets[i];
            ptr += image.pitches[i] * byteY[i];
            int w = byteWidth[i];
            for (uint32_t j = 0; j < byteHeight[i]; j++) {
                ret = m_io(ptr + byteX[i], w, fs);
                if (!ret)
                    goto out;
                ptr += image.pitches[i];
            }
        }
    out:
        vaUnmapBuffer(*m_display, image.buf);
        vaDestroyImage(*m_display, image.image_id);
        return ret;

    }
private:
    SharedPtr<VADisplay>  m_display;
    FileIoFunc  m_io;
};


class VaapiFrameReader:public FrameReader
{
public:
    VaapiFrameReader(const SharedPtr<VADisplay>& display)
        :m_frameio(new VaapiFrameIO<std::ifstream>(display, readFromFile))
    {
    }
    bool read(std::ifstream& ifs, const SharedPtr<VideoFrame>& frame)
    {
        return m_frameio->doIO(ifs, frame);
    }
private:
    SharedPtr< VaapiFrameIO<std::ifstream> > m_frameio;
    static bool readFromFile(char* ptr, int size, std::ifstream& ifs)
    {
        return ifs.read(ptr, size).good();
    }
};

class VaapiFrameWriter:public FrameWriter
{
public:
    VaapiFrameWriter(const SharedPtr<VADisplay>& display)
        :m_frameio(new VaapiFrameIO<std::ofstream>(display, writeToFile))
    {
    }
    bool write(std::ofstream& ofs, const SharedPtr<VideoFrame>& frame)
    {
        return m_frameio->doIO(ofs, frame);
    }
private:
    SharedPtr< VaapiFrameIO<std::ofstream> > m_frameio;
    static bool writeToFile(char* ptr, int size, std::ofstream& ofs)
    {
        return ofs.write(ptr, size).good();
    }
};
//vaapi related operation end

class VppInput;
class VppInputFile;

class VppInput {
public:
    static SharedPtr<VppInput>
        create(const char* inputFileName, uint32_t fourcc = 0, int width = 0, int height = 0, bool useCAPI = false);
    virtual bool init(const char* inputFileName = 0, uint32_t fourcc = 0, int width = 0, int height = 0) = 0;
    virtual bool read(SharedPtr<VideoFrame>& frame) = 0;
    virtual const char * getMimeType() const = 0;
    virtual int getWidth() { return m_width; }
    virtual int getHeight() { return m_height; }
    virtual uint32_t getFourcc() { return m_fourcc; }

    virtual ~VppInput() {}
protected:
    uint32_t m_fourcc;
    int m_width;
    int m_height;
};

class VppInputFile : public VppInput {
public:
    //inherit VppInput
    bool init(const char* inputFileName, uint32_t fourcc, int width, int height);
    virtual bool read(SharedPtr<VideoFrame>& frame);
    const char *getMimeType() const { return "unknown"; }
    bool config(const SharedPtr<FrameAllocator>& allocator, const SharedPtr<FrameReader>& reader);
    VppInputFile();
    ~VppInputFile();
protected:
    std::ifstream m_ifs;
    bool m_readToEOS;
    SharedPtr<FrameReader> m_reader;
    SharedPtr<FrameAllocator> m_allocator;
};

class VppOutput
{
public:
    static SharedPtr<VppOutput> create(const char* outputFileName,
        uint32_t fourcc = 0, int width = 0,
        int height = 0,
        const char* codecName = NULL,
        int fps = 30);
    bool getFormat(uint32_t& fourcc, int& width, int& height);
    virtual bool output(const SharedPtr<VideoFrame>& frame) = 0;
    VppOutput();
    virtual ~VppOutput(){}
protected:
    virtual bool init(const char* outputFileName, uint32_t fourcc, int width,
        int height, const char* codecName, int fps = 30)
        = 0;
    uint32_t m_fourcc;
    int m_width;
    int m_height;
};

class VppOutputFile : public VppOutput
{
public:
    bool config(const SharedPtr<FrameWriter>& writer);

    //inherit VppOutput
    bool output(const SharedPtr<VideoFrame>& frame);
    virtual ~VppOutputFile();
    VppOutputFile();
protected:
    virtual bool init(const char* outputFileName, uint32_t fourcc, int width,
        int height, const char* codecName, int fps = 30);

private:
    bool write(const SharedPtr<VideoFrame>& frame);
    SharedPtr<FrameWriter> m_writer;
    std::ofstream m_ofs;
};

#endif      //vppinputoutput_h
