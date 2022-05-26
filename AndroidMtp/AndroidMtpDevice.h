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

#ifndef _MTP_DEVICE_H
#define _MTP_DEVICE_H

#include "MtpEventPacket.h"
#include "MtpDataPacket.h"
#include "MtpRequestPacket.h"
#include "MtpResponsePacket.h"
#include "MtpTypes.h"

#include <mutex>

struct libusb_device_handle;
struct libusb_transfer;
struct libusb_device;
struct libusb_request;
struct libusb_endpoint_descriptor;

namespace android {

class MtpDeviceInfo;
class MtpEventPacket;
class MtpObjectInfo;
class MtpStorageInfo;

class AndroidMtpDevice {
private:
    struct libusb_device*   mDevice;
    struct libusb_device_handle*mDeviceHandle;
    int                     mInterface;
    uint16_t                mVendorId;
    uint16_t                mProductId;
    std::string             mDeviceName;
    std::string             mManufacturerName;
    std::string             mSerialNumber;
    struct libusb_request*  mRequestIn1;
    struct libusb_request*  mRequestIn2;
    struct libusb_request*  mRequestOut;
    struct libusb_request*  mRequestIntr;
    MtpDeviceInfo*          mDeviceInfo;
    MtpPropertyList         mDeviceProperties;

    // current session ID
    MtpSessionID            mSessionID;
    // current transaction ID
    MtpTransactionID        mTransactionID;

    MtpRequestPacket        mRequest;
    MtpDataPacket           mData;
    MtpResponsePacket       mResponse;
    MtpEventPacket          mEventPacket;

    // set to true if we received a response packet instead of a data packet
    bool                    mReceivedResponse;
    bool                    mProcessingEvent;
    int                     mCurrentEventHandle;

    // to check if a sendObject request follows the last sendObjectInfo request.
    MtpTransactionID        mLastSendObjectInfoTransactionID;
    MtpObjectHandle         mLastSendObjectInfoObjectHandle;

    // to ensure only one MTP transaction at a time
    std::mutex              mMutex;
    std::mutex              mEventMutex;
    std::mutex              mEventMutexForInterrupt;

    // Remember the device's packet division mode.
    UrbPacketDivisionMode   mPacketDivisionMode;
    bool                    mNotificationIsReleased = false;
    unsigned int            mNotification;
public:

public:
    typedef bool (*ReadObjectCallback)
            (void* data, uint32_t offset, uint32_t length, void* clientData);

    AndroidMtpDevice(struct libusb_device* device,
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
              const libusb_endpoint_descriptor *ep_intr);
    
    static AndroidMtpDevice*       init(libusb_device *device);
    static AndroidMtpDevice*       initWithDeviceAtLocation(unsigned deviceLocation,
                                                      uint16_t vid, uint16_t pid);

    virtual                 ~AndroidMtpDevice();

    void                    initialize();
    void                    close();
    void                    print();
    const char*             getDeviceName();

    bool                    openSession();
    bool                    closeSession();

    MtpDeviceInfo*          getDeviceInfo();
    MtpStorageIDList*       getStorageIDs();
    MtpStorageInfo*         getStorageInfo(MtpStorageID storageID);
    MtpObjectHandleList*    getObjectHandles(MtpStorageID storageID, MtpObjectFormat format,
                                    MtpObjectHandle parent);
    MtpObjectInfo*          getObjectInfo(MtpObjectHandle handle);
    void*                   getThumbnail(MtpObjectHandle handle, int& outLength);
    MtpObjectHandle         sendObjectInfo(MtpObjectInfo* info);
    bool                    sendObject(MtpObjectHandle handle, uint32_t size, int srcFD);
    bool                    deleteObject(MtpObjectHandle handle);
    MtpObjectHandle         getParent(MtpObjectHandle handle);
    MtpStorageID            getStorageID(MtpObjectHandle handle);

    MtpObjectPropertyList*  getObjectPropsSupported(MtpObjectFormat format);

    MtpProperty*            getDevicePropDesc(MtpDeviceProperty code);
    bool                    setDevicePropValueStr(MtpProperty* property);
    MtpProperty*            getObjectPropDesc(MtpObjectProperty code, MtpObjectFormat format);

    // Reads value of |property| for |handle|. Returns true on success.
    bool                    setObjectPropValue(MtpObjectHandle handle, MtpProperty* property);
    bool                    getObjectPropValue(MtpObjectHandle handle, MtpProperty* property);

    bool                    readObject(MtpObjectHandle handle, ReadObjectCallback callback,
                                    uint32_t objectSize, void* clientData);
    bool                    readObject(MtpObjectHandle handle, const char* destPath, int group,
                                    int perm);
    bool                    readObject(MtpObjectHandle handle, int fd);
    bool                    readPartialObject(MtpObjectHandle handle,
                                              uint32_t offset,
                                              uint32_t size,
                                              uint32_t *writtenSize,
                                              ReadObjectCallback callback,
                                              void* clientData);
    bool                    readPartialObject64(MtpObjectHandle handle,
                                                uint64_t offset,
                                                uint32_t size,
                                                uint32_t *writtenSize,
                                                ReadObjectCallback callback,
                                                void* clientData);
    // Starts a request to read MTP event from MTP device. It returns a request handle that
    // can be used for blocking read or cancel. If other thread has already been processing an
    // event returns -1.
    int                     submitEventRequest();
    // Waits for MTP event from the device and returns MTP event code. It blocks the current thread
    // until it receives an event from the device. |handle| should be a request handle returned
    // by |submitEventRequest|. The function writes event parameters to |parameters|. Returns 0 for
    // cancellations. Returns -1 for errors.
    int                     reapEventRequest(int handle, uint32_t (*parameters)[3]);
    // Cancels an event request. |handle| should be request handle returned by
    // |submitEventRequest|. If there is a thread blocked by |reapEventRequest| with the same
    // |handle|, the thread will resume.
    void                    discardEventRequest(int handle);

private:
    // If |objectSize| is not NULL, it checks object size before reading data bytes.
    bool                    readObjectInternal(MtpObjectHandle handle,
                                               ReadObjectCallback callback,
                                               const uint32_t* objectSize,
                                               void* clientData);
    // If |objectSize| is not NULL, it checks object size before reading data bytes.
    bool                    readData(ReadObjectCallback callback,
                                     const uint32_t* objectSize,
                                     uint32_t* writtenData,
                                     void* clientData);
    bool                    sendRequest(MtpOperationCode operation);
    bool                    sendData();
    bool                    readData();
    bool                    writeDataHeader(MtpOperationCode operation, int dataLength);
    MtpResponseCode         readResponse();
    
public:
    // iokit methods
    void                    setNotification(uint32_t notification);
    uint32_t                getNotification();
    void                    releaseNotification();
    bool                    notificationIsReleased();
};

}; // namespace android

#endif // _MTP_DEVICE_H
