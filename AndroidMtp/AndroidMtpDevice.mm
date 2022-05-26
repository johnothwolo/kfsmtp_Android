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

#define LOG_TAG "AndroidMtpDevice"

#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <signal.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <IOKit/IOKitLib.h>

#include "MtpDebug.h"
#include "AndroidMtpDevice.h"
#include "MtpDeviceInfo.h"
#include "MtpEventPacket.h"
#include "MtpObjectInfo.h"
#include "MtpProperty.h"
#include "MtpStorageInfo.h"
#include "MtpStringBuffer.h"
#include "MtpUtils.h"


#include "usbhost.h"

typedef struct PrivateUsbData {
    io_object_t             notification;
    io_name_t               name;
    io_name_t               vendor;
    io_name_t               serial;
    uint32_t                productID;
    uint32_t                vendorID;
    uint32_t                locationID;
    uint16_t                devicePort;
    uint8_t                 deviceClass;
    uint32_t                serialIndex;
} PrivateUsbData;

namespace android {

#if 0
static bool isMtpDevice(uint16_t vendor, uint16_t product) {
    // Sandisk Sansa Fuze
    if (vendor == 0x0781 && product == 0x74c2)
        return true;
    // Samsung YP-Z5
    if (vendor == 0x04e8 && product == 0x503c)
        return true;
    return false;
}
#endif

namespace {

bool writeToFd(void* data, uint32_t /* unused_offset */, uint32_t length, void* clientData) {
    const int fd = *static_cast<int*>(clientData);
    const ssize_t result = write(fd, data, length);
    if (result < 0) {
        return false;
    }
    return static_cast<uint32_t>(result) == length;
}

}  // namespace


AndroidMtpDevice* AndroidMtpDevice::initWithDeviceAtLocation(unsigned deviceLocation,
                                                 uint16_t vid, uint16_t pid)
{
    libusb_device *dev = nullptr;
    libusb_device **devs;
    int i = 0;
    size_t cnt = 0;
    unsigned int busNumber = deviceLocation >> 24;
    
    cnt = libusb_get_device_list(NULL, &devs);

    while ((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0) {
            gMtpUSBError = r;
            fprintf(stderr, "failed to get device descriptor");
            return nullptr;
        }
        int bnum = libusb_get_bus_number(dev);
        if (desc.idVendor == vid && desc.idProduct == pid && bnum == (int)busNumber)
            goto found;
    }
    
    // FIXME: cannot unref device before opening it
    libusb_free_device_list(devs, 1);
    return nullptr;
found:
    libusb_ref_device(dev);
    libusb_free_device_list(devs, 1);
    return AndroidMtpDevice::init(dev);
}

// this allowss us to open a device with a specific vid, pid, and serial number if multiple
// similar devices are connected.
AndroidMtpDevice* AndroidMtpDevice::init(libusb_device *device)
{
    libusb_device_handle *handle;
    libusb_device_descriptor desc;
    char manufacturerName[256] = {0};
    char productName[256] = {0};
    char sn[256] = {0};
    gMtpUSBError = 0;
    
    if (device == nullptr) {
        fprintf(stderr, "libusb_device_with_vid_pid_sn failed for device: %s\n", libusb_strerror(gMtpUSBError));
        return NULL;
    }

    gMtpUSBError = libusb_get_device_descriptor(device, &desc);
    if (gMtpUSBError != LIBUSB_SUCCESS){
        fprintf(stderr, "libusb_get_device_descriptor failed for device: %s\n", libusb_strerror(gMtpUSBError));
        return nullptr;
    }
    
    gMtpUSBError = libusb_open(device, &handle);
    if (gMtpUSBError != LIBUSB_SUCCESS) {
      /* Could not open this device */
      fprintf(stderr, "libusb_open failed for device: %s\n", libusb_strerror(gMtpUSBError));
      return nullptr;
    }
    
    for (uint8_t i = 0; i < desc.bNumConfigurations; i++) {
        libusb_config_descriptor *config;
        
        gMtpUSBError = libusb_get_config_descriptor (device, i, &config);
        if (gMtpUSBError != LIBUSB_SUCCESS) {
            std::cerr << "libusb_get_config_descriptor " << i << " ";
            std::cerr << "failed with error: " << libusb_strerror(gMtpUSBError) << "." << std::endl;
            continue;
        }
        
        if (desc.iManufacturer)
            gMtpUSBError = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, (u_char*)manufacturerName, sizeof(manufacturerName));
        
        if (desc.iProduct)
            gMtpUSBError = libusb_get_string_descriptor_ascii(handle, desc.iProduct, (u_char*)productName, sizeof(productName));
        
        for (uint8_t j = 0; j < config->bNumInterfaces; j++) {
            for (int k = 0; k < config->interface[j].num_altsetting; k++) {
                /* Current interface descriptor */
                const struct libusb_interface_descriptor *interface = &config->interface[j].altsetting[k];
                if (interface->bInterfaceClass == LIBUSB_CLASS_PTP &&
                    interface->bInterfaceSubClass == 1 && // Still Image Capture
                    interface->bInterfaceProtocol == 1)     // Picture Transfer Protocol (PIMA 15470)
                {
                    fprintf(stdout, "Found camera: \"%s\" \"%s\"\n", manufacturerName, productName);

                } else if (interface->bInterfaceClass == 0xFF &&
                           interface->bInterfaceSubClass == 0xFF &&
                           interface->bInterfaceProtocol == 0) {
                    char interfaceName[256] = {0};
                    

                    gMtpUSBError = libusb_get_string_descriptor_ascii(handle,
                                config->interface[j].altsetting[k].iInterface,
                                (u_char*)interface,
                                sizeof(interface));
                    
                    if(gMtpUSBError != LIBUSB_SUCCESS){
                        fprintf(stdout, "could not get interface string descriptor %s\n", libusb_strerror(gMtpUSBError));
                        continue;
                    } else if (strstr(interfaceName, "MTP")) {
                        continue;
                    }
                    
                    // just quickly get the serial number
                    gMtpUSBError = libusb_get_string_descriptor_ascii(handle,
                                                             desc.iSerialNumber,
                                                             (u_char*)sn,
                                                             sizeof(sn));
                    if (gMtpUSBError < 0) printf("[WARNING] failed to get serial number");
                    
                    // Looks like an android style MTP device
                    fprintf(stdout, "Found MTP device: \"%s\" \"%s\"\n", manufacturerName, productName);
                }
#if 0
                else {
                    // look for special cased devices based on vendor/product ID
                    // we are doing this mainly for testing purposes
                    uint16_t vendor = usb_device_get_vendor_id(device);
                    uint16_t product = usb_device_get_product_id(device);
                    if (!isMtpDevice(vendor, product)) {
                        // not an MTP or PTP device
                        continue;
                    }
                    // request MTP OS string and descriptor
                    // some music players need to see this before entering MTP mode.
                    char buffer[256];
                    memset(buffer, 0, sizeof(buffer));
                    int gMtpUSBError = usb_device_control_transfer(device,
                                                          USB_DIR_IN|USB_RECIP_DEVICE|USB_TYPE_STANDARD,
                                                          USB_REQ_GET_DESCRIPTOR, (USB_DT_STRING << 8) | 0xEE,
                                                          0, buffer, sizeof(buffer), 0);
                    printf("usb_device_control_transfer returned %d errno: %d\n", gMtpUSBError, errno);
                    if (gMtpUSBError > 0) {
                        printf("got MTP string %s\n", buffer);
                        gMtpUSBError = usb_device_control_transfer(device,
                                                          USB_DIR_IN|USB_RECIP_DEVICE|USB_TYPE_VENDOR, 1,
                                                          0, 4, buffer, sizeof(buffer), 0);
                        printf("OS descriptor got %d\n", gMtpUSBError);
                    } else {
                        printf("no MTP string\n");
                    }
                }
#else
                else {
                    continue;
                }
#endif
                // if we got here, then we have a likely MTP or PTP device
                // interface should be followed by three endpoints
                const libusb_endpoint_descriptor *ep;
                const libusb_endpoint_descriptor *ep_in_desc = nullptr;
                const libusb_endpoint_descriptor *ep_out_desc = nullptr;
                const libusb_endpoint_descriptor *ep_intr_desc = nullptr;
                struct libusb_ss_endpoint_companion_descriptor *ep_ss_ep_comp_desc = NULL;
                for (int l = 0; l < config->interface[i].altsetting[j].bNumEndpoints; l++) {
                    ep = &config->interface[j].altsetting[k].endpoint[l];
                    printf("endpoint[%d].address: %02X\n", k, ep->bEndpointAddress);

                    if (!ep || ep->bDescriptorType != LIBUSB_DT_ENDPOINT) {
                        fprintf(stderr, "endpoints not found\n");;
                        break;
                    }
                    
                    if (ep->bmAttributes  == LIBUSB_TRANSFER_TYPE_BULK) {
                        if ((ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
                            LIBUSB_ENDPOINT_DIR_MASK)
                            ep_in_desc = ep;
                        else if ((ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == 0)
                            ep_out_desc = ep;
                    } else if (ep->bmAttributes == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
                        if ((ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
                            LIBUSB_ENDPOINT_DIR_MASK) {
                            ep_intr_desc = ep;
                        }
                    }
                    
                    printf("max packet size: %04X\n", ep->wMaxPacketSize);
                    printf("polling interval: %02X\n", ep->bInterval);
                    libusb_get_ss_endpoint_companion_descriptor(NULL, ep, &ep_ss_ep_comp_desc);
                    if (ep_ss_ep_comp_desc != nullptr) {
                        printf("max burst: %02X   (USB 3.0)\n", ep_ss_ep_comp_desc->bMaxBurst);
                        printf("bytes per interval: %04X (USB 3.0)\n", ep_ss_ep_comp_desc->wBytesPerInterval);
                        libusb_free_ss_endpoint_companion_descriptor(ep_ss_ep_comp_desc);
                    }
                }
                if (!ep_in_desc || !ep_out_desc || !ep_intr_desc) {
                    fprintf(stderr, "endpoints not found\n");
                    break;
                }
                
                // if we got here, then we found the right interface
                gMtpUSBError = libusb_claim_interface(handle, interface->bInterfaceNumber);
                if (gMtpUSBError != LIBUSB_SUCCESS) {
                    if(gMtpUSBError == LIBUSB_ERROR_BUSY) {
                        // disconnect kernel driver and try again
                        if (libusb_kernel_driver_active(handle, config->interface[j].altsetting[k].iInterface))
                        {
                            /*
                             * Specifically avoid probing anything else than USB mass storage devices
                             * and non-associated drivers in Linux.
                             */
                            if (libusb_detach_kernel_driver(handle, interface->bInterfaceNumber) != LIBUSB_SUCCESS) {
                                fprintf(stderr, "libusb_detach_kernel_driver() failed, continuing anyway...");
                                std::cout << "Error: cannot detach busy device";
                                ::raise(SIGTRAP);
                            }
                        }
                        gMtpUSBError = libusb_claim_interface(handle, interface->bInterfaceNumber);
                    } else {
                        fprintf(stderr, "usb_device_claim_interface failed errno: %s\n\n", libusb_strerror(gMtpUSBError));
                        libusb_close(handle);
                        libusb_unref_device(device);
                        libusb_free_config_descriptor(config);
                        return NULL;
                    }
                }
                
                AndroidMtpDevice* mtpDevice = new AndroidMtpDevice(device,
                                                     handle,
                                                     config->interface[j].altsetting[k].bInterfaceNumber,
                                                     config->interface[j].altsetting[k].bAlternateSetting,
                                                     desc.idProduct,
                                                     desc.idVendor,
                                                     productName,
                                                     manufacturerName,
                                                     sn,
                                                     ep_in_desc,
                                                     ep_out_desc,
                                                     ep_intr_desc);
                mtpDevice->initialize();
                libusb_free_config_descriptor(config);
                return mtpDevice;
            }
            fprintf(stderr, "moving onto next interface\n");
        }
        libusb_free_config_descriptor(config);
    }

    libusb_close(handle);
    fprintf(stderr, "device not a valid mtp interface\n");;
    return NULL;
}

AndroidMtpDevice::AndroidMtpDevice(struct libusb_device* device,
                     struct libusb_device_handle* handle,
                     int interface,
                     int altsetting,
                     uint16_t vendorId,
                     uint16_t productId,
                     std::string deviceName,
                     std::string manufacturerName,
                     std::string serialNumber,
                     const libusb_endpoint_descriptor *ep_in,
                     const libusb_endpoint_descriptor *ep_out,
                     const libusb_endpoint_descriptor *ep_intr)
    :   mDevice(device),
        mNotification(0),
        mNotificationIsReleased(false),
        mInterface(interface),
        mRequestIn1(NULL),
        mRequestIn2(NULL),
        mRequestOut(NULL),
        mRequestIntr(NULL),
        mDeviceInfo(NULL),
        mDeviceHandle(handle),
        mSessionID(0),
        mVendorId(vendorId),
        mProductId(productId),
        mDeviceName(deviceName),
        mManufacturerName(manufacturerName),
        mSerialNumber(serialNumber),
        mTransactionID(0),
        mReceivedResponse(false),
        mProcessingEvent(false),
        mCurrentEventHandle(0),
        mLastSendObjectInfoTransactionID(0),
        mLastSendObjectInfoObjectHandle(0),
        mPacketDivisionMode(FIRST_PACKET_HAS_PAYLOAD)
{
    mRequestIn1 = libusb_request_new(device, handle, ep_in);
    mRequestIn2 = libusb_request_new(device, handle, ep_in);
    mRequestOut = libusb_request_new(device, handle, ep_out);
    mRequestIntr = libusb_request_new(device, handle, ep_intr);
}

AndroidMtpDevice::~AndroidMtpDevice() {
    close();
    for (size_t i = 0; i < mDeviceProperties.size(); i++)
        delete mDeviceProperties[i];
    liusb_request_free(mRequestIn1);
    liusb_request_free(mRequestIn2);
    liusb_request_free(mRequestOut);
    liusb_request_free(mRequestIntr);
}

//int bulk_in_big_req(void *buf, int len,  int time_out, int *len_out){
//    int req_len;
//    int actual;
//    int rc;
//    unsigned char *cur;
//
//    assert(len%1024==0); //len shall be a max_packet size multiple assume super-speed 1024 bulk
//    cur = (unsigned char*)buf;
//    while (len) {
//        req_len = std::min(1024*1024*2, len);
////        rc = libusb_bulk_transfer( devh, /*ep_no*/1 ,  cur, req_len, &actual, time_out );
//        cur  += actual;
//        len -= actual;
//        if (rc)
//            break;
//        if (actual!=req_len)
//            break; //short ZLP end here
//        //todo decrease time out and brake as needed
//    }
//    *len_out = (int)(cur - (unsigned char*)buf);
//    return rc;
//}


uint32_t AndroidMtpDevice::getNotification()
{
    return mNotification;
}

void AndroidMtpDevice::setNotification(uint32_t notification)
{
    mNotification = notification;
}

void AndroidMtpDevice::releaseNotification()
{
    IOObjectRelease(mNotification);
    mNotificationIsReleased = true;
}

bool AndroidMtpDevice::notificationIsReleased()
{
    return mNotificationIsReleased == true;
}



void AndroidMtpDevice::initialize() {
    openSession();
    mDeviceInfo = getDeviceInfo();
    if (mDeviceInfo) {
        if (mDeviceInfo->mDeviceProperties) {
            int count = (int)mDeviceInfo->mDeviceProperties->size();
            for (int i = 0; i < count; i++) {
                MtpDeviceProperty propCode = (*mDeviceInfo->mDeviceProperties)[i];
                MtpProperty* property = getDevicePropDesc(propCode);
                if (property)
                    mDeviceProperties.push_back(property);
            }
        }
    }
}

void AndroidMtpDevice::close() {
    if (mDevice && mDeviceHandle) {
        libusb_clear_stall(mRequestIn1, mRequestOut, mRequestIntr);
        libusb_release_interface(mDeviceHandle, (int)mInterface);
        libusb_reset_device (mDeviceHandle);
        libusb_close(mDeviceHandle);
        libusb_unref_device(mDevice);
        mDevice = NULL;
        mDeviceHandle = NULL;
    }
}

void AndroidMtpDevice::print() {
    if (!mDeviceInfo)
        return;

    mDeviceInfo->print();

    if (mDeviceInfo->mDeviceProperties) {
        fprintf(stdout, "***** DEVICE PROPERTIES *****\n");
        int count = (int)mDeviceInfo->mDeviceProperties->size();
        for (int i = 0; i < count; i++) {
            MtpDeviceProperty propCode = (*mDeviceInfo->mDeviceProperties)[i];
            MtpProperty* property = getDevicePropDesc(propCode);
            if (property) {
                property->print();
                delete property;
            }
        }
    }

    if (mDeviceInfo->mPlaybackFormats) {
            fprintf(stdout, "***** OBJECT PROPERTIES *****\n");
        int count = (int)mDeviceInfo->mPlaybackFormats->size();
        for (int i = 0; i < count; i++) {
            MtpObjectFormat format = (*mDeviceInfo->mPlaybackFormats)[i];
            fprintf(stdout, "*** FORMAT: %s\n", MtpDebug::getFormatCodeName(format));
            MtpObjectPropertyList* props = getObjectPropsSupported(format);
            if (props) {
                for (size_t j = 0; j < props->size(); j++) {
                    MtpObjectProperty prop = (*props)[j];
                    MtpProperty* property = getObjectPropDesc(prop, format);
                    if (property) {
                        property->print();
                        delete property;
                    } else {
                        fprintf(stderr, "could not fetch property: %s\n",
                                MtpDebug::getObjectPropCodeName(prop));
                    }
                }
            }
        }
    }
}

const char* AndroidMtpDevice::getDeviceName() {
    if (!mDeviceName.empty())
        return mDeviceName.c_str();
    else
        return "???";
}

bool AndroidMtpDevice::openSession() {
    std::lock_guard<std::mutex> lg(mMutex);

    mSessionID = 0;
    mTransactionID = 0;
    MtpSessionID newSession = 1;
    mRequest.reset();
    mRequest.setParameter(1, newSession);
    if (!sendRequest(MTP_OPERATION_OPEN_SESSION))
        return false;
    MtpResponseCode ret = readResponse();
    if (ret == MTP_RESPONSE_SESSION_ALREADY_OPEN)
        newSession = mResponse.getParameter(1);
    else if (ret != MTP_RESPONSE_OK)
        return false;

    mSessionID = newSession;
    mTransactionID = 1;
    return true;
}

bool AndroidMtpDevice::closeSession() {
    // FIXME
    return true;
}

MtpDeviceInfo* AndroidMtpDevice::getDeviceInfo() {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    if (!sendRequest(MTP_OPERATION_GET_DEVICE_INFO))
        return NULL;
    if (!readData())
        return NULL;
    MtpResponseCode ret = readResponse();
    if (ret == MTP_RESPONSE_OK) {
        MtpDeviceInfo* info = new MtpDeviceInfo;
        if (info->read(mData))
            return info;
        else
            delete info;
    }
    return NULL;
}

MtpStorageIDList* AndroidMtpDevice::getStorageIDs() {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    if (!sendRequest(MTP_OPERATION_GET_STORAGE_IDS))
        return NULL;
    if (!readData())
        return NULL;
    MtpResponseCode ret = readResponse();
    if (ret == MTP_RESPONSE_OK) {
        return mData.getAUInt32();
    }
    return NULL;
}

MtpStorageInfo* AndroidMtpDevice::getStorageInfo(MtpStorageID storageID) {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    mRequest.setParameter(1, storageID);
    if (!sendRequest(MTP_OPERATION_GET_STORAGE_INFO))
        return NULL;
    if (!readData())
        return NULL;
    MtpResponseCode ret = readResponse();
    if (ret == MTP_RESPONSE_OK) {
        MtpStorageInfo* info = new MtpStorageInfo(storageID);
        if (info->read(mData))
            return info;
        else
            delete info;
    }
    return NULL;
}

MtpObjectHandleList* AndroidMtpDevice::getObjectHandles(MtpStorageID storageID,
            MtpObjectFormat format, MtpObjectHandle parent) {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    mRequest.setParameter(1, storageID);
    mRequest.setParameter(2, format);
    mRequest.setParameter(3, parent);
    if (!sendRequest(MTP_OPERATION_GET_OBJECT_HANDLES))
        return NULL;
    if (!readData())
        return NULL;
    MtpResponseCode ret = readResponse();
    if (ret == MTP_RESPONSE_OK) {
        return mData.getAUInt32();
    }
    return NULL;
}

MtpObjectInfo* AndroidMtpDevice::getObjectInfo(MtpObjectHandle handle) {
    std::lock_guard<std::mutex> lg(mMutex);

    // FIXME - we might want to add some caching here

    mRequest.reset();
    mRequest.setParameter(1, handle);
    if (!sendRequest(MTP_OPERATION_GET_OBJECT_INFO))
        return NULL;
    if (!readData())
        return NULL;
    MtpResponseCode ret = readResponse();
    if (ret == MTP_RESPONSE_OK) {
        MtpObjectInfo* info = new MtpObjectInfo(handle);
        if (info->read(mData))
            return info;
        else
            delete info;
    }
    return NULL;
}

void* AndroidMtpDevice::getThumbnail(MtpObjectHandle handle, int& outLength) {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    mRequest.setParameter(1, handle);
    if (sendRequest(MTP_OPERATION_GET_THUMB) && readData()) {
        MtpResponseCode ret = readResponse();
        if (ret == MTP_RESPONSE_OK) {
            return mData.getData(&outLength);
        }
    }
    outLength = 0;
    return NULL;
}

MtpObjectHandle AndroidMtpDevice::sendObjectInfo(MtpObjectInfo* info) {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    MtpObjectHandle parent = info->mParent;
    if (parent == 0)
        parent = MTP_PARENT_ROOT;

    mRequest.setParameter(1, info->mStorageID);
    mRequest.setParameter(2, parent);

    mData.reset();
    mData.putUInt32(info->mStorageID);
    mData.putUInt16(info->mFormat);
    mData.putUInt16(info->mProtectionStatus);
    mData.putUInt32(info->mCompressedSize);
    mData.putUInt16(info->mThumbFormat);
    mData.putUInt32(info->mThumbCompressedSize);
    mData.putUInt32(info->mThumbPixWidth);
    mData.putUInt32(info->mThumbPixHeight);
    mData.putUInt32(info->mImagePixWidth);
    mData.putUInt32(info->mImagePixHeight);
    mData.putUInt32(info->mImagePixDepth);
    mData.putUInt32(info->mParent);
    mData.putUInt16(info->mAssociationType);
    mData.putUInt32(info->mAssociationDesc);
    mData.putUInt32(info->mSequenceNumber);
    mData.putString(info->mName);

    char created[100], modified[100];
    formatDateTime(info->mDateCreated, created, sizeof(created));
    formatDateTime(info->mDateModified, modified, sizeof(modified));

    mData.putString(created);
    mData.putString(modified);
    if (info->mKeywords)
        mData.putString(info->mKeywords);
    else
        mData.putEmptyString();

   if (sendRequest(MTP_OPERATION_SEND_OBJECT_INFO) && sendData()) {
        MtpResponseCode ret = readResponse();
        if (ret == MTP_RESPONSE_OK) {
            mLastSendObjectInfoTransactionID = mRequest.getTransactionID();
            mLastSendObjectInfoObjectHandle = mResponse.getParameter(3);
            info->mStorageID = mResponse.getParameter(1);
            info->mParent = mResponse.getParameter(2);
            info->mHandle = mResponse.getParameter(3);
            return info->mHandle;
        }
    }
    return (MtpObjectHandle)-1;
}

bool AndroidMtpDevice::sendObject(MtpObjectHandle handle, uint32_t size, int srcFD) {
    std::lock_guard<std::mutex> lg(mMutex);

    if (mLastSendObjectInfoTransactionID + 1 != mTransactionID ||
            mLastSendObjectInfoObjectHandle != handle) {
        fprintf(stderr, "A sendObject request must follow the sendObjectInfo request.\n");;
        return false;
    }

    mRequest.reset();
    if (sendRequest(MTP_OPERATION_SEND_OBJECT)) {
        mData.setOperationCode(mRequest.getOperationCode());
        mData.setTransactionID(mRequest.getTransactionID());
        const int64_t writeResult = mData.write(mRequestOut, mPacketDivisionMode, srcFD, size);
        const MtpResponseCode ret = readResponse();
        return ret == MTP_RESPONSE_OK && writeResult > 0;
    }
    return false;
}

bool AndroidMtpDevice::deleteObject(MtpObjectHandle handle) {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    mRequest.setParameter(1, handle);
    if (sendRequest(MTP_OPERATION_DELETE_OBJECT)) {
        MtpResponseCode ret = readResponse();
        if (ret == MTP_RESPONSE_OK)
            return true;
    }
    return false;
}

MtpObjectHandle AndroidMtpDevice::getParent(MtpObjectHandle handle) {
    MtpObjectInfo* info = getObjectInfo(handle);
    if (info) {
        MtpObjectHandle parent = info->mParent;
        delete info;
        return parent;
    } else {
        return -1;
    }
}

MtpObjectHandle AndroidMtpDevice::getStorageID(MtpObjectHandle handle) {
    MtpObjectInfo* info = getObjectInfo(handle);
    if (info) {
        MtpObjectHandle storageId = info->mStorageID;
        delete info;
        return storageId;
    } else {
        return -1;
    }
}

MtpObjectPropertyList* AndroidMtpDevice::getObjectPropsSupported(MtpObjectFormat format) {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    mRequest.setParameter(1, format);
    if (!sendRequest(MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED))
        return NULL;
    if (!readData())
        return NULL;
    MtpResponseCode ret = readResponse();
    if (ret == MTP_RESPONSE_OK) {
        return mData.getAUInt16();
    }
    return NULL;

}

MtpProperty* AndroidMtpDevice::getDevicePropDesc(MtpDeviceProperty code) {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    mRequest.setParameter(1, code);
    if (!sendRequest(MTP_OPERATION_GET_DEVICE_PROP_DESC))
        return NULL;
    if (!readData())
        return NULL;
    MtpResponseCode ret = readResponse();
    if (ret == MTP_RESPONSE_OK) {
        MtpProperty* property = new MtpProperty;
        if (property->read(mData))
            return property;
        else
            delete property;
    }
    return NULL;
}

bool AndroidMtpDevice::setDevicePropValueStr(MtpProperty* property) {
    if (property == nullptr)
        return false;

    std::lock_guard<std::mutex> lg(mMutex);

    if (property->getDataType() != MTP_TYPE_STR) {
        return false;
    }

    mRequest.reset();
    mRequest.setParameter(1, property->getPropertyCode());

    mData.reset();
    mData.putString(property->getCurrentValue().str);

   if (sendRequest(MTP_OPERATION_SET_DEVICE_PROP_VALUE) && sendData()) {
        MtpResponseCode ret = readResponse();
        if (ret != MTP_RESPONSE_OK) {
            fprintf(stdout, "%s: Response=0x%04X\n", __func__, ret);
            return false;
        }
    }
    return true;
}

MtpProperty* AndroidMtpDevice::getObjectPropDesc(MtpObjectProperty code, MtpObjectFormat format) {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    mRequest.setParameter(1, code);
    mRequest.setParameter(2, format);
    if (!sendRequest(MTP_OPERATION_GET_OBJECT_PROP_DESC))
        return NULL;
    if (!readData())
        return NULL;
    const MtpResponseCode ret = readResponse();
    if (ret == MTP_RESPONSE_OK) {
        MtpProperty* property = new MtpProperty;
        if (property->read(mData))
            return property;
        else
            delete property;
    }
    return NULL;
}

bool AndroidMtpDevice::setObjectPropValue(MtpObjectHandle handle, MtpProperty* property) {
    if (property == nullptr)
        return false;

    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    mRequest.setParameter(1, handle);
    mRequest.setParameter(2, property->getPropertyCode());

    mData.reset();
    mData.putString(property->getCurrentValue().str);
    
    if (sendRequest(MTP_OPERATION_SET_OBJECT_PROP_VALUE) && sendData()) {
        MtpResponseCode ret = readResponse();
        if (ret != MTP_RESPONSE_OK) {
            fprintf(stdout, "%s: Response=0x%04X\n", __func__, ret);
            return false;
        }
    }
    
    return true;
}

bool AndroidMtpDevice::getObjectPropValue(MtpObjectHandle handle, MtpProperty* property) {
    if (property == nullptr)
        return false;

    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    mRequest.setParameter(1, handle);
    mRequest.setParameter(2, property->getPropertyCode());
    if (!sendRequest(MTP_OPERATION_GET_OBJECT_PROP_VALUE))
        return false;
    if (!readData())
        return false;
    if (readResponse() != MTP_RESPONSE_OK)
        return false;
    property->setCurrentValue(mData);
    return true;
}

bool AndroidMtpDevice::readObject(MtpObjectHandle handle,
                           ReadObjectCallback callback,
                           uint32_t expectedLength,
                           void* clientData) {
    return readObjectInternal(handle, callback, &expectedLength, clientData);
}

// reads the object's data and writes it to the specified file path
bool AndroidMtpDevice::readObject(MtpObjectHandle handle, const char* destPath, int group, int perm) {
    fprintf(stdout, "readObject: %s\n", destPath);
    int fd = ::open(destPath, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        fprintf(stderr, "open failed for %s\n", destPath);
        return false;
    }

    fchown(fd, getuid(), group);
    // set permissions
    int mask = umask(0);
    fchmod(fd, perm);
    umask(mask);

    bool result = readObject(handle, fd);
    ::close(fd);
    return result;
}

bool AndroidMtpDevice::readObject(MtpObjectHandle handle, int fd) {
    fprintf(stdout, "readObject: %d\n", fd);
    return readObjectInternal(handle, writeToFd, NULL /* expected size */, &fd);
}

bool AndroidMtpDevice::readObjectInternal(MtpObjectHandle handle,
                                   ReadObjectCallback callback,
                                   const uint32_t* expectedLength,
                                   void* clientData) {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    mRequest.setParameter(1, handle);
    if (!sendRequest(MTP_OPERATION_GET_OBJECT)) {
        fprintf(stderr, "Failed to send a read request.\n");;
        return false;
    }

    return readData(callback, expectedLength, nullptr, clientData);
}

bool AndroidMtpDevice::readData(ReadObjectCallback callback,
                            const uint32_t* expectedLength,
                            uint32_t* writtenSize,
                            void* clientData) {
    if (!mData.readDataHeader(mRequestIn1)) {
        fprintf(stderr, "Failed to read header.\n");;
        return false;
    }

    // If object size 0 byte, the remote device may reply a response packet without sending any data
    // packets.
    if (mData.getContainerType() == MTP_CONTAINER_TYPE_RESPONSE) {
        mResponse.copyFrom(mData);
        return mResponse.getResponseCode() == MTP_RESPONSE_OK;
    }

    const uint32_t fullLength = mData.getContainerLength();
    if (fullLength < MTP_CONTAINER_HEADER_SIZE) {
        fprintf(stderr, "fullLength is too short: %d\n", fullLength);
        return false;
    }
    const uint32_t length = fullLength - MTP_CONTAINER_HEADER_SIZE;
    if (expectedLength && length != *expectedLength) {
        fprintf(stderr, "readObject error length: %d\n", fullLength);
        return false;
    }

    uint32_t offset = 0;
    bool writingError = false;

    {
        int initialDataLength = 0;
        void* const initialData = mData.getData(&initialDataLength);
        if (fullLength > MTP_CONTAINER_HEADER_SIZE && initialDataLength == 0) {
            // According to the MTP spec, the responder (MTP device) can choose two ways of sending
            // data. a) The first packet contains the head and as much of the payload as possible
            // b) The first packet contains only the header. The initiator (MTP host) needs
            // to remember which way the responder used, and send upcoming data in the same way.
            fprintf(stdout, "Found short packet that contains only a header.\n");
            mPacketDivisionMode = FIRST_PACKET_ONLY_HEADER;
        }
        if (initialData) {
            if (initialDataLength > 0) {
                if (!callback(initialData, offset, initialDataLength, clientData)) {
                    fprintf(stderr, "Failed to write initial data.\n");;
                    writingError = true;
                }
                offset += initialDataLength;
            }
            free(initialData);
        }
    }

    // USB reads greater than 16K don't work.
    char buffer1[MTP_BUFFER_SIZE], buffer2[MTP_BUFFER_SIZE];
    mRequestIn1->buffer = (u_char*) buffer1;
    mRequestIn2->buffer = (u_char*) buffer2;
    struct libusb_request* req = NULL;

    while (offset < length) {
        // Wait for previous read to complete.
        void* writeBuffer = NULL;
        int writeLength = 0;
        if (req) {
            const int read = mData.readDataWait(req);
            if (read < 0) {
                fprintf(stderr, "readDataWait failed.\n");;
                return false;
            }
            writeBuffer = req->buffer;
            writeLength = read;
        }

        // Request to read next chunk.
        const uint32_t nextOffset = offset + writeLength;
        if (nextOffset < length) {
            // Queue up a read request.
            const size_t remaining = length - nextOffset;
            req = (req == mRequestIn1 ? mRequestIn2 : mRequestIn1);
            req->buffer_length = remaining > MTP_BUFFER_SIZE ?
                    static_cast<size_t>(MTP_BUFFER_SIZE) : (int)remaining;
            if (mData.readDataAsync(req) != 0) {
                fprintf(stderr, "readDataAsync failed\n");;
                return false;
            }
        }

        // Write previous buffer.
        if (writeBuffer && !writingError) {
            if (!callback(writeBuffer, offset, writeLength, clientData)) {
                fprintf(stderr, "write failed\n");;
                writingError = true;
            }
        }
        offset = nextOffset;
    }

    if (writtenSize) {
        *writtenSize = length;
    }

    return readResponse() == MTP_RESPONSE_OK;
}

bool AndroidMtpDevice::readPartialObject(MtpObjectHandle handle,
                                  uint32_t offset,
                                  uint32_t size,
                                  uint32_t *writtenSize,
                                  ReadObjectCallback callback,
                                  void* clientData) {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    mRequest.setParameter(1, handle);
    mRequest.setParameter(2, offset);
    mRequest.setParameter(3, size);
    if (!sendRequest(MTP_OPERATION_GET_PARTIAL_OBJECT)) {
        fprintf(stderr, "Failed to send a read request.\n");;
        return false;
    }
    // The expected size is null because it requires the exact number of bytes to read though
    // MTP_OPERATION_GET_PARTIAL_OBJECT allows devices to return shorter length of bytes than
    // requested. Destination's buffer length should be checked in |callback|.
    return readData(callback, nullptr /* expected size */, writtenSize, clientData);
}

bool AndroidMtpDevice::readPartialObject64(MtpObjectHandle handle,
                                    uint64_t offset,
                                    uint32_t size,
                                    uint32_t *writtenSize,
                                    ReadObjectCallback callback,
                                    void* clientData) {
    std::lock_guard<std::mutex> lg(mMutex);

    mRequest.reset();
    mRequest.setParameter(1, handle);
    mRequest.setParameter(2, 0xffffffff & offset);
    mRequest.setParameter(3, 0xffffffff & (offset >> 32));
    mRequest.setParameter(4, size);
    if (!sendRequest(MTP_OPERATION_GET_PARTIAL_OBJECT_64)) {
        fprintf(stderr, "Failed to send a read request.\n");;
        return false;
    }
    // The expected size is null because it requires the exact number of bytes to read though
    // MTP_OPERATION_GET_PARTIAL_OBJECT_64 allows devices to return shorter length of bytes than
    // requested. Destination's buffer length should be checked in |callback|.
    return readData(callback, nullptr /* expected size */, writtenSize, clientData);
}

bool AndroidMtpDevice::sendRequest(MtpOperationCode operation) {
    fprintf(stdout, "sendRequest: %s\n", MtpDebug::getOperationCodeName(operation));
    mReceivedResponse = false;
    mRequest.setOperationCode(operation);
    if (mTransactionID > 0)
        mRequest.setTransactionID(mTransactionID++);
    int ret = mRequest.write(mRequestOut);
    mRequest.dump();
    return (ret > 0);
}

bool AndroidMtpDevice::sendData() {
    fprintf(stdout, "sendData\n");
    mData.setOperationCode(mRequest.getOperationCode());
    mData.setTransactionID(mRequest.getTransactionID());
    int ret = mData.write(mRequestOut, mPacketDivisionMode);
    mData.dump();
    return (ret >= 0);
}

bool AndroidMtpDevice::readData() {
    mData.reset();
    int ret = mData.read(mRequestIn1);
    fprintf(stdout, "readData returned %d\n", ret);
    if (ret >= MTP_CONTAINER_HEADER_SIZE) {
        if (mData.getContainerType() == MTP_CONTAINER_TYPE_RESPONSE) {
            fprintf(stdout, "got response packet instead of data packet\n");
            // we got a response packet rather than data
            // copy it to mResponse
            mResponse.copyFrom(mData);
            mReceivedResponse = true;
            return false;
        }
        mData.dump();
        return true;
    }
    else {
        fprintf(stdout, "readResponse failed\n");
        return false;
    }
}

MtpResponseCode AndroidMtpDevice::readResponse() {
    fprintf(stdout, "readResponse\n");
    if (mReceivedResponse) {
        mReceivedResponse = false;
        return mResponse.getResponseCode();
    }
    int ret = mResponse.read(mRequestIn1);
    // handle zero length packets, which might occur if the data transfer
    // ends on a packet boundary
    if (ret == 0)
        ret = mResponse.read(mRequestIn1);
    if (ret >= MTP_CONTAINER_HEADER_SIZE) {
        mResponse.dump();
        return mResponse.getResponseCode();
    } else {
        fprintf(stdout, "readResponse failed\n");
        return -1;
    }
}

int AndroidMtpDevice::submitEventRequest() {
    if (!mEventMutex.try_lock()) {
        // An event is being reaped on another thread.
        return -1;
    }
    if (mProcessingEvent) {
        // An event request was submitted, but no reapEventRequest called so far.
        return -1;
    }
    std::lock_guard<std::mutex> lg(mEventMutexForInterrupt);
    mEventPacket.sendRequest(mRequestIntr);
    const int currentHandle = ++mCurrentEventHandle;
    mProcessingEvent = true;
    mEventMutex.unlock();
    return currentHandle;
}

int AndroidMtpDevice::reapEventRequest(int handle, uint32_t (*parameters)[3]) {
    std::lock_guard<std::mutex> lg(mEventMutex);
    if (!mProcessingEvent || mCurrentEventHandle != handle || !parameters) {
        return -1;
    }
    mProcessingEvent = false;
    const int readSize = mEventPacket.readResponse(mRequestIntr);
    const int result = mEventPacket.getEventCode();
    // MTP event has three parameters.
    (*parameters)[0] = mEventPacket.getParameter(1);
    (*parameters)[1] = mEventPacket.getParameter(2);
    (*parameters)[2] = mEventPacket.getParameter(3);
    return readSize != 0 ? result : 0;
}

void AndroidMtpDevice::discardEventRequest(int handle) {
    std::lock_guard<std::mutex> lg(mEventMutexForInterrupt);
    if (mCurrentEventHandle != handle) {
        return;
    }
}

}  // namespace android
