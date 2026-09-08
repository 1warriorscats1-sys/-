// switch_runtime.cpp — Nintendo Switch entry point for the TH095 port
// (the Switch twin of linux_runtime.cpp).
//
// Differences from linux_runtime.cpp:
//  * no execinfo/backtrace and no sigaction+SA_SIGINFO in newlib — a plain
//    signal() handler that writes "<data dir>/th095-crash.txt";
//  * no data-directory discovery here: file access is transparently
//    translated to the SD data directory by switch_compat.cpp
//    (TranslatePath); without th095.dat the game shows its own error
//    screens, which is more honest than failing silently;
//  * the window, input (gamepad) and audio come from the Win32
//    compatibility layer (switch_compat.cpp) once the game's WinMain runs.

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPTSTR, int);

extern "C" const char *th095_switch_data_dir();

namespace th095
{
namespace port
{

volatile sig_atomic_t g_reportingCrash = 0;

void WriteCrashLine(int file, const char *line)
{
    if (line != NULL) write(file, line, strlen(line));
}

void ReportFatalSignal(int signalNumber)
{
    if (g_reportingCrash)
        _exit(128 + signalNumber);
    g_reportingCrash = 1;

    char path[512];
    snprintf(path, sizeof(path), "%s/th095-crash.txt", th095_switch_data_dir());
    int file = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file >= 0)
    {
        char line[160];
        snprintf(line, sizeof(line), "signal=%d pid=%ld\n", signalNumber,
                 static_cast<long>(getpid()));
        WriteCrashLine(file, line);
        fsync(file);
        close(file);
    }

    signal(signalNumber, SIG_DFL);
    raise(signalNumber);
    _exit(128 + signalNumber);
}

void InstallCrashReporter()
{
    signal(SIGSEGV, ReportFatalSignal);
    signal(SIGABRT, ReportFatalSignal);
    signal(SIGFPE, ReportFatalSignal);
    signal(SIGILL, ReportFatalSignal);
    signal(SIGBUS, ReportFatalSignal);
}

void LogArchiveRequest(const char *)
{
    // No archive-access log on the Switch: it would be an SD write on the
    // hot path.
}

} // namespace port
} // namespace th095

int main(int, char **)
{
    th095::port::InstallCrashReporter();

    // Clear a stale crash report from a previous run.
    char path[512];
    snprintf(path, sizeof(path), "%s/th095-crash.txt", th095_switch_data_dir());
    unlink(path);

    return WinMain(NULL, NULL, NULL, 0);
}
