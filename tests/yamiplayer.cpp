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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tests/decodeinput.h"
#include "common/log.h"
#include <Yami.h>
#include <stdlib.h>
#include <fcntl.h>
#include <va/va_drm.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <unistd.h>
#ifdef __ENABLE_X11__
#include <X11/Xlib.h>
#include <va/va_x11.h>
#endif
#ifdef __ENABLE_WAYLAND__
#include <va/va_wayland.h>
#endif

using namespace YamiMediaCodec;

#define CPPPRINT(...) std::cout << __VA_ARGS__ << std::endl
#define FILE_OUTPUT 0
#define X11_RENDERING 1
#define WAYLAND_RENDERING 2

#define checkVaapiStatus(status, prompt)                     \
    (                                                        \
        {                                                    \
            bool ret;                                        \
            ret = (status == VA_STATUS_SUCCESS);             \
            if (!ret)                                        \
                ERROR("%s: %s", prompt, vaErrorStr(status)); \
            ret;                                             \
        })

typedef struct YamiPlayerParameter {
    string inputFile;
    string outputFile;
    uint32_t outputFrameNumber;
    uint16_t surfaceNumber;
    uint16_t extraSurfaceNumber;
    uint32_t readSize;
    uint16_t outputMode;
    bool getFirstFrame;
    bool enableLowLatency;
} YamiPlayerParameter;

void printHelp(const char* app)
{
    CPPPRINT(app << " -i input.264 -m 0");
    CPPPRINT("   -i media file to decode");
    CPPPRINT("   -o specify the name of dumped output file");
    CPPPRINT("   -r read size, only for 264, default 120539");
    CPPPRINT("   -s surface number for decoding, mostly for vp9 decoding in fast-boot mode.");
    CPPPRINT("   -e extra surface number for decoding, mostly for avc decoding in fast-boot mode.");
    CPPPRINT("   -l low latency");
    CPPPRINT("   -g just to get surface of the first decoded frame");
    CPPPRINT("   -n specify how many frames to be decoded");
    CPPPRINT("   -m render mode, default 0");
    CPPPRINT("      0: dump video frame to file in NV12 format [*]");
    CPPPRINT("      1: render to X window [*]");
    CPPPRINT("      2: render to wayland window [*]");
}

void clearParameter(YamiPlayerParameter* parameters)
{
    if (parameters != NULL) {
        parameters->outputFrameNumber = 0;
        parameters->surfaceNumber = 0;
        parameters->extraSurfaceNumber = 0;
        parameters->readSize = 0;
        parameters->outputMode = 0;
        parameters->getFirstFrame = 0;
        parameters->enableLowLatency = 0;
    }
}

bool processCmdLine(int argc, char** argv, YamiPlayerParameter* parameters)
{
    char opt;
    while ((opt = getopt(argc, argv, "h?r:s:e:lgi:o:n:m:")) != -1) {
        switch (opt) {
        case 'h':
        case '?':
            printHelp(argv[0]);
            return false;
        case 'r':
            parameters->readSize = atoi(optarg);
            break;
        case 's':
            parameters->surfaceNumber = atoi(optarg);
            break;
        case 'e':
            parameters->extraSurfaceNumber = atoi(optarg);
            break;
        case 'l':
            parameters->enableLowLatency = true;
            break;
        case 'g':
            parameters->getFirstFrame = true;
            break;
        case 'i':
            parameters->inputFile.assign(optarg);
            break;
        case 'o':
            parameters->outputFile.assign(optarg);
            break;
        case 'n':
            parameters->outputFrameNumber = atoi(optarg);
            break;
        case 'm':
            parameters->outputMode = atoi(optarg);
            break;
        default:
            printHelp(argv[0]);
            return false;
        }
    }

    if (optind < argc) {
        int indexOpt = optind;
        CPPPRINT("unrecognized option: ");
        while (indexOpt < argc)
            CPPPRINT(argv[indexOpt++]);
        CPPPRINT("");
        return false;
    }

    if (parameters->inputFile.empty()) {
        printHelp(argv[0]);
        ERROR("no input file.");
        return false;
    }
#ifndef __ENABLE_X11__
    if (X11_RENDERING == parameters->outputMode) {
        ERROR("x11 is disabled, so not support readering to X window!");
        return false;
    }
#endif
#ifndef __ENABLE_WAYLAND__
    if (WAYLAND_RENDERING == parameters->outputMode) {
        ERROR("WAYLAND is disabled, so not support readering to WAYLAND window!");
        return false;
    }
#endif
    return true;
}

struct VADisplayTerminator {
    VADisplayTerminator() {}
    void operator()(VADisplay* display)
    {
        vaTerminate(*display);
        delete display;
    }
};

class DecodeOutput {
public:
    static DecodeOutput* create(int renderMode, const char* outputFile);
    virtual bool output(const SharedPtr<VideoFrame>& frame) = 0;
    SharedPtr<NativeDisplay> nativeDisplay();
    virtual ~DecodeOutput() {}
protected:
    virtual bool setVideoSize(uint32_t with, uint32_t height);

    virtual bool init();

    uint32_t m_width;
    uint32_t m_height;
    SharedPtr<VADisplay> m_vaDisplay;
    SharedPtr<NativeDisplay> m_nativeDisplay;
};

bool DecodeOutput::init()
{
    m_nativeDisplay.reset(new NativeDisplay);
    m_nativeDisplay->type = NATIVE_DISPLAY_VA;
    m_nativeDisplay->handle = (intptr_t)*m_vaDisplay;
    if (!m_vaDisplay || !m_nativeDisplay) {
        ERROR("init display error");
        return false;
    }
    return true;
}

SharedPtr<NativeDisplay> DecodeOutput::nativeDisplay()
{
    return m_nativeDisplay;
}

bool DecodeOutput::setVideoSize(uint32_t with, uint32_t height)
{
    m_width = with;
    m_height = height;
    return true;
}

struct VADisplayDeleter {
    VADisplayDeleter(int fd)
        : m_fd(fd)
    {
    }
    void operator()(VADisplay* display)
    {
        vaTerminate(*display);
        delete display;
        close(m_fd);
    }

private:
    int m_fd;
};

class DecodeOutputFile : public DecodeOutput {
public:
    DecodeOutputFile(const char* outputFile)
    {
        m_outputFile.assign(outputFile);
    }
    ~DecodeOutputFile()
    {
        if (m_ofs.is_open())
            m_ofs.close();
    }
    virtual bool output(const SharedPtr<VideoFrame>& frame);
    virtual bool init();

private:
    bool writeNV12ToFile(VASurfaceID surface, int w, int h);
    bool getPlaneResolution_NV12(uint32_t pixelWidth, uint32_t pixelHeight, uint32_t byteWidth[3], uint32_t byteHeight[3]);

protected:
    //const char* m_outputFile;
private:
    std::ofstream m_ofs;
    string m_outputFile;
};

SharedPtr<VADisplay> createVADisplay()
{
    SharedPtr<VADisplay> display;
    int fd = open("/dev/dri/renderD128", O_RDWR);
    if (fd < 0) {
        ERROR("can't open /dev/dri/renderD128, try to /dev/dri/card0");
        fd = open("/dev/dri/card0", O_RDWR);
    }
    if (fd < 0) {
        ERROR("can't open drm device");
        return display;
    }
    VADisplay vadisplay = vaGetDisplayDRM(fd);
    int majorVersion, minorVersion;
    VAStatus vaStatus = vaInitialize(vadisplay, &majorVersion, &minorVersion);
    if (vaStatus != VA_STATUS_SUCCESS) {
        ERROR("va init failed, status =  %d", vaStatus);
        close(fd);
        return display;
    }
    display.reset(new VADisplay(vadisplay), VADisplayDeleter(fd));
    return display;
}

bool DecodeOutputFile::init()
{
    m_vaDisplay = createVADisplay();
    if (!m_vaDisplay)
        return false;
    return DecodeOutput::init();
}

bool DecodeOutputFile::output(const SharedPtr<VideoFrame>& frame)
{
    DecodeOutput::setVideoSize(frame->crop.width, frame->crop.height);

    if (!m_ofs.is_open()) {
        if (m_outputFile.empty()) {
            ERROR("please enter an output file name!");
        }
        m_ofs.open(m_outputFile.c_str(), std::ofstream::out | std::ofstream::trunc);
        if (!m_ofs) {
            ERROR("fail to open output file: %s", m_outputFile.c_str());
            return false;
        }
        CPPPRINT("output file: " << m_outputFile.c_str());
    }

    if (!writeNV12ToFile((VASurfaceID)frame->surface, m_width, m_height))
        return false;
    return true;
}

bool DecodeOutputFile::writeNV12ToFile(VASurfaceID surface, int w, int h)
{
    if (!m_ofs.is_open()) {
        ERROR("No output file for NV12.\n");
        return false;
    }

    VAImage image;
    VAStatus status = vaDeriveImage(*m_vaDisplay, surface, &image);
    if (status != VA_STATUS_SUCCESS) {
        ERROR("vaDeriveImage failed = %d\n", status);
        return false;
    }
    uint32_t byteWidth[3], byteHeight[3], planes;
    planes = 2; //the planes of NV12.
    if (!getPlaneResolution_NV12(w, h, byteWidth, byteHeight)) {
        return false;
    }
    char* buf;
    status = vaMapBuffer(*m_vaDisplay, image.buf, (void**)&buf);
    if (status != VA_STATUS_SUCCESS) {
        vaDestroyImage(*m_vaDisplay, image.image_id);
        ERROR("vaMapBuffer failed = %d", status);
        return false;
    }
    bool ret = true;
    for (uint32_t i = 0; i < planes; i++) {
        char* ptr = buf + image.offsets[i];
        int w = byteWidth[i];
        for (uint32_t j = 0; j < byteHeight[i]; j++) {
            if (!m_ofs.write(reinterpret_cast<const char*>(ptr), w).good()) {
                goto out;
            }
            ptr += image.pitches[i];
        }
    }
out:
    vaUnmapBuffer(*m_vaDisplay, image.buf);
    vaDestroyImage(*m_vaDisplay, image.image_id);
    return ret;
}

bool DecodeOutputFile::getPlaneResolution_NV12(uint32_t pixelWidth, uint32_t pixelHeight, uint32_t byteWidth[3], uint32_t byteHeight[3])
{
    int w = pixelWidth;
    int h = pixelHeight;
    uint32_t* width = byteWidth;
    uint32_t* height = byteHeight;
    //NV12 is special since it  need add one for width[1] when w is odd
    {
        width[0] = w;
        height[0] = h;
        width[1] = w + (w & 1);
        height[1] = (h + 1) >> 1;
        return true;
    }
}

#ifdef __ENABLE_X11__
class DecodeOutputX11 : public DecodeOutput {
public:
    DecodeOutputX11();
    virtual ~DecodeOutputX11();
    bool init();
    bool output(const SharedPtr<VideoFrame>& frame);

protected:
    virtual bool setVideoSize(uint32_t width, uint32_t height);
    bool createX11Display();

    Display* m_display;
    Window m_window;
};

bool DecodeOutputX11::setVideoSize(uint32_t width, uint32_t height)
{
    if (m_window) {
        //todo, resize window;
    }
    else {
        DefaultScreen(m_display);

        XSetWindowAttributes x11WindowAttrib;
        x11WindowAttrib.event_mask = ExposureMask;
        m_window = XCreateWindow(m_display, DefaultRootWindow(m_display),
            0, 0, width, height, 0, CopyFromParent, InputOutput,
            CopyFromParent, CWEventMask, &x11WindowAttrib);
        XMapWindow(m_display, m_window);

        // If we allow vaPutSurface to be called before the window is exposed
        // then those frames will not get displayed on the window.  Thus, wait
        // for the Expose event from X before we return.
        XEvent e;
        while (true) {
            XNextEvent(m_display, &e);
            if (e.type == Expose)
                break;
        }
    }
    XSync(m_display, false);
    {
        DEBUG("m_window=%lu", m_window);
        XWindowAttributes wattr;
        XGetWindowAttributes(m_display, m_window, &wattr);
    }
    return DecodeOutput::setVideoSize(width, height);
}

bool DecodeOutputX11::createX11Display()
{
    SharedPtr<VADisplay> display;

    m_display = XOpenDisplay(NULL);
    if (!m_display) {
        ERROR("Failed to XOpenDisplay for DecodeOutputX11");
        return false;
    }

    VADisplay vaDisplay = vaGetDisplay(m_display);
    int major, minor;
    VAStatus status = vaInitialize(vaDisplay, &major, &minor);
    if (!checkVaapiStatus(status, "vaInitialize"))
        return false;
    m_vaDisplay.reset(new VADisplay(vaDisplay), VADisplayTerminator());

    return true;
}

bool DecodeOutputX11::init()
{
    return createX11Display() && DecodeOutput::init();
}

DecodeOutputX11::DecodeOutputX11()
    : m_display(NULL)
    , m_window(0)
{
}

DecodeOutputX11::~DecodeOutputX11()
{
    m_vaDisplay.reset();
    if (m_window)
        XDestroyWindow(m_display, m_window);
    if (m_display)
        XCloseDisplay(m_display);
}

bool DecodeOutputX11::output(const SharedPtr<VideoFrame>& frame)
{
    if (!setVideoSize(frame->crop.width, frame->crop.height))
        return false;

    VAStatus status = vaPutSurface(*m_vaDisplay, (VASurfaceID)frame->surface,
        m_window, frame->crop.x, frame->crop.y, frame->crop.width, frame->crop.height,
        0, 0, frame->crop.width, frame->crop.height,
        NULL, 0, 0);
    return checkVaapiStatus(status, "vaPutSurface");
}
#endif

#ifdef __ENABLE_WAYLAND__
struct display {
    SharedPtr<wl_display> display;
    SharedPtr<wl_compositor> compositor;
    SharedPtr<wl_shell> shell;
    SharedPtr<wl_shell_surface> shell_surface;
    SharedPtr<wl_surface> surface;
};

class DecodeOutputWayland : public DecodeOutput {
public:
    DecodeOutputWayland();
    virtual ~DecodeOutputWayland();
    bool output(const SharedPtr<VideoFrame>& frame);
    bool init();

protected:
    virtual bool setVideoSize(uint32_t width, uint32_t height);
    bool createWaylandDisplay();
    static void registryHandle(void* data, struct wl_registry* registry,
        uint32_t id, const char* interface, uint32_t version);
    static void frameRedrawCallback(void* data, struct wl_callback* callback, uint32_t time);
    bool ensureWindow(unsigned int width, unsigned int height);
    bool vaPutSurfaceWayland(VASurfaceID surface,
        const VARectangle* srcRect, const VARectangle* dstRect);
    bool m_redrawPending;
    struct display m_waylandDisplay;
};

void DecodeOutputWayland::registryHandle(
    void* data,
    struct wl_registry* registry,
    uint32_t id,
    const char* interface,
    uint32_t version)
{
    struct display* d = (struct display*)data;

    if (strcmp(interface, "wl_compositor") == 0)
        d->compositor.reset((struct wl_compositor*)wl_registry_bind(registry, id,
                                &wl_compositor_interface, 1),
            wl_compositor_destroy);
    else if (strcmp(interface, "wl_shell") == 0)
        d->shell.reset((struct wl_shell*)wl_registry_bind(registry, id,
                           &wl_shell_interface, 1),
            wl_shell_destroy);
}

void DecodeOutputWayland::frameRedrawCallback(void* data,
    struct wl_callback* callback, uint32_t time)
{
    *(bool*)data = false;
    wl_callback_destroy(callback);
}

bool DecodeOutputWayland::ensureWindow(unsigned int width, unsigned int height)
{
    struct display* const d = &m_waylandDisplay;

    if (!d->surface) {
        d->surface.reset(wl_compositor_create_surface(d->compositor.get()), wl_surface_destroy);
        if (!d->surface)
            return false;
    }

    if (!d->shell_surface) {
        d->shell_surface.reset(wl_shell_get_shell_surface(d->shell.get(), d->surface.get()),
            wl_shell_surface_destroy);
        if (!d->shell_surface)
            return false;
        wl_shell_surface_set_toplevel(d->shell_surface.get());
    }
    return DecodeOutput::setVideoSize(width, height);
}

bool DecodeOutputWayland::vaPutSurfaceWayland(VASurfaceID surface,
    const VARectangle* srcRect,
    const VARectangle* dstRect)
{
    VAStatus vaStatus;
    struct wl_buffer* buffer;
    struct wl_callback* callback;
    struct display* const d = &m_waylandDisplay;
    struct wl_callback_listener frame_callback_listener = { frameRedrawCallback };

    if (m_redrawPending) {
        wl_display_flush(d->display.get());
        while (m_redrawPending) {
            wl_display_dispatch(d->display.get());
        }
    }

    if (!ensureWindow(dstRect->width, dstRect->height))
        return false;

    vaStatus = vaGetSurfaceBufferWl(*m_vaDisplay, surface, VA_FRAME_PICTURE, &buffer);
    if (vaStatus != VA_STATUS_SUCCESS)
        return false;

    wl_surface_attach(d->surface.get(), buffer, 0, 0);
    wl_surface_damage(d->surface.get(), dstRect->x,
        dstRect->y, dstRect->width, dstRect->height);
    wl_display_flush(d->display.get());
    m_redrawPending = true;
    callback = wl_surface_frame(d->surface.get());
    wl_callback_add_listener(callback, &frame_callback_listener, &m_redrawPending);
    wl_surface_commit(d->surface.get());
    return true;
}

bool DecodeOutputWayland::output(const SharedPtr<VideoFrame>& frame)
{
    VARectangle srcRect, dstRect;
    if (!setVideoSize(frame->crop.width, frame->crop.height))
        return false;

    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.width = frame->crop.width;
    srcRect.height = frame->crop.height;

    dstRect.x = frame->crop.x;
    dstRect.y = frame->crop.y;
    dstRect.width = frame->crop.width;
    dstRect.height = frame->crop.height;
    return vaPutSurfaceWayland((VASurfaceID)frame->surface, &srcRect, &dstRect);
}

bool DecodeOutputWayland::setVideoSize(uint32_t width, uint32_t height)
{
    return ensureWindow(width, height);
}

bool DecodeOutputWayland::createWaylandDisplay()
{
    int major, minor;
    SharedPtr<VADisplay> display;
    struct display* d = &m_waylandDisplay;
    struct wl_registry_listener registry_listener = {
        DecodeOutputWayland::registryHandle,
        NULL,
    };

    d->display.reset(wl_display_connect(NULL), wl_display_disconnect);
    if (!d->display) {
        return false;
    }
    wl_display_set_user_data(d->display.get(), d);
    struct wl_registry* registry = wl_display_get_registry(d->display.get());
    wl_registry_add_listener(registry, &registry_listener, d);
    wl_display_dispatch(d->display.get());
    VADisplay vaDisplayHandle = vaGetDisplayWl(d->display.get());
    VAStatus status = vaInitialize(vaDisplayHandle, &major, &minor);
    if (!checkVaapiStatus(status, "vaInitialize"))
        return false;
    m_vaDisplay.reset(new VADisplay(vaDisplayHandle), VADisplayTerminator());
    return true;
}

bool DecodeOutputWayland::init()
{
    return createWaylandDisplay() && DecodeOutput::init();
}

DecodeOutputWayland::DecodeOutputWayland()
    : m_redrawPending(false)
{
}

DecodeOutputWayland::~DecodeOutputWayland()
{
    m_vaDisplay.reset();
}
#endif

DecodeOutput* DecodeOutput::create(int renderMode, const char* outputFile)
{
    DecodeOutput* output;
    switch (renderMode) {
    case FILE_OUTPUT:
        output = new DecodeOutputFile(outputFile);
        break;
#ifdef __ENABLE_X11__
    case X11_RENDERING:
        output = new DecodeOutputX11();
        break;
#endif
#ifdef __ENABLE_WAYLAND__
    case WAYLAND_RENDERING:
        output = new DecodeOutputWayland();
        break;
#endif
    default:
        ERROR("renderMode:%d, do not support this render mode\n", renderMode);
        return NULL;
    }
    if (!output->init())
        ERROR("DecodeOutput init failed\n");
    return output;
}

class YamiPlayer {
public:
    uint64_t getFrameNum() { return m_frameNum; }
    bool init(int argc, char** argv)
    {
        clearParameter(&m_parameters);
        if (!processCmdLine(argc, argv, &m_parameters))
            return false;

        if (m_parameters.readSize)
            m_input.reset(DecodeInput::create(m_parameters.inputFile.c_str(), m_parameters.readSize));
        else
            m_input.reset(DecodeInput::create(m_parameters.inputFile.c_str()));
        if (!m_input) {
            ERROR("failed to open %s", m_parameters.inputFile.c_str());
            return false;
        }
        INFO("input initialization finished with file: %s", m_parameters.inputFile.c_str());

        //init decoder
        m_decoder.reset(createVideoDecoder(m_input->getMimeType()), releaseVideoDecoder);
        if (!m_decoder) {
            ERROR("failed create decoder for %s", m_input->getMimeType());
            return false;
        }

        //init output
        m_output.reset(DecodeOutput::create(m_parameters.outputMode, m_parameters.outputFile.c_str()));
        if (!m_output) {
            ERROR("DecodeOutput::create failed.\n");
            return false;
        }
        m_nativeDisplay = m_output->nativeDisplay();
        if (!m_nativeDisplay) {
            ERROR("DecodeTest init failed.\n");
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

        configBuffer.enableLowLatency = m_parameters.enableLowLatency;
        if (m_parameters.surfaceNumber) {
            //How many surfaces for decoding;
            //VP9 supporting;
            //AVC unsupporting;
            configBuffer.flag |= HAS_SURFACE_NUMBER;
            configBuffer.surfaceNumber = m_parameters.surfaceNumber;
        }
        if (m_parameters.extraSurfaceNumber) {
            //How many extra surfaces for decoding;
            //VP9, AVC supporting;
            configBuffer.extraSurfaceNum = m_parameters.extraSurfaceNumber;
        }

        Decode_Status status = m_decoder->start(&configBuffer);
        assert(status == DECODE_SUCCESS);

        VideoDecodeBuffer inputBuffer;
        while ((!m_parameters.outputFrameNumber) || (m_parameters.outputFrameNumber > 0 && m_frameNum < m_parameters.outputFrameNumber)) {
            SharedPtr<VideoFrame> frame = m_decoder->getOutput();
            if (frame) {
                if (m_parameters.getFirstFrame) {
                    m_frameNum++;
                    break;
                }
                if (m_output->output(frame)) {
                    m_frameNum++;
                    continue;
                }
                else
                    return false;
            }
            else if (m_eos)
                break;

            if (m_input->getNextDecodeUnit(inputBuffer)) {
                status = m_decoder->decode(&inputBuffer);
                if (DECODE_FORMAT_CHANGE == status) {
                    //drain the former buffers
                    while ((!m_parameters.outputFrameNumber) || (m_parameters.outputFrameNumber > 0 && m_frameNum < m_parameters.outputFrameNumber)) {
                        frame = m_decoder->getOutput();
                        if (frame) {
                            if (m_output->output(frame)) {
                                m_frameNum++;
                                continue;
                            }
                            else
                                return false;
                        }
                        else
                            break;
                    }

                    //const VideoFormatInfo* formatInfo = m_decoder->getFormatInfo();
                    status = m_decoder->decode(&inputBuffer);
                }
                if (status != DECODE_SUCCESS) {
                    ERROR("decode error status = %d", status);
                    break;
                }
            }
            else {
                inputBuffer.data = NULL;
                inputBuffer.size = 0;
                m_decoder->decode(&inputBuffer);
                m_eos = true;
            }
        }
        m_decoder->stop();
        return true;
    }
    YamiPlayer()
    {
        m_parameters.inputFile.clear();
        m_parameters.outputFile.clear();
        m_parameters.outputFrameNumber = 0;
        m_parameters.outputMode = FILE_OUTPUT;
        m_parameters.getFirstFrame = false;

        m_eos = false;
        m_frameNum = 0;
    }
    ~YamiPlayer()
    {
        m_decoder.reset();
    }

private:
    SharedPtr<NativeDisplay> m_nativeDisplay;
    SharedPtr<IVideoDecoder> m_decoder;
    SharedPtr<DecodeInput> m_input;
    bool m_eos;
    std::ofstream m_ofs;
    uint64_t m_frameNum;
    YamiPlayerParameter m_parameters;
    SharedPtr<DecodeOutput> m_output;
};

int main(int argc, char** argv)
{
    YamiPlayer player;
    if (!player.init(argc, argv)) {
        ERROR("init player failed with %s", argv[1]);
        return -1;
    }
    if (!player.run()) {
        ERROR("run simple player failed");
        return -1;
    }
    CPPPRINT("get frame number: " << player.getFrameNum());
    return 0;
}
