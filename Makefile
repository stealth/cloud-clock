CXX=c++
LD=ld
CFLAGS=-Wall -c -O2 -std=c++11 -pedantic -DUSE_CAPS


all: http_dated



http_dated: httpdate.o misc.o log.o main.o config.o
	$(CXX) *.o -lcap -o httpdated

log.o: log.cc log.h
	$(CXX) $(CFLAGS) log.cc

misc.o: misc.cc misc.h
	$(CXX) $(CFLAGS) misc.cc

httpdate.o: httpdate.cc httpdate.h
	$(CXX) $(CFLAGS) httpdate.cc

main.o: main.cc
	$(CXX) $(CFLAGS) main.cc

config.o: config.cc config.h
	$(CXX) $(CFLAGS) config.cc


clean:
	rm -rf *.o

