#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

// ------------------------ 테스트 대상 소스 포함 ------------------------
// 원본 코드의 main과 충돌하지 않도록 이름을 바꿈.
#define main gen_main
#include "../src/main.cpp"
#undef main
// ---------------------------------------------------------------------

namespace {

// 임시 디렉토리 생성 (mkdtemp)
std::string MakeTempDir() {
    std::string tmpl = "./ut_dir_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    char* p = mkdtemp(buf.data());
    if (!p) {
        perror("mkdtemp");
        throw std::runtime_error("mkdtemp failed");
    }
    char absbuf[PATH_MAX];
    if (!realpath(p, absbuf)) {
        perror("realpath");
        throw std::runtime_error("realpath failed");
    }
    return std::string(absbuf);
}

// 재귀 삭제 (C++11, <filesystem> 없이 dirent로 구현)
void RmRf(const std::string& path) {
    DIR* dp = opendir(path.c_str());
    if (!dp) {
        // 파일이거나 접근 불가면 unlink 시도
        unlink(path.c_str());
        return;
    }
    struct dirent* ent;
    while ((ent = readdir(dp)) != nullptr) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        std::string child = path + "/" + ent->d_name;
        struct stat st{};
        if (lstat(child.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                RmRf(child);
            } else {
                unlink(child.c_str());
            }
        }
    }
    closedir(dp);
    rmdir(path.c_str());
}

struct CwdGuard {
    char oldcwd[PATH_MAX];
    explicit CwdGuard(const std::string& new_cwd) {
        if (!getcwd(oldcwd, sizeof(oldcwd))) throw std::runtime_error("getcwd failed");
        if (chdir(new_cwd.c_str()) != 0) throw std::runtime_error("chdir failed");
    }
    ~CwdGuard() { chdir(oldcwd); }
};

// stdout 캡처 (freopen)
struct StdoutCapture {
    FILE* old_stdout = nullptr;
    std::string out_path;
    explicit StdoutCapture(const std::string& path) : out_path(path) {
        fflush(stdout);
        old_stdout = stdout;
        FILE* fp = freopen(out_path.c_str(), "w", stdout);
        if (!fp) throw std::runtime_error("freopen stdout failed");
    }
    ~StdoutCapture() {
        fflush(stdout);
        // stdout 원상복구
        freopen("/dev/tty", "w", stdout); // TTY 없는 환경이면 무시됨
        if (old_stdout) stdout = old_stdout;
    }
};

std::string ReadAll(const std::string& path) {
    std::ifstream ifs(path.c_str());
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

void WriteAll(const std::string& path, const std::string& content) {
    std::ofstream ofs(path.c_str());
    ofs << content;
    ofs.flush();
}

} // namespace

// ---------------------------------------------------------------------
// 테스트 1) 기본 트리 파싱/출력 검증 (REPLACE_FEATURE 미적용 케이스)
//   입력:
//     [ts] :root
//     [ts] ::childA
//     [ts] ::childB
//   기대 출력(순서/형식):
//     [1] mkdir -p -m 750 root/
//     [2] mkdir -m 750 root/childA/
//     [2] mkdir -m 750 root/childB/
// ---------------------------------------------------------------------
TEST(DirListGenerator, SimpleTree_NoReplaceFeature) {
    const std::string tmp = MakeTempDir();
    CwdGuard guard(tmp);

    // 샘플 dir_list 파일 생성 (접미사: "dir_list.txt")
    const char* fname = "simple_dir_list.txt";
    // sscanf("%*[^]]] %[^\r\n]", dpath)을 통과하려면 ']' 뒤에 공백 + 경로가 필요
    const std::string content =
        "[ts] :root\n"
        "[ts] ::childA\n"
        "[ts] ::childB\n";
    WriteAll(fname, content);

    // 실행 & stdout 캡처
    const std::string outpath = tmp + "/stdout_simple.txt";
    {
        StdoutCapture cap(outpath);
        ASSERT_EQ(0, gen_main());
    }

    // 결과 비교
    const std::string out = ReadAll(outpath);
    const std::string expected =
        "[1] mkdir -p -m 750 root/\n"
        "[2] mkdir -m 750 root/childA/\n"
        "[2] mkdir -m 750 root/childB/\n";
    EXPECT_EQ(out, expected);

    // 정리
    RmRf(tmp);
}

// ---------------------------------------------------------------------
// 테스트 2) REPLACE_FEATURE 동작 검증
//   - root 경로에 "/edl_ufbm/"이 포함되면
//     "/edl_ufbm1/", "/edl_ufbm2/" 2가지 변형 루트가 생성되어야 함.
//   입력:
//     [ts] :proj/edl_ufbm/x
//     [ts] ::c1
//     [ts] ::c2
//   기대 출력(순서/형식):
//     [1] mkdir -p -m 750 proj/edl_ufbm1/x/
//     [2] mkdir -m 750 proj/edl_ufbm1/x/c1/
//     [2] mkdir -m 750 proj/edl_ufbm1/x/c2/
//     [1] mkdir -p -m 750 proj/edl_ufbm2/x/
//     [2] mkdir -m 750 proj/edl_ufbm2/x/c1/
//     [2] mkdir -m 750 proj/edl_ufbm2/x/c2/
// ---------------------------------------------------------------------
TEST(DirListGenerator, ReplaceFeature_DuplicatesRootAndChildren) {
    const std::string tmp = MakeTempDir();
    CwdGuard guard(tmp);

    const char* fname = "edl_dir_list.txt";
    const std::string content =
        "[ts] :proj/edl_ufbm/x\n"
        "[ts] ::c1\n"
        "[ts] ::c2\n";
    WriteAll(fname, content);

    const std::string outpath = tmp + "/stdout_replace.txt";
    {
        StdoutCapture cap(outpath);
        ASSERT_EQ(0, gen_main());
    }

    const std::string out = ReadAll(outpath);
    const std::string expected =
        "[1] mkdir -p -m 750 proj/edl_ufbm1/x/\n"
        "[2] mkdir -m 750 proj/edl_ufbm1/x/c1/\n"
        "[2] mkdir -m 750 proj/edl_ufbm1/x/c2/\n"
        "[1] mkdir -p -m 750 proj/edl_ufbm2/x/\n"
        "[2] mkdir -m 750 proj/edl_ufbm2/x/c1/\n"
        "[2] mkdir -m 750 proj/edl_ufbm2/x/c2/\n";
    EXPECT_EQ(out, expected);

    RmRf(tmp);
}
