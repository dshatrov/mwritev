#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/file.h>

// TEST
#include <linux/syscalls.h>

#include "mwritev.h"

#if 0
int mwritev_call (MwritevSession * const session,
		  int    const num_fds,
		  void * const user_fds,
		  void * const user_iovs,
		  void * const user_num_iovs,
		  void * const user_ret_res)
{
    printk (KERN_INFO "num_fds: %d\nfds: 0x%lx\niovs: 0x%lx\nnum_iovs: 0x%lx\nret_res: 0x%lx\n",
	    num_fds, (unsigned long) user_fds, (unsigned long) user_iovs,
	    (unsigned long) user_num_iovs, (unsigned long) user_ret_res);

    int *user_fds_ptr = user_fds;
    struct iovec **user_iovs_ptr = user_iovs;
    int *user_num_iovs_ptr = user_num_iovs;
    int *user_ret_res_ptr = user_ret_res;

    int i;
    for (i = 0; i < num_fds; ++i) {
	int fd;
	get_user (fd, user_fds_ptr);
	++user_fds_ptr;

	if (fd == -1) {
	    ++user_iovs_ptr;
	    ++user_num_iovs_ptr;

	    put_user (-EBADF, user_ret_res_ptr);
	    ++user_ret_res_ptr;
	    continue;
	}

	struct iovec *iovs;
	get_user (iovs, user_iovs_ptr);
	++user_iovs_ptr;

	int num_iovs;
	get_user (num_iovs, user_num_iovs_ptr);
	++user_num_iovs_ptr;

	printk (KERN_INFO "mwritev_call: fd #%d: %d\n", i, fd);

	ssize_t res = -EBADF;
	struct file * const file = fget (fd);
	if (file) {
	    loff_t pos = file->f_pos;
	    printk (KERN_INFO "mwritev_call: calling vfs_writev(), iovs: 0x%lx, num_iovs: 0x%x\n",
		    (unsigned long) iovs, (unsigned) num_iovs);
//	    res = vfs_writev (file, iovs, num_iovs, &pos);
	    printk (KERN_INFO "mwritev_call: res: %d\n", (int) res);
	    file->f_pos = pos;
	    fput (file);
	}

	put_user (res, user_ret_res_ptr);
	++user_ret_res_ptr;
    }

    return 0;
}
#endif

int mwritev_call (MwritevSession * const session,
		  int    const num_fds,
		  void * const user_fds,
		  void * const user_iovs,
		  void * const user_num_iovs,
		  void * const user_ret_res)
{
//    printk (KERN_INFO "num_fds: %d\nfds: 0x%lx\niovs: 0x%lx\nnum_iovs: 0x%lx\nret_res: 0x%lx\n",
//	    num_fds, (unsigned long) user_fds, (unsigned long) user_iovs,
//	    (unsigned long) user_num_iovs, (unsigned long) user_ret_res);

    if (num_fds > MWRITEV_MAX_FDS)
	return -EINVAL;

    copy_from_user (session->fds,      user_fds,      num_fds * sizeof (*session->fds));
    copy_from_user (session->iovs,     user_iovs,     num_fds * sizeof (*session->iovs));
    copy_from_user (session->num_iovs, user_num_iovs, num_fds * sizeof (*session->num_iovs));

    int i;
    for (i = 0; i < num_fds; ++i) {
	int fd = session->fds [i];

	if (fd == -1) {
	    session->ret_res [i] = -EBADF;
	    continue;
	}

	struct iovec *iovs = session->iovs [i];
	int num_iovs = session->num_iovs [i];

//	printk (KERN_INFO "mwritev_call: fd #%d: %d\n", i, fd);

	ssize_t res = -EBADF;
	if (mwritev_writev_syscall) {
	    res = mwritev_writev_syscall (fd, iovs, num_iovs);
	} else {
	    struct file * const file = fget (fd);
	    if (file) {
		loff_t pos = file->f_pos;
//		printk (KERN_INFO "mwritev_call: calling vfs_writev(), iovs: 0x%lx, num_iovs: 0x%x\n",
//			(unsigned long) iovs, (unsigned) num_iovs);
		res = vfs_writev (file, iovs, num_iovs, &pos);
//		printk (KERN_INFO "mwritev_call: res: %d\n", (int) res);
		file->f_pos = pos;
		fput (file);
	    }
	}

	session->ret_res [i] = res;
    }

    copy_to_user (user_ret_res, session->ret_res, num_fds * sizeof (*session->ret_res));
    return 0;
}

//extern void *sys_call_table [];

int mwritev_call_single (MwritevSingleArgs *args)
{
    if (mwritev_writev_syscall)
	return mwritev_writev_syscall (args->fd, args->iovs, args->num_iovs);

    ssize_t res = -EBADF;
    struct file * const file = fget (args->fd);
    if (file) {
	loff_t pos = file->f_pos;
	res = vfs_writev (file, args->iovs, args->num_iovs, &pos);
	file->f_pos = pos;
	fput (file);
    }

//    put_user ((int) res, args->ret_res);
//    return 0;
    return res;
//#endif

//    asmlinkage int (*writev_syscall) (unsigned long fd, const struct iovec __user * iovs, unsigned long num_iovs) = sys_call_table [__NR_writev];
//    return writev_syscall (args->fd, args->iovs, args->num_iovs);

//    return sys_writev (args->fd, args->iovs, args->num_iovs);

//    sys_read (0, NULL, 0);
}

