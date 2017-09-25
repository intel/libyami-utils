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
#ifndef vppinputdecode_h
#define vppinputdecode_h
#include <Yami.h>
#include "decodeinput.h"

#include "vppinputoutput.h"

class VppInputDecode : public VppInput
{
public:
    VppInputDecode()
        : m_eos(false)
        , m_error(false)
    {
    }
    bool init(const char* inputFileName, uint32_t fourcc = 0, int width = 0, int height = 0);
    bool read(SharedPtr<VideoFrame>& frame);
    const char *getMimeType() const { return m_input->getMimeType(); }

    bool config(NativeDisplay& nativeDisplay);
    void setTargetLayer(uint32_t temporal = 0, uint32_t spacial = 0, uint32_t quality = 0)
    {
        m_temporalLayer = temporal;
        m_spacialLayer = spacial;
        m_qualityLayer = quality;
    }
    void setLowLatency(bool lowLatency = false)
    {
        m_enableLowLatency = lowLatency;
    }
    virtual ~VppInputDecode() {}
private:
    bool m_eos;
    bool m_error;
    SharedPtr<IVideoDecoder> m_decoder;
    SharedPtr<DecodeInput>   m_input;
    SharedPtr<VideoFrame>    m_first;
    //m_xxxLayer layer number, 0: decode all layers, >0: decode up to target layer.
    uint32_t m_temporalLayer;
    uint32_t m_spacialLayer;
    uint32_t m_qualityLayer;

    //if set this flag to true, AVC decoder will output the ready frames ASAP.
    bool m_enableLowLatency;
};
#endif //vppinputdecode_h

