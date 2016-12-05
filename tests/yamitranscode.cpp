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

#include "vppinputdecode.h"
#include "vppinputoutput.h"
#include "vppoutputencode.h"
#include "encodeinput.h"
#include "tests/vppinputasync.h"
#include "common/log.h"
#include <Yami.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

using namespace YamiMediaCodec;

static void print_help(const char* app)
{
    printf("%s <options>\n", app);
    printf("   -i <source filename> load a raw yuv file or a compressed video file\n");
    printf("   -W <width> -H <height>\n");
    printf("   -o <coded file> optional\n");
    printf("   -b <bitrate: kbps> optional\n");
    printf("   -f <frame rate> optional\n");
    printf("   -c <codec: HEVC|AVC|VP8|JPEG>\n");
    printf("   -s <fourcc: I420|NV12|IYUV|YV12>\n");
    printf("   -N <number of frames to encode(camera default 50), useful for camera>\n");
    printf("   -t <AVC scalability temporal layer number  (default 1)> optional\n");
    printf("   --qp <initial qp> optional\n");
    printf("   --rcmode <CBR|CQP> optional\n");
    printf("   --ipperiod <0 (I frame only) | 1 (I and P frames) | N (I,P and B frames, B frame number is N-1)> optional\n");
    printf("   --intraperiod <Intra frame period(default 30)> optional\n");
    printf("   --refnum <number of referece frames(default 1)> optional\n");
    printf("   --idrinterval <AVC/HEVC IDR frame interval(default 0)> optional\n");
    printf("   --disable-cabac <AVC is to use CABAC or not, for Main and High Profile, default is enabled\n");
    printf("   --enable-dct8x8 <AVC is to use DCT8x8 or not, for High Profile, default is disabled\n");
    printf("   --disable-deblock <AVC is to use Deblock or not, default is enabled\n");
    printf("   --deblockalphadiv2 <AVC Alpha offset of debloking filter divided 2 (default 2)> optional\n");
    printf("   --deblockbetadiv2 <AVC Beta offset of debloking filter divided 2 (default 2)> optional\n");
    printf("   --qpip <qp difference between adjacent I/P (default 0)> optional\n");
    printf("   --qpib <qp difference between adjacent I/B (default 0)> optional\n");
    printf("   --priorityid <AVC priority_id of prefix nal unit (default 0)> optional\n");
    printf("   --ow <output width> optional\n");
    printf("   --oh <output height> optional\n");
    printf("   --btl0 <svc-t layer 0 bitrate: kbps> optional\n");
    printf("   --btl1 <svc-t layer 1 bitrate: kbps > optional\n");
    printf("   --btl2 <svc-t layer 2 bitrate: kbps> optional\n");
    printf("   --btl3 <svc-t layer 3 bitrate: kbps> optional\n");
    printf("   --lowpower <Enable AVC low power mode (default 0, Disabled)> optional\n");
    printf("   --startup-size <how many frames preload to memory> for performance mesurement only\n");
    printf("   VP9 encoder specific options:\n");
    printf("   --refmode <VP9 Reference frames mode (default 0 last(previous), "
           "gold/alt (previous key frame) | 1 last (previous) gold (one before "
           "last) alt (one before gold)> optional\n");
}

static VideoRateControl string_to_rc_mode(char *str)
{
    VideoRateControl rcMode;

    if (!strcasecmp (str, "CBR"))
        rcMode = RATE_CONTROL_CBR;
    else if (!strcasecmp (str, "CQP"))
        rcMode = RATE_CONTROL_CQP;
    else {
        printf("Unsupport  RC mode\n");
        rcMode = RATE_CONTROL_NONE;
    }
    return rcMode;
}

static bool processCmdLine(int argc, char *argv[], TranscodeParams& para)
{
    char opt;
    const struct option long_opts[] = {
        { "help", no_argument, NULL, 'h' },
        { "qp", required_argument, NULL, 0 },
        { "rcmode", required_argument, NULL, 0 },
        { "ipperiod", required_argument, NULL, 0 },
        { "intraperiod", required_argument, NULL, 0 },
        { "refnum", required_argument, NULL, 0 },
        { "idrinterval", required_argument, NULL, 0 },
        { "disable-cabac", no_argument, NULL, 0 },
        { "enable-dct8x8", no_argument, NULL, 0 },
        { "disable-deblock", no_argument, NULL, 0 },
        { "deblockalphadiv2", required_argument, NULL, 0 },
        { "deblockbetadiv2", required_argument, NULL, 0 },
        { "qpip", required_argument, NULL, 0 },
        { "qpib", required_argument, NULL, 0 },
        { "priorityid", required_argument, NULL, 0 },
        { "refmode", required_argument, NULL, 0 },
        { "ow", required_argument, NULL, 0 },
        { "oh", required_argument, NULL, 0 },
        { "btl0", required_argument, NULL, 0 },
        { "btl1", required_argument, NULL, 0 },
        { "btl2", required_argument, NULL, 0 },
        { "btl3", required_argument, NULL, 0 },
        { "lowpower", no_argument, NULL, 0 },
        { "startup-size", required_argument, NULL, 0 },
        { NULL, no_argument, NULL, 0 }
    };
    int option_index;

    if (argc < 2) {
        fprintf(stderr, "can not encode without option, please type 'yamitranscode -h' to help\n");
        return false;
    }

    while ((opt = getopt_long_only(argc, argv, "W:H:b:f:c:s:i:o:N:h:t:", long_opts,&option_index)) != -1)
    {
        switch (opt) {
        case 'h':
        case '?':
            print_help (argv[0]);
            return false;
        case 'i':
            para.inputFileName = optarg;
            break;
        case 'o':
            para.outputFileName = optarg;
            break;
        case 'W':
            para.iWidth = atoi(optarg);
            break;
        case 'H':
            para.iHeight = atoi(optarg);
            break;
        case 'b':
            para.m_encParams.bitRate = atoi(optarg) * 1024;//kbps to bps
            break;
        case 'f':
            para.m_encParams.fps = atoi(optarg);
            break;
        case 'c':
            para.m_encParams.codec = optarg;
            break;
        case 's':
            if (strlen(optarg) == 4)
                para.fourcc = VA_FOURCC(optarg[0], optarg[1], optarg[2], optarg[3]);
            break;
        case 'N':
            para.frameCount = atoi(optarg);
            break;
        case 't':
            para.m_encParams.temporalLayerNum = atoi(optarg);
            break;
        case 0:
             switch (option_index) {
                case 1:
                    para.m_encParams.initQp = atoi(optarg);
                    break;
                case 2:
                    para.m_encParams.rcMode = string_to_rc_mode(optarg);
                    break;
                case 3:
                    para.m_encParams.ipPeriod = atoi(optarg);
                    break;
                case 4:
                    para.m_encParams.intraPeriod = atoi(optarg);
                    break;
                case 5:
                    para.m_encParams.numRefFrames= atoi(optarg);
                    break;
                case 6:
                    para.m_encParams.idrInterval = atoi(optarg);
                    break;
                case 7:
                    para.m_encParams.enableCabac = false;
                    break;
                case 8:
                    para.m_encParams.enableDct8x8 = true;
                    break;
                case 9:
                    para.m_encParams.enableDeblockFilter = false;
                    break;
                case 10:
                    para.m_encParams.deblockAlphaOffsetDiv2 = atoi(optarg);
                    break;
                case 11:
                    para.m_encParams.deblockBetaOffsetDiv2 = atoi(optarg);
                    break;
                case 12:
                    para.m_encParams.diffQPIP = atoi(optarg);
                    break;
                case 13:
                    para.m_encParams.diffQPIB = atoi(optarg);
                    break;
                case 14:
                    para.m_encParams.priorityId = atoi(optarg);
                    break;
                case 15:
                    para.m_encParams.m_encParamsVP9.referenceMode = atoi(optarg);
                    break;
                case 16:
                    para.oWidth = atoi(optarg);
                    break;
                case 17:
                    para.oHeight = atoi(optarg);
                    break;
                case 18:
                    para.m_encParams.layerBitRate[0] = atoi(optarg) * 1024;//kbps to bps;
                    break;
                case 19:
                    para.m_encParams.layerBitRate[1] = atoi(optarg) * 1024;//kbps to bps;
                    break;
                case 20:
                    para.m_encParams.layerBitRate[2] = atoi(optarg) * 1024;//kbps to bps;
                    break;
                case 21:
                    para.m_encParams.layerBitRate[3] = atoi(optarg) * 1024;//kbps to bps;
                    break;
                case 22:
                    para.m_encParams.enableLowPower = true;
                    break;
                case 23:
                    para.startupSize = atoi(optarg);
                    break;
            }
        }
    }
    if (optind < argc) {
        int indexOpt = optind;
        printf("unrecognized option: ");
        while (indexOpt < argc)
            printf("%s ", argv[indexOpt++]);
        printf("\n");
        print_help(argv[0]);
        return false;
    }

    if (para.inputFileName.empty()) {
        fprintf(stderr, "can not encode without input file\n");
        return false;
    }
    if (para.outputFileName.empty())
        para.outputFileName = "test.264";

    if ((para.m_encParams.rcMode == RATE_CONTROL_CBR) && (para.m_encParams.bitRate <= 0)) {
        fprintf(stderr, "please make sure bitrate is positive when CBR mode\n");
        return false;
    }

    if (!strncmp(para.inputFileName.c_str(), "/dev/video", strlen("/dev/video")) && !para.frameCount)
        para.frameCount = 50;

    if (!para.oWidth)
        para.oWidth = para.iWidth;
    if (!para.oHeight)
        para.oHeight = para.iHeight;

    return true;
}

SharedPtr<VppInput> createInput(TranscodeParams& para, const SharedPtr<VADisplay>& display)
{
    SharedPtr<VppInput> input(VppInput::create(para.inputFileName.c_str(), 0, para.iWidth, para.iHeight));
    if (!input) {
        ERROR("creat input failed");
        return input;
    }
    SharedPtr<VppInputFile> inputFile = DynamicPointerCast<VppInputFile>(input);
    if (inputFile) {
        SharedPtr<FrameReader> reader(new VaapiFrameReader(display));
        const static size_t allocatorExtra = 5;
        SharedPtr<FrameAllocator> alloctor(new PooledFrameAllocator(display, para.startupSize + allocatorExtra));
        if(!inputFile->config(alloctor, reader)) {
            ERROR("config input failed");
            input.reset();
        }
    }
    else {
        if (para.startupSize) {
            ERROR("curretly, startup size is only work for yuv input");
        }
    }
    SharedPtr<VppInputDecode> inputDecode = DynamicPointerCast<VppInputDecode>(input);
    if (inputDecode) {
        NativeDisplay nativeDisplay;
        nativeDisplay.type = NATIVE_DISPLAY_VA;
        nativeDisplay.handle = (intptr_t)*display;
        if(!inputDecode->config(nativeDisplay)) {
            ERROR("config input decode failed");
            input.reset();
        }
    }
    if (input) {
        input = VppInputAsync::create(input, 3, para.startupSize); //make input in other thread.
    }
    return input;
}

SharedPtr<VppOutput> createOutput(TranscodeParams& para, const SharedPtr<VADisplay>& display)
{
    SharedPtr<VppOutput> output = VppOutput::create(
        para.outputFileName.c_str(), para.fourcc, para.oWidth, para.oHeight,
        para.m_encParams.codec.c_str());
    SharedPtr<VppOutputFile> outputFile = DynamicPointerCast<VppOutputFile>(output);
    if (outputFile) {
        SharedPtr<FrameWriter> writer(new VaapiFrameWriter(display));
        if (!outputFile->config(writer)) {
            ERROR("config writer failed");
            output.reset();
        }
        return output;
    }
    SharedPtr<VppOutputEncode> outputEncode = DynamicPointerCast<VppOutputEncode>(output);
    if (outputEncode) {
        NativeDisplay nativeDisplay;
        nativeDisplay.type = NATIVE_DISPLAY_VA;
        nativeDisplay.handle = (intptr_t)*display;
        if (!outputEncode->config(nativeDisplay, &para.m_encParams)) {
            ERROR("config output encode failed");
            output.reset();
        }
        return output;
    }
    return output;
}

SharedPtr<FrameAllocator> createAllocator(const SharedPtr<VppOutput>& output, const SharedPtr<VADisplay>& display, int32_t extraSize)
{
    uint32_t fourcc;
    int width, height;
    SharedPtr<FrameAllocator> allocator(new PooledFrameAllocator(display, std::max(extraSize, 5)));
    if (!output->getFormat(fourcc, width, height)
        || !allocator->setFormat(fourcc, width,height)) {
        allocator.reset();
        ERROR("get Format failed");
    }
    return allocator;
}

class TranscodeTest
{
public:
    bool init(int argc, char* argv[])
    {
        if (!processCmdLine(argc, argv, m_cmdParam))
            return false;

        m_display = createVADisplay();
        if (!m_display) {
            printf("create display failed");
            return false;
        }
        if (!createVpp()) {
            ERROR("create vpp failed");
            return false;
        }
        m_input = createInput(m_cmdParam, m_display);
        m_output =  createOutput(m_cmdParam, m_display);
        if (!m_input || !m_output) {
            ERROR("create input or output failed");
            return false;
        }
        m_allocator = createAllocator(m_output, m_display, m_cmdParam.m_encParams.ipPeriod);
        return bool(m_allocator);
    }

    bool run()
    {

        SharedPtr<VideoFrame> src;
        FpsCalc fps;
        uint32_t count = 0;
        while (m_input->read(src)) {
            SharedPtr<VideoFrame> dest = m_allocator->alloc();
            if (!dest) {
                ERROR("failed to get output frame");
                break;
            }
//disable scale for performance measure
//#define DISABLE_SCALE 1
#ifndef DISABLE_SCALE
            YamiStatus status = m_vpp->process(src, dest);
            if (status != YAMI_SUCCESS) {
                ERROR("failed to scale yami return %d", status);
                break;
            }
#else
            dest = src;
#endif

            if(!m_output->output(dest))
                break;
            count++;
            fps.addFrame();
            if(count >= m_cmdParam.frameCount)
                break;
        }
        src.reset();
        m_output->output(src);

        fps.log();

        return true;
    }
private:
    bool createVpp()
    {
        NativeDisplay nativeDisplay;
        nativeDisplay.type = NATIVE_DISPLAY_VA;
        nativeDisplay.handle = (intptr_t)*m_display;
        m_vpp.reset(createVideoPostProcess(YAMI_VPP_SCALER), releaseVideoPostProcess);
        return m_vpp->setNativeDisplay(nativeDisplay) == YAMI_SUCCESS;
    }

    SharedPtr<VADisplay> m_display;
    SharedPtr<VppInput> m_input;
    SharedPtr<VppOutput> m_output;
    SharedPtr<FrameAllocator> m_allocator;
    SharedPtr<IVideoPostProcess> m_vpp;
    TranscodeParams m_cmdParam;
};

int main(int argc, char** argv)
{

    TranscodeTest trans;
    if (!trans.init(argc, argv)) {
        ERROR("init transcode with command line parameters failed");
        return -1;
    }
    if (!trans.run()){
        ERROR("run transcode failed");
        return -1;
    }
    printf("transcode done\n");
    return  0;

}
