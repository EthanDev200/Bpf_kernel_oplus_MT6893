#ifndef __KSU_SUSFS_H
#define __KSU_SUSFS_H

#include <linux/types.h>

#ifdef CONFIG_KSU_SUSFS
extern bool susfs_is_current_ksu_domain(void);
extern bool susfs_is_current_zygote_domain(void);
extern void ksu_susfs_init(void);
extern bool susfs_handle_ioctl(unsigned int cmd, unsigned long arg);
extern void ksu_try_umount(const char *mnt, bool check_mnt, int flags, uid_t uid);
#else
static inline bool susfs_is_current_ksu_domain(void) { return false; }
static inline bool susfs_is_current_zygote_domain(void) { return false; }
static inline void ksu_susfs_init(void) {}
static inline void ksu_try_umount(const char *mnt, bool check_mnt, int flags, uid_t uid) {}
#endif

#endif
