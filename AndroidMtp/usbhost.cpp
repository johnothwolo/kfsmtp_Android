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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
// #define DEBUG 1
#if DEBUG
#   define D printf
#else
#   define D(...)
#endif

#include <string>
#include <cstdio>
#include <vector>
#include <string>
#include <cstdlib>
#include <iostream>
#include <unordered_map>

#include <unistd.h>
#include <stddef.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <fcntl.h>
#include <cerrno>
#include <ctype.h>
#include <poll.h>
#include <pthread.h>

#include <libkern/OSByteOrder.h>
#include "usbhost.h"

#define USB_FEATURE_HALT    0x00

int gMtpUSBError = 0;

// in case multiple lookups are made, we'll be able to separate them by key(vid+pid) combo.
static std::unordered_map<uint64_t, std::vector<libusb_device*>> deviceMatchingCache;

// set error if there's more thna one device.
// program should call libusb_device_open_with_sn or libusb_free_vid_pid
libusb_device*
libusb_device_with_vid_pid(uint16_t vendor_id, uint16_t product_id)
{
    struct libusb_device **devs;
    std::vector<libusb_device*> matching;
    struct libusb_device *dev;
    uint64_t key = (vendor_id << 16 | product_id);
    size_t i = 0;

    if (deviceMatchingCache.find(key) != deviceMatchingCache.end()){
        gMtpUSBError = LIBUSB_ERROR_MULTIPLE_DEVICES;
        libusb_strerror(LIBUSB_ERROR_MULTIPLE_DEVICES);
        return nullptr;
    }
    
    if (libusb_get_device_list(nullptr, &devs) < 0)
        return nullptr;

    while ((dev = devs[i++]) != nullptr) {
        struct libusb_device_descriptor desc;
        gMtpUSBError = libusb_get_device_descriptor(dev, &desc);
        if (gMtpUSBError != 0)
            goto out;
        if (desc.idVendor == vendor_id && desc.idProduct == product_id) {
            matching.push_back(dev);
        }
    }

    if (matching.size() == 1) {
        return dev;
    } else if (matching.size() > 1) {
        // cache our devices to filter them later.
        deviceMatchingCache[key] = matching;
        gMtpUSBError = LIBUSB_ERROR_MULTIPLE_DEVICES;
        libusb_strerror(LIBUSB_ERROR_MULTIPLE_DEVICES);
        return nullptr; // too many devices
    }

out:
    libusb_free_device_list(devs, 0);
    return dev;
}

libusb_device*
libusb_device_with_vid_pid_sn(uint16_t vendor_id, uint16_t product_id, const char* serialNumber)
{
    std::vector<libusb_device*> devices;
    libusb_device *dev;
    struct libusb_device_handle *dev_handle = nullptr;
    uint64_t key = (vendor_id << 16 | product_id);

    // if we don't find device VID/PID in cache call libusb_device_open_with_vid_pid_1
    if (deviceMatchingCache.find(key) == deviceMatchingCache.end()){
        dev = libusb_device_with_vid_pid(vendor_id, product_id);
        if (gMtpUSBError == 0) {
            // if one device exists, just return
            std::cerr << "Error: " << libusb_error_name(gMtpUSBError) << std::endl;
            return nullptr;
        }
        if (gMtpUSBError != LIBUSB_ERROR_MULTIPLE_DEVICES) // if there's an error
            return nullptr;
        gMtpUSBError = 0; // if there are multiple devices continue
    }
    
    devices = deviceMatchingCache[(vendor_id << 16 | product_id)];
    
    for (auto &device : devices) {
        char sn[256] = {0};
        struct libusb_device_descriptor desc;
        
        gMtpUSBError = libusb_get_device_descriptor(device, &desc);
        if (gMtpUSBError != 0) {
            std::cerr << "Error: " << libusb_error_name(gMtpUSBError) << std::endl;
            return nullptr;
        }
        
        gMtpUSBError = libusb_open(device, &dev_handle);
        if (gMtpUSBError != 0) {
            std::cerr << "Error: " << libusb_error_name(gMtpUSBError) << std::endl;

            
            // close device handle
            libusb_close(dev_handle);
            return nullptr;
        }
        
        gMtpUSBError = libusb_get_string_descriptor_ascii(dev_handle, desc.iSerialNumber,
                                               (u_char*)sn, sizeof(sn));
        if (gMtpUSBError != 0) {
            std::cerr << "Error: " << libusb_error_name(gMtpUSBError) << std::endl;
            libusb_close(dev_handle);
            return nullptr;
        }
        
        // close device handle
        libusb_close(dev_handle);
        
        if (strstr(sn, serialNumber)){
            gMtpUSBError = 0;
            return device;
        }
        
    }
    
    gMtpUSBError = LIBUSB_ERROR_NO_DEVICE;
    return nullptr;
}

void libusb_free_devices_with_vid_pid(uint16_t vendor_id, uint16_t product_id)
{
    auto devices = deviceMatchingCache[(vendor_id << 16 | product_id)];
    
    for (auto &device : devices)
        libusb_unref_device(device);
}

struct libusb_request*
libusb_request_new(struct libusb_device *dev, libusb_device_handle *devh,
                   const struct libusb_endpoint_descriptor *ep_desc)
{
    char transferType = 0;
    if ((ep_desc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK)
        transferType = LIBUSB_TRANSFER_TYPE_BULK;
    else if ((ep_desc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_INTERRUPT)
        transferType = LIBUSB_TRANSFER_TYPE_INTERRUPT;
    else {
        D("Unsupported endpoint type %d", ep_desc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK);
        return nullptr;
    }
    
    libusb_request *req = new libusb_request;
    if (!req) return nullptr;

    req->dev = dev;
    req->handle = devh;
    req->max_packet_size = OSSwapHostToLittleInt16(ep_desc->wMaxPacketSize);
    req->private_data = (void*)ep_desc;
    req->endpoint = ep_desc->bEndpointAddress;
    return req;
}

void
liusb_request_free(struct libusb_request* request)
{
    delete request;
}



static int usb_endpoint_status(libusb_request* req, uint16_t* status)
{
    return libusb_control_transfer(req->handle,
                                   req->endpoint | LIBUSB_RECIPIENT_ENDPOINT,
                                   LIBUSB_REQUEST_GET_STATUS,
                                   USB_FEATURE_HALT,
                                   req->endpoint,
                                   (unsigned char *) status,
                                   2,
                                   USB_CONTROL_TRANSFER_TIMEOUT_MS);
}


void libusb_clear_stall(libusb_request* input, libusb_request* output,
                        __unused libusb_request *intr)
{
    uint16_t status;
    int ret;
    
    /* check the inep status */
    status = 0;
    ret = usb_endpoint_status(input, &status);
    if (ret < 0) {
        perror ("inep: usb_get_endpoint_status()");
    } else if (status) {
        printf("Clearing stall on IN endpoint\n");
        ret = libusb_clear_halt(input->handle, input->endpoint);
        if (ret != LIBUSB_SUCCESS) {
            perror ("usb_clear_stall_feature()");
        }
    }
    
    /* check the outep status */
    status = 0;
    ret = usb_endpoint_status(output, &status);
    if (ret < 0) {
        perror("outep: usb_get_endpoint_status()");
    } else if (status) {
        printf("Clearing stall on OUT endpoint\n");
        ret = libusb_clear_halt(output->handle, output->endpoint);
        if (ret != LIBUSB_SUCCESS) {
            perror("usb_clear_stall_feature()");
        }
    }
    
    /* TODO: do we need this for INTERRUPT (ptp_usb->intep) too? */
}

