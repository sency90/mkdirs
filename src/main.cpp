#include <dirent.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
const std::string DIRS_FNAME_SUFFIX = "dir_list.txt";
// const std::string DIRS_FNAME_SUFFIX_REVERSE = ReverseStr(DIRS_FNAME_SUFFIX);
std::ifstream ifs;
std::stringstream ss;
};  // namespace

int RevStrncmp(std::string lhs, std::string rhs, int n) {
    std::reverse(lhs.begin(), lhs.end());
    std::reverse(rhs.begin(), rhs.end());
    return strncmp(lhs.c_str(), rhs.c_str(), n);
}

int main() {
    std::string line, buf;
    char dpath[1024];

    DIR *dp = opendir("./");
    struct dirent *p;
    while (p = readdir(dp)) {
        if (p->d_type == DT_DIR) continue;
        if (RevStrncmp(p->d_name, DIRS_FNAME_SUFFIX,
                       DIRS_FNAME_SUFFIX.length()) != 0)
            continue;

        ifs.open(p->d_name);
        while (std::getline(ifs, line)) {
            if(line.empty()) continue;
            sscanf(line.c_str(), "%*[^]]] %[^\r\n]", dpath);
            printf("%s\n", dpath);
        }
        ifs.close();
    }
    if (dp) closedir(dp);
    return 0;
}
