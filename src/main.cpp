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
#include <string_view>
#include <functional>
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
              const std::string &dpath, int recursive_level) {
    if (!dpath.empty()) {
        if(recursive_level==1) {
            cmd = "mkdir -p -m 750 " + dpath;
        }
        else {
            cmd = "mkdir -m 750 " + dpath;
        }
        printf("[%d] %s\n", recursive_level, cmd.c_str());
    }
    if (v.size() <= x) return;
    for (int y : v[x]) {
        Traverse(v, y, dpath + dir_map[y] + "/", recursive_level+1);
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

#define REPLACE_FEATURE
#ifdef REPLACE_FEATURE
        const int ROOT_NODE = 1;
        if (dir_map.size() > ROOT_NODE) {
            // replace "/edl_ufbm/" -> // "/edl_ufbm1/" and "/edl_ufbm2/")

            std::string_view needle = "/edl_ufbm/";

            const auto it = std::search(
                dir_map[ROOT_NODE].begin(), dir_map[ROOT_NODE].end(),
                std::boyer_moore_searcher(needle.begin(), needle.end()));
            if (it == dir_map[ROOT_NODE].end()) continue;

            auto &siblings = v[parent[ROOT_NODE]];
            if (auto jt =
                    std::find(siblings.begin(), siblings.end(), ROOT_NODE);
                jt != siblings.end()) {
                siblings.erase(jt);
            }

            const int idx = int(it - dir_map[ROOT_NODE].begin());

            auto AddVariant = [&](std::string_view replacement) {
                ++cur_node;
                if ((int)v.size() <= cur_node) v.resize(cur_node + 1);

                std::string new_name = dir_map[ROOT_NODE];
                new_name.replace(idx, needle.size(), replacement);
                dir_map.push_back(std::move(new_name));

                parent.push_back(parent[ROOT_NODE]);
                v[parent[ROOT_NODE]].push_back(cur_node);

                v[cur_node] = v[ROOT_NODE];
            };

            AddVariant("/edl_ufbm1/");
            AddVariant("/edl_ufbm2/");
        }
#endif  // REPLACE_FEATURE

        Traverse(v, 0, "", 0);
    }
    if (dp) closedir(dp);
    return 0;
}
