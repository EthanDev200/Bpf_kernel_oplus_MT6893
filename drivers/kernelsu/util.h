#ifndef __KSU_UTIL_H
#define __KSU_UTIL_H

#include <linux/types.h>

#ifndef preempt_enable_no_resched_notrace
#define preempt_enable_no_resched_notrace()                                    \
    do {                                                                       \
        barrier();                                                             \
        __preempt_count_dec();                                                 \
    } while (0)
#endif

#ifndef preempt_disable_notrace
#define preempt_disable_notrace()                                              \
    do {                                                                       \
        __preempt_count_inc();                                                 \
        barrier();                                                             \
    } while (0)
#endif

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0)
#define TWA_RESUME 1
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 2)
struct seccomp_filter;
static inline void ksu_seccomp_allow_cache(struct seccomp_filter *filter, int nr)
{
}
#endif

bool try_set_access_flag(unsigned long addr);

#endif
