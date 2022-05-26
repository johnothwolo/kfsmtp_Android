//
//  fs.h
//  KFS
//
//  Created by John Othwolo on 5/9/22.
//  Copyright © 2022 FadingRed LLC. All rights reserved.
//

#ifndef fs_h
#define fs_h

#include <memory>
#include <string>
#include <unordered_map>
#include <cstdlib>
#include <sys/syslimits.h>
#include "AndroidMtp/AndroidMtpDevice.h"
#include "AndroidMtp/MtpDeviceInfo.h"
#include "AndroidMtp/MtpObjectInfo.h"

extern "C" {
#  include <KFS/KFS.h>
}

struct libusb_device;
class androidfs;

// the two below are for the root folder that contains root folders for each storage device.
enum {
    ROOT_STORAGE_ID                 = ~0, // Storage id for folder containing storage device folders.
    ROOT_FILE_HANDLE                = ~1, // folder handle for folder containing storage device folders.
    STORAGE_DEVICE_FILE_HANDLE      = MTP_PARENT_ROOT, // folder handle for storage device folder.
    INVALID_FILE_HANDLE             = ~3  // invalid handle.
};

// forgot what this exactly means, but i got it from libmtp
enum { MTP_GOH_ALL_FORMATS = 0 };

// types of modification
enum {
    MOD_UTIMES = 0x1,
    MOD_NAME = 0x2,
    MOD_FILEDATA = 0x4,
};

typedef android::MtpStorageInfo *MtpStorageInfo_t;

class mnode_t : public android::MtpObjectInfo {
private:
//    long                      m_usecount;        /* reference count of users */
public:
    std::vector<char>         mData;            /* data for fs read/write */
    bool                      mFetched = false;
    bool                      mModified = false;
    time_t                    mDateAccessed = time(NULL);
    std::vector<mnode_t>      mChildren;
public:
    mnode_t(void); // empty constructor
    mnode_t(const mnode_t& objInfo); // copy constructor
    mnode_t(android::MtpObjectInfo* objInfo); // copy superclass constructor
    mnode_t(android::MtpObjectHandle handle); // superclass-like constructor
    mnode_t(android::MtpObjectHandle handle, char *name); // custom constructor

public:
    void                  push_back(mnode_t node);
    mnode_t*              getChild(const std::string &childName);
    
public:
    const std::string     name()         { return mName; }
    int                   nodeType()     { return mAssociationType; }
    int                   fileId()       { return mHandle; }
    int                   fileSize()     { return mCompressedSize; }
    int                   storageId()    { return mStorageID; }
    time_t                dateAccessed() { return mDateAccessed; }
    time_t                dateModified() { return mDateModified; }
    time_t                dateCreated()  { return mDateCreated; }
    bool                  isModified()   { return mModified; }
    
public:
    bool                  isFolder(){ return mFormat == MTP_FORMAT_ASSOCIATION; }

public:
    void                  setmtime(time_t tm) { mDateModified = tm; }
    void                  setatime(time_t tm) { mDateAccessed = tm; }
    
public:
    mnode_t&              operator [](int i)       { return mChildren[i]; }
    mnode_t               operator [](int i) const { return mChildren[i]; }

};

struct fscontext_t {
    libusb_device *device;
    androidfs *fs;
};

class androidfs {
public:
    androidfs() = default;
   ~androidfs() = default;
public:
    std::vector<std::string> getPathComponents(std::string &path);
    bool inside_fs() { return in_fs; }
    void fs_wait() { pthread_cond_wait(&control_cv, &control_mtx); }
public:
    int mount(fscontext_t *ctx, char *mountPoint);
    void setup_root();
    void buildDirectoryTree();
    mnode_t* root();
    int lookup(std::string &path, mnode_t **mnode, fscontext_t *ctx);

    int symlink(const char *path, const char *value, int *error, fscontext_t *context);
    int readlink(const char *path, char **value, int *error, fscontext_t *context);
    int chmod(const char *path, kfsmode_t mode, int *error, fscontext_t *context);
    int getattr(const char *path, kfsstat_t *result, int *error, fscontext_t *context);
    int mkdir(const char *path, int *error, fscontext_t *context);
    int unlink(const char *path, int *error, fscontext_t *context);
    int rmdir(const char *path, int *error, fscontext_t *context);
    int rename(const char *path, const char *new_path, int *error, fscontext_t *context);
    int truncate(const char *path, uint64_t size, int *error, fscontext_t *context);
    int utime(const char *path, const kfstime_t *atime, const kfstime_t *mtime, int *error, fscontext_t *context);
    int read(const char *path, char *buf, size_t offset, size_t length, int *error, fscontext_t *context);
    int write(const char *path, const char *buf, size_t offset, size_t length, int *error, fscontext_t *context);
    int statfs(const char *path, kfsstatfs_t *result, int *error, fscontext_t *context);
    int readdir(const char *path, kfscontents_t *contents, int *error, fscontext_t *context);
    int create(const char *path, int *error, fscontext_t *context);
    int remove(const char *path, int *error, fscontext_t *context);
    int fsync(const char *path, int *error, fscontext_t *context);
    int open(const char *path, int flags);
    int truncate(const char *path, off_t new_size);

private:
    bool hasPartialObjectSupport();
    
    void fs_in(){
        in_fs = 1;
        pthread_cond_signal(&control_cv);
    }

    void fs_out(){
        in_fs = 0;
    }

    
private:
    kfsfilesystem_t m_kfs_filesystem;
    kfsid_t m_kfs_id;
    fscontext_t m_kfs_context;
    android::AndroidMtpDevice *m_device = nullptr; // androidmtp handles synchronization
    android::MtpDeviceInfo *m_deviceInfo = nullptr;
    bool mStorageDeviceFoldersInitialized = false;
    std::vector<mnode_t*> m_modifiedNodes;
    std::vector<MtpStorageInfo_t> m_storageInfo;
    pthread_mutex_t control_mtx = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t control_cv = PTHREAD_COND_INITIALIZER;
    int in_fs = 0;
    mnode_t m_root;
    /*
     The root node will contain the root folders for each storage device (e.g. if the phone has internal and sdcard, there will 2 folders, 1 for each).
     If there's only one storage device (usually internal storage), there will be only one folder for it.
     This root node directory will be immutable, and will only have readonly permissions.
     The attributes might also be set to 0 or some random junk.
     TODO: maybe make a fallback mode root mode for devices with only internal storage.
     */
};

#endif /* fs_h */
