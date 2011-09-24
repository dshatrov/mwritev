#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>

// TEST
#include <linux/syscalls.h>
#include <linux/utsname.h>
#include <linux/smp_lock.h>

#include "mwritev.h"

asmlinkage int (*mwritev_writev_syscall) (unsigned long fd, const struct iovec __user * iovs, unsigned long num_iovs) = NULL;

static unsigned long mwritev_major;
static unsigned long mwritev_minor;

static dev_t mwritev_dev;
static struct cdev mwritev_cdev;

static struct class *mwritev_class;

static int mwritev_open (struct inode *inode,
			 struct file  *filp)
{
    MwritevSession * const session = kmalloc (sizeof (MwritevSession), GFP_KERNEL);
    if (!session)
	return -ENOMEM;

    memset (session, 0, sizeof (session));

    session->fds      = kmalloc (MWRITEV_MAX_FDS * sizeof (*session->fds),      GFP_KERNEL);
    session->iovs     = kmalloc (MWRITEV_MAX_FDS * sizeof (*session->iovs),     GFP_KERNEL);
    session->num_iovs = kmalloc (MWRITEV_MAX_FDS * sizeof (*session->num_iovs), GFP_KERNEL);
    session->ret_res  = kmalloc (MWRITEV_MAX_FDS * sizeof (*session->ret_res),  GFP_KERNEL);

    if (!session->fds      ||
	!session->iovs     ||
	!session->num_iovs ||
	!session->ret_res)
    {
	printk (KERN_ERR "mwritev_open: could not allocate memory\n");
	goto _failure;
    }

    filp->private_data = session;

    return 0;

_failure:
    if (session->fds)
	kfree (session->fds);
    if (session->iovs)
	kfree (session->iovs);
    if (session->num_iovs)
	kfree (session->num_iovs);
    if (session->ret_res)
	kfree (session->ret_res);

    session->fds      = NULL;
    session->iovs     = NULL;
    session->num_iovs = NULL;
    session->ret_res  = NULL;

    return -ENOMEM;
}

static int mwritev_release (struct inode *inode,
			    struct file  *filp)
{
    MwritevSession * const session = filp->private_data;

    if (session->fds)
	kfree (session->fds);
    if (session->iovs)
	kfree (session->iovs);
    if (session->num_iovs)
	kfree (session->num_iovs);
    if (session->ret_res)
	kfree (session->ret_res);

    if (session)
	kfree (session);

    return 0;
}

// ioctl() appears to be considerably slower than write().
static long mwritev_ioctl (struct file   * const filp,
			   unsigned int    const cmd,
			   unsigned long         _arg)
{
    MwritevSession * const session = filp->private_data;

//    printk (KERN_INFO "mwritev_ioctl");

    // FIXME get_user() may fail?

    int num_fds;
    get_user (num_fds, (int*) _arg);
    _arg += sizeof (int);

    void *user_fds;
    get_user (user_fds, (void**) _arg);
    _arg += sizeof (void*);

    void *user_iovs;
    get_user (user_iovs, (void**) _arg);
    _arg += sizeof (void*);

    void *user_num_iovs;
    get_user (user_num_iovs, (void**) _arg);
    _arg += sizeof (void*);

    void *user_ret_res;
    get_user (user_ret_res, (void**) _arg);
    _arg += sizeof (void*);

    return mwritev_call (session, num_fds, user_fds, user_iovs, user_num_iovs, user_ret_res);
}

static ssize_t mwritev_write (struct file       * const filp,
			      char const __user *       buf,
			      size_t              const count,
			      loff_t            * const f_pos)
{
    MwritevSession * const session = filp->private_data;

//    printk (KERN_INFO "mwritev_write: count: %lu", (unsigned long) count);

    // FIXME get_user() may fail?

#if 0
    if (count != sizeof (int) + sizeof (void*) * 4)
	return -EINVAL;

    int num_fds;
    get_user (num_fds, (int*) buf);
    buf += sizeof (int);

    void *user_fds;
    get_user (user_fds, (void**) buf);
    buf += sizeof (void*);

    void *user_iovs;
    get_user (user_iovs, (void**) buf);
    buf += sizeof (void*);

    void *user_num_iovs;
    get_user (user_num_iovs, (void**) buf);
    buf += sizeof (void*);

    void *user_ret_res;
    get_user (user_ret_res, (void**) buf);
    buf += sizeof (void*);

    return mwritev_call (num_fds, user_fds, user_iovs, user_num_iovs, user_ret_res);
#endif

    if (count != sizeof (MwritevArgs))
	return -EINVAL;

    MwritevArgs args;
    copy_from_user (&args, buf, sizeof (MwritevArgs));

    return mwritev_call (session, args.num_fds, args.fds, args.iovs, args.num_iovs, args.ret_res);
}

static ssize_t mwritev_write_single (struct file       * const filep,
				     char const __user * const buf,
				     size_t              const count,
				     loff_t            * const f_pos)
{
    if (count != sizeof (MwritevSingleArgs))
	return -EINVAL;

    MwritevSingleArgs args;
    copy_from_user (&args, buf, sizeof (MwritevSingleArgs));

    return mwritev_call_single (&args);
}

static struct file_operations mwritev_fops = {
    .owner          = THIS_MODULE,
    .open           = mwritev_open,
    .release        = mwritev_release,
    .unlocked_ioctl = mwritev_ioctl,
    .write          = mwritev_write
//    .write          = mwritev_write_single
};

static int mwritev_setup_cdev (void)
{
    mwritev_dev = MKDEV (mwritev_major, mwritev_minor);
    cdev_init (&mwritev_cdev, &mwritev_fops);
    mwritev_cdev.owner = THIS_MODULE;

    int const res = cdev_add (&mwritev_cdev, mwritev_dev, 1);
    if (res) {
	printk (KERN_WARNING "mwritev_set_cdev: cdev_add() failed, res %d\n", res);
	goto _first_error;
    }

    struct device * const class_dev =
	    device_create (mwritev_class, NULL, mwritev_dev, NULL, "mwritev");
    if (IS_ERR (class_dev)) {
	printk (KERN_WARNING "mwritev_setup_cdev: device_create() failed\n");
	goto _delete_cdev;
    }

    return 0;

_delete_cdev:
    cdev_del (&mwritev_cdev);
_first_error:
    return -1;
}

static void mwritev_cleanup_cdev (void)
{
    device_destroy (mwritev_class, mwritev_dev);
    cdev_del (&mwritev_cdev);
}

static int mwritev_init_devnumbers (void)
{
    int res;

    if (mwritev_major) {
	mwritev_dev = MKDEV (mwritev_major, mwritev_minor);
	res = register_chrdev_region (mwritev_dev, 1, "mwritev");
    } else {
	res = alloc_chrdev_region (&mwritev_dev, mwritev_minor, 1, "mwritev");
	mwritev_major = (unsigned long) MAJOR (mwritev_dev);
    }

    if (res < 0) {
	printk (KERN_WARNING "mwritev: failed to get major device number\n");
	return -1;
    }

    printk ("mwritev_major: %lu, mwritev_minor: %lu\n",
	    mwritev_major, mwritev_minor);

    return 0;
}

static void mwritev_cleanup_devnumbers (void)
{
    unregister_chrdev_region (mwritev_dev, 1);
}

static int mwritev_create_class (void)
{
    mwritev_class = class_create (THIS_MODULE, "mwritev");
    if (IS_ERR (mwritev_class)) {
	printk (KERN_ERR "mwritev: failed to create mwritev class\n");
	return -1;
    }

    return 0;
}

static void mwritev_destroy_class (void)
{
    class_destroy (mwritev_class);
}

static int mwritev_find_writev_syscall (void)
{
#if 0
// Search for a writev syscall entry in the kernel's syscall table.
// Uncommenting these lines makes the code less portable between kernel versions.

    printk (KERN_INFO "sys_close: 0x%lx\n", (unsigned long) sys_close);

    unsigned long *syscall_table;
    int got_syscall_table = 0;
    int i = 0;
    for (syscall_table = (unsigned long*) &_unlock_kernel;
	 (unsigned long) syscall_table < (unsigned long) &loops_per_jiffy;
	 ++syscall_table)
    {
	printk (KERN_INFO "#%d: 0x%lx\n", i, (unsigned long) syscall_table [__NR_close]);
	if (syscall_table [__NR_close] == (unsigned long) sys_close) {
	    got_syscall_table = 1;
	    break;
	}

	++i;
    }

    if (got_syscall_table)
	mwritev_writev_syscall = (void*) syscall_table [__NR_writev];

    printk (KERN_INFO "mwritev_writev_syscall: 0x%lx\n", (unsigned long) mwritev_writev_syscall);
#endif

    return 0;
}

static int __init mwritev_init (void)
{
    printk (KERN_INFO "mwritev_init\n");

    if (mwritev_find_writev_syscall ())
	goto _first_error;

    if (mwritev_create_class ())
	goto _first_error;

    if (mwritev_init_devnumbers ())
	goto _destroy_class;

    if (mwritev_setup_cdev ())
	goto _cleanup_devnumbers;

    return 0;

//_destroy_cdev:
//    mwritev_cleanup_cdev ();
_cleanup_devnumbers:
    mwritev_cleanup_devnumbers ();
_destroy_class:
    mwritev_destroy_class ();
_first_error:
    return -EFAULT;
}
    
static void __exit mwritev_exit (void)
{
    printk (KERN_INFO "mwritev_ext\n");

    mwritev_cleanup_cdev ();
    mwritev_cleanup_devnumbers ();
    mwritev_destroy_class ();
}

module_init (mwritev_init)
module_exit (mwritev_exit)

MODULE_AUTHOR ("Dmitry Shatrov");
MODULE_DESCRIPTION ("mwritev module");
MODULE_LICENSE ("GPL");

