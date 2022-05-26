//
//  mntopts.cpp
//  kfs_mtpAndroid
//
//  Created by John Othwolo on 5/9/22.
//  Copyright © 2022 FadingRed LLC. All rights reserved.
//

#include "discovery.hpp"

enum {
    kUSBHubInterfaceClass = 9
};

// for device entries
struct device_entry {
    char *vendor;
    uint16_t vendor_id;
    char *product;
    uint16_t product_id;
    uint32_t device_flags;
} device_entry_t;

static std::vector<device_entry> mtp_device_table = {
#import "MusicPlayers.h"
};

static bool deviceIsSupported(libusb_device_descriptor &desc) {
    for (auto &entry : mtp_device_table) {
        if(entry.product_id == desc.idProduct && entry.vendor_id == desc.idVendor)
            return true;
    }
    return false;
}

libusb_device**
GetConnectedMtpDevices(std::vector<mtp_libusb_device_t> &devList, libusb_context *context){
    libusb_device **list = NULL;
    int rc = 0;
    size_t count = 0;
    
    assert(rc == 0);
    
    count = libusb_get_device_list(context, &list);
    assert(count > 0);
    
    for (size_t idx = 0; idx < count; ++idx) {
        mtp_libusb_device_t mtpdev;
        mtpdev.dev = list[idx];
        
        rc = libusb_get_device_descriptor(mtpdev.dev, &mtpdev.desc);
        assert(rc == 0);
        if (mtpdev.desc.bDeviceClass != kUSBHubInterfaceClass) {
            // First check if we know about the device already.
            // Devices well known to us will not have their descriptors
            // probed, it caused problems with some devices.
            if(deviceIsSupported(mtpdev.desc)){
                libusb_device_handle *devh;
                libusb_open(mtpdev.dev, &devh);
                
                if (devh == nullptr) continue;
                
                std::string &serial = mtpdev.serial;
                std::string &vendorName = mtpdev.vendorName;
                std::string &productName = mtpdev.productName;
                
                // get string descriptors
                libusb_get_string_descriptor_ascii(devh, mtpdev.desc.iManufacturer, (u_char*)vendorName.data(), (int)vendorName.size());
                libusb_get_string_descriptor_ascii(devh, mtpdev.desc.iProduct, (u_char*)productName.data(), (int)productName.size());
                libusb_get_string_descriptor_ascii(devh, mtpdev.desc.iSerialNumber, (u_char*)serial.data(), (int)serial.size());
                
                libusb_close(devh);
                devList.push_back(mtpdev);
            }
        }
    }
    
    return list; // free this later...
}

