//
//  mnode.cpp
//  kfs_mtpAndroid
//
//  Created by John Othwolo on 5/10/22.
//  Copyright © 2022 FadingRed LLC. All rights reserved.
//

#include <stdio.h>
#include "AndroidMtp/mtp.h"
#include "AndroidMtp/MtpObjectInfo.h"
#include "fs.h"

//struct old_mnode {
//private:
//    std::string               m_name; // name?
//    int                       m_id; // node id
//    int                       m_storageId; // storage id
//    android::MtpObjectInfo   *m_objectInfo = nullptr;
//    void                     *m_data = nullptr;            /* private data for fs */
//    mnode_t                  *m_parent = nullptr;
//    bool                      m_modified = false;
//    long                      m_atime; // access time
//    long                      m_mtime; // mod time
//    std::vector<struct mnode> m_children;
////    long                      m_usecount;        /* reference count of users */
//public:
//    mnode(struct mnode *parent,
//          int storageId,
//          android::MtpObjectHandle handle,
//          android::MtpObjectInfo *objInfo);
//    mnode(struct mnode *parent,
//          int storageId,
//          android::MtpObjectHandle handle,
//          std::string name);
//    mnode();
//    ~mnode();
//public:
//    int                   nodeType() { return m_objectInfo->mAssociationType; };
//    bool                  isFolder();
//    void                  push_back(mnode_t mnode);
//    void                  setObjectInfo(android::MtpObjectInfo *info) { m_objectInfo = info; };
//    mnode_t              *get_child(const std::string &cpm);
//    const std::string     getName() { if (m_name.empty()) return m_objectInfo->mName;
//                                      else return m_name; };
//    int                   fileId()  { return m_id; };
//    int                   storageId() { return m_storageId; };
//    bool                  isModified() { return m_modified; };
//    void                  setmtime(long time) { m_mtime = time; };
//    void                  setatime(long time) { m_atime = time; };
//    int                   childrenCount() { return (int)m_children.size(); };
//    int                   fileSize()  { return (int)m_objectInfo->mCompressedSize; };
//    int                   dateCreated()  { return (int)m_objectInfo->mDateCreated; };
//    int                   dateModified() { return (int)m_objectInfo->mDateModified; };
//};

//mnode::mnode(mnode_t *parent,
//             int storageId,
//             android::MtpObjectHandle handle,
//             android::MtpObjectInfo *objInfo) :
//m_parent(parent), m_id(handle), m_objectInfo(objInfo), m_storageId(storageId)
//{}
//
//
//mnode::mnode(mnode_t *parent,
//             int storageId,
//             android::MtpObjectHandle handle,
//             std::string name) :
//m_parent(parent), m_id(handle), m_name(name), m_storageId(storageId)
//{}
//
//mnode::mnode()
//{}
//
//mnode::~mnode()
//{}


mnode_t::mnode_t(void)
:android::MtpObjectInfo::MtpObjectInfo(0)
{
    mHandle = 0;
    mStorageID = 0;
    mFormat = 0;
    mProtectionStatus = 0;
    mCompressedSize = 0;
    mThumbFormat = 0;
    mThumbCompressedSize = 0;
    mThumbPixWidth = 0;
    mThumbPixHeight = 0;
    mImagePixWidth = 0;
    mImagePixHeight = 0;
    mImagePixDepth = 0;
    mParent = 0;
    mAssociationType = 0;
    mAssociationDesc = 0;
    mSequenceNumber = 0;
    mName = nullptr;
    mDateCreated = 0;
    mDateModified = 0;
    mKeywords = nullptr;
}

mnode_t::mnode_t(android::MtpObjectHandle handle)
:android::MtpObjectInfo::MtpObjectInfo(handle)
{
    mStorageID = 0;
    mFormat = 0;
    mProtectionStatus = 0;
    mCompressedSize = 0;
    mThumbFormat = 0;
    mThumbCompressedSize = 0;
    mThumbPixWidth = 0;
    mThumbPixHeight = 0;
    mImagePixWidth = 0;
    mImagePixHeight = 0;
    mImagePixDepth = 0;
    mParent = 0;
    mAssociationType = 0;
    mAssociationDesc = 0;
    mSequenceNumber = 0;
    mName = nullptr;
    mDateCreated = 0;
    mDateModified = 0;
    mKeywords = nullptr;
}

mnode_t::mnode_t(const mnode_t& objInfo):
MtpObjectInfo::MtpObjectInfo(objInfo.mHandle)
{
    if (objInfo.mName == nullptr || objInfo.mKeywords == nullptr)
        throw "BUG: object strings are null!???";
    
    mHandle = objInfo.mHandle;
    mStorageID = objInfo.mStorageID;
    mFormat = objInfo.mFormat;
    mProtectionStatus = objInfo.mProtectionStatus;
    mCompressedSize = objInfo.mCompressedSize;
    mThumbFormat = objInfo.mThumbFormat;
    mThumbCompressedSize = objInfo.mThumbCompressedSize;
    mThumbPixWidth = objInfo.mThumbPixWidth;
    mThumbPixHeight = objInfo.mThumbPixHeight;
    mImagePixWidth = objInfo.mImagePixWidth;
    mImagePixHeight = objInfo.mImagePixHeight;
    mImagePixDepth = objInfo.mImagePixDepth;
    mParent = objInfo.mParent;
    mAssociationType = objInfo.mAssociationType;
    mAssociationDesc = objInfo.mAssociationDesc;
    mSequenceNumber = objInfo.mSequenceNumber;
    mName = strdup(objInfo.mName);
    mDateCreated = objInfo.mDateCreated;
    mDateModified = objInfo.mDateModified;
    mKeywords = strdup(objInfo.mKeywords);
}

// we won't strdup string here because objInfo is dynamically allocated
mnode_t::mnode_t(android::MtpObjectInfo* objInfo):
MtpObjectInfo::MtpObjectInfo(objInfo->mHandle)
{
    mHandle = objInfo->mHandle;
    mStorageID = objInfo->mStorageID;
    mFormat = objInfo->mFormat;
    mProtectionStatus = objInfo->mProtectionStatus;
    mCompressedSize = objInfo->mCompressedSize;
    mThumbFormat = objInfo->mThumbFormat;
    mThumbCompressedSize = objInfo->mThumbCompressedSize;
    mThumbPixWidth = objInfo->mThumbPixWidth;
    mThumbPixHeight = objInfo->mThumbPixHeight;
    mImagePixWidth = objInfo->mImagePixWidth;
    mImagePixHeight = objInfo->mImagePixHeight;
    mImagePixDepth = objInfo->mImagePixDepth;
    mParent = objInfo->mParent;
    mAssociationType = objInfo->mAssociationType;
    mAssociationDesc = objInfo->mAssociationDesc;
    mSequenceNumber = objInfo->mSequenceNumber;
    mName = objInfo->mName;
    mDateCreated = objInfo->mDateCreated;
    mDateModified = objInfo->mDateModified;
    mKeywords = objInfo->mKeywords;
    // our class
}

// caller is responsible for what happens to name...
mnode_t::mnode_t(android::MtpObjectHandle handle, char *name):
android::MtpObjectInfo::MtpObjectInfo(handle)
{
    // never free memory you didn't malloc.
    // ~MtpObjectHandle() frees mName, and if 'name' is a hardcoded string, we'll probably crash.
    // so we always duplicate, because ~MtpObjectHandle() will always free.
    mName = strdup(name);
     // we don't have a keyword with his constructor, so to prevent a null free, we just duplicate name into mKeywords.
    mKeywords = strdup(name);
    mFetched = true; // this directory's files are custom made so it doesn't need to be fetched
}

mnode_t* mnode_t::getChild(const std::string &childName)
{
    // search for child in cache and return
    for(auto &child : mChildren){
        if (child.name() == childName)
            return &child;
    }
    // if it doesn't exist, return null so that
    return nullptr;
}


void mnode_t::push_back(mnode_t mnode)
{
    this->mChildren.push_back(mnode);
}
