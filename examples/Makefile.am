bin_PROGRAMS =

if ENABLE_X11
bin_PROGRAMS += simpleplayer

if BUILD_OCL_BLENDER
bin_PROGRAMS += blend
endif

endif

if ENABLE_DMABUF
bin_PROGRAMS += grid
endif

AM_CFLAGS = \
	-I../tests \
	$(LIBYAMI_CFLAGS) \
	$(LIBVA_CFLAGS) \
	$(NULL)

DECODE_INPUT_SOURCES = \
	../tests/decodeinput.cpp \
	$(NULL)

if ENABLE_AVFORMAT
DECODE_INPUT_SOURCES += ../tests/decodeinputavformat.cpp
AM_CFLAGS += -D__STDC_CONSTANT_MACROS \
	$(LIBAVFORMAT_CFLAGS)
endif

VPP_INPUT_SOURCES = \
	$(DECODE_INPUT_SOURCES) \
	../tests/vppinputdecode.cpp \
	../tests/vppinputasync.cpp \
	../tests/vppoutputencode.cpp \
	../tests/encodeinput.cpp \
	../tests/encodeInputDecoder.cpp \
	../tests/encodeInputCamera.cpp \
	$(NULL)

AM_CPPFLAGS = $(AM_CFLAGS)
YAMI_COMMON_LIBS = \
	$(LIBVA_LIBS) \
	$(LIBVA_DRM_LIBS) \
	$(LIBYAMI_LIBS) \
	$(NULL)

if ENABLE_X11
YAMI_COMMON_LIBS += $(LIBVA_X11_LIBS) -lX11
endif

YAMI_DECODE_LIBS = \
	$(YAMI_COMMON_LIBS) \
	$(NULL)

if ENABLE_TESTS_GLES
YAMI_DECODE_LIBS += $(LIBEGL_LIBS) $(LIBGLES2_LIBS)
endif

if ENABLE_AVFORMAT
YAMI_DECODE_LIBS += $(LIBAVFORMAT_LIBS)
endif

YAMI_ENCODE_LIBS = \
	$(YAMI_DECODE_LIBS) \
	$(NULL)

VPP_INPUT_LIBS = \
	$(LIBYAMI_LIBS) \
	$(YAMI_ENCODE_LIBS) \
	$(NULL)

VPP_INPUT_CFLAGS = \
	$(LIBVA_CFLAGS) \
	$(NULL)

if ENABLE_X11
simpleplayer_LDADD = $(YAMI_DECODE_LIBS)
simpleplayer_LDFLAGS = $(LIBYAMI_CFLAGS)
simpleplayer_SOURCES = simpleplayer.cpp $(DECODE_INPUT_SOURCES)

blend_LDADD = $(VPP_INPUT_LIBS) -lpthread
blend_LDFLAGS = $(VPP_INPUT_LDFLAGS) $(LIBYAMI_CFLAGS)
blend_SOURCES	= blend.cpp $(VPP_INPUT_SOURCES)
endif

if ENABLE_DMABUF
grid_LDADD = $(VPP_INPUT_LIBS) $(LIBDRM_LIBS) -lpthread
grid_CPPFLAGS = $(LIBYAMI_CFLAGS) $(LIBDRM_CFLAGS)
grid_LDFLAGS = $(VPP_INPUT_LDFLAGS) $(LIBYAMI_CFLAGS) $(LIBDRM_CFLAGS)
grid_SOURCES = grid.cpp $(VPP_INPUT_SOURCES)
endif

#autotools distclean will try to do rm -rf ../tests/.deps which results
#on failure, instead specify instructions here
distclean:
	-rm -rf ./.deps
	-rm -rf ./.libs
	-rm -rf ./*.o
	-rm -rf Makefile
	-test -z "" || rm -f
	-rm -rf ${bin_PROGRAMS}
