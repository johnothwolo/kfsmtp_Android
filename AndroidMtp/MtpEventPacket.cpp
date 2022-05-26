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

#define LOG_TAG "MtpEventPacket"

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

#include "IMtpHandle.h"
#include "MtpEventPacket.h"

#include "usbhost.h"

namespace android {

MtpEventPacket::MtpEventPacket()
    :   MtpPacket(512)
{
}

MtpEventPacket::~MtpEventPacket() {
}


static void interrupt_callback(libusb_transfer *transfer) {
    auto req = (struct libusb_request *)transfer->user_data;
    req->actual_length = transfer->actual_length;
}

int MtpEventPacket::sendRequest(struct libusb_request *request) {
    libusb_transfer transfer = {0};
    int ret = 0;
    
    request->buffer = mBuffer;
    request->buffer_length = (int)mBufferSize;
    mPacketSize = 0;
    libusb_fill_interrupt_transfer(&transfer,
                                   request->handle,
                                   request->endpoint,
                                   request->buffer,
                                   request->buffer_length,
                                   interrupt_callback,
                                   request,
                                   5000);
    
    
    if ((ret = libusb_submit_transfer(&transfer)) != 0) {
        fprintf(stderr, "libusb_endpoint_queue failed, errno: %d\n", errno);
        return -1;
    }
    return 0;
}

int MtpEventPacket::readResponse(struct libusb_request *req) {
    req->waiting = true;
    int ret = pthread_cond_wait(&req->cond, &req->mutex);
    if (ret == 0) {
        mPacketSize = req->actual_length;
        return req->actual_length;
    } else {
        return -1;
    }
}

}  // namespace android
