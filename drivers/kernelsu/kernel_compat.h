#ifndef __KSU_H_KERNEL_COMPAT
#define __KSU_H_KERNEL_COMPAT

#include <linux/fs.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/sched/task.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
#define ksu_access_ok(addr, size) __access_ok((unsigned long)(addr), (size), get_fs())
#else
#define ksu_access_ok(addr, size) access_ok(addr, size)
#endif

#ifndef TWA_RESUME
#define TWA_RESUME 1
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
#ifndef copy_from_user_nofault
static inline long copy_from_user_nofault(void *dst, const void __user *src, size_t size)
{
    return probe_kernel_read(dst, src, size);
}
#endif

#ifndef copy_to_user_nofault
static inline long copy_to_user_nofault(void __user *dst, const void *src, size_t size)
{
    return probe_kernel_write(dst, src, size);
}
#endif
#endif

/*
 * ksu_copy_from_user_retry
 * try nofault copy first, if it fails, try with plain
 * paramters are the same as copy_from_user
 * 0 = success
 */
static long ksu_copy_from_user_retry(void *to, const void __user *from,
                                     unsigned long count)
{
    long ret = copy_from_user_nofault(to, from, count);
    if (likely(!ret))
        return ret;

    // we faulted! fallback to slow path
    return copy_from_user(to, from, count);
}

#endif
