/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#ifndef UNIX_OPSYSTEMINFO_H
#define UNIX_OPSYSTEMINFO_H

#include "platforms/posix/posix_system_info.h"

#ifdef PI_HOST_RESOLVER
#include "modules/pi/network/OpSocketAddress.h"
#endif

#include "modules/pi/OpPluginDetectionListener.h"

#include <pthread.h>

class UnixOpSystemInfo: public PosixSystemInfo
{
private:
#if defined(USE_OP_THREAD_TOOLS) && defined(THREAD_SUPPORT) && !defined(POSIX_OK_THREAD)
	static pthread_t m_main_thread;
#endif // USE_OP_THREAD_TOOLS and THREAD_SUPPORT but not POSIX_OK_THREAD

public:
	UnixOpSystemInfo();

	virtual OP_STATUS Construct();

	UINT32 GetPhysicalMemorySizeMB();

#ifdef HAS_ATVEF_SUPPORT
	/** @todo (OGW made default behaviour - Eirik, we must fix) */
	BackChannelStatus GetBackChannelStatus() { return BACKCHANNEL_PERMANENT; }
#endif // HAS_ATVEF_SUPPORT

	void GetProxySettingsL(const uni_char *protocol, int &enabled, OpString &proxyserver);
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	void GetAutoProxyURLL(OpString *url, BOOL *enabled);
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION
	void GetProxyExceptionsL(OpString *exceptions, BOOL *enabled);
	void GetPluginPathL(const OpStringC &dfpath, OpString &newpath);
# if defined (USE_PLUGIN_EVENT_API) && defined (STATIC_NS4PLUGINS)
	OP_STATUS GetStaticPlugin(StaticPlugin& plugin_spec);
# endif // USE_PLUGIN_EVENT_API && STATIC_NS4PLUGINS
	void GetDefaultJavaClassPathL(OpString &target);
	void GetDefaultJavaPolicyFilenameL(OpString &target);

#if defined(USE_OP_THREAD_TOOLS) && defined(THREAD_SUPPORT) && !defined(POSIX_OK_THREAD)
	BOOL IsInMainThread();
#endif // USE_OP_THREAD_TOOLS and THREAD_SUPPORT but not POSIX_OK_THREAD

	OP_STATUS GetFileHandler(const OpString* filename, OpString& contentType, OpString& handler);
	OP_STATUS GetFileHandlers(const OpString& filename,
							  const OpString &content_type,
							  OpVector<OpString>& handlers,
							  OpVector<OpString>& handler_names,
							  OpVector<OpBitmap>& handler_icons,
							  URLType type,
							  UINT32 icon_size  = 16);
	OP_STATUS OpenFileFolder(const OpStringC & file_path, BOOL treat_folders_as_files = TRUE);
	OP_STATUS GetFileTypeInfo(const OpStringC& filename,
							  const OpStringC& content_type,
							  OpString & content_type_name,
							  OpBitmap *& content_type_bitmap,
							  UINT32 content_type_bitmap_size  = 16);
	OP_STATUS GetProtocolHandler(const OpString& uri_string, OpString& protocol, OpString& handler);
	OP_STATUS GetFileTypeName(const uni_char* filename, uni_char *out, size_t out_max_len);
	BOOL AllowExecuteDownloadedContent() {return FALSE;}

	void ComposeExternalMail(const uni_char* to, const uni_char* cc, const uni_char* bcc, const uni_char* subject, const uni_char* message, const uni_char* raw_address, MAILHANDLER mailhandler);

	OP_STATUS PlatformExecuteApplication(const uni_char* application,
										 const uni_char* args,
										 BOOL silent_errors); 

	virtual OP_STATUS OpenURLInExternalApp(const URL& url);

	// Implementation specific methods:
	BOOL IsFullKeyboardAccessActive();

#ifdef PI_POWER_STATUS
	virtual BOOL IsPowerConnected();
	virtual BYTE GetBatteryCharge();
	virtual BOOL IsLowPowerState();
#endif // PI_POWER_STATUS

#ifdef SYNCHRONOUS_HOST_RESOLVING
# ifndef POSIX_OK_DNS
	BOOL HasSyncLookup();
# endif
#endif

#if !(defined(POSIX_CAP_USER_LANGUAGE) && defined(POSIX_OK_SYS_LOCALE))
	OP_STATUS GetUserLanguages(OpString *result);
	void GetUserCountry(uni_char result[3]);
#endif // !(POSIX_CAP_USER_LANGUAGE && POSIX_OK_SYS_LOCALE)

#if defined(GUID_GENERATE_SUPPORT)
	OP_STATUS GenerateGuid(OpGuid &guid);
#endif // GUID_GENERATE_SUPPORT
	
	class OpFolderLister* GetLocaleFolders();

	virtual OpString GetLanguageFolder(const OpStringC &lang_code);

	OP_STATUS GetDefaultWindowTitle(OpString& title);

#ifdef OPSYSTEMINFO_GETBINARYPATH
	virtual OP_STATUS GetBinaryPath(OpString *path);
#endif // OPSYSTEMINFO_GETBINARYPATH

    // Support for desktop-specific {...} variables // DSK-326076
	virtual OP_STATUS ExpandSystemVariablesInString(const uni_char* in, OpString* out);
	virtual uni_char* SerializeFileName(const uni_char *path);

#ifdef OPSYSTEMINFO_CPU_FEATURES
	virtual unsigned int GetCPUFeatures();
#endif // OPSYSTEMINFO_CPU_FEATURES

private:
#ifdef EMBROWSER_SUPPORT
	virtual OP_STATUS SetCustomHighlightColor(BOOL use_custom, COLORREF new_color) {return OpStatus::OK;}
#endif //EMBROWSER_SUPPORT

#ifndef POSIX_OK_DNS
	virtual OP_STATUS GetSystemIp(OpString& ip);
#endif

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual BOOL IsScreenReaderRunning() {return FALSE;}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef PI_HOST_RESOLVER
	OP_STATUS GetDnsAddress(OpSocketAddress** dns_address);
	OP_STATUS GetDnsSuffixes(OpString* result);
	OP_STATUS GetDnsAddressAndSuffixes();

	OpString m_dns_address;
	OpString m_dns_suffixes;
#endif // PI_HOST_RESOLVER

	OpString8 m_system_encoding;

	OpString m_default_text_editor;

#ifdef _UNIX_DESKTOP_
	size_t m_system_memory_in_mb;
#endif
};

OP_STATUS DetectUnixPluginViewers(const OpStringC& suggested_plugin_paths, OpPluginDetectionListener* listener); // base/common/unix_plugindetect.cpp

#endif // UNIX_OPSYSTEMINFO_H
