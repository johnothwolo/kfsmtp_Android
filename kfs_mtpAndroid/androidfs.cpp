//
//  fs.cpp
//  kfs_mtpAndroid
//
//  Created by John Othwolo on 5/9/22.
//  Copyright © 2022 FadingRed LLC. All rights reserved.
//

#include <signal.h>
#include <stdio.h>
#include <libgen.h>
#include <vector>
#include <sstream>
#include "AndroidMtp/MtpTypes.h"
#include "AndroidMtp/MtpProperty.h"
#include "AndroidMtp/MtpObjectInfo.h"
#include "AndroidMtp/MtpDeviceInfo.h"
#include "AndroidMtp/MtpStorageInfo.h"
#include "AndroidMtp/MtpEventPacket.h"
#include "fs.h"
#include "kfsops.h"



static std::string cutLastComponent(std::string path){
    std::string ret;
    char *ptr = (char*)path.c_str();
    
    // skip to end
    while(*ptr != 0) ptr++;
    
    // back up to last '/' and temporarily turn it into '\0'
    while(*ptr != '/') ptr--;
    *ptr = '\0';
    ret = std::string(path.c_str());
    *ptr = '/'; // fix the path
    return ret;
}

std::vector<std::string>
androidfs::getPathComponents(std::string &path) {
    std::vector<std::string> cnlist;
    std::string componentName;
    std::stringstream ss(path);
    while (std::getline(ss, componentName, '/')) {
        // the first loop will have an empty string
        if (componentName.empty())
            continue;
        cnlist.push_back(componentName);
    }
    return cnlist;
}

// called in mount()
void
androidfs::setup_root()
{
    // setup root node
    // set root node to name of device
    std::stringstream name;
    name << m_deviceInfo->mManufacturer<<" "<<m_deviceInfo->mModel;
    m_root = mnode_t(ROOT_FILE_HANDLE, (char*)name.str().c_str());
    // set type to folder
    m_root.mAssociationType = MTP_ASSOCIATION_TYPE_GENERIC_FOLDER;
    m_root.mFormat = MTP_FORMAT_ASSOCIATION;
    m_root.mParent = INVALID_FILE_HANDLE;
    m_root.mThumbFormat = 0;
    m_root.mProtectionStatus = 0;
    m_root.mCompressedSize = 0;
    m_root.mThumbCompressedSize = 0;
    m_root.mThumbPixWidth = 0;
    m_root.mThumbPixHeight = 0;
    m_root.mImagePixWidth = 0;
    m_root.mImagePixHeight = 0;
    m_root.mImagePixDepth = 0;
    m_root.mAssociationDesc = 0;
    m_root.mSequenceNumber = 0;
    m_root.mDateCreated = time(nullptr);
    m_root.mDateModified = time(nullptr);
    
    // setup device storage folders
    if(!mStorageDeviceFoldersInitialized){
        for (auto storage : m_storageInfo) {
            auto node = mnode_t(STORAGE_DEVICE_FILE_HANDLE, storage->mStorageDescription);
            // set type to folder
            node.mAssociationType = MTP_ASSOCIATION_TYPE_GENERIC_FOLDER;
            node.mFormat = MTP_FORMAT_ASSOCIATION;
            node.mParent = INVALID_FILE_HANDLE;
            node.mThumbFormat = 0;
            node.mProtectionStatus = 0;
            node.mCompressedSize = 0;
            node.mThumbCompressedSize = 0;
            node.mThumbPixWidth = 0;
            node.mThumbPixHeight = 0;
            node.mImagePixWidth = 0;
            node.mImagePixHeight = 0;
            node.mImagePixDepth = 0;
            node.mAssociationDesc = 0;
            node.mSequenceNumber = 0;
            node.mDateCreated = time(nullptr);
            node.mDateModified = time(nullptr);
            node.mStorageID = storage->mStorageID; // set storageId for each storage device folder
            // initialize a new cached node and insert it
            m_root.push_back(node);
        }
        mStorageDeviceFoldersInitialized = true;
    }
};


mnode_t*
androidfs::root(){
    return &m_root;
}


int
androidfs::lookup(std::string &path, mnode_t **mnodep, fscontext_t *ctx){
    std::istringstream ss(path);
    mnode_t *currentNode = this->root();
    auto components = getPathComponents(path);
    
    // TODO: fastpath (sorta) for single storage device
    // if there's only one storage device, append its name
//    if (m_root->childrenCount() == 1)
//        path = '/' + m_root->getName() + path;
    
    if (path == "/"){
        *mnodep = currentNode;
        return 0;
    }
//    else {
//        raise(SIGTRAP);
//    }
    
    for (int i = 0; i < (int)components.size(); i++) {
        bool found = false;
        mnode_t *tmpNode = nullptr;
        std::string &componentName = components[i];
        // if this component isn't the last component name
        // and it isn't a folder return ENOTDIR...
        // e.g. invalid input like "/fol/fol/fol/file/fol/fol"
//        if (i < ((int)components.size()-1) && currentNode->isFolder()) {
//            return -KFSERR_NOTDIR;
//        }
                
        // check for a cached child node matching this dir or file name.
        tmpNode = currentNode->getChild(componentName);
        
        // if the child isn't in our cache get its object
        // then initialize a new node with the object
        if (tmpNode == nullptr) {
            // if we didn't find the directory in the special root folder, return einval.
            // The network's probably looking for something that isn't a storage device folder
            if (currentNode == &m_root){
                return KFSERR_NOENT;
            }
            
            // if the directory was fetched, then the file simply doesn't exist
            if (currentNode->mFetched)
                return KFSERR_NOENT;
            
            // get list of handles in this directory
            auto objList = m_device->getObjectHandles(currentNode->storageId(), MTP_GOH_ALL_FORMATS, currentNode->fileId());
            
            // loop through object handles and cache every handle we go over...
            // until we find our matching file
            for (int j = 0; j < (int)objList->size(); j++) {
                // get object info for this handle
                auto objInfo = m_device->getObjectInfo(j);
                // if there was an error just continue to next handle
                if (objInfo == nullptr)
                    continue;
                // initialize a new cached node and insert it
                currentNode->push_back(mnode_t(objInfo));
                
                // if the node matches, just set tmpNode then let the caching finish
                if (componentName == std::string(objInfo->mName)){
                    found = true;
                    tmpNode = currentNode->getChild(componentName);
                }
                
            }
            
            if (found != true && tmpNode == nullptr)
                return -KFSERR_NOENT;
        }
        
        currentNode = tmpNode;
    }

    *mnodep = currentNode;
    
    return 0;
}

int
androidfs::mount(fscontext_t *ctx, char *mountPoint){
    // Connect to MTP device
    m_device = android::AndroidMtpDevice::init(ctx->device);
    if (!m_device) return false;
    // open session
    m_device->initialize();

    // get storage info
    auto stids = m_device->getStorageIDs();
    for (unsigned i = 0; i < stids->size(); i++) {
        auto stinfo = m_device->getStorageInfo(stids->at(i));
        m_storageInfo.push_back(stinfo);
    }
    
    m_deviceInfo = m_device->getDeviceInfo(); // get device info...
    setup_root(); // setup m_root
    
    // TODO: get capabilities...?
    
    kfsoptions_t opts = {mountPoint};
    
    m_kfs_filesystem.options = opts;
    m_kfs_filesystem.context = ctx;
    
    // write support
//    m_kfs_filesystem.create = fs_create;
//    m_kfs_filesystem.write = fs_write;
//    m_kfs_filesystem.remove = fs_remove;
//    m_kfs_filesystem.rename = fs_rename;
//    m_kfs_filesystem.truncate = fs_truncate;
//    m_kfs_filesystem.mkdir = fs_mkdir;
//    m_kfs_filesystem.rmdir = fs_rmdir;
    
    // read support
    m_kfs_filesystem.read = fs_read;
    m_kfs_filesystem.statfs = fs_statfs;
    m_kfs_filesystem.stat = fs_getattr;
    m_kfs_filesystem.utimes = fs_utime;
    m_kfs_filesystem.readdir = fs_readdir;
    
    // not supported on mtp
    m_kfs_filesystem.symlink = fs_symlink;    // 0
    m_kfs_filesystem.readlink = fs_readlink;  // 0
    m_kfs_filesystem.chmod = fs_chmod;        // 0
    
    m_kfs_id = kfs_mount(&m_kfs_filesystem);
    if (m_kfs_id < 0) {
        throw ("mount error: " + std::string(strerror(errno)));
        return false;
    }
    return true;
}

int androidfs::getattr(const char *cpath, kfsstat_t *result, int *error, fscontext_t *context)
{
    fs_in();
    int ret = 0;
    mnode_t *node;
    std::string path(cpath);
    
    memset(result, 0, sizeof(struct kfsstat));
    
    // lookup node
    ret = lookup(path, &node, context);
    if (ret != 0){
        *error = ret;
        goto out;
    }
    
    if (node->isFolder()) {
        result->type = KFS_DIR;
        result->size = 512;
        result->used = 512;
        result->mode = (kfsmode_t)(S_IFDIR | 0775);
        result->mtime.nsec = node->dateModified();
        result->ctime.nsec = node->dateCreated();
        result->atime.nsec = node->dateAccessed();
    } else if (node->nodeType() == MTP_FORMAT_UNDEFINED) {
        *error = KFSERR_NOENT;
        goto out;
    } else {
        result->type = KFS_REG;
        result->size = node->fileSize();
        result->mode = static_cast<kfsmode_t>(result->mode | 0644);
        result->mtime.nsec = node->dateModified();
        result->atime.nsec = node->dateAccessed();
        result->ctime.nsec = node->dateCreated();
        result->used = (node->fileSize() / 512) + (node->fileSize() % 512 > 0 ? 1 : 0);
    }
    
out:
    fs_out();
    return ret;
}


int androidfs::mkdir(const char *cpath, int *error, fscontext_t *context)
{
    fs_in();
    int ret = 0, newObjectId;
    mnode_t *parentNode, newChildNode;
    std::string lookupPath = cutLastComponent(cpath); // cut last component
    
    raise(SIGTRAP);
    
    ret = lookup(lookupPath, &parentNode, context);
    if (ret != 0){
        *error = ret;
        goto out;
    }
    
    // cannot create directory in root node!!!
    if (parentNode == &m_root)
        return EINVAL;
        
    // create associated ObjectInfo
    newChildNode.mCompressedSize = 0;
    newChildNode.mFormat = MTP_FORMAT_ASSOCIATION;
    newChildNode.mProtectionStatus = 0;
    newChildNode.mAssociationType = MTP_ASSOCIATION_TYPE_GENERIC_FOLDER;
    newChildNode.mParent = parentNode->fileId();
    newChildNode.mStorageID = parentNode->storageId();
    newObjectId = m_device->sendObjectInfo(&newChildNode);
    newChildNode.mHandle = newObjectId;
    // TODO: create mnode...
    
out:
    fs_out();
    return ret;
}

int androidfs::unlink(const char *cpath, int *error, fscontext_t *context)
{
    fs_in();
    int ret = 0;
    mnode_t *node;
    std::string path(cpath);
    android::MtpObjectInfo newObjectInfo(0); // create object
    
    // lookup node
    ret = lookup(path, &node, context);
    if (ret != 0){
        *error = ret;
        goto out;
    }
    
    // TODO: ...
    // don't forget, if the node is a directory and there's
    // stuff in it, fail the delete.
    
    // delete object
    if (m_device->deleteObject(node->fileId())){
        *error = EINVAL;
        goto out;
    }
out:
    fs_out();
    return ret;
}

int androidfs::rmdir(const char *path, int *error, fscontext_t *context)
{
    fs_in();
    int ret = ENOSYS;
    raise(SIGTRAP);
    fs_out();
    return ret;
}

int androidfs::rename(const char *cpath, const char *newpath, int *error, fscontext_t *context)
{
    fs_in();
    int ret = 0;
    std::string path(cpath);
    mnode_t *existingNode, newNode;
    
    raise(SIGTRAP);
    
    // TODO: check if the storage for this file is writable
    // TODO: also check if the mount was mounted ro/rw
    android::MtpProperty objectProp(MTP_PROPERTY_NAME, MTP_TYPE_STR, /*check comment above*/true, 0);
    
    // get our node
    ret = lookup(path, &existingNode, context);
    if (ret != 0){
        *error = ret;
        goto out;
    }
    
    // check if it's modified
    if (existingNode->isModified()){
        // push the node if it's modified.
        // change the modification time too.
    }
    
    // set the object property (the filename) then send
    objectProp.setCurrentValue(basename((char*)newpath));
    if (!m_device->setObjectPropValue(existingNode->fileId(), &objectProp)){
        *error = KFSERR_IO;
        goto out;
    }
    
out:
    fs_out();
    return ret;
}

int androidfs::utime(const char *cpath, const kfstime_t *atime, const kfstime_t *mtime, int *error, fscontext_t *context)
{
    fs_in();
    int ret = 0;
    std::string path(cpath);
    mnode_t *node;
    
    // get our node
    ret = lookup(path, &node, context);
    if (ret != 0){
        *error = ret;
        fs_out();
        return -ret;
    }
    
    // set times
    node->setatime(atime->sec);
    node->setmtime(mtime->sec);
    node->mModified = true; // set modified
    m_modifiedNodes.push_back(node); // add to list of modified nodes
    
    fs_out();
    return 0;
}

int androidfs::create(const char *cpath, int *error, fscontext_t *context)
{
    fs_in();
    int ret = 0, newObjectId;
    mnode_t *parentNode, newChildNode;
    android::MtpObjectInfo *newObjectInfo; // create object
    std::string lookupPath = cutLastComponent(cpath); // cut last component
    
    ret = lookup(lookupPath, &parentNode, context);
    if (ret != 0){
        *error = ret;
        goto out;
    }
    
    raise(SIGTRAP);
    
    newObjectInfo = new android::MtpObjectInfo(0);
    
    // create associated ObjectInfo
    newObjectInfo->mCompressedSize = 0;
    newObjectInfo->mFormat = MTP_FORMAT_UNDEFINED;
    newObjectInfo->mProtectionStatus = 0;
    newObjectInfo->mAssociationType = 0;
    newObjectInfo->mParent = parentNode->fileId();
    newObjectInfo->mStorageID = parentNode->storageId();
    newObjectId = m_device->sendObjectInfo(newObjectInfo);
    newObjectInfo->mHandle = newObjectId;
    // TODO: create mnode...
    newChildNode = mnode_t(newObjectInfo);
out:
    fs_out();
    return ret;
}

// this needs more research
int androidfs::fsync(const char *path, int *error, fscontext_t *context)
{
    fs_in();
    int rval;
    
    raise(SIGTRAP);
    
    for (auto &node : m_modifiedNodes) {
        m_device->sendObjectInfo(node);
    }
    
    raise(SIGTRAP);
    
    fs_out();
    return 0;
}

int androidfs::open(const char *cpath, int flags)
{
    fs_in();
    const std::string path(cpath);
    raise(SIGTRAP);
    fs_out();
    return 0;
}

static bool read_cb(void* data, uint32_t offset, uint32_t length, void* clientBuffer)
{
    memcpy(clientBuffer, data, length);
    return true;
}

int androidfs::read(const char *cpath, char *buf, size_t offset, size_t length, int *error, fscontext_t *context)
{
    fs_in();
    int ret = 0;
    unsigned size;
    mnode_t *node;
    std::string path = cpath; // cut last component
    
    ret = lookup(path, &node, context);
    if (ret != 0){
        *error = ret;
        goto out;
    }
    
    // resize our vector
    node->mData.resize(length);
    
    // TODO: fix
    if (hasPartialObjectSupport()) {
        m_device->readPartialObject(node->mHandle, (unsigned)offset,
                                    (unsigned)length, &size, read_cb, buf);
    }
    else {
        raise(SIGTRAP);
    }
    
out:
    fs_out();
    return ret;
}

int androidfs::write(const char *path, const char *buf, size_t offset, size_t length, int *error, fscontext_t *context)
{
    fs_in();
    ssize_t rval = 0;
    
    fs_out();
    return ((int) rval);
}

int androidfs::statfs(const char *path, kfsstatfs_t *result, int *error, fscontext_t *context)
{
    fs_in();
    uint64_t bs = 1024;
    int total = 0, total_free = 0;
    
    for (auto st : m_storageInfo){
        total += st->mMaxCapacity;
        total_free += st->mFreeSpaceBytes;
    }
    
    result->size = total / bs;
    result->free = total_free / bs;
    fs_out();
    return 0;
}


int androidfs::readdir(const char *cpath, kfscontents_t *contents, int *error, fscontext_t *context)
{
    fs_in();
    
    std::string path(cpath);
    mnode_t *node;
    int ret;
    
    // get node
    ret = lookup(path, &node, context);
    if (ret != 0){
        *error = ret;
        fs_out();
        return -ret;
    }
    
    // append these two or else there will be an infinte loop
    kfscontents_append(contents, ".");
    kfscontents_append(contents, "..");
    
    // if the directory node is empty
    // fetch nodes from device and append them. The root node will always be populated.
    if (node->mChildren.size() == 0 && !node->mFetched && !node->mModified){
        // get list of handles in this directory
        auto objList = m_device->getObjectHandles(node->storageId(), MTP_FORMAT_UNDEFINED, node->fileId());
        
        if (objList == nullptr){
            return EIO;
        }
        
        // loop through object handles and cache every handle we go over...
        for (int j = 0; j < (int)objList->size(); j++) {
            // get object info for this handle
            auto objInfo = m_device->getObjectInfo(j);
            // if there was an error just continue to next handle
            if (objInfo == nullptr)
                continue;
            // initialize a new cached node and insert it
            node->push_back(mnode_t(objInfo));
            // append node name
            kfscontents_append(contents, objInfo->mName);
        }
        node->mFetched = true;
    } else for (auto child : node->mChildren) {
        // append cached node names
        kfscontents_append(contents, child.mName);
    }
    
    fs_out();
    return 0;
}

int androidfs::truncate(const char *path, off_t new_size)
{
    fs_in();
    raise(SIGTRAP);
    fs_out();
    return 0;
}

bool androidfs::hasPartialObjectSupport() {
    for (auto op : *m_deviceInfo->mOperations) {
        if (op == MTP_OPERATION_GET_PARTIAL_OBJECT_64)
            return true;
    }
    return false;
}
