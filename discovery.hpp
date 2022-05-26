//
//  discovery.hpp
//  kfs_mtpAndroid
//
//  Created by John Othwolo on 5/9/22.
//  Copyright © 2022 FadingRed LLC. All rights reserved.
//

#ifndef discovery_hpp
#define discovery_hpp

#include <string>
#include <vector>
#include <libusb-1.0/libusb.h>

typedef struct mtp_libusb_device {
    libusb_device *dev = nullptr;
    libusb_device_descriptor desc = {0};
    std::string serial = std::string(1024,'\0');
    std::string vendorName = std::string(1024,'\0');
    std::string productName = std::string(1024,'\0');
} mtp_libusb_device_t;

libusb_device**
GetConnectedMtpDevices(std::vector<mtp_libusb_device_t> &devList, libusb_context *context);

#endif /* discovery_hpp */
