//
//  kfsops.h
//  KFS
//
//  Created by John Othwolo on 5/9/22.
//  Copyright © 2022 FadingRed LLC. All rights reserved.
//

#ifndef kfsops_h
#define kfsops_h

bool fs_getattr(const char *path, kfsstat_t *result, int *error, void *context);
bool fs_mkdir(const char *path, int *error, void *context);
bool fs_rmdir(const char *path, int *error, void *context);
bool fs_rename(const char *path, const char *newpath, int *error, void *context);
bool fs_utime(const char *path, const kfstime_t *atime, const kfstime_t *mtime, int *error, void *context);
ssize_t fs_read(const char *path, char *buf, size_t offset, size_t length, int *error, void *context);
ssize_t fs_write(const char *path, const char *buf, size_t offset, size_t length, int *error, void *context);
bool fs_truncate(const char *path, uint64_t size, int *error, void *context);
bool fs_statfs(const char *path, kfsstatfs_t *result, int *error, void *context);
bool fs_readdir(const char *path, kfscontents_t *contents, int *error, void *context);
bool fs_symlink(const char *path, const char *value, int *error, void *context);
bool fs_readlink(const char *path, char **value, int *error, void *context);
bool fs_chmod(const char *path, kfsmode_t mode, int *error, void *context);
bool fs_create(const char *path, int *error, void *context);
bool fs_remove(const char *path, int *error, void *context);
bool fs_fsync(const char *path, int *error, void *context);

#endif /* kfsops_h */
