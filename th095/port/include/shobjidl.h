#pragma once
#include <windows.h>

struct IPersistFile
{
    virtual HRESULT Load(LPCWSTR, DWORD) = 0;
    virtual ULONG Release() = 0;
};

struct IShellLink
{
    virtual HRESULT QueryInterface(REFIID, void **) = 0;
    virtual HRESULT GetPath(LPSTR, int, WIN32_FIND_DATA *, DWORD) = 0;
    virtual ULONG Release() = 0;
};

// The reconstruction uses the ANSI spelling.
typedef IShellLink IShellLinkA;
typedef IShellLink *LPIShellLink;
typedef IShellLinkA *LPIShellLinkA;

// Defined once in win32_compat.cpp (single TU) to avoid
// multiple-definition errors across TUs that include this header.
extern const GUID CLSID_ShellLink;
extern const GUID IID_IShellLinkA;
extern const GUID IID_IPersistFile;
