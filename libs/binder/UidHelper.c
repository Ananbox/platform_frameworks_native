#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <binder/UidHelper.h>

static const char*
string_copy(char* dst, size_t dstlen, const char* src, size_t srclen)
{
    const char* srcend = src + srclen;
    const char* dstend = dst + dstlen;
    if (dstlen == 0)
        return src;
    dstend--; /* make room for terminating zero */
    while (dst < dstend && src < srcend && *src != '\0')
        *dst++ = *src++;
    *dst = '\0'; /* zero-terminate result */
    return src;
}

/* Open 'filename' and map it into our address-space.
 * Returns buffer address, or NULL on error
 * On exit, *filesize will be set to the file's size, or 0 on error
 */
static void*
map_file(const char* filename, size_t* filesize)
{
    int  fd, ret, old_errno;
    struct stat  st;
    size_t  length = 0;
    void*   address = NULL;
    *filesize = 0;
    /* open the file for reading */
    fd = TEMP_FAILURE_RETRY(open(filename, O_RDONLY));
    if (fd < 0) {
        return NULL;
    }
    /* get its size */
    ret = TEMP_FAILURE_RETRY(fstat(fd, &st));
    if (ret < 0)
        goto EXIT;
    /* Ensure that the size is not ridiculously large */
    length = (size_t)st.st_size;
    if ((off_t)length != st.st_size) {
        errno = ENOMEM;
        goto EXIT;
    }
    /* Memory-map the file now */
    do {
        address = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    } while (address == MAP_FAILED && errno == EINTR);
    if (address == MAP_FAILED) {
        address = NULL;
        goto EXIT;
    }
    /* We're good, return size */
    *filesize = length;
EXIT:
    /* close the file, preserve old errno for better diagnostics */
    old_errno = errno;
    close(fd);
    errno = old_errno;
    return address;
}

/* unmap the file, but preserve errno */
static void
unmap_file(void*  address, size_t  size)
{
    int old_errno = errno;
    TEMP_FAILURE_RETRY(munmap(address, size));
    errno = old_errno;
}

/* Return TRUE iff a character is a space or tab */
static inline int
is_space(char c)
{
    return (c == ' ' || c == '\t');
}
/* Skip any space or tab character from 'p' until 'end' is reached.
 * Return new position.
 */
static const char*
skip_spaces(const char*  p, const char*  end)
{
    while (p < end && is_space(*p))
        p++;
    return p;
}
/* Skip any non-space and non-tab character from 'p' until 'end'.
 * Return new position.
 */
static const char*
skip_non_spaces(const char* p, const char* end)
{
    while (p < end && !is_space(*p))
        p++;
    return p;
}
/* Find the first occurence of 'ch' between 'p' and 'end'
 * Return its position, or 'end' if none is found.
 */
static const char*
find_first(const char* p, const char* end, char ch)
{
    while (p < end && *p != ch)
        p++;
    return p;
}
/* Check that the non-space string starting at 'p' and eventually
 * ending at 'end' equals 'name'. Return new position (after name)
 * on success, or NULL on failure.
 *
 * This function fails is 'name' is NULL, empty or contains any space.
 */
static const char*
compare_name(const char* p, const char* end, const char* name)
{
    /* 'name' must not be NULL or empty */
    if (name == NULL || name[0] == '\0' || p == end)
        return NULL;
    /* compare characters to those in 'name', excluding spaces */
    while (*name) {
        /* note, we don't check for *p == '\0' since
         * it will be caught in the next conditional.
         */
        if (p >= end || is_space(*p))
            goto BAD;
        if (*p != *name)
            goto BAD;
        p++;
        name++;
    }
    /* must be followed by end of line or space */
    if (p < end && !is_space(*p))
        goto BAD;
    return p;
BAD:
    return NULL;
}
/* Parse one or more whitespace characters starting from '*pp'
 * until 'end' is reached. Updates '*pp' on exit.
 *
 * Return 0 on success, -1 on failure.
 */
static int
parse_spaces(const char** pp, const char* end)
{
    const char* p = *pp;
    if (p >= end || !is_space(*p)) {
        errno = EINVAL;
        return -1;
    }
    p   = skip_spaces(p, end);
    *pp = p;
    return 0;
}
/* Parse a positive decimal number starting from '*pp' until 'end'
 * is reached. Adjust '*pp' on exit. Return decimal value or -1
 * in case of error.
 *
 * If the value is larger than INT_MAX, -1 will be returned,
 * and errno set to EOVERFLOW.
 *
 * If '*pp' does not start with a decimal digit, -1 is returned
 * and errno set to EINVAL.
 */
static int
parse_positive_decimal(const char** pp, const char* end)
{
    const char* p = *pp;
    int value = 0;
    int overflow = 0;
    if (p >= end || *p < '0' || *p > '9') {
        errno = EINVAL;
        return -1;
    }
    while (p < end) {
        int      ch = *p;
        unsigned d  = (unsigned)(ch - '0');
        int      val2;
        if (d >= 10U) /* d is unsigned, no lower bound check */
            break;
        val2 = value*10 + (int)d;
        if (val2 < value)
            overflow = 1;
        value = val2;
        p++;
    }
    *pp = p;
    if (overflow) {
        errno = EOVERFLOW;
        value = -1;
    }
    return value;
}

// ananbox: get_package_info() copied from system/core/run-as
int
get_package_info(const char* pkgName, uid_t userId, PackageInfo *info)
{
    char*        buffer;
    size_t       buffer_len;
    const char*  p;
    const char*  buffer_end;
    int          result = -1;
    info->uid          = 0;
    info->isDebuggable = 0;
    info->dataDir[0]   = '\0';
    info->seinfo[0]    = '\0';
    buffer = (char *)map_file(PACKAGES_LIST_FILE, &buffer_len);
    if (buffer == NULL)
        return -1;
    p          = buffer;
    buffer_end = buffer + buffer_len;
    /* expect the following format on each line of the control file:
     *
     *  <pkgName> <uid> <debugFlag> <dataDir> <seinfo>
     *
     * where:
     *  <pkgName>    is the package's name
     *  <uid>        is the application-specific user Id (decimal)
     *  <debugFlag>  is 1 if the package is debuggable, or 0 otherwise
     *  <dataDir>    is the path to the package's data directory (e.g. /data/data/com.example.foo)
     *  <seinfo>     is the seinfo label associated with the package
     *
     * The file is generated in com.android.server.PackageManagerService.Settings.writeLP()
     */
    while (p < buffer_end) {
        /* find end of current line and start of next one */
        const char*  end  = find_first(p, buffer_end, '\n');
        const char*  next = (end < buffer_end) ? end + 1 : buffer_end;
        const char*  q;
        int          uid, debugFlag;
        /* first field is the package name */
        p = compare_name(p, end, pkgName);
        if (p == NULL)
            goto NEXT_LINE;
        /* skip spaces */
        if (parse_spaces(&p, end) < 0)
            goto BAD_FORMAT;
        /* second field is the pid */
        uid = parse_positive_decimal(&p, end);
        if (uid < 0)
            return -1;
        info->uid = (uid_t) uid;
        // ananbox: just get uid is enough
        result = 0;
        goto EXIT;
        /* skip spaces */
        if (parse_spaces(&p, end) < 0)
            goto BAD_FORMAT;
        /* third field is debug flag (0 or 1) */
        debugFlag = parse_positive_decimal(&p, end);
        switch (debugFlag) {
        case 0:
            info->isDebuggable = 0;
            break;
        case 1:
            info->isDebuggable = 1;
            break;
        default:
            goto BAD_FORMAT;
        }
        /* skip spaces */
        if (parse_spaces(&p, end) < 0)
            goto BAD_FORMAT;
        /* fourth field is data directory path and must not contain
         * spaces.
         */
        q = skip_non_spaces(p, end);
        if (q == p)
            goto BAD_FORMAT;
        /* If userId == 0 (i.e. user is device owner) we can use dataDir value
         * from packages.list, otherwise compose data directory as
         * /data/user/$uid/$packageId
         */
        if (userId == 0) {
            p = string_copy(info->dataDir, sizeof info->dataDir, p, q - p);
        } else {
            snprintf(info->dataDir,
                     sizeof info->dataDir,
                     "/data/user/%d/%s",
                     userId,
                     pkgName);
            p = q;
        }
        /* skip spaces */
        if (parse_spaces(&p, end) < 0)
            goto BAD_FORMAT;
        /* fifth field is the seinfo string */
        q = skip_non_spaces(p, end);
        if (q == p)
            goto BAD_FORMAT;
        string_copy(info->seinfo, sizeof info->seinfo, p, q - p);
        /* Ignore the rest */
        result = 0;
        goto EXIT;
    NEXT_LINE:
        p = next;
    }
    /* the package is unknown */
    errno = ENOENT;
    result = -1;
    goto EXIT;
BAD_FORMAT:
    errno = EINVAL;
    result = -1;
EXIT:
    unmap_file(buffer, buffer_len);
    return result;
}

uid_t getCallingUidHelper(pid_t pid) {
    FILE *fp;
    PackageInfo info;
    char proc[32];
    char cmdline[MAX_CMDLINE_LEN];
    uid_t uid = 0;
    sprintf(proc, "/proc/%d/cmdline", pid);
    fp = fopen(proc, "r");

    if (fp == NULL) {
        return uid;
    }
    if (fgets(cmdline, MAX_CMDLINE_LEN, fp) != NULL) {
        get_package_info(cmdline, 0, &info);
        uid = info.uid;
    }
    fclose(fp);

    return uid;
}
