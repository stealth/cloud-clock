/*
 * Copyright (C) 2010 Sebastian Krahmer.
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

#include <vector>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <algorithm>
#include <functional>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


#include "log.h"
#include "misc.h"
#include "httpdate.h"


using namespace std;


bool operator<(struct addrinfo a1, struct addrinfo a2)
{
	return memcmp(&a1, &a2, sizeof(a1)) < 0;
}


// automatically close() all files when leaving scope
class auto_fd_map : public map<int, string> {

public:
	auto_fd_map()
	{
	}

	virtual ~auto_fd_map()
	{
		for (map<int, string>::iterator i = this->begin(); i != this->end(); ++i)
			close(i->first);
	}
};


http_date::~http_date()
{
}


int http_date::time_servers(const map<string, string> &ms)
{
	struct addrinfo *ai = NULL, hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;

	int e = 0;
	for (map<string, string>::const_iterator i = ms.begin(); i != ms.end(); ++i) {
		if ((e = getaddrinfo(i->first.c_str(), i->second.c_str(), &hints, &ai)) < 0) {
			err<<"http_date::time_servers::getaddrinfo:"<<gai_strerror(e);
			return -1;
		}
		servers[*ai] = i->first;
		// do not call freeaddrinfo() as this was not a deep copy
	}

	return 0;
}


time_t http_date::average_time(const vector<time_t> &vt)
{
	if (vt.size() == 0)
		return 0;

	time_t min = vt[0];
	vector<time_t>::const_iterator i;

	if (vt.size() <= 2)
		return min;

	// my own tricky algorithm to average all times and kill
	// high and low breakouts without overflowing the time_t
	for (i = vt.begin(); i != vt.end(); ++i) {
		if (min < *i)
			min = *i;
	}

	// The average diff
	time_t adiff = 0;
	for (i = vt.begin(); i != vt.end(); ++i) {
		adiff += (*i - min);
	}
	adiff /= vt.size();

	// recalculate min and diff without breakouts
	int n = 0;
	time_t diff = 0, average = min + adiff;
	min = average;
	for (i = vt.begin(); i != vt.end(); ++i) {
		if (abs((double)(average - *i)) <= 2*adiff && *i < min)
			min = *i;
	}

	for (i = vt.begin(); i != vt.end(); ++i) {
		if (abs((double)(average - *i)) <= 2*adiff) {
			diff += (*i - min);
			++n;
		}
	}


	if (n == 0)
		return 0;

	diff /= n;

	return min + diff;
}


int http_date::loop(int seconds)
{
	auto_fd_map sfds;
	vector<time_t> vt;
	struct addrinfo ai;
	int pe = 0; socklen_t pe_len = sizeof(pe);
	char buf[1024];
	int sfd;
	time_t tp = 0;
#ifdef TCP_INFO
	uint64_t avg_rtt = 0, nrtt = 0;
#else
	uint64_t avg_rtt = 500000, nrtt = 1;
#endif

	//  No log I/O before we calculate/set the time to have a minimum of accuracy
	vector<string> log_strings;
	log_strings.reserve(64);


	for (map<struct addrinfo, string>::iterator i = servers.begin();
	     i != servers.end(); ++i) {
		ai = i->first;
		if ((sfd = socket(ai.ai_family, ai.ai_socktype, ai.ai_protocol)) < 0) {
			err<<"http_date::loop::socket:"<<strerror(errno);
			return -1;
		}
		if (nonblock(sfd) < 0) {
			err<<"http_date::loop::nonblock:"<<strerror(errno);
			return -1;
		}
		if (connect(sfd, (struct sockaddr *)ai.ai_addr, ai.ai_addrlen) < 0 &&
		    errno != EINPROGRESS) {
			close(sfd);
			err<<"http_date::loop::connect("<<i->second<<"):"<<strerror(errno);
			log_strings.push_back(err.str());
			continue;
		}
		sfds[sfd] = i->second;
	}

	sleep(seconds);

	for (auto_fd_map::iterator i = sfds.begin(); i != sfds.end(); ++i) {
		pe = 0;
		if (getsockopt(i->first, SOL_SOCKET, SO_ERROR, &pe, &pe_len) < 0) {
			err<<"http_date::loop::loop::getsockopt:"<<strerror(errno);
			return -1;
		}
		if (pe != 0) {
			err<<"http_date::loop::connect("<<i->second<<"):"<<strerror(errno);
			log_strings.push_back(err.str());
			continue;
		}
		if (writen(i->first, "HEAD / HTTP/1.0\r\n\r\n", 19) <= 0) {
			err<<"http_date::loop::loop::write("<<i->second<<"):"<<strerror(errno);
			log_strings.push_back(err.str());
			continue;
		}
	}

	sleep(seconds);

	string header = "", date = "";
	struct tm tm;
	for (auto_fd_map::iterator i = sfds.begin(); i != sfds.end(); ++i) {
		memset(buf, 0, sizeof(buf));
		if (read(i->first, buf, sizeof(buf)) <= 0) {
			err<<"http_date::loop::loop::read("<<i->second<<"):"<<strerror(errno);
			log_strings.push_back(err.str());
			continue;
		}

		header = buf;
		string::size_type d = string::npos, nl = string::npos;
		if ((d = header.find("Date: ")) == string::npos)
			continue;
		if ((nl = header.find("\r\n", d)) == string::npos)
			continue;
		date = header.substr(d + 6, nl - (d + 6));
		if (date.length() > 40)
			continue;

#ifdef USE_TCP_INFO
		struct tcp_info ti;
		socklen_t sl = sizeof(ti);
		// Kick off hosts with huge RTT so they cant mess up time measurement.
		if (getsockopt(i->first, SOL_TCP, TCP_INFO, &ti, &sl) == 0) {
			if (ti.tcpi_total_retrans > 0 || ti.tcpi_rcv_rtt >= 5000000 ||
			    ti.tcpi_rtt >= 10000000)
				continue;
			avg_rtt += ti.tcpi_rtt;
			++nrtt;
		}
#endif

		memset(&tm, 0, sizeof(tm));
		strptime(date.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &tm);
		date += " ";
		date += i->second;
		log_strings.push_back(date);

		tp = timegm(&tm);

		// nullify constant delay slot
		tp += seconds;

		vt.push_back(tp);
	}


	if ((tp = average_time(vt)) == 0)
		log_strings.push_back("Weird. Cannot compute an average time! All servers down ?!");
	else
		log_strings.push_back(ctime(&tp));

	if (nrtt > 0)
		avg_rtt /= nrtt;
	else
		avg_rtt = 500000;

	int r = 0;
	struct timeval tv;
	tv.tv_sec = tp;
	tv.tv_usec = avg_rtt;
	if (!no_set_time && tp)
		if ((r = settimeofday(&tv, NULL)) < 0)
			err<<"http_date::loop::loop::settimeofday:"<<strerror(errno);

	for_each (log_strings.begin(), log_strings.end(), ptr_fun(&Log::log));

	return r;
}


