/* lowmem.cpp — run a program as if on a machine with little memory.
 *
 *     hormiga_lowmem <limit-MB> <program> [arguments...]
 *
 * The program runs with a hard cap on the memory it may commit: a Windows job
 * object (JOB_OBJECT_LIMIT_PROCESS_MEMORY), or RLIMIT_AS elsewhere. An
 * allocation past the cap fails inside the program, exactly as it would on a
 * device that has no more. Reports the program's exit and, on Windows, its peak
 * committed memory, and exits with the program's code (or 99 if it could not
 * be started). This is how one machine tests the promise that Hormiga runs on
 * a 2 GB device (okf/concepts/platform/store.md). */
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: hormiga_lowmem <limit-MB> <program> [arguments...]\n");
        return 99;
    }
    const unsigned long long limit = std::strtoull(argv[1], nullptr, 10) * 1024ull * 1024ull;
#if defined(_WIN32)
    std::string cmd;
    for (int i = 2; i < argc; ++i) {
        if (i > 2) cmd += ' ';
        cmd += '"';
        cmd += argv[i];
        cmd += '"';
    }
    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    info.ProcessMemoryLimit = (SIZE_T)limit;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof info)) {
        std::fprintf(stderr, "lowmem: could not create the job object\n");
        return 99;
    }
    STARTUPINFOA si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    std::vector<char> line(cmd.begin(), cmd.end());
    line.push_back(0);
    if (!CreateProcessA(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
        std::fprintf(stderr, "lowmem: could not start %s\n", argv[2]);
        return 99;
    }
    AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION after{};
    QueryInformationJobObject(job, JobObjectExtendedLimitInformation, &after, sizeof after, nullptr);
    std::fprintf(stderr, "lowmem: limit %llu MB, peak %.0f MB, exit %lu\n", limit >> 20,
                 (double)after.PeakProcessMemoryUsed / (1024.0 * 1024.0), (unsigned long)code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(job);
    return (int)code;
#else
    const pid_t pid = fork();
    if (pid < 0) return 99;
    if (pid == 0) {
        rlimit r{(rlim_t)limit, (rlim_t)limit};
        setrlimit(RLIMIT_AS, &r);
        execvp(argv[2], argv + 2);
        _exit(99);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    const int code = WIFEXITED(status) ? WEXITSTATUS(status) : 98;
    std::fprintf(stderr, "lowmem: limit %llu MB, exit %d\n", limit >> 20, code);
    return code;
#endif
}
