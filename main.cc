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

#include <map>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef USE_CAPS
#include <pwd.h>
#include <grp.h>
#include <sys/prctl.h>
#include <sys/capability.h>
#endif

#include "log.h"
#include "misc.h"
#include "config.h"
#include "httpdate.h"


using namespace std;



void usage(const char *p)
{
	printf("\n%s\t[-R chroot (%s)] [-u user (%s)] [-s delay (%ds)]\n"
	       "\t\t[-S time-frame (%ds)] <-T server/config> [-N] [-F]\n\n",
	       p, Config::chroot.c_str(), Config::user.c_str(),
	       Config::delay, Config::sleep);
	exit(0);
}


void die(const char *m)
{
	perror(m);
	exit(1);
}


void parse_time_server(const string &s, map<string, string> &ms)
{
	FILE *f = NULL;
	char buf[64], *nl = NULL;
	string::size_type n = string::npos;

	// Not a file? Then it must be host~port
	if ((f = fopen(s.c_str(), "r")) == NULL) {
		if ((n = s.find("~")) != string::npos) {
			if (n + 1 >= s.size())
				ms[s.substr(0, n)] = "80";
			else
				ms[s.substr(0, n)] = s.substr(n + 1, s.size() - (n + 1));
		} else
				ms[s] = "80";
		return;
	}

	string line = "";
	for (;!feof(f);) {
		memset(buf, 0, sizeof(buf));
		fgets(buf, sizeof(buf), f);
		if (strlen(buf) <= 2 || *buf == '#')
			continue;
		if ((nl = strchr(buf, '\n')) != NULL)
			*nl = 0;
		if ((nl = strchr(buf, '#')) != NULL)
			*nl = 0;
		line = buf;
		if ((n = line.find("~")) != string::npos) {
			if (n + 1 >= line.size())
				ms[line.substr(0, n)] = "80";
			else
				ms[line.substr(0, n)] = line.substr(n + 1, line.size() - (n + 1));
		} else
			ms[line] = "80";
	}
	fclose(f);
}


int main(int argc, char **argv)
{
	http_date hd;
	map<string, string> ms;
	int c = 0, dev_null = 0;


	while ((c = getopt(argc, argv, "FNT:s:S:u:R:")) != -1) {
		switch (c) {
		case 'F':
			Config::foreground = 1;
			break;
		case 'N':
			Config::no_set = 1;
			break;
		case 'T':
			parse_time_server(optarg, ms);
			break;
		case 's':
			Config::delay = atoi(optarg);
			break;
		case 'S':
			Config::sleep = atoi(optarg);
			break;
		case 'R':
			Config::chroot = optarg;
			break;
		case 'u':
			Config::user = optarg;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (ms.size() < 1)
		usage(argv[0]);

	if (hd.time_servers(ms) < 0)
		die(hd.why());

	// important to open log before possibly chroot
	if (!Config::foreground) {
		Log::init(Log::HTTPDATE_SYSLOG);
		dev_null = open("/dev/null", O_RDWR);
	} else
		Log::init(Log::HTTPDATE_STDOUT);

	if (geteuid())
		Config::no_set = 1;

	hd.no_set(Config::no_set);

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGURG, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

#ifdef USE_CAPS

	// It might be called as user to just print out web server times
	if (geteuid() == 0) {
		if (Config::chroot != "/" && transfer_localtime(Config::chroot.c_str()) < 0)
			fprintf(stderr, "Transfering of /etc/localtime into chroot failed. Running anywway.\n");

		struct passwd *pw = getpwnam(Config::user.c_str());
		if (!pw)
			die("unknown user:getpwnam");

		if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) < 0)
			die("prctl");

		if (chroot(Config::chroot.c_str()) < 0)
			die("chroot");

		if (setgid(pw->pw_gid) < 0)
			die("setgid");
		if (initgroups(Config::user.c_str(), pw->pw_gid) < 0)
			die("initgroups");
		if (setuid(pw->pw_uid) < 0)
			die("setuid");

		cap_t my_caps;
		cap_value_t cv[1] = {CAP_SYS_TIME};

		if ((my_caps = cap_init()) == NULL)
			die("cap_init");
		if (cap_set_flag(my_caps, CAP_EFFECTIVE, 1, cv, CAP_SET) < 0)
			die("cap_set_flag");
		if (cap_set_flag(my_caps, CAP_PERMITTED, 1, cv, CAP_SET) < 0)
			die("cap_set_flag");
		if (cap_set_proc(my_caps) < 0)
			die("cap_set_proc");
		cap_free(my_caps);
	}
#endif

	if (chdir("/") < 0)
		die("chdir");

	if (!Config::foreground) {
		if (fork() > 0)
			exit(0);
		setsid();
		Log::log("started");

		dup2(dev_null, 0);
		dup2(dev_null, 1);
		dup2(dev_null, 2);
		close(dev_null);
	}

	for (;;) {
		if (hd.loop(Config::delay) < 0) {
			Log::log(hd.why());
			exit(1);
		}

		if (Config::foreground)
			break;

		sleep(Config::sleep);
	}

	return 0;
}


