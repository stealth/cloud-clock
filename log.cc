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

#include <syslog.h>
#include <string>
#include <cstdio>
#include "log.h"


namespace Log {


bool log_is_open = 0;

http_date_log_t how = HTTPDATE_SYSLOG;

using namespace std;


void init(http_date_log_t l)
{
	how = l;

	if (how == HTTPDATE_SYSLOG) {
		openlog("httpdated", LOG_NOWAIT|LOG_PID|LOG_NDELAY, LOG_DAEMON);
		log_is_open = 1;
	}
}


void log(const string &msg)
{
	if (how == HTTPDATE_NOLOG)
		return;

	if (!log_is_open && how == HTTPDATE_SYSLOG) {
		openlog("httpdated", LOG_NOWAIT|LOG_PID|LOG_NDELAY, LOG_DAEMON);
		log_is_open = 1;
	}

	if (how == HTTPDATE_STDOUT)
		printf("%s\n", msg.c_str());
	else if (how == HTTPDATE_SYSLOG)
		syslog(LOG_ERR, "%s", msg.c_str());
}

}

