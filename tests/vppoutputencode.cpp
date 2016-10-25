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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "vppoutputencode.h"
#include <Yami.h>

EncodeParamsVP9::EncodeParamsVP9()
    : referenceMode(0)
{ /* do nothig*/
}

EncodeParams::EncodeParams()
    : rcMode(RATE_CONTROL_CQP)
    , initQp(26)
    , bitRate(0)
    , fps(30)
    , ipPeriod(1)
    , intraPeriod(30)
    , numRefFrames(1)
    , idrInterval(0)
    , codec("")
    , enableprintFrameLatency(false)
    , FrameLatencyLogFile("Latencylog")
    , enableprintBitRate(false)
    , BitRateLogFile("BitRatelog")
    , enableCabac(true)
    , enableDct8x8(false)
    , enableDeblockFilter(true)
    , deblockAlphaOffsetDiv2(2)
    , deblockBetaOffsetDiv2(2)
    , diffQPIP(0)
    , diffQPIB(0)
    , temporalLayerNum(1)
    , priorityId(0)
    , enableLowPower(false)
{
    memset(layerBitRate, 0, sizeof(layerBitRate));
}

TranscodeParams::TranscodeParams()
    : m_encParams()
    , frameCount(UINT_MAX)
    , iWidth(0)
    , iHeight(0)
    , oWidth(0)
    , oHeight(0)
    , fourcc(0)
{
    /*nothing to do*/
}

bool VppOutputEncode::init(const char* outputFileName, uint32_t fourcc,
    int width, int height, const char* codecName)
{
    if(!width || !height)
        if (!guessResolution(outputFileName, width, height))
            return false;
    m_fourcc = fourcc != YAMI_FOURCC_P010 ? YAMI_FOURCC_NV12 : YAMI_FOURCC_P010;
    m_width = width;
    m_height = height;
    m_output.reset(EncodeOutput::create(outputFileName, m_width, m_height, codecName));
    return bool(m_output);
}

void VppOutputEncode::initOuputBuffer()
{
    uint32_t maxOutSize;
    m_encoder->getMaxOutSize(&maxOutSize);
    m_buffer.resize(maxOutSize);
    m_outputBuffer.bufferSize = maxOutSize;
    m_outputBuffer.format = OUTPUT_EVERYTHING;
    m_outputBuffer.data = &m_buffer[0];

}

static void setEncodeParam(const SharedPtr<IVideoEncoder>& encoder,
    int width, int height, const EncodeParams* encParam,
    const char* mimeType, uint32_t fourcc)
{
    //configure encoding parameters
    VideoParamsCommon encVideoParams;
    encVideoParams.size = sizeof(VideoParamsCommon);
    encoder->getParameters(VideoParamsTypeCommon, &encVideoParams);
    encVideoParams.resolution.width = width;
    encVideoParams.resolution.height = height;

    //frame rate parameters.
    encVideoParams.frameRate.frameRateDenom = 1;
    encVideoParams.frameRate.frameRateNum = encParam->fps;

    //picture type and bitrate
    encVideoParams.intraPeriod = encParam->intraPeriod;
    encVideoParams.ipPeriod = encParam->ipPeriod;
    encVideoParams.rcParams.bitRate = encParam->bitRate;
    encVideoParams.rcParams.initQP = encParam->initQp;
    encVideoParams.rcParams.diffQPIP = encParam->diffQPIP;
    encVideoParams.rcParams.diffQPIB = encParam->diffQPIB;
    encVideoParams.rcMode = encParam->rcMode;
    encVideoParams.numRefFrames = encParam->numRefFrames;
    encVideoParams.enableLowPower = encParam->enableLowPower;
    if (YAMI_FOURCC_P010 == fourcc)
        encVideoParams.bitDepth = 10;
    else
        encVideoParams.bitDepth = 8;

    memcpy(encVideoParams.rcParams.layerBitRate, encParam->layerBitRate,
           sizeof(encParam->layerBitRate));
    encVideoParams.size = sizeof(VideoParamsCommon);
    encoder->setParameters(VideoParamsTypeCommon, &encVideoParams);

    // configure AVC encoding parameters
    VideoParamsAVC encVideoParamsAVC;
    if (!strcmp(mimeType, YAMI_MIME_H264)) {
        encVideoParamsAVC.size = sizeof(VideoParamsAVC);
        encoder->getParameters(VideoParamsTypeAVC, &encVideoParamsAVC);
        encVideoParamsAVC.idrInterval = encParam->idrInterval;
        encVideoParamsAVC.size = sizeof(VideoParamsAVC);
#if YAMI_CHECK_API_VERSION(0, 2, 1)
        encVideoParamsAVC.enableCabac = encParam->enableCabac;
        encVideoParamsAVC.enableDct8x8 = encParam->enableDct8x8;
        encVideoParamsAVC.enableDeblockFilter = encParam->enableDeblockFilter;
        encVideoParamsAVC.deblockAlphaOffsetDiv2
            = encParam->deblockAlphaOffsetDiv2;
        encVideoParamsAVC.deblockBetaOffsetDiv2
            = encParam->deblockBetaOffsetDiv2;
#else
        ERROR("version num of YamiAPI should be greater than or enqual to %s, "
              "\n%s ",
              "0.2.1", "or enableCabac, enableDct8x8 and enableDeblockFilter "
                       "will use the default value");
#endif
        encVideoParamsAVC.temporalLayerNum = encParam->temporalLayerNum;
        encVideoParamsAVC.priorityId = encParam->priorityId;

        encoder->setParameters(VideoParamsTypeAVC, &encVideoParamsAVC);

        VideoConfigAVCStreamFormat streamFormat;
        streamFormat.size = sizeof(VideoConfigAVCStreamFormat);
        streamFormat.streamFormat = AVC_STREAM_FORMAT_ANNEXB;
        encoder->setParameters(VideoConfigTypeAVCStreamFormat, &streamFormat);
    }

    // configure VP9 encoding parameters
    VideoParamsVP9 encVideoParamsVP9;
    if (!strcmp(mimeType, YAMI_MIME_VP9)) {
      encoder->getParameters(VideoParamsTypeVP9, &encVideoParamsVP9);
         encVideoParamsVP9.referenceMode = encParam->m_encParamsVP9.referenceMode;
         encoder->setParameters(VideoParamsTypeVP9, &encVideoParamsVP9);
    }
}

bool VppOutputEncode::config(NativeDisplay& nativeDisplay, const EncodeParams* encParam)
{
    m_encoder.reset(createVideoEncoder(m_output->getMimeType()), releaseVideoEncoder);
    if (!m_encoder)
        return false;
    m_encoder->setNativeDisplay(&nativeDisplay);
    m_mime = m_output->getMimeType();
    setEncodeParam(m_encoder, m_width, m_height, encParam, m_mime, m_fourcc);
    m_bitRate.init(encParam);
    m_frameLatency.init(encParam);
    Encode_Status status = m_encoder->start();
    assert(status == ENCODE_SUCCESS);
    initOuputBuffer();
    return true;
}

bool VppOutputEncode::output(const SharedPtr<VideoFrame>& frame)
{
    Encode_Status status = ENCODE_SUCCESS;
    bool drain = !frame;
    if (frame) {

        status = m_encoder->encode(frame);
        if (status != ENCODE_SUCCESS) {
            fprintf(stderr, "encode failed status = %d\n", status);
            return false;
        }
    }
    else {
        m_encoder->flush();
    }
    do {
        status = m_encoder->getOutput(&m_outputBuffer, drain);
        if (status == ENCODE_SUCCESS){
            m_frameLatency.getFrameDelay(m_outputBuffer);
            m_bitRate.getBitRate(m_outputBuffer);
            if (!m_output->write(m_outputBuffer.data, m_outputBuffer.dataSize))
                assert(0);
        }

        if (status == ENCODE_BUFFER_TOO_SMALL) {
            m_outputBuffer.bufferSize = (m_outputBuffer.bufferSize * 3) / 2;
            m_buffer.resize(m_outputBuffer.bufferSize);
            m_outputBuffer.data = &m_buffer[0];
        }
    } while (status != ENCODE_BUFFER_NO_MORE);
    return true;

}

void VppOutputEncode::printBitRate()
{
    m_bitRate.printBitRate();
}

void VppOutputEncode::printFrameLatency()
{
    m_frameLatency.printFrameLatency();
}

void BitRateCalc::init(const EncodeParams* encParam)
{
    m_enablePrintBitRate = encParam->enableprintBitRate;
    m_totalDataSize = 0;
    m_fps = encParam->fps;
    m_frameCount = 0;
    if (m_enablePrintBitRate){
        m_bitRateLogFile = fopen((encParam->BitRateLogFile).c_str(),"w");
    }
}

void BitRateCalc::getBitRate(VideoEncOutputBuffer m_outputBuffer){
    if (m_enablePrintBitRate)
    {
        m_frameCount++;
        m_totalDataSize += m_outputBuffer.dataSize;
        fprintf(m_bitRateLogFile,"Bitrate after %ld frames is: %ld bits/second\n", m_frameCount, m_outputBuffer.dataSize * 8 * m_fps);
    }
}

void BitRateCalc::printBitRate()
{
    if (m_enablePrintBitRate){
        fprintf(m_bitRateLogFile,"Average BitRate after %ld frame: %ld bits/second\n",
            m_frameCount, m_totalDataSize / m_frameCount * 8 * m_fps);
        printf("Average BitRate after %ld frame: %ld bits/second\n",
            m_frameCount, m_totalDataSize / m_frameCount * 8 * m_fps);
        fclose(m_bitRateLogFile);
    }
}

void FrameLatencyCalc::init(const EncodeParams* encParam)
{
    m_enablePrintLatency = encParam->enableprintFrameLatency;
    if (m_enablePrintLatency){
        m_latencyLogFile = fopen((encParam->FrameLatencyLogFile).c_str(),"w");
    }
}

void FrameLatencyCalc::getFrameDelay(VideoEncOutputBuffer m_outputBuffer)
{
    if (m_enablePrintLatency)
    {
        struct timeval end;
        gettimeofday(&end,NULL);
        int64_t now = 1000000 * end.tv_sec + end.tv_usec;
        int64_t delay = now - m_outputBuffer.timeStamp;
        m_frameCount++;
        if (m_frameCount == 1){
            m_maxLatency = m_minLatency = delay;
            m_totalLatency = 0;
            m_maxLatencyFrame = 1;
        }
        else{
            if (delay > m_maxLatency){
                m_maxLatency = delay;
                m_maxLatencyFrame = m_frameCount;
            }
            if (delay < m_minLatency)
                m_minLatency = delay;
            m_totalLatency += delay;
        }
        fprintf(m_latencyLogFile, "Frame Count:%ld, Latency:%ldus\n", m_frameCount, delay);
    }
}

void FrameLatencyCalc::printFrameLatency()
{
    if (m_enablePrintLatency){
        fprintf(m_latencyLogFile, 
            "Frame Count:%ld, MaxLatency:%ldus at %ldframe, MinLatency:%ldus, Average Latency:%ldus\n", 
            m_frameCount, m_maxLatency, m_maxLatencyFrame, m_minLatency, m_totalLatency / m_frameCount);
        printf("Frame Count:%ld, MaxLatency:%ldus at %ldframe, MinLatency:%ldus, Average Latency:%ldus\n", 
            m_frameCount, m_maxLatency, m_maxLatencyFrame, m_minLatency, m_totalLatency / m_frameCount);
        fclose(m_latencyLogFile);
    }
}

