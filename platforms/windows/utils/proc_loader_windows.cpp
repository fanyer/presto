// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2012 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#include "core/pch.h"

//See proc_loader.h for documentation
#include "platforms/windows_common/utils/proc_loader.h"

//Please, keep this list ordered:
//The libraries appear in alphabetical order and procedures loaded from them appear under the corresponding library, also sorted alphabetically

READY_LIBRARY(advapi32, UNI_L("advapi32.dll"));
LIBRARY_CALL(advapi32, BOOL, WINAPI, CreateProcessWithTokenW, (HANDLE hToken, DWORD dwLogonFlags, LPCWSTR lpApplicationName, LPWSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInfo), (hToken, dwLogonFlags, lpApplicationName, lpCommandLine, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInfo), FALSE);

#if defined(MEMTOOLS_ENABLE_CODELOC) && defined(MEMTOOLS_NO_DEFAULT_CODELOC) && defined(MEMORY_LOG_USAGE_PER_ALLOCATION_SITE)
READY_LIBRARY(dbghelp, UNI_L("dbghelp.dll"));
LIBRARY_CALL(dbghelp, BOOL, WINAPI, SymFromAddr, (HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol), (hProcess, Address, Displacement, Symbol), FALSE);
LIBRARY_CALL(dbghelp, BOOL, WINAPI, SymGetLineFromAddr64, (HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line), (hProcess, dwAddr, pdwDisplacement, Line), FALSE);
LIBRARY_CALL(dbghelp, BOOL, WINAPI, SymInitialize, (HANDLE hProcess, PCTSTR UserSearchPath, BOOL fInvadeProcess), (hProcess, UserSearchPath, fInvadeProcess), FALSE);
LIBRARY_CALL(dbghelp, DWORD, WINAPI, SymSetOptions, (DWORD SymOptions), (SymOptions), -1);
#endif //defined(MEMTOOLS_ENABLE_CODELOC) && defined(MEMTOOLS_NO_DEFAULT_CODELOC) && defined(MEMORY_LOG_USAGE_PER_ALLOCATION_SITE)

READY_LIBRARY(dwmapi, UNI_L("dwmapi.dll"));
LIBRARY_CALL(dwmapi, BOOL, WINAPI, DwmDefWindowProc, (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *plResult), (hwnd, msg, wParam, lParam, plResult), FALSE);
LIBRARY_CALL(dwmapi, HRESULT, WINAPI, DwmExtendFrameIntoClientArea, (HWND hWnd, const MARGINS *pMarInset), (hWnd, pMarInset), E_NOTIMPL);
LIBRARY_CALL(dwmapi, HRESULT, WINAPI, DwmInvalidateIconicBitmaps, (HWND hwnd), (hwnd), E_NOTIMPL);
LIBRARY_CALL(dwmapi, HRESULT, WINAPI, DwmSetIconicLivePreviewBitmap, (HWND hwnd, HBITMAP hbmp, POINT *pptClient, DWORD dwSITFlags), (hwnd, hbmp, pptClient, dwSITFlags), E_NOTIMPL);
LIBRARY_CALL(dwmapi, HRESULT, WINAPI, DwmSetIconicThumbnail, (HWND hwnd, HBITMAP hbmp, DWORD dwSITFlags), (hwnd, hbmp, dwSITFlags), E_NOTIMPL);
LIBRARY_CALL(dwmapi, HRESULT, WINAPI, DwmSetWindowAttribute, (HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute), (hwnd,	dwAttribute, pvAttribute, cbAttribute), E_NOTIMPL);

READY_LIBRARY(kernel32, UNI_L("kernel32.dll"));
LIBRARY_CALL(kernel32, int, WINAPI, GetSystemDefaultLocaleName, (LPWSTR lpLocaleName, int cchLocaleName), (lpLocaleName, cchLocaleName), 0);
LIBRARY_CALL(kernel32, BOOL, WINAPI, SetProcessDEPPolicy, (DWORD dwFlags), (dwFlags), FALSE);

READY_LIBRARY(ntdll, UNI_L("ntdll.dll"));
LIBRARY_CALL(ntdll, NTSTATUS, WINAPI, NtQueryInformationFile, (HANDLE FileHandle, IO_STATUS_BLOCK* IoStatusBlock, PVOID FileInformation, ULONG Length, NtDll::FILE_INFORMATION_CLASS FileInformationClass), (FileHandle, IoStatusBlock, FileInformation, Length, FileInformationClass), NT_STATUS_NOT_IMPLEMENTED);
LIBRARY_CALL(ntdll, NTSTATUS, WINAPI, NtQueryInformationProcess, (HANDLE ProcessHandle, NtDll::PROCESS_INFORMATION_CLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength), (ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength), NT_STATUS_NOT_IMPLEMENTED);
LIBRARY_CALL(ntdll, NTSTATUS, WINAPI, NtQueryObject, (HANDLE Handle, NtDll::OBJECT_INFORMATION_CLASS ObjectInformationClass, PVOID ObjectInformation, ULONG ObjectInformationLength, PULONG ReturnLength), (Handle, ObjectInformationClass, ObjectInformation, ObjectInformationLength, ReturnLength), NT_STATUS_NOT_IMPLEMENTED);
LIBRARY_CALL(ntdll, NTSTATUS, WINAPI, NtQuerySystemInformation, (NtDll::SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength), (SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength), NT_STATUS_NOT_IMPLEMENTED);
LIBRARY_CALL(ntdll, NTSTATUS, WINAPI, NtSetInformationProcess, (HANDLE ProcessHandle, NtDll::PROCESS_INFORMATION_CLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength), (ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength), NT_STATUS_NOT_IMPLEMENTED);

READY_LIBRARY(shell32, UNI_L("shell32.dll"));
LIBRARY_CALL(shell32, HRESULT, WINAPI, SHCreateItemFromParsingName, (PCWSTR pszPath, IBindCtx *pbc, REFIID riid, void **ppv), (pszPath, pbc, riid, ppv), E_NOTIMPL);
LIBRARY_CALL(shell32, HRESULT, WINAPI, SHGetKnownFolderPath, (REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, __out PWSTR *ppszPath), (rfid, dwFlags, hToken, ppszPath), E_NOTIMPL);
LIBRARY_CALL(shell32, HRESULT, WINAPI, SHGetStockIconInfo, (SHSTOCKICONID siid, UINT uFlags, __inout SHSTOCKICONINFO *psii), (siid, uFlags, psii), E_NOTIMPL);
LIBRARY_CALL(shell32, HRESULT, WINAPI, SHOpenWithDialog, (HWND hwndParent, const OPENASINFO *poainfo), (hwndParent, poainfo), E_NOTIMPL);

READY_LIBRARY(user32, UNI_L("user32.dll"));
LIBRARY_CALL(user32, BOOL, WINAPI, CloseGestureInfoHandle, (HGESTUREINFO hGestureInfo), (hGestureInfo), FALSE);
LIBRARY_CALL(user32, BOOL, WINAPI, GetGestureInfo, (HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo), (hGestureInfo, pGestureInfo), FALSE);
LIBRARY_CALL(user32, HPOWERNOTIFY, WINAPI, RegisterPowerSettingNotification, (HANDLE hRecipient, LPCGUID PowerSettingGuid, DWORD Flags), (hRecipient, PowerSettingGuid, Flags), NULL);
LIBRARY_CALL(user32, BOOL,  WINAPI, SetGestureConfig, (HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize), (hwnd, dwReserved, cIDs, pGestureConfig, cbSize), FALSE);
LIBRARY_CALL(user32, BOOL, WINAPI, ShutdownBlockReasonCreate, (HWND hWnd, LPCWSTR pwszReason), (hWnd, pwszReason), FALSE);
LIBRARY_CALL(user32, HPOWERNOTIFY, WINAPI, UnregisterPowerSettingNotification, (HPOWERNOTIFY Handle), (Handle), 0);

READY_LIBRARY(uxtheme, UNI_L("uxtheme.dll"));
LIBRARY_CALL(uxtheme, HRESULT, WINAPI, DrawThemeTextEx, (HTHEME theme, HDC hdc, int part, int state, LPCWSTR str, int len, DWORD dwFlags, LPRECT pRect, const DTTOPTS *pOptions), (theme, hdc, part, state, str, len, dwFlags, pRect, pOptions), E_NOTIMPL);
LIBRARY_CALL(uxtheme, HRESULT, WINAPI, SetWindowThemeAttribute, (HWND hwnd, enum WINDOWTHEMEATTRIBUTETYPE eAttribute, __in_bcount(cbAttribute) PVOID pvAttribute, DWORD cbAttribute), (hwnd, eAttribute, pvAttribute, cbAttribute), E_NOTIMPL);
LIBRARY_CALL(uxtheme, BOOL, WINAPI, BeginPanningFeedback, (HWND hwnd), (hwnd), FALSE);
LIBRARY_CALL(uxtheme, BOOL, WINAPI, EndPanningFeedback, (HWND hwnd, BOOL fAnimateBack), (hwnd, fAnimateBack), FALSE);
LIBRARY_CALL(uxtheme, BOOL, WINAPI, UpdatePanningFeedback, (HWND hwnd, LONG lTotalOverpanOffsetX, LONG lTotalOverpanOffsetY, BOOL fInInertia), (hwnd, lTotalOverpanOffsetX, lTotalOverpanOffsetY, fInInertia), FALSE);

READY_LIBRARY(wlanapi, UNI_L("wlanapi.dll"));
LIBRARY_CALL(wlanapi, DWORD, WINAPI, WlanCloseHandle, (HANDLE hClientHandle, PVOID pReserved), (hClientHandle, pReserved), ERROR_NOT_SUPPORTED);
LIBRARY_CALL(wlanapi, DWORD, WINAPI, WlanEnumInterfaces, (HANDLE hClientHandle, PVOID pReserved, PWLAN_INTERFACE_INFO_LIST *ppInterfaceList), (hClientHandle, pReserved, ppInterfaceList), ERROR_NOT_SUPPORTED);
LIBRARY_CALL(wlanapi, VOID, WINAPI, WlanFreeMemory, (PVOID pMemory), (pMemory), );
LIBRARY_CALL(wlanapi, DWORD, WINAPI, WlanGetNetworkBssList, (HANDLE hClientHandle, CONST GUID *pInterfaceGuid, CONST PDOT11_SSID pDot11Ssid, DOT11_BSS_TYPE dot11BssType, BOOL bSecurityEnabled, PVOID pReserved, PWLAN_BSS_LIST *ppWlanBssList), (hClientHandle, pInterfaceGuid, pDot11Ssid, dot11BssType, bSecurityEnabled, pReserved, ppWlanBssList), ERROR_NOT_SUPPORTED);
LIBRARY_CALL(wlanapi, DWORD, WINAPI, WlanOpenHandle, (DWORD dwClientVersion, PVOID pReserved, PDWORD pdwNegotiatedVersion, PHANDLE phClientHandle), (dwClientVersion, pReserved, pdwNegotiatedVersion, phClientHandle), ERROR_INVALID_FUNCTION);
