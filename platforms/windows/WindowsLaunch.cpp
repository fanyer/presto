// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
//
//
#include "core\pch.h"

#include <wintrust.h>
#include <Softpub.h>
#include "platforms/windows/WindowsLaunch.h"
#include "modules/encodings/decoders/utf8-decoder.h"

////////////////////////////////////////////////////////////

LaunchPI* LaunchPI::Create()
{
	// Can't use OP_NEW as this is used outside of core
	return new WindowsLaunchPI();
}

////////////////////////////////////////////////////////////

WindowsLaunchPI::WindowsLaunchPI()
{
}

char* WindowsLaunchPI::EscapeAndQuoteArg(const char* arg)
{
	char* escaped_arg;

	BOOL has_space = (op_strchr(arg, ' ') != NULL);
	if (!op_strchr(arg, '"') && !has_space)
	{
		escaped_arg = new char[1 + op_strlen(arg)];
		op_strcpy(escaped_arg, arg);
		return escaped_arg;
	}

	escaped_arg = new char[1 + (has_space?2:0) + op_strlen(arg)*2]; //worst case

	if (escaped_arg == NULL)
		return escaped_arg;			//OOM

	char* q = escaped_arg;

	if (has_space)
		*(q++) = '"';

	const char* p = arg;

	while (*p)
	{
		if (*p == '"')
		{
			*(q++) = '\\';
			const char *r = p;
			while (*(--r) == '\\') *(q++) = '\\';
		}
		*(q++) = *(p++);
	}

	if (has_space)
		*(q++) = '"';

	*q=0;

	return escaped_arg;
}

uni_char* WindowsLaunchPI::ArgCat(int argc, const char* const* argv)
{
	if (!argc)
		return NULL;

	char *cmd_line;

	unsigned cline_len = 0;
	for (int tmp_argc = 0; tmp_argc < argc; tmp_argc++)
		cline_len += op_strlen(argv[tmp_argc])*2 + 3 + 1;	//Worst case

	cmd_line = new char[cline_len];
	if (!cmd_line)
		return NULL;

	char *pos = cmd_line;
	for (int tmp_argc = 0; tmp_argc < argc; tmp_argc++)
	{
		char* escaped_arg = EscapeAndQuoteArg(argv[tmp_argc]);
		if (!escaped_arg)
		{
			delete[] cmd_line;
			return NULL;
		}

		char* current_arg = escaped_arg;

		while (*current_arg)
			*pos++ = (unsigned char)*current_arg++;

		delete[] escaped_arg;

		*pos++ = ' ';
	}

	*(--pos) = 0;

	//Need to use platform conversion functions from UTF8 to UTF16, because this can be used without core.
	int uni_cline_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, cmd_line, -1, NULL, 0);
	uni_char* uni_cmd_line = new uni_char[uni_cline_len];
	if (!uni_cmd_line)
	{
		delete[] cmd_line;
		return NULL;
	}
	
	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, cmd_line, -1, uni_cmd_line, uni_cline_len) == 0)
	{
		delete[] cmd_line;
		delete[] uni_cmd_line;
		return NULL;
	}

	delete[] cmd_line;

	return uni_cmd_line;
}

BOOL WindowsLaunchPI::Launch(const uni_char *exe_name, int argc, const char* const* argv)
{

	uni_char* uni_cmd_line = ArgCat(argc, argv);

	HINSTANCE error = ShellExecute(g_main_hwnd, NULL, exe_name, uni_cmd_line, NULL, SW_NORMAL);

	delete[] uni_cmd_line;

	return (error > (HINSTANCE)32);

}

BOOL WindowsLaunchPI::LaunchAndWait(const uni_char *exe_name, int argc, const char* const* argv)
{
	uni_char* uni_cmd_line = ArgCat(argc, argv);
	OpAutoArray<uni_char> aptr(uni_cmd_line);
	SHELLEXECUTEINFO info;
	memset(&info, 0, sizeof(info));

	info.cbSize = sizeof(info);
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	info.lpVerb = UNI_L("open");
	info.lpFile = exe_name;
	info.lpParameters = uni_cmd_line;
	info.nShow = SW_SHOW;
	
	DWORD exit_code;

	if ( ! ShellExecuteEx(&info) )
	{
		return FALSE;
	}
	else
	{
		if (WaitForSingleObject(info.hProcess, INFINITE) == WAIT_FAILED)
		{
			return FALSE;
		}
		if (GetExitCodeProcess(info.hProcess, &exit_code) == FALSE)
		{
			return FALSE;
		}
	}
	CloseHandle(info.hProcess);
	return exit_code == 0 ? TRUE : FALSE;
}

BOOL WindowsLaunchPI::VerifyExecutable(const uni_char *executable, const uni_char* vendor_name)
{
	WINTRUST_DATA WTData;
	WINTRUST_FILE_INFO FileInfo;
	GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;

	ZeroMemory(&WTData, sizeof(WTData));
	ZeroMemory(&FileInfo, sizeof(FileInfo));
	WTData.cbStruct = sizeof(WTData);
	WTData.dwUIChoice = WTD_UI_NONE;
	WTData.dwUnionChoice = WTD_CHOICE_FILE;
	WTData.pFile = &FileInfo;
	FileInfo.cbStruct = sizeof(FileInfo);
	FileInfo.pcwszFilePath = executable;

	if (WinVerifyTrust(NULL, &action, &WTData) != ERROR_SUCCESS)
		return FALSE;

	DWORD size;
	DWORD dwEncoding;
    PCMSG_SIGNER_INFO pSignerInfo = NULL;
	PCCERT_CONTEXT pCertContext = NULL;
    HCERTSTORE hStore = NULL;
    HCRYPTMSG hMsg = NULL; 
    CERT_INFO CertInfo;
    uni_char* signer_name = NULL;
	BOOL result = FALSE;

	if (!CryptQueryObject(CERT_QUERY_OBJECT_FILE, executable,
		CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED, CERT_QUERY_FORMAT_FLAG_BINARY, 0,
		&dwEncoding, NULL, NULL, &hStore, &hMsg, NULL))
		return FALSE;

	do
	{
		if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, NULL, &size))
			break;

		if ((pSignerInfo = (PCMSG_SIGNER_INFO)LocalAlloc(LPTR, size)) == NULL)
			break;

		if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, (PVOID)pSignerInfo, &size))
			break;

		CertInfo.Issuer = pSignerInfo->Issuer;
		CertInfo.SerialNumber = pSignerInfo->SerialNumber;

		if ((pCertContext = CertFindCertificateInStore(hStore, dwEncoding, 0, CERT_FIND_SUBJECT_CERT, (PVOID)&CertInfo, NULL)) == NULL)
			break;

		if ((size = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, NULL, 0)) == 0)
			break;

		if ((signer_name =  new uni_char[size]) == 0)
			break;

		if (!CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, signer_name, size))
			break;

		if (vendor_name == NULL)
			vendor_name = UNI_L("Opera Software ASA");

		if (wcscmp(signer_name, vendor_name) == 0)
			result = TRUE;

	}
	while (false);

	if (signer_name != NULL) delete[] signer_name;
	if (pSignerInfo != NULL) LocalFree(pSignerInfo);
	if (pCertContext != NULL) CertFreeCertificateContext(pCertContext);
	if (hStore != NULL) CertCloseStore(hStore, 0);
	if (hMsg != NULL) CryptMsgClose(hMsg);

	return result;

}

////////////////////////////////////////////////////////////

