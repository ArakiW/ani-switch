// SPDX-License-Identifier: AGPL-3.0
#include <sqlite3.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <sys/stat.h>

static bool failReads = false;
static ssize_t switchPread(int fd, void* out, size_t count, off_t offset) {
    struct stat info{};
    if (fstat(fd, &info) != 0) return -1;
    if (failReads || offset > info.st_size) { errno = EIO; return -1; }
    return pread(fd, out, count, offset);
}
static ssize_t switchRead(int fd, void* out, size_t count) {
    auto offset = lseek(fd, 0, SEEK_CUR);
    if (offset < 0) return -1;
    struct stat info{};
    if (fstat(fd, &info) != 0) return -1;
    if (failReads || offset > info.st_size) { errno = EIO; return -1; }
    return read(fd, out, count);
}

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s at %d\n", #c, __LINE__); std::exit(1); } } while (0)

static char* switchCwd(char* out, size_t size) {
    const char* cwd = "sdmc:/switch/aniswitch";
    if (size <= std::strlen(cwd)) return nullptr;
    std::strcpy(out, cwd);
    return out;
}
int main() {
    auto* vfs = sqlite3_vfs_find("unix-none");
    CHECK(vfs && vfs->xSetSystemCall);
    CHECK(vfs->xSetSystemCall(vfs, "getcwd", reinterpret_cast<sqlite3_syscall_ptr>(switchCwd)) == SQLITE_OK);
    CHECK(vfs->xSetSystemCall(vfs, "read", reinterpret_cast<sqlite3_syscall_ptr>(switchRead)) == SQLITE_OK);
    CHECK(vfs->xSetSystemCall(vfs, "pread", reinterpret_cast<sqlite3_syscall_ptr>(switchPread)) == SQLITE_OK);
    const char* inputs[] = {"./ani-switch.db", "sdmc:/switch/aniswitch/ani-switch.db", "../aniswitch/./ani-switch.db", "/switch/aniswitch/ani-switch.db"};
    char out[1024];
    for (const auto* input : inputs) {
        int rc = vfs->xFullPathname(vfs, input, sizeof(out), out);
        std::printf("path %s -> rc=%d %s\n", input, rc, out);
        CHECK(rc == SQLITE_OK);
        CHECK(std::strcmp(out, "sdmc:/switch/aniswitch/ani-switch.db") == 0);
    }
    CHECK(vfs->xFullPathname(vfs, "sdmc:/../../a.db", sizeof(out), out) == SQLITE_OK);
    CHECK(std::strcmp(out, "sdmc:/a.db") == 0);
    CHECK(vfs->xFullPathname(vfs, inputs[0], 8, out) == SQLITE_CANTOPEN);

    std::filesystem::create_directories("sdmc:/switch/aniswitch");
    CHECK(!std::filesystem::exists("sdmc:/switch/aniswitch/ani-switch.db"));
    sqlite3* db = nullptr;
    auto open = [&] {
        int rc = sqlite3_open_v2("./ani-switch.db", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "unix-none");
        if (rc != SQLITE_OK) std::fprintf(stderr, "open: %s\n", sqlite3_errmsg(db));
        CHECK(rc == SQLITE_OK);
    };
    open();
    CHECK(sqlite3_exec(db, "PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL; CREATE TABLE test(value TEXT); INSERT INTO test VALUES('persisted');", nullptr, nullptr, nullptr) == SQLITE_OK);
    CHECK(sqlite3_close(db) == SQLITE_OK);
    open();
    sqlite3_stmt* statement = nullptr;
    CHECK(sqlite3_prepare_v2(db, "SELECT value FROM test", -1, &statement, nullptr) == SQLITE_OK);
    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    CHECK(std::strcmp(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)), "persisted") == 0);
    sqlite3_finalize(statement);
    CHECK(sqlite3_exec(db, "BEGIN; UPDATE test SET value='rolled back'; ROLLBACK;", nullptr, nullptr, nullptr) == SQLITE_OK);
    CHECK(sqlite3_close(db) == SQLITE_OK);
    open();
    CHECK(sqlite3_prepare_v2(db, "SELECT value FROM test", -1, &statement, nullptr) == SQLITE_OK);
    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    CHECK(std::strcmp(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)), "persisted") == 0);
    sqlite3_finalize(statement);
    CHECK(sqlite3_close(db) == SQLITE_OK);
    open();
    failReads = true;
    CHECK(sqlite3_prepare_v2(db, "SELECT value FROM test", -1, &statement, nullptr) != SQLITE_OK);
    sqlite3_finalize(statement);
    failReads = false;
    CHECK(sqlite3_close(db) == SQLITE_OK);
    CHECK(std::filesystem::exists("sdmc:/switch/aniswitch/ani-switch.db"));
    CHECK(!std::filesystem::exists("sdmc:/switch/aniswitch/ani-switch.db-journal"));
    std::filesystem::remove("sdmc:/switch/aniswitch/ani-switch.db");
    std::puts("test_sqlite_devoptab: OK");
}
