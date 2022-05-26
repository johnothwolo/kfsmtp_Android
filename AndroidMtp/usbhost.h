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
#ifndef __USB_HOST_H
#define __USB_HOST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <libusb-1.0/libusb.h>
#include <stdint.h>

namespace {

static constexpr int USB_CONTROL_TRANSFER_TIMEOUT_MS = 200;

}  // namespace

enum {
    /* more than one device matching vid/pid */
    LIBUSB_ERROR_MULTIPLE_DEVICES = -90,
};

extern int gMtpUSBError;

struct libusb_request
{
    libusb_device *dev;
    libusb_device_handle *handle;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond;
    bool waiting = false;
    u_char* buffer;
    int buffer_length;
    int actual_length;
    int max_packet_size;
    void *private_data; /* struct usbdevfs_urb* */
    char endpoint;
    void *client_data;  /* free for use by client */
};

libusb_device*
libusb_device_with_vid_pid(uint16_t vendor_id, uint16_t product_id);
libusb_device*
libusb_device_with_vid_pid_sn(uint16_t vendor_id, uint16_t product_id, const char *sn);
void
libusb_free_devices_with_vid_pid(uint16_t vendor_id, uint16_t product_id);
struct libusb_request*
libusb_request_new(struct libusb_device *dev, libusb_device_handle *devh, const struct libusb_endpoint_descriptor *ep_desc);
void
liusb_request_free(struct libusb_request*);
void
libusb_clear_stall(libusb_request* input, libusb_request* output,
                   __unused libusb_request *intr);



#ifdef __cplusplus
}
#endif

#endif /* __USB_HOST_H */
