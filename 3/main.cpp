#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef DWORD pid_t;
#else
#include <unistd.h>
#include <sys/file.h>
#include <fcntl.h>
#include <sys/types.h>
#endif

static char* exec_path = nullptr;

pid_t get_pid() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto duration = now_ms.time_since_epoch();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration - secs);
    std::time_t tt = secs.count();
#ifdef _WIN32
    std::tm tm;
    localtime_s(&tm, &tt);
#else
    std::tm tm = *localtime(&tt);
#endif
    char buf[100];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    char full[100];
    snprintf(full, sizeof(full), "%s.%03d", buf, (int)millis.count());
    return std::string(full);
}

void log_line(const std::string& msg) {
#ifdef _WIN32
    HANDLE h = CreateFileA("log.txt", FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    OVERLAPPED ov = {0};
    if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &ov)) { CloseHandle(h); return; }
    DWORD written;
    WriteFile(h, msg.c_str(), (DWORD)msg.length(), &written, NULL);
    WriteFile(h, "\n", 1, &written, NULL);
    FlushFileBuffers(h);
    UnlockFileEx(h, 0, 1, 0, &ov);
    CloseHandle(h);
#else
    int fd = open("log.txt", O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    write(fd, msg.c_str(), msg.length());
    write(fd, "\n", 1);
    fsync(fd);
    flock(fd, LOCK_UN);
    close(fd);
#endif
}

void log_event(const char* type) {
    std::string ts = get_timestamp();
    pid_t pid = get_pid();
    char buf[200];
    snprintf(buf, sizeof(buf), "%s pid=%d time=%s", type, (int)pid, ts.c_str());
    log_line(buf);
}

void log_counter(int value) {
    std::string ts = get_timestamp();
    pid_t pid = get_pid();
    char buf[200];
    snprintf(buf, sizeof(buf), "COUNTER pid=%d time=%s value=%d", (int)pid, ts.c_str(), value);
    log_line(buf);
}

int read_counter() {
#ifdef _WIN32
    HANDLE h = CreateFileA("counter.txt", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    OVERLAPPED ov = {0};
    if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &ov)) { CloseHandle(h); return 0; }
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    char buf[32] = {0};
    DWORD read;
    ReadFile(h, buf, sizeof(buf)-1, &read, NULL);
    int val = 0;
    if (read > 0) val = atoi(buf);
    UnlockFileEx(h, 0, 1, 0, &ov);
    CloseHandle(h);
    return val;
#else
    int fd = open("counter.txt", O_RDWR | O_CREAT, 0666);
    if (fd < 0) return 0;
    flock(fd, LOCK_EX);
    char buf[32] = {0};
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    int val = 0;
    if (n > 0) val = atoi(buf);
    flock(fd, LOCK_UN);
    close(fd);
    return val;
#endif
}

void write_counter(int val) {
#ifdef _WIN32
    HANDLE h = CreateFileA("counter.txt", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    OVERLAPPED ov = {0};
    if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &ov)) { CloseHandle(h); return; }
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    SetEndOfFile(h);
    char buf[32];
    int len = sprintf(buf, "%d", val);
    DWORD written;
    WriteFile(h, buf, len, &written, NULL);
    FlushFileBuffers(h);
    UnlockFileEx(h, 0, 1, 0, &ov);
    CloseHandle(h);
#else
    int fd = open("counter.txt", O_RDWR | O_CREAT, 0666);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    ftruncate(fd, 0);
    lseek(fd, 0, SEEK_SET);
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d", val);
    write(fd, buf, len);
    fsync(fd);
    flock(fd, LOCK_UN);
    close(fd);
#endif
}

#ifdef _WIN32
static HANDLE master_lock_handle = INVALID_HANDLE_VALUE;
#else
static int master_lock_fd = -1;
#endif

bool try_acquire_master_lock() {
#ifdef _WIN32
    HANDLE h = CreateFileA("master.lck", GENERIC_READ, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    OVERLAPPED ov = {0};
    if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov)) {
        CloseHandle(h);
        return false;
    }
    master_lock_handle = h;
    return true;
#else
    int fd = open("master.lck", O_RDWR | O_CREAT, 0666);
    if (fd < 0) return false;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        return false;
    }
    master_lock_fd = fd;
    return true;
#endif
}

void release_master_lock() {
#ifdef _WIN32
    if (master_lock_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(master_lock_handle);
        master_lock_handle = INVALID_HANDLE_VALUE;
    }
#else
    if (master_lock_fd >= 0) {
        close(master_lock_fd);
        master_lock_fd = -1;
    }
#endif
}

pid_t spawn_child(const char* arg) {
#ifdef _WIN32
    std::string cmd_str = "\"" + std::string(exec_path) + "\" " + arg;
    std::vector<char> cmd_vec(cmd_str.begin(), cmd_str.end());
    cmd_vec.push_back('\0');
    STARTUPINFO si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessA(NULL, cmd_vec.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return 0;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return pi.dwProcessId;
#else
    pid_t pid = fork();
    if (pid == 0) {
        execl(exec_path, exec_path, arg, (char*)nullptr);
        exit(1);
    } else if (pid > 0) {
        return pid;
    } else {
        return 0;
    }
#endif
}

bool is_process_alive(pid_t pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (h == NULL) return false;
    DWORD code;
    BOOL ret = GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return ret && (code == STILL_ACTIVE);
#else
    return (kill(pid, 0) == 0);
#endif
}

void child1_behavior() {
    log_event("CHILD1_START");
    int c = read_counter();
    write_counter(c + 10);
    log_event("CHILD1_EXIT");
    exit(0);
}

void child2_behavior() {
    log_event("CHILD2_START");
    int c = read_counter();
    write_counter(c * 2);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    c = read_counter();
    write_counter(c / 2);
    log_event("CHILD2_EXIT");
    exit(0);
}

int main(int argc, char* argv[]) {
    exec_path = argv[0];

    if (argc > 1) {
        if (strcmp(argv[1], "--child1") == 0) {
            child1_behavior();
            return 0;
        }
        if (strcmp(argv[1], "--child2") == 0) {
            child2_behavior();
            return 0;
        }
    }

    log_event("START");
    bool is_master = try_acquire_master_lock();

    std::atomic<bool> quit(false);
    std::thread input_thread([&](){
        std::string line;
        while (std::getline(std::cin, line) && !quit) {
            if (line.rfind("set ", 0) == 0) {
                try {
                    int val = std::stoi(line.substr(4));
                    write_counter(val);
                } catch (...) {}
            } else if (line == "quit") {
                quit = true;
                break;
            }
        }
    });

    auto t_start = std::chrono::steady_clock::now();
    auto t_last_inc = t_start;
    auto t_last_log = t_start;
    auto t_last_spawn = t_start;
    pid_t child1 = 0, child2 = 0;

    while (!quit) {
        auto now = std::chrono::steady_clock::now();

        if (now - t_last_inc >= std::chrono::milliseconds(300)) {
            int c = read_counter();
            write_counter(c + 1);
            t_last_inc = now;
        }

        if (is_master) {
            if (now - t_last_log >= std::chrono::seconds(1)) {
                int c = read_counter();
                log_counter(c);
                t_last_log = now;
            }

            if (now - t_last_spawn >= std::chrono::seconds(3)) {
                bool c1_alive = (child1 != 0 && is_process_alive(child1));
                bool c2_alive = (child2 != 0 && is_process_alive(child2));
                if (c1_alive || c2_alive) {
                    log_line("SKIP_SPAWN: previous children still running");
                } else {
                    child1 = spawn_child("--child1");
                    child2 = spawn_child("--child2");
                }
                t_last_spawn = now;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (is_master) release_master_lock();
    if (input_thread.joinable()) input_thread.join();
    log_event("EXIT");
    return 0;
}