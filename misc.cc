/*
 * Copyright (C) 2011 Sebastian Krahmer.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Sebastian Krahmer.
 * 4. The name Sebastian Krahmer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include "misc.h"


int nonblock(int fd)
{
	int f = fcntl(fd, F_GETFL);
	f |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, f) < 0)
                return -1;

#ifdef __linux__
	int one = 1; socklen_t len = sizeof(one);
	if (setsockopt(fd, SOL_TCP, TCP_NODELAY, &one, len) < 0)
		return -1;
#endif
	return 0;
}

int writen(int fd, const void *buf, size_t len)
{
	int o = 0, n;
	char *ptr = (char*)buf;

	while (len > 0) {
		if ((n = write(fd, ptr + o, len)) < 0)
			return n;
		len -= n;
		o += n;
	}
	return o;
}


int transfer_localtime(const char *root)
{
	char path[1024], buf[1024];
	int fd1, fd2, r;
	struct stat st;

	if (stat(root, &st) < 0)
		return -1;

	if (st.st_uid != 0)
		return -1;

	snprintf(path, sizeof(path), "%s/etc", root);

	if (mkdir(path, 0755) < 0 && errno != EEXIST)
		return -1;

	snprintf(path, sizeof(path), "%s/etc/localtime", root);

	if ((fd1 = open("/etc/localtime", O_RDONLY)) < 0)
		return -1;

	if ((fd2 = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
		close(fd1);
		return -1;
	}

	for (;;) {
		if ((r = read(fd1, buf, sizeof(buf))) <= 0)
			break;
		if (write(fd2, buf, r) <= 0)
			break;
	}
	close(fd1);
	close(fd2);
	return r;
}

