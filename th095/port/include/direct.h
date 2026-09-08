#pragma once
// Port shim for the MSVC <direct.h> used by the TH095 reconstruction.
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#define _mkdir(path) mkdir((path), 0755)
#define _getcwd(buf, size) getcwd((buf), (size))
#define _chdir chdir
