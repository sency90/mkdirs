#include <dirent.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#define main gen_main
#include "../src/main.cpp"
#undef main

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
        struct stat st {};
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
        if (!getcwd(oldcwd, sizeof(oldcwd)))
            throw std::runtime_error("getcwd failed");
        if (chdir(new_cwd.c_str()) != 0)
            throw std::runtime_error("chdir failed");
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
        freopen("/dev/tty", "w", stdout);
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

}  // namespace

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
    // sscanf("%*[^]]] %[^\r\n]", dpath)을 통과하려면 ']' 뒤에 공백 + 경로가
    // 필요
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

// ---------------------------------------------------------------------
// 테스트 3) 비매칭 파일만 존재하면 출력이 없어야 함
//   - *_dir_list.txt 가 아니면 무시
// ---------------------------------------------------------------------
TEST(DirListGenerator, IgnoreNonMatchingFilesProducesNoOutput) {
    const std::string tmp = MakeTempDir();
    CwdGuard guard(tmp);

    // 비매칭 파일만 생성
    WriteAll("not_match.txt", "[ts] :root\n");

    const std::string outpath = tmp + "/stdout_none.txt";
    {
        StdoutCapture cap(outpath);
        ASSERT_EQ(0, gen_main());
    }
    const std::string out = ReadAll(outpath);
    EXPECT_TRUE(out.empty());

    RmRf(tmp);
}

// ---------------------------------------------------------------------
// 테스트 4) 깊은 계층 & 상위로 되돌아가기 (RepositionParentNode 분기 커버)
//   입력:
//     :root
//     ::A
//     :::A1
//     ::B     (3 -> 2로 감소)
//     :C      (2 -> 1로 감소)
// ---------------------------------------------------------------------
TEST(DirListGenerator, DeepHierarchy_UpAndDownLevels) {
    const std::string tmp = MakeTempDir();
    CwdGuard guard(tmp);

    const char* fname = "deep_dir_list.txt";
    const std::string content =
        "[ts] :root\n"
        "[ts] ::A\n"
        "[ts] :::A1\n"
        "[ts] ::B\n"
        "[ts] :C\n";
    WriteAll(fname, content);

    const std::string outpath = tmp + "/stdout_deep.txt";
    {
        StdoutCapture cap(outpath);
        ASSERT_EQ(0, gen_main());
    }

    const std::string out = ReadAll(outpath);
    const std::string expected =
        "[1] mkdir -p -m 750 root/\n"
        "[2] mkdir -m 750 root/A/\n"
        "[3] mkdir -m 750 root/A/A1/\n"
        "[2] mkdir -m 750 root/B/\n"
        "[2] mkdir -m 750 root/C/\n";
    EXPECT_EQ(out, expected);

    RmRf(tmp);
}

// ---------------------------------------------------------------------
// 테스트 5) 빈 줄 / CRLF 섞여 있어도 정상 파싱
// ---------------------------------------------------------------------
TEST(DirListGenerator, HandlesEmptyLinesAndCRLF) {
    const std::string tmp = MakeTempDir();
    CwdGuard guard(tmp);

    const char* fname = "crlf_dir_list.txt";
    const std::string content =
        "[ts] :root\r\n"
        "\r\n"
        "[ts] ::A\r\n"
        "\n"
        "[ts] ::B\r\n";
    WriteAll(fname, content);

    const std::string outpath = tmp + "/stdout_crlf.txt";
    {
        StdoutCapture cap(outpath);
        ASSERT_EQ(0, gen_main());
    }

    const std::string out = ReadAll(outpath);
    const std::string expected =
        "[1] mkdir -p -m 750 root/\n"
        "[2] mkdir -m 750 root/A/\n"
        "[2] mkdir -m 750 root/B/\n";
    EXPECT_EQ(out, expected);

    RmRf(tmp);
}

// ---------------------------------------------------------------------
// 테스트 6) 루트만 있는 경우 (Traverse의 level==1 분기만 타게)
// ---------------------------------------------------------------------
TEST(DirListGenerator, RootOnly_NoChildren) {
    const std::string tmp = MakeTempDir();
    CwdGuard guard(tmp);

    const char* fname = "root_only_dir_list.txt";
    const std::string content = "[ts] :just_root\n";
    WriteAll(fname, content);

    const std::string outpath = tmp + "/stdout_rootonly.txt";
    {
        StdoutCapture cap(outpath);
        ASSERT_EQ(0, gen_main());
    }

    const std::string out = ReadAll(outpath);
    const std::string expected = "[1] mkdir -p -m 750 just_root/\n";
    EXPECT_EQ(out, expected);

    RmRf(tmp);
}

// ---------------------------------------------------------------------
// 테스트 7) 유사 토큰이지만 미치환 ("/edl_ufbm/" 아님 → it==end 브랜치 커버)
//   - "/edl_ufbmX/" 처럼 슬래시로 감싸지지 않으면 치환되면 안 됨
// ---------------------------------------------------------------------
TEST(DirListGenerator, SimilarButNotReplaceToken) {
    const std::string tmp = MakeTempDir();
    CwdGuard guard(tmp);

    const char* fname = "similar_token_dir_list.txt";
    const std::string content =
        "[ts] :proj/edl_ufbmX/x\n"
        "[ts] ::c1\n";
    WriteAll(fname, content);

    const std::string outpath = tmp + "/stdout_similar.txt";
    {
        StdoutCapture cap(outpath);
        ASSERT_EQ(0, gen_main());
    }

    const std::string out = ReadAll(outpath);
    const std::string expected =
        "[1] mkdir -p -m 750 proj/edl_ufbmX/x/\n"
        "[2] mkdir -m 750 proj/edl_ufbmX/x/c1/\n";
    EXPECT_EQ(out, expected);

    RmRf(tmp);
}