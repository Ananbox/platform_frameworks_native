#ifndef __UIDHELPER_H__
#define __UIDHELPER_H__
#include <sys/types.h>

typedef struct {
    uid_t  uid;
    char   isDebuggable;
    char   dataDir[PATH_MAX];
    char   seinfo[PATH_MAX];
} PackageInfo;

#define PACKAGES_LIST_FILE "/data/system/packages.list"
#define MAX_CMDLINE_LEN 1024

#ifdef __cplusplus
extern "C"
#endif
uid_t getCallingUidHelper(pid_t pid);
#endif
