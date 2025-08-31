#include <dirent.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
// #include <stack>

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

int GetColonCnt(const std::string &dpath, int prv_level) {
    if (prv_level == 0) {
        int result = 0;
        for (int i = 0; i < dpath.size(); i++) {
            if (dpath[i] == ':')
                result++;
            else
                break;
        }
        return result;
    }

    //::abc -> level==2
    if (dpath.size() >= prv_level && dpath[prv_level - 1] == ':') {
        int result = prv_level;
        for (int i = prv_level; i < dpath.size(); i++) {
            if (dpath[i] == ':') {
                result++;
            } else {
                break;
            }
        }
        return result;
    }

    int result = 0;
    for (int i = 0; i < dpath.size(); i++) {
        if (dpath[i] == ':') {
            result++;
        } else {
            break;
        }
    }
    return result;
}

std::string GetCurDname(const std::string &line, int colon_cnt) {
    return line.c_str() + colon_cnt;
}

std::vector<std::string> dir_map;
std::vector<std::vector<int>> v;
std::vector<int> parent;

int RepositionParentNode(int cur_colon_cnt, int prv_colon_cnt,
                         int parent_node) {
    if (cur_colon_cnt == prv_colon_cnt) {
        return parent_node;
    } else if (cur_colon_cnt > prv_colon_cnt) {
        return v[parent_node].back();
    } else {
        int diff = prv_colon_cnt - cur_colon_cnt;
        while (diff--) {
            parent_node = parent[parent_node];
        }
        return parent_node;
    }
}

std::string cmd;
void Traverse(std::vector<std::vector<int>> &v, int x,
              const std::string &dpath) {
    if (!dpath.empty()) {
        cmd = "mkdir -p -m 750 " + dpath;
        printf("%s\n", cmd.c_str());
    }
    if (v.size() <= x) return;
    for (int y : v[x]) {
        Traverse(v, y, dpath + dir_map[y] + "/");
    }
}

void Init() {
    v.clear();
    parent.clear();
    parent.push_back(0);
    dir_map.clear();
    dir_map.push_back("");
}

int main() {
    std::string line, buf;
    char dpath[2048];

    std::vector<std::string> dirs;

    DIR *dp = opendir("./");
    struct dirent *p;
    while (p = readdir(dp)) {
        if (p->d_type == DT_DIR) continue;
        if (RevStrncmp(p->d_name, DIRS_FNAME_SUFFIX,
                       DIRS_FNAME_SUFFIX.length()) != 0) {
            continue;
        }

        Init();

        int parent_node = 0, cur_node = 0;
        int prv_colon_cnt = 0;
        ifs.open(p->d_name);
        while (std::getline(ifs, line)) {
            if (line.empty()) continue;
            cur_node++;

            sscanf(line.c_str(), "%*[^]]] %[^\r\n]", dpath);
            int cur_colon_cnt = GetColonCnt(dpath, prv_colon_cnt);
            parent_node =
                RepositionParentNode(cur_colon_cnt, prv_colon_cnt, parent_node);

            if (v.size() <= parent_node)
                v.resize(parent_node + 1, std::vector<int>());
            v[parent_node].push_back(cur_node);
            parent.push_back(parent_node);
            dir_map.push_back(GetCurDname(dpath, cur_colon_cnt));

            prv_colon_cnt = cur_colon_cnt;
        }
        ifs.close();

        Traverse(v, 0, "");
    }
    if (dp) closedir(dp);
    return 0;
}
