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

const GUID CLSID_ShellLink = { 0x00021401, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46} };
const GUID IID_IShellLinkA = { 0x000214F9, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46} };
const GUID IID_IPersistFile = { 0x0000010b, 0x0000, 0x0000, {0xC0, 0, 0, 0, 0, 0, 0x46} };
