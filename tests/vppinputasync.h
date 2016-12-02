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
#ifndef vppinputasync_h
#define vppinputasync_h
#include "common/condition.h"
#include "common/lock.h"
#include <deque>

#include "vppinputoutput.h"

using namespace YamiMediaCodec;
class VppInputAsync : public VppInput
{
public:

    bool read(SharedPtr<VideoFrame>& frame);

    /*startupSize means how many buffers will preload to memory in startup, it will overide queueSize*/
    static SharedPtr<VppInput>
    create(const SharedPtr<VppInput>& input, uint32_t queueSize, uint32_t startupSize = 0);

    VppInputAsync();
    virtual ~VppInputAsync();
    virtual int getWidth() { return m_input->getWidth(); }
    virtual int getHeight() { return m_input->getHeight(); }
    virtual uint32_t getFourcc() { return m_input->getFourcc(); }

    //do not use this
    bool init(const char* inputFileName, uint32_t fourcc, int width, int height);
private:
    bool init(const SharedPtr<VppInput>& input, uint32_t queueSize, uint32_t startupSize);
    static void* start(void* async);
    void loop();
    bool preloadFrames(uint32_t startupSize);

    Condition  m_cond;
    Lock       m_lock;
    SharedPtr<VppInput> m_input;
    bool       m_eos;

    typedef std::deque<SharedPtr<VideoFrame> > FrameQueue;
    FrameQueue m_queue;
    uint32_t   m_queueSize;
    uint32_t m_startupSize;

    pthread_t  m_thread;
    bool       m_quit;

};
#endif //vppinputasync_h
