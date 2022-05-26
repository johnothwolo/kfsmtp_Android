//
//  main.cpp
//  kfs_mtpAndroid
//
//  Created by John Othwolo on 5/9/22.
//  Copyright © 2022 FadingRed LLC. All rights reserved.
//

#include <iostream>
#include <sstream>
#include <libgen.h>
#include <getopt.h>
#include <sys/stat.h>
#include <wordexp.h>
#include "discovery.hpp"
#include "fs.h"

static int verbose_flag = 0;
static struct option long_options[] = {
    /* These options set a flag. */
    {"verbose", no_argument,       &verbose_flag, 1},
    /* These options don’t set a flag.
     We distinguish them by their indices. */
    {"help",     no_argument,       0, 'h'},
    {"version",  no_argument,       0, 'V'},
    {"list",     no_argument,       0, 'l'},
    {"device",   required_argument, 0, 'd'},
    {0, 0, 0, 0}
};

static void printHelp(char* progname) {
    std::cerr << "usage: " << basename(progname)
              << " <source> mountpoint [options]\n\n"
        << "general options:\n"
/*        << "    -o opt,[opt...]        mount options\n"*/
        << "    -h   --help            print help\n"
        << "    -V   --version         print version\n"
        << "\n"
        << "    -v   --verbose         verbose output, implies -f\n"
        << "    -l   --list            print available devices. Supports <source> option\n"
        << "    -d   --device          select a device number to mount\n"
/*        << "    -o enable-move         enable the move operations\n\n";*/
        << "\nThis is an experimental program.\n";
}

static void listMtpDevices(libusb_context *context){
    std::vector<mtp_libusb_device_t> devices;
    auto libusb_list = GetConnectedMtpDevices(devices, context);
    if (devices.size() == 0){
        std::cout << "No devices found\n";
    } else for (size_t i = 0; i < devices.size(); i++){
        std::cout << i <<" : " << devices[i].vendorName << " " << devices[i].productName << " - " << devices[i].serial << std::endl;
    }
    libusb_free_device_list(libusb_list, (int)devices.size());
}


int main(int argc, char* argv[])
{
    char c;
    bool list = false, printVersion = false;
    int deviceArg = -1, ret;
    int option_index = 0; /* getopt_long stores the option index here. */
    std::string path;
    androidfs fs;
    bool found_device = false;
    libusb_context *context = nullptr;

    if (argc < 2){
        printHelp(argv[0]);
        return 0;
    } else do {
        c = getopt_long(argc, (char**)argv, "hVvld:", long_options, &option_index);
        /* Detect the end of the options. */
        if (c == -1)
            break;
        switch (c) {
            case 'V':
                printVersion = true;
            case 'v':
                break;
            case 'l':
                list = true;
                break;
            case 'd':
                deviceArg = atoi(optarg);
                break;
            case 'h': /* FALLTHROUGH */
            case '?':
                printHelp(argv[0]);
                return 0;
                __builtin_unreachable();
            default:
                abort();
        }
    } while(1);

    // init libusb
    ret = libusb_init(&context);
    if (ret != 0){
        std::cout << "Libusb error: " << libusb_strerror(ret) << std::endl;
    }
    
    if (printVersion){
        printf("Version 1.0\n");
        return 0;
    } else if (list) {
        listMtpDevices(context);
        return 0;
    } else {
        if (deviceArg < 0 || argv[optind] == nullptr) {
            printHelp(argv[0]);
            return 0;
        }
        wordexp_t exp;
        wordexp((const char*)argv[optind], &exp, 0);
        path = std::string(exp.we_wordv[0]);
        wordfree(&exp);
    }
    // setup mount path
    struct stat st;
    if(stat(path.c_str(), &st) == -1){
        perror("Failed to prepare mountpoint");
        if(mkdir(path.c_str(), 777) == -1){
            perror("Failed to prepare mountpoint");
            return 0;
        }
    }
    
    // get device list
    std::vector<mtp_libusb_device_t> devices;
    auto libusb_list = GetConnectedMtpDevices(devices, context);
    
    if (devices.size() == 0){
        std::cout << "No devices found" << std::endl;
        goto out;
    } else for (size_t i = 0; i < devices.size(); i++) {
        if (deviceArg == (int)i) {
            fscontext_t ctx = { devices[i].dev, &fs };
            if(fs.mount(&ctx, (char*)path.c_str())){
                for (;;){
                    if(fs.inside_fs()){
                        // if the fs code is executing continue looping.
                        pthread_yield_np();
                    }
                    // else wait
                    fs.fs_wait();
                }
            } else {
                fprintf(stderr, "failed to mount fs for device #%d\n", deviceArg);
                goto out;
            };
//            found_device = true;
        }
    }
    
    // if wrong index was selected, say so.
    if (!found_device){
        fprintf(stderr, "Device #%d doesn't exist in the list below:\n", deviceArg);
        for (size_t i = 0; i < devices.size(); i++){
            std::cout << "  " << i <<" : " << devices[i].vendorName << " " << devices[i].productName << " - " << devices[i].serial << std::endl;
        }
    }
    
out:
    libusb_free_device_list(libusb_list, (int)devices.size()); // free list
    libusb_exit(context); // exit context
    return 0;
}

