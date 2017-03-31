/*
 * Copyright (C) 2011-2016 Intel Corporation. All rights reserved.
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

#include <stdint.h>
#include <Yami.h>

class V4L2Device;
class V4L2Renderer {
public:
    static SharedPtr<V4L2Renderer> create(const SharedPtr<V4L2Device>&, VideoDataMemoryType memoryType);
    virtual bool setDisplay() = 0;
    virtual bool setupOutputBuffers(uint32_t wdith, uint32_t height) = 0;
    bool renderOneFrame();
    virtual bool queueOutputBuffers() = 0;

protected:
    V4L2Renderer(const SharedPtr<V4L2Device>&, VideoDataMemoryType memoryType);
    virtual ~V4L2Renderer(){};
    bool getDpbSize(uint32_t& dpbSize);
    bool requestBuffers(uint32_t& count);
    bool queueBuffer(uint32_t index, unsigned long userptr = 0);
    bool dequeBuffer(uint32_t& index);
    virtual bool render(uint32_t& index) = 0;
    virtual bool queueOutputBuffersAtStart(uint32_t count) = 0;

    SharedPtr<V4L2Device> m_device;
    VideoDataMemoryType m_memoryType;
};
