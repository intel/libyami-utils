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
#ifndef vppoutputencode_h
#define vppoutputencode_h
#include <Yami.h>
#include "encodeinput.h"
#include <string>
#include <vector>

#include "vppinputoutput.h"
using std::string;

struct EncodeParamsVP9 {
    EncodeParamsVP9();
    uint32_t referenceMode; // Reference mode scheme, <0 | 1>
};

class EncodeParams {
public:
    EncodeParams();

    VideoRateControl rcMode;
    int32_t initQp;
    int32_t bitRate;
    int32_t fps;
    int32_t ipPeriod;
    int32_t intraPeriod;
    int32_t numRefFrames;
    int32_t idrInterval;
    string codec;
    bool enableCabac;
    bool enableDct8x8;
    bool enableDeblockFilter;
    int8_t deblockAlphaOffsetDiv2; //same as slice_alpha_c0_offset_div2 defined in h264 spec 7.4.3
    int8_t deblockBetaOffsetDiv2; //same as slice_beta_offset_div2 defined in h264 spec 7.4.3
    int8_t diffQPIP;// P frame qp minus initQP
    int8_t diffQPIB;// B frame qp minus initQP
    uint32_t temporalLayerNum; // svc-t temporal layer number
    uint32_t priorityId; // h264 priority_id in prefix nal unit
    EncodeParamsVP9 m_encParamsVP9;
    uint32_t layerBitRate[4]; // specify each scalable layer bitrate
    bool enableLowPower;
    uint32_t targetPercentage;
    uint32_t windowSize; // use for HRD CPB length in ms
    unsigned int initBufferFullness; /* in bits */
    unsigned int bufferSize; /* in bits */
};

class TranscodeParams
{
public:
    TranscodeParams();

    EncodeParams m_encParams;
    uint32_t frameCount;
    uint32_t iWidth; /*input video width*/
    uint32_t iHeight; /*input vide height*/
    uint32_t oWidth; /*output video width*/
    uint32_t oHeight; /*output vide height*/
    uint32_t fourcc;
    string inputFileName;
    string outputFileName;
};

class VppOutputEncode : public VppOutput
{
public:
    virtual bool output(const SharedPtr<VideoFrame>& frame);
    virtual ~VppOutputEncode(){}
    bool config(NativeDisplay& nativeDisplay, const EncodeParams* encParam = NULL);
protected:
    virtual bool init(const char* outputFileName, uint32_t fourcc, int width,
                      int height, const char* codecName);

private:
    void initOuputBuffer();
    const char* m_mime;
    SharedPtr<IVideoEncoder> m_encoder;
    VideoEncOutputBuffer m_outputBuffer;
    std::vector<uint8_t> m_buffer;
    SharedPtr<EncodeOutput> m_output;
};

#endif
