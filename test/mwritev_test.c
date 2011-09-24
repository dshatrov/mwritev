#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

typedef struct {
    int  num_fds;
    int *fds;
    struct iovec **iovs;
    int *num_iovs;
    int *ret_res;
} __attribute__ ((packed)) MwritevArgs;

int call_mwritev (int  mwritev_fd,
		  int  num_fds,
		  int *fds,
		  struct iovec **iovs,
		  int *num_iovs,
		  int *ret_res)
{
    fprintf (stderr, "num_fds: %d\nfds: 0x%lx\niovs: 0x%lx\nnum_iovs: 0x%lx\nret_res: 0x%lx\n",
	     num_fds, (unsigned long) fds, (unsigned long) iovs, (unsigned long) num_iovs,
	     (unsigned long) ret_res);

    MwritevArgs args = {
	.num_fds  = num_fds,
	.fds      = fds,
	.iovs     = iovs,
	.num_iovs = num_iovs,
	.ret_res  = ret_res
    };

    int const res = ioctl (mwritev_fd, 0, (void*) &args);
    if (res == -1) {
	perror ("ioctl() failed");
	return -1;
    }

    return 0;
}

int main (void)
{
    int mwritev_fd;
    for (;;) {
	mwritev_fd = open ("/dev/mwritev", 0);
	if (mwritev_fd == -1) {
	    if (errno == EINTR)
		continue;

	    perror ("open() failed");
	    return EXIT_FAILURE;
	}

	break;
    }

    {
	int const num_fds = 2;

	int fds [num_fds];
	struct iovec *iovs [num_fds];
	int num_iovs [num_fds];
	int vres [num_fds];

#if 0
	int i;
	for (i = num_fds; i > 0; --i) {
	    fds [i - 1] = i * 2;
	    fprintf (stderr, "fds [%d] = %d\n", i - 1, fds [i - 1]);
	}
#endif

	fds [0] = mwritev_fd;
//	iovs [0] = (struct iovec*) 0x11223344;
//	num_iovs [0] = 13;
	iovs [0] = NULL;
	num_iovs [0] = 0;

	fds [1] = mwritev_fd;
//	iovs [1] = (struct iovec*) 0x44332211;
//	num_iovs [1] = 0xee;
	iovs [1] = NULL;
	num_iovs [1] = 0;

	int const res = call_mwritev (mwritev_fd, num_fds, fds, iovs, num_iovs, vres);
	if (res) {
	    fprintf (stderr, "call_mwritev() failed, res: %d\n", res);
	} else {
	    int i;
	    for (i = 0; i < num_fds; ++i) {
		fprintf (stderr, "res #%d: %d (%s)\n", i, vres [i], strerror (-vres [i]));
	    }
	}
    }

    for (;;) {
	int const res = close (mwritev_fd);
	if (res == -1) {
	    if (errno == EINTR)
		continue;

	    perror ("close() failed");
	    return EXIT_FAILURE;
	}

	break;
    }

    return 0;
}

