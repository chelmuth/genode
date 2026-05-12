/*
 * \brief  'readv()' and 'writev()' implementations
 * \author Josef Soentgen
 * \author Christian Prochaska
 * \date   2012-04-10
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/mutex.h>

/* libc includes */
#include <sys/uio.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

/* libc-internal includes */
#include <internal/errno.h>
#include <internal/types.h>

using namespace Libc;


struct Read
{
	ssize_t operator()(int fd, void *buf, size_t count)
	{
		return read(fd, buf, count);
	}
};


struct Write
{
	ssize_t operator()(int fd, const void *buf, size_t count)
	{
		return write(fd, buf, count);
	}
};


template <typename Rw_func>
static ssize_t readv_writev_impl(Rw_func rw_func, int fd, const struct iovec *iov, int iovcnt)
{
	/* FIXME this should be a pthread_mutex because function uses blocking operations */
	static Mutex rw_mutex;

	Mutex::Guard guard(rw_mutex);

	ssize_t bytes_transferred_total = 0;
	size_t iov_len_total = 0;

	if (iovcnt < 1 || iovcnt > IOV_MAX)
		return Errno(EINVAL);

	for (int i = 0; i < iovcnt; i++)
		iov_len_total += iov[i].iov_len;

	if (iov_len_total > SSIZE_MAX)
		return Errno(EINVAL);

	while (iovcnt > 0) {

		ssize_t bytes_transferred = rw_func(fd, iov->iov_base, iov->iov_len);

		if (bytes_transferred == -1)
			return -1;

		bytes_transferred_total += bytes_transferred;

		if (bytes_transferred < iov->iov_len)
			return bytes_transferred_total;

		iov++;
		iovcnt--;
	}

	return bytes_transferred_total;
}


extern "C" ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
	return readv_writev_impl(Read(), fd, iov, iovcnt);
}

extern "C" __attribute__((alias("readv")))
ssize_t __sys_readv(int fd, const struct iovec *iov, int iovcnt);

extern "C" __attribute__((alias("readv")))
ssize_t _readv(int fd, const struct iovec *iov, int iovcnt);


extern "C" ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
	return readv_writev_impl(Write(), fd, iov, iovcnt);
}

extern "C" __attribute__((alias("writev")))
ssize_t __sys_writev(int fd, const struct iovec *iov, int iovcnt);

extern "C" __attribute__((alias("writev")))
ssize_t _writev(int fd, const struct iovec *iov, int iovcnt);
