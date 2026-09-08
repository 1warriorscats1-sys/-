// linux_runtime.cpp — host (x86-64 Linux) entry point for the TH095 port.
//
// Modeled on the TH08 port's linux_runtime.cpp (N0zoM1z0/th08 + saekaze's
// th08-switch, MIT). TH095 keeps its own game entry point (WinMain in
// upstream src/Main.cpp); this file only provides:
//   * a fatal-signal reporter writing th095-crash.txt into the data folder,
//   * data-directory discovery (--data-dir / cwd / TH095_DATA_DIR),
//   * process main().
//
// The window, input and audio come from the Win32 compatibility layer
// (win32_compat.cpp) once the game's WinMain runs.

#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPTSTR, int);

namespace th095
{
namespace port
{

int g_argumentCount = 0;
char **g_arguments = NULL;
volatile sig_atomic_t g_reportingCrash = 0;

void WriteCrashLine(int file, const char *line)
{
    if (line != NULL) write(file, line, strlen(line));
}

void ReportFatalSignal(int signalNumber, siginfo_t *signalInfo, void *)
{
    if (g_reportingCrash)
        _exit(128 + signalNumber);
    g_reportingCrash = 1;

    int file = open("th095-crash.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file >= 0)
    {
        char line[160];
        snprintf(line, sizeof(line), "signal=%d fault-address=%p pid=%ld\n", signalNumber,
                 signalInfo != NULL ? signalInfo->si_addr : NULL, static_cast<long>(getpid()));
        WriteCrashLine(file, line);

        void *frames[64];
        int frameCount = backtrace(frames, sizeof(frames) / sizeof(frames[0]));
        backtrace_symbols_fd(frames, frameCount, file);
        fsync(file);
        close(file);
    }

    signal(signalNumber, SIG_DFL);
    raise(signalNumber);
    _exit(128 + signalNumber);
}

void InstallSignalHandler(int signalNumber)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = ReportFatalSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigaction(signalNumber, &action, NULL);
}

void InstallCrashReporter()
{
    InstallSignalHandler(SIGSEGV);
    InstallSignalHandler(SIGABRT);
    InstallSignalHandler(SIGFPE);
    InstallSignalHandler(SIGILL);
    InstallSignalHandler(SIGBUS);
}

// Chooses the game data directory. Order:
//   1. --data-dir DIR / --data-dir=DIR argument,
//   2. $TH095_DATA_DIR,
//   3. the current working directory.
// Returns false (host builds are strict; the game data must be present)
// unless the directory was explicitly given.
bool ConfigureDataDirectory(bool *dataDirectoryMissing)
{
    if (dataDirectoryMissing != NULL)
        *dataDirectoryMissing = false;

    const char *directory = NULL;
    for (int index = 1; index < g_argumentCount; ++index)
    {
        if (strcmp(g_arguments[index], "--data-dir") == 0)
        {
            if (++index >= g_argumentCount)
            {
                fprintf(stderr, "th095-port: --data-dir requires a directory path\n");
                return false;
            }
            directory = g_arguments[index];
        }
        else if (strncmp(g_arguments[index], "--data-dir=", 11) == 0)
        {
            directory = g_arguments[index] + 11;
        }
    }

    if (directory == NULL)
        directory = getenv("TH095_DATA_DIR");

    if (directory != NULL && directory[0] != '\0')
    {
        if (chdir(directory) != 0)
        {
            fprintf(stderr, "th095-port: unable to enter data directory: %s\n", directory);
            return false;
        }
    }

    struct stat info;
    if (stat("th095.dat", &info) != 0 || !S_ISREG(info.st_mode))
    {
        fprintf(stderr,
                "th095-port: th095.dat not found in %s. Pass --data-dir DIR with your legally\n"
                "obtained original TH095 1.02a game files (th095.dat, thbgm.dat, ...).\n",
                directory != NULL ? directory : "the working directory");
        if (dataDirectoryMissing != NULL)
            *dataDirectoryMissing = true;
        return false;
    }

    unlink("th095-crash.txt");
    unlink("th095-files.txt");
    unlink("th095-render.txt");
    return true;
}

void LogArchiveRequest(const char *path)
{
    FILE *file = fopen("th095-files.txt", "ab");
    if (file == NULL)
        return;
    fprintf(file, "thread=%08lx path=%s\n", (unsigned long)GetCurrentThreadId(), path != NULL ? path : "<null>");
    fclose(file);
}

void SetArguments(int argc, char **argv)
{
    g_argumentCount = argc;
    g_arguments = argv;
}

} // namespace port
} // namespace th095

int main(int argc, char **argv)
{
    th095::port::SetArguments(argc, argv);
    th095::port::InstallCrashReporter();
    if (!th095::port::ConfigureDataDirectory(NULL))
        return 2;
    return WinMain(NULL, NULL, NULL, 0);
}
