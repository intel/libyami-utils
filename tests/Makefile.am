bin_PROGRAMS = yamidecode yamiencode yamivpp yamitranscode
if ENABLE_V4L2
bin_PROGRAMS += v4l2encode v4l2decode
endif

#to compile within yocto
extra_includes = \
        -I$(top_srcdir) \
        $(NULL)

YAMI_COMMON_CFLAGS = \
	$(LIBVA_CFLAGS) \
	$(LIBYAMI_CFLAGS) \
	$(extra_includes) \
	$(NULL)

DECODE_INPUT_SOURCES = \
	decodeinput.cpp \
	$(NULL)

YAMI_COMMON_LIBS = \
	$(LIBVA_LIBS) \
	$(LIBVA_DRM_LIBS) \
	$(LIBYAMI_LIBS) \
	$(NULL)

YAMI_DECODE_LIBS = \
	$(YAMI_COMMON_LIBS) \
	$(NULL)

if ENABLE_V4L2
YAMI_COMMON_CFLAGS += \
	$(LIBYAMIV4L2_CFLAGS) \
	$(NULL)

YAMI_COMMON_LIBS += \
	$(LIBYAMIV4L2_LIBS) \
	$(NULL)
endif

if ENABLE_AVFORMAT
DECODE_INPUT_SOURCES += decodeinputavformat.cpp
YAMI_COMMON_CFLAGS += -D__STDC_CONSTANT_MACROS \
	$(LIBAVFORMAT_CFLAGS)
YAMI_DECODE_LIBS += $(LIBAVFORMAT_LIBS)
endif

if ENABLE_X11
YAMI_COMMON_LIBS += $(LIBVA_X11_LIBS) -lX11
endif

if ENABLE_WAYLAND
YAMI_COMMON_LIBS += $(LIBVA_WAYLAND_LIBS)
endif
if ENABLE_EGL
YAMI_DECODE_LIBS += $(LIBEGL_LIBS) $(LIBGLES2_LIBS)
endif

if HAVE_LIBBSD
YAMI_DECODE_LIBS += $(LIBBSD_LIBS)
endif

YAMI_ENCODE_LIBS = \
	$(YAMI_DECODE_LIBS) \
	$(NULL)

V4L2_DECODE_LIBS = \
	$(YAMI_DECODE_LIBS) \
	$(NULL)

V4L2_DECODE_LIBS += $(LIBEGL_LIBS) $(LIBGLES2_LIBS)

V4L2_ENCODE_LIBS = \
	$(YAMI_ENCODE_LIBS) \
	$(NULL)

CAPI_DECODE_LIBS = \
	$(YAMI_DECODE_LIBS) \
	$(NULL)

CAPI_ENCODE_LIBS = \
	$(YAMI_ENCODE_LIBS) \
	$(NULL)

YAMI_VPP_LIBS = \
	$(YAMI_DECODE_LIBS) \
	$(YAMI_ENCODE_LIBS) \
	$(NULL)

YAMI_VPP_CFLAGS = \
	$(LIBVA_CFLAGS) \
	$(NULL)

yamidecode_LDADD    = $(YAMI_VPP_LIBS)
yamidecode_CPPFLAGS = $(YAMI_COMMON_CFLAGS) $(AM_CPPFLAGS)
yamidecode_SOURCES  = decode.cpp decodehelp.cpp $(DECODE_INPUT_SOURCES) decodeoutput.cpp vppinputoutput.cpp vppinputdecode.cpp vppoutputencode.cpp encodeinput.cpp encodeInputCamera.cpp encodeInputDecoder.cpp vppinputdecodecapi.cpp md5.c
if ENABLE_EGL
yamidecode_SOURCES += ../egl/egl_util.c ./egl/gles2_help.c
endif

yamiencode_LDADD    = $(YAMI_ENCODE_LIBS)
yamiencode_CPPFLAGS = $(YAMI_COMMON_CFLAGS) $(AM_CPPFLAGS)
yamiencode_SOURCES  = encode.cpp encodeinput.cpp encodeInputCamera.cpp encodeInputDecoder.cpp $(DECODE_INPUT_SOURCES)

v4l2decode_LDADD   = $(V4L2_DECODE_LIBS)
v4l2decode_CPPFLAGS = $(YAMI_COMMON_CFLAGS) $(AM_CPPFLAGS)
v4l2decode_SOURCES = v4l2decode.cpp V4L2Renderer.cpp V4L2Device.cpp decodehelp.cpp $(DECODE_INPUT_SOURCES)

if ENABLE_EGL
v4l2decode_SOURCES += ./egl/gles2_help.c
endif
v4l2decode_LDADD += -ldl


v4l2encode_LDADD   = $(V4L2_ENCODE_LIBS)
v4l2encode_CPPFLAGS = $(YAMI_COMMON_CFLAGS) $(AM_CPPFLAGS)
v4l2encode_SOURCES = v4l2encode.cpp encodeinput.h encodeinput.cpp encodeInputCamera.cpp encodeInputDecoder.cpp $(DECODE_INPUT_SOURCES)

yamivpp_LDADD    = $(YAMI_VPP_LIBS)
yamivpp_CPPFLAGS = $(YAMI_COMMON_CFLAGS) $(AM_CPPFLAGS)
yamivpp_SOURCES  = vppinputdecode.cpp vppinputoutput.cpp vppoutputencode.cpp  vpp.cpp encodeinput.cpp encodeInputCamera.cpp encodeInputDecoder.cpp $(DECODE_INPUT_SOURCES) vppinputdecodecapi.cpp

yamitranscode_LDADD    = $(YAMI_VPP_LIBS)
yamitranscode_CPPFLAGS = $(YAMI_COMMON_CFLAGS) $(AM_CPPFLAGS)
yamitranscode_LDFLAGS  = -pthread $(AM_LDFLAGS)
yamitranscode_SOURCES  = vppinputdecode.cpp vppinputoutput.cpp vppoutputencode.cpp  yamitranscode.cpp encodeinput.cpp encodeInputCamera.cpp encodeInputDecoder.cpp $(DECODE_INPUT_SOURCES) vppinputasync.cpp vppinputdecodecapi.cpp 

bin_PROGRAMS += yamiinfo
yamiinfo_SOURCES = yamiinfo.cpp
yamiinfo_CPPFLAGS = $(YAMI_COMMON_CFLAGS) $(AM_CPPFLAGS)
yamiinfo_LDADD = $(YAMI_COMMON_LIBS)
yamiinfo_LDFLAGS = -Wl,--no-as-needed \
	$(AM_LDFLAGS) \
	$(NULL)
