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

#include "vppinputdecodecapi.h"
#include "vppinputdecode.h"
#include "decodeoutput.h"
#include "decodehelp.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

SharedPtr<VppInput> createInput(DecodeParameter& para, SharedPtr<NativeDisplay>& display)
{
    SharedPtr<VppInput> input(VppInput::create(para.inputFile, para.renderFourcc, para.width, para.height, para.useCAPI));
    if (!input) {
        fprintf(stderr, "VppInput create failed.\n");
        return input;
    }
    if(para.useCAPI){
        SharedPtr<VppInputDecodeCapi> inputDecode = DynamicPointerCast<VppInputDecodeCapi>(input);
        if (inputDecode && inputDecode->config(*display))
            return input;
    }else{
        SharedPtr<VppInputDecode> inputDecode = DynamicPointerCast<VppInputDecode>(input);
        if (inputDecode) {
            inputDecode->setTargetLayer(para.temporalLayer);
            if (inputDecode->config(*display))
                return input;
        }
    }
    input.reset();
    fprintf(stderr, "VppInputDecode config failed.\n");
    return input;
}

class DecodeTest {
public:
    bool init(int argc, char** argv)
    {
        memset(&m_params, 0, sizeof(m_params));
        if (!processCmdLine(argc, argv, &m_params)) {
            fprintf(stderr, "process arguments failed.\n");
            return false;
        }
        m_output.reset(DecodeOutput::create(m_params.renderMode, m_params.renderFourcc, m_params.inputFile, m_params.outputFile.c_str()));
        if (!m_output) {
            fprintf(stderr, "DecodeOutput::create failed.\n");
            return false;
        }
        m_nativeDisplay = m_output->nativeDisplay();
        m_vppInput = createInput(m_params, m_nativeDisplay);

        if (!m_nativeDisplay || !m_vppInput) {
            fprintf(stderr, "DecodeTest init failed.\n");
            return false;
        }
        return true;
    }
    bool run()
    {
        FpsCalc fps;
        SharedPtr<VideoFrame> src;
        uint32_t count = 0;
        while (m_vppInput->read(src)) {
            if (!m_output->output(src))
                break;
            count++;
            fps.addFrame();
            if (count == m_params.renderFrames)
                break;
        }
        fps.log();
        return true;
    }

private:
    SharedPtr<DecodeOutput> m_output;
    SharedPtr<NativeDisplay> m_nativeDisplay;
    SharedPtr<VppInput> m_vppInput;
    DecodeParameter m_params;
};

int main(int argc, char* argv[])
{
    DecodeTest decode;
    if (!decode.init(argc, argv))
        return 1;
    decode.run();
    return 0;
}
