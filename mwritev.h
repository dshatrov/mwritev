#ifndef __MWRITEV__MWRITEV__H__
#define __MWRITEV__MWRITEV__H__

#define MWRITEV_MAX_FDS 4096

typedef struct {
    int *fds;
    struct iovec **iovs;
    int *num_iovs;
    int *ret_res;
} MwritevSession;

typedef struct {
    int  num_fds;
    int *fds;
    struct iovec **iovs;
    int *num_iovs;
    int *ret_res;
} __attribute__ ((packed)) MwritevArgs;

int mwritev_call (MwritevSession *session,
		  int     num_fds,
		  void   *user_fds,
		  void   *user_iovs,
		  void   *user_num_iovs,
		  void   *user_ret_res);

typedef struct {
    int  fd;
    struct iovec *iovs;
    int  num_iovs;
    int *ret_res;
} __attribute__ ((packed)) MwritevSingleArgs;

int mwritev_call_single (MwritevSingleArgs *args);

extern asmlinkage int (*mwritev_writev_syscall) (unsigned long fd, const struct iovec __user * iovs, unsigned long num_iovs);

#endif /* __MWRITEV__MWRITEV__H__ */

