#ifndef _LINUX_SUS_SU_H
#define _LINUX_SUS_SU_H

#ifdef CONFIG_KSU_SUSFS_SUS_SU

int sus_su_fifo_init(int *maj_dev_num, char *drv_path);
int sus_su_fifo_exit(int *maj_dev_num, char *drv_path);

#endif /* CONFIG_KSU_SUSFS_SUS_SU */

#endif /* _LINUX_SUS_SU_H */
