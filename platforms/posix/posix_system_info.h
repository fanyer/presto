/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_SYSTEM_INFO_H
#define POSIX_SYSTEM_INFO_H __FILE__
# ifdef POSIX_OK_SYS // API_POSIX_SYS

#include POSIX_SYSINFO_BASEINC // see TWEAK_POSIX_SYSINFO_BASEINC


/** Partial implementation of OpSystemInfo.
 *
 * This class provides the parts of OpSystemInfo that posix currently can, using
 * the POSIX APIs.  Projects which extend OpSystemInfo can arrange for it to be
 * based on their extended base-class.  All projects using this class should
 * derive their own class from it, adding implementations of the methods it
 * doesn't implement.  Your compiler should tell you about unimplemented
 * methods, since they're pure virtual on the virtual base classes.
 *
 * An instance of this class requires second-stage construction: call its
 * Construct() method and check the status it returns.  Derived classes which
 * have their own second-stage construction requirements should over-ride this
 * method (it's virtual) with one that calls that of this base-class.
 */
class PosixSystemInfo :
	public PosixOpSysInfoBase // see TWEAK_POSIX_SYSINFO_BASE
{
protected:
	friend class OpSystemInfo; // for its Create()
	/** Constructor.
	 *
	 * Only for use by OpSystemInfo::Create() or a derived class's constructor.
	 */
	PosixSystemInfo();

	/** Set-up.
	 *
	 * Must be called by OpSystemInfo::Create(), possibly via derived class
	 * over-riding this method.
	 *
	 * @return OK on success, else error value indicating problem.
	 */
	virtual OP_STATUS Construct();

public:

#ifdef POSIX_OK_DNS
# ifdef SYNCHRONOUS_HOST_RESOLVING // FEATURE_SYNCHRONOUS_DNS
	virtual BOOL HasSyncLookup() { return TRUE; } // yes, we can do synchronous.
# endif // SYNCHRONOUS_HOST_RESOLVING
# if 0 //def PI_HOST_RESOLVER
	// TODO: implement us !
	virtual OP_STATUS GetDnsAddress(OpSocketAddress** dns_address, int number = 0);
	virtual OP_STATUS GetDnsSuffixes(OpString* result);
	virtual OpAutoVector<OpSocketAddress> *ResolveExternalHost(const uni_char* hostname);
# endif
#endif // POSIX_OK_DNS

#ifdef POSIX_OK_FILE_UTIL
# ifdef OPSYSTEMINFO_FILEUTILS
	virtual BOOL GetIsExecutable(OpString* filename); // can we run this ?
# endif
# ifdef POSIX_OK_NATIVE
	virtual OP_STATUS PathsEqual(const uni_char* p0, const uni_char* p1, BOOL* equal);
# endif
#endif // POSIX_OK_FILE_UTIL

#ifdef POSIX_OK_HOST
	/** Platform name for use by DOM.
	 *
	 * Initialized by constructor; the sysname value returned by uname is used,
	 * where available (else "Unix").  For use by GetPlatformStr().
	 */
	uni_char m_platform[POSIX_PLATNAME_LENGTH]; // ARRAY OK 2010-08-12 markuso
public:
	virtual const uni_char *GetPlatformStr()
	{
		OP_ASSERT(m_platform[0] || !"Did you neglect to call Construct() ?");
		return m_platform;
	}
#endif // POSIX_OK_HOST

#ifdef POSIX_OK_SYS_LOCALE
private:
	OpString8 m_system_encoding; ///< Lazily-cached value for GetSystemEncodingL().
	const char *GetLanginfoEncodingL() const;
public:
	virtual const char *GetSystemEncodingL();

	static OP_STATUS LookupUserLanguages(OpString *result); // For use before InitL().
	virtual OP_STATUS GetUserLanguages(OpString *result)
		{ return LookupUserLanguages(result); }
	virtual void GetUserCountry(uni_char result[3]);

# ifdef EXTERNAL_APPLICATIONS_SUPPORT
private:
	OpString m_default_text_editor; ///< Lazily-cached value for GetDefaultTextEditorL().
public:
	virtual const uni_char* GetDefaultTextEditorL();
# endif
#endif // POSIX_OK_SYS_LOCALE

#if defined(POSIX_OK_DNS) || defined(OPSYSTEMINFO_GETSYSTEMIP)
private:
	OpString8 m_host_name; ///< Cached value of GetLocalHostName, computed by constructor.
#endif // POSIX_OK_DNS || OPSYSTEMINFO_GETSYSTEMIP
#ifdef OPSYSTEMINFO_GETSYSTEMIP
	OpString8 m_sys_ip; ///< Cached value of GetSystemIp, computed by constructor.
	/** Discover the local system's IP address.
	 *
	 * Implemented in net/posix_interface.cpp; called by Construct() to set
	 * m_sys_ip.  If it can't get an answer any efficient way, it launches an
	 * asynchronous DNS look-up on m_host_name (if set), so m_sys_ip might not
	 * be set until somewhat after this method returns !
	 *
	 * @return OK on success, else relevant error.
	 */
	OP_STATUS QueryLocalIP();
public:
	virtual OP_STATUS GetSystemIp(OpString& ip) { return ip.Set(m_sys_ip); }
#endif

#ifdef POSIX_OK_DNS // API_POSIX_DNS
public:
	/** Helper function for PosixHostResolver::GetLocalHostName().
	 *
	 * @param name OpString to be set to local host name.
	 * @return See OpStatus.
	 */
	OP_STATUS GetLocalHostName(OpString& name) { return name.Set(m_host_name); }
#endif // POSIX_OK_DNS

#ifdef OPSYSTEMINFO_GETPROCESSTIME
private:
	long m_cpu_tick_rate; // Cached value from 'sysconf(_SC_CLK_TCK)'.
public:
	virtual OP_STATUS GetProcessTime(double *time);
#endif // OPSYSTEMINFO_GETPROCESSTIME

#ifdef POSIX_OK_MISC
	virtual OP_STATUS OpFileLengthToString(OpFileLength length, OpString8* result);
#endif

#ifdef POSIX_OK_PATH
# ifdef POSIX_EXTENDED_SYSINFO
	virtual BOOL RemoveIllegalFilenameCharacters(OpString& path, BOOL replace);
	virtual BOOL RemoveIllegalPathCharacters(OpString& path, BOOL replace);
	virtual OP_STATUS GetIllegalFilenameCharacters(OpString* illegalchars);
# endif // POSIX_EXTENDED_SYSINFO
# ifdef OPSYSTEM_GET_UNIQUE_FILENAME_PATTERN
	virtual const uni_char* GetUniqueFileNamePattern() { return UNI_L("_%d"); }
# endif
#endif // POSIX_OK_PATH

#ifdef POSIX_OK_PROCESS // API_POSIX_PROCESS
# ifdef OPSYSTEMINFO_GETPID /* Unused, so not worth cacheing ! */
	virtual INT32 GetCurrentProcessId() { return static_cast<INT32>(getpid()); }
# endif
# ifdef OPSYSTEMINFO_GETBINARYPATH
#  ifdef POSIX_HAS_GETBINARYPATH
	virtual OP_STATUS GetBinaryPath(OpString *path);
#  endif
# endif
# ifdef EXTERNAL_APPLICATIONS_SUPPORT
#  ifdef DU_REMOTE_URL_HANDLER
#define PosixExecuteApplication PlatformExecuteApplication
#  else
#define PosixExecuteApplication ExecuteApplication
#  endif
	virtual OP_STATUS PosixExecuteApplication(const uni_char* application,
											  const uni_char* args,
											  BOOL silent_errors);
# endif // EXTERNAL_APPLICATIONS_SUPPORT
#endif // POSIX_OK_PROCESS

#ifdef POSIX_SERIALIZE_FILENAME
	/* Only defined in pi #ifdef PI_EXPAND_ENV_VARIABLES, but posix
	 * provides this method even if pi doesn't !  Even so, we want it virtual so
	 * that Mac can over-ride it with its added complications. */
	virtual OP_STATUS ExpandSystemVariablesInString(const uni_char* in, OpString* out);

	/** Use system variables to re-express a path-name.
	 *
	 * This is an extension to the porting interface, so may be superseded by pi
	 * at some point.  The implementation here handles various environment
	 * variables using the idioms expanded by ExpandSystemVariablesInString.
	 * Derived classes which over-ride that may over-ride this also (but calling
	 * this from the over-ride is recommended); but if you just want to expand
	 * the set of environment variables supported, discuss with the module owner
	 * - we can extend it or tweakify it.
	 *
	 * @param path Raw (but possibly relative) path to file or directory.
	 * @return NULL on OOM, else a pointer to a freshly allocated string, which
	 * the caller is responsible for delete[]ing when no longer needed.
	 */
	virtual uni_char* SerializeFileName(const uni_char *path);
#endif // POSIX_SERIALIZE_FILENAME

#ifdef USE_OP_THREAD_TOOLS
# ifdef THREAD_SUPPORT
#  ifdef POSIX_OK_THREAD
	/** Are we currently running in Opera's main thread? */
	virtual BOOL IsInMainThread() { return g_opera->posix_module.OnCoreThread(); }
#  else
	/* You'll get a linker error if you don't implement this yourself: import
	 * API_POSIX_THREAD to get the above, or stop using THREAD_SUPPORT, unless
	 * you can do it properly !
	 */
#  endif
# else // we can't possibly *not* be in the only thread:
	virtual BOOL IsInMainThread() { return TRUE; }
# endif // THREAD_SUPPORT
#endif // USE_OP_THREAD_TOOLS

#if 0 // Possibly handled by LSB, maybe even POSIX:
	virtual void GetProxySettingsL(const uni_char *protocol, int &enabled, OpString &proxyserver);
# ifdef OPSYSTEMINFO_GETFILETYPENAME
	virtual OP_STATUS GetFileTypeName(const uni_char* filename, uni_char *out, size_t out_max_len);
# endif
# ifdef OPSYSTEMINFO_FILEUTILS
	virtual OP_STATUS GetFileHandler(const OpString* filename, OpString& contentType, OpString& handler);
# endif // OPSYSTEMINFO_FILEUTILS
	/* The plugin methods might be sensible once Unix's plugins are broken out
	 * into a separate module, albeit some kind of neutral API for talking to it
	 * may be needed. */
# ifdef _PLUGIN_SUPPORT_
	virtual void GetPluginPathL(const OpStringC &dfpath, OpString &newpath) = 0;
# endif // _PLUGIN_SUPPORT_
# ifdef PI_PLUGIN_DETECT
	virtual OP_STATUS DetectPlugins(const OpStringC& suggested_plugin_paths, class OpPluginDetectionListener* listener);
# endif // PI_PLUGIN_DETECT
#endif // 0

private:
#ifdef POSIX_USE_PROC_CPUINFO
    void InitCPUInfo();

    BOOL m_cpu_info_initialized;

# ifdef OPSYSTEMINFO_CPU_FEATURES
    unsigned int m_cpu_features;
# endif // OPSYSTEMINFO_CPU_FEATURES

# ifdef OPSYSTEMINFO_CPU_ARCHITECTURE
    CPUArchitecture m_cpu_architecture;
# endif // OPSYSTEMINFO_CPU_ARCHITECTURE

public:
# ifdef OPSYSTEMINFO_CPU_FEATURES
    virtual unsigned int GetCPUFeatures()
    {
        InitCPUInfo();
        return m_cpu_features;
    }
# endif // OPSYSTEMINFO_CPU_FEATURES

# ifdef OPSYSTEMINFO_CPU_ARCHITECTURE
    virtual OpSystemInfo::CPUArchitecture GetCPUArchitecture()
    {
        InitCPUInfo();
        return m_cpu_architecture;
    }
# endif // OPSYSTEMINFO_CPU_ARCHITECTURE

#endif // POSIX_USE_PROC_CPUINFO
};

# endif // POSIX_OK_SYS
#endif // POSIX_SYSTEM_INFO_H
