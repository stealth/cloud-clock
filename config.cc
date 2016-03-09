#include <string>


namespace Config {

using namespace std;

string server_or_file = "", user = "nobody", chroot = "/var/lib/empty";

bool no_set = 0, foreground = 0;

int delay = 1, sleep = 60*60*6;

}

