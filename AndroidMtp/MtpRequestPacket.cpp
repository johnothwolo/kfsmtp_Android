/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "MtpRequestPacket"

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

#include "IMtpHandle.h"
#include "MtpRequestPacket.h"

#include "usbhost.h"

namespace android {

MtpRequestPacket::MtpRequestPacket()
    :   MtpPacket(512),
        mParameterCount(0)
{
}

MtpRequestPacket::~MtpRequestPacket() {
}


    // write our buffer to the given endpoint (host mode)
int MtpRequestPacket::write(struct libusb_request *request)
{
    putUInt32(MTP_CONTAINER_LENGTH_OFFSET, (int)mPacketSize);
    putUInt16(MTP_CONTAINER_TYPE_OFFSET, MTP_CONTAINER_TYPE_COMMAND);
    request->buffer = mBuffer;
    request->buffer_length = (int)mPacketSize;
    return transfer(request);
}

}  // namespace android
