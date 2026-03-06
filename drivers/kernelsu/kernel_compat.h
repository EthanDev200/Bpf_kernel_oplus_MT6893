#ifndef __KSU_H_KERNEL_COMPAT
#define __KSU_H_KERNEL_COMPAT

#include <linux/fs.h>
#include <linux/version.h>
#include <linux/uaccess.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
static inline long copy_from_user_nofault(void *dst, const void __user *src, size_t size)
{
    return probe_kernel_read(dst, (const void __force *)src, size);
}

static inline long copy_to_user_nofault(void __user *dst, const void *src, size_t size)
{
    return probe_kernel_write((void __force *)dst, src, size);
}
#endif

/*
 * ksu_copy_from_user_retry
 * try nofault copy first, if it fails, try with plain
 * paramters are the same as copy_from_user
 * 0 = success
 */
static inline long ksu_copy_from_user_retry(void *to, const void __user *from,
                                            unsigned long count)
{
    long ret = copy_from_user_nofault(to, from, count);
    if (likely(!ret))
        return ret;

    // we faulted! fallback to slow path
    return copy_from_user(to, from, count);
}

#endif
