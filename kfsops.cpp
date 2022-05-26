//
//  vnops.cpp
//  kfs_mtpAndroid
//
//  Created by John Othwolo on 5/10/22.
//  Copyright © 2022 FadingRed LLC. All rights reserved.
//

#include <stdio.h>
#include "fs.h"
#include "kfsops.h"

bool fs_getattr(const char *path, kfsstat_t *result, int *error, void *context)
{
    fscontext_t *ctx = (fscontext_t*)context;
    androidfs *fs = (androidfs*)ctx->fs;

    int ret = fs->getattr(path, result, error, (fscontext_t*)context);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}

bool fs_mkdir(const char *path, int *error, void *context)
{
    fscontext_t *ctx = (fscontext_t*)context;
    int ret = ((androidfs*)ctx->fs)->mkdir(path, error, (fscontext_t*)context);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}

bool fs_rmdir(const char *path, int *error, void *context)
{
    fscontext_t *ctx = (fscontext_t*)context;
    int ret = ((androidfs*)ctx->fs)->rmdir(path, error, (fscontext_t*)context);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}

bool fs_rename(const char *path, const char *newpath, int *error, void *context)
{
    fscontext_t *ctx = (fscontext_t*)context;
    int ret = ((androidfs*)ctx->fs)->rename(path, newpath, error, (fscontext_t*)context);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}

bool fs_utime(const char *path, const kfstime_t *atime, const kfstime_t *mtime, int *error, void *context)
{
    fscontext_t *ctx = (fscontext_t*)context;
    int ret = ((androidfs*)ctx->fs)->utime(path, atime, mtime, error, (fscontext_t*)context);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}

ssize_t fs_read(const char *path, char *buf, size_t offset, size_t length, int *error, void *context)
{
    fscontext_t *ctx = (fscontext_t*)context;
    int ret = ((androidfs*)ctx->fs)->read(path, buf, offset, length, error, (fscontext_t*)context);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}

ssize_t fs_write(const char *path, const char *buf, size_t offset, size_t length, int *error, void *context)
{
    fscontext_t *ctx = (fscontext_t*)context;
    int ret = ((androidfs*)ctx->fs)->write(path, buf, offset, length, error, (fscontext_t*)context);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}

bool fs_truncate(const char *path, uint64_t size, int *error, void *context)
{
    fscontext_t *ctx = (fscontext_t*)context;
    int ret = ((androidfs*)ctx->fs)->truncate(path, size);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}

bool fs_statfs(const char *path, kfsstatfs_t *result, int *error, void *context)
{
    fscontext_t *ctx = (fscontext_t*)context;
    int ret = ((androidfs*)ctx->fs)->statfs(path, result, error, (fscontext_t*)context);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}

bool fs_readdir(const char *path, kfscontents_t *contents, int *error, void *context)
{
    fscontext_t *ctx = (fscontext_t*)context;
    int ret = ((androidfs*)ctx->fs)->readdir(path, contents, error, (fscontext_t*)context);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}

bool fs_symlink(const char *path, const char *value, int *error, void *context){
    return false;
}

bool fs_readlink(const char *path, char **value, int *error, void *context) {
    return false;
}

bool fs_chmod(const char *path, kfsmode_t mode, int *error, void *context) {
    return false;
}

bool fs_create(const char *path, int *error, void *context)
{
    fscontext_t *ctx = (fscontext_t*)context;
    int ret = ((androidfs*)ctx->fs)->create(path, error, (fscontext_t*)context);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}

bool fs_remove(const char *path, int *error, void *context){
    fscontext_t *ctx = (fscontext_t*)context;
    int ret = ((androidfs*)ctx->fs)->unlink(path, error, (fscontext_t*)context);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}

bool fs_fsync(const char *path, int *error, void *context){
    fscontext_t *ctx = (fscontext_t*)context;
    int ret = ((androidfs*)ctx->fs)->fsync(path, error, (fscontext_t*)context);
    if(ret == 0){
        *error = 0;
        return true;
    }
    else {
        *error = ret;
        return false;
    }
}
