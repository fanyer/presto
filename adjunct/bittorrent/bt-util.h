/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef BT_UTIL_H
#define BT_UTIL_H

// ----------------------------------------------------

#include "adjunct/m2/src/util/buffer.h"
#include "adjunct/desktop_util/adt/oplist.h"

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"

#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslopt.h"
#include "modules/libssl/sslrand.h"

#include "bt-globals.h"

class PrefsFile;
class OpFile;

//#ifndef SHA_DIGEST_LENGTH
//#define SHA_DIGEST_LENGTH SSL_MAX_HASH_LENGTH
//#endif // SHA_DIGEST_LENGTH

#if defined (MSWIN)
#define FASTCALL    __fastcall
#else
#define FASTCALL
#endif

// HAVE_LTH_MALLOC,

#define BT_LISTENER_PORT					18768
#define MAXUPLOADS							20
#define BT_MAX_OUTGOING_CONNECTIONS			70
#define BT_MAX_TOTAL_CONNECTIONS			100
#define FILE_CREATE_STEPS					1024*1024*50	// create 50M on each iteration
#define SEED_TIMER_INTERVAL					80			// check every n/1000th of a second to allow for other tasks to proceed
#define BUFFER_SIZE							(2*1024*1024)	// 2M buffer
#define DEFAULT_BT_CACHE_SIZE				(10*1024*1024)	// 10M default cache size
#define DEFAULT_BT_CACHE_HEADROOM			(2*1024*1024)	// 2M default cache headroom
#define UNCHOKE_DELAY_30					30
#define UNCHOKE_DELAY_10					10
#define UNCHOKE_DELAY_1						1

#define DOWNLOAD_MAXFILES		32	// max files, hardcoded
#define DOWNLOAD_MAXTRANSFERS	8	// max transfers, hardcoded
#define BT_REQUEST_SIZE		16384
#define USE_BT_DISKCACHE		1	// use disk cache

#define GET_RELATIVE_OFFSET(offset, fileoffset)	(offset - fileoffset)

#if defined(MSWIN)
# include "platforms/windows/windows_ui/res/#buildnr.rci"
#elif defined(UNIX)
# include "platforms/unix/product/config.h"
#elif defined(_MACINTOSH_)
# include "platforms/mac/Resources/buildnum.h"
#elif defined(WIN_CE)
# include "platforms/win_ce/res/buildnum.h"
#else
# pragma error("include the file where VER_BUILD_NUMBER is defined")
#endif

#define _METHODNAME_ ""

//#define BT_FILELOGGING_ENABLED		// only enabled if set in the opera6.ini file
#define BT_UPLOAD_DESTRICTION		// when defined, will enforce a dynamic upload speed restriction (if enabled in prefs)
#define BT_UPLOAD_FIXED_RESTRICTION
//#define BT_UPLOAD_DISABLED			// when defined, will not create a listening socket

//#define DEBUG_BT_TRANSFERPANEL			// used in retail builds to add information to the transfer panel

#ifndef _MACINTOSH_
#ifdef _DEBUG
#define DEBUG_BT_TORRENT
//#define DEBUG_BT_RESOURCE_TRACKING
//#define DEBUG_BT_RESOURCES
//#define DEBUG_BT_UPLOAD
//#define DEBUG_BT_DOWNLOAD
//#define DEBUG_BT_PROFILING
//#define DEBUG_BT_FILEPROFILING
//#define DEBUG_BT_FILEACCESS
//#define DEBUG_BT_HAVE
//#define DEBUG_BT_CHOKING
#define DEBUG_BT_TRACKER
//#define DEBUG_BT_SOCKETS
//#define DEBUG_BT_THROTTLE
//#define DEBUG_BT_BLOCKSELECTION
//#define DEBUG_BT_PEERSPEED
//#define DEBUG_BT_CONNECT
//#define DEBUG_BT_DISKCACHE
//#define DEBUG_BT_METAFILE
#define DEBUG_BT_PEX
#endif
#endif

extern void OutputDebugLogging(const uni_char *tmp, ...);
extern void OutputDebugLogging(const uni_char *tmp, char *);

//#if defined(_DEBUG) && defined (MSWIN)
# define _DEBUGTRACE(x, y) OutputDebugLogging(x, y);
//{	uni_char tmp[2000]; _snwprintf(tmp, 2000, x, y); OutputDebugStringW(tmp); }
# define _DEBUGTRACE8(x, y) OutputDebugLogging(x, y);
//{	char tmp[2000]; _snprintf(tmp, 2000, x, y); OutputDebugStringA(tmp); }

# ifdef DEBUG_BT_PEX
#  define DEBUGTRACE_PEX(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_PEX(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_PEX(x, y) ((void)0)
#  define DEBUGTRACE8_PEX(x, y) ((void)0)
# endif

# ifdef DEBUG_BT_METAFILE
#  define DEBUGTRACE_METAFILE(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_METAFILE(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_METAFILE(x, y) ((void)0)
#  define DEBUGTRACE8_METAFILE(x, y) ((void)0)
# endif

# ifdef DEBUG_BT_CONNECT
#  define DEBUGTRACE_CONNECT(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_CONNECT(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_CONNECT(x, y) ((void)0)
#  define DEBUGTRACE8_CONNECT(x, y) ((void)0)
# endif

# ifdef DEBUG_BT_TORRENT
#  define DEBUGTRACE_TORRENT(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_TORRENT(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_TORRENT(x, y) ((void)0)
#  define DEBUGTRACE8_TORRENT(x, y) ((void)0)
# endif

# ifdef DEBUG_BT_PEERSPEED
#  define DEBUGTRACE_PEERSPEED(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_PEERSPEED(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_PEERSPEED(x, y) ((void)0)
#  define DEBUGTRACE8_PEERSPEED(x, y) ((void)0)
# endif	// DEBUG_BT_PEERSPEED

# ifdef DEBUG_BT_BLOCKSELECTION
#  define DEBUGTRACE_BLOCK(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_BLOCK(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_BLOCK(x, y) ((void)0)
#  define DEBUGTRACE8_BLOCK(x, y) ((void)0)
# endif	// DEBUG_BT_BLOCKSELECTION

# ifdef DEBUG_BT_UPLOAD
#  define DEBUGTRACE_UP(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_UP(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_UP(x, y) ((void)0)
#  define DEBUGTRACE8_UP(x, y) ((void)0)
# endif	// DEBUG_BT_UPLOAD

# ifdef DEBUG_BT_DOWNLOAD
#  define DEBUGTRACE8(x, y) _DEBUGTRACE8(x, y)
#  define DEBUGTRACE(x, y) _DEBUGTRACE(x, y)
# else
#  define DEBUGTRACE(x, y) ((void)0)
#  define DEBUGTRACE8(x, y) ((void)0)
# endif	// DEBUG_BT_DOWNLOAD

# ifdef DEBUG_BT_HAVE
#  define DEBUGTRACE8_HAVE(x, y) _DEBUGTRACE8(x, y)
#  define DEBUGTRACE_HAVE(x, y) _DEBUGTRACE(x, y)
# else
#  define DEBUGTRACE_HAVE(x, y) ((void)0)
#  define DEBUGTRACE8_HAVE(x, y) ((void)0)
# endif	// DEBUG_BT_HAVE

# ifdef DEBUG_BT_RESOURCES
#  define DEBUGTRACE_RES(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_RES(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_RES(x, y) ((void)0)
#  define DEBUGTRACE8_RES(x, y) ((void)0)
# endif	// DEBUG_BT_RESOURCES

# ifdef DEBUG_BT_THROTTLE
#  define DEBUGTRACE_THROTTLE(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_THROTTLE(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_THROTTLE(x, y) ((void)0)
#  define DEBUGTRACE8_THROTTLE(x, y) ((void)0)
# endif // DEBUG_BT_THROTTLE

# ifdef DEBUG_BT_FILEACCESS
#  define DEBUGTRACE_FILE(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_FILE(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_FILE(x, y) ((void)0)
#  define DEBUGTRACE8_FILE(x, y) ((void)0)
# endif	// DEBUG_BT_FILEACCESS

# ifdef DEBUG_BT_CHOKING
#  define DEBUGTRACE_CHOKE(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_CHOKE(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_CHOKE(x, y) ((void)0)
#  define DEBUGTRACE8_CHOKE(x, y) ((void)0)
# endif	// DEBUG_BT_CHOKING

# ifdef DEBUG_BT_TRACKER
#  define DEBUGTRACE_TRACKER(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_TRACKER(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_TRACKER(x, y) ((void)0)
#  define DEBUGTRACE8_TRACKER(x, y) ((void)0)
# endif	// DEBUG_BT_TRACKER

# ifdef DEBUG_BT_SOCKETS
#  define DEBUGTRACE_SOCKET(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_SOCKET(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_SOCKET(x, y) ((void)0)
#  define DEBUGTRACE8_SOCKET(x, y) ((void)0)
# endif	// DEBUG_BT_TRACKER

# ifdef DEBUG_BT_DISKCACHE
#  define DEBUGTRACE_DISKCACHE(x, y) _DEBUGTRACE(x, y)
#  define DEBUGTRACE8_DISKCACHE(x, y) _DEBUGTRACE8(x, y)
# else
#  define DEBUGTRACE_DISKCACHE(x, y) ((void)0)
#  define DEBUGTRACE8_DISKCACHE(x, y) ((void)0)
# endif	//

//#define ENTER_METHOD	DEBUGTRACE8("Entering %s, this: ", _METHODNAME_); DEBUGTRACE8("0x%08x\n", this)
//#define LEAVE_METHOD	DEBUGTRACE8("Leaving %s, this: ", _METHODNAME_); DEBUGTRACE8("0x%08x\n", this)
# define ENTER_METHOD
# define LEAVE_METHOD
/*
# else
#  define DEBUGTRACE(x, y) ((void)0)
#  define DEBUGTRACE8(x, y) ((void)0)
#  define DEBUGTRACE_UP(x, y) ((void)0)
#  define DEBUGTRACE8_UP(x, y) ((void)0)
#  define DEBUGTRACE_RES(x, y) ((void)0)
#  define DEBUGTRACE8_RES(x, y) ((void)0)
#  define DEBUGTRACE_FILE(x, y) ((void)0)
#  define DEBUGTRACE8_FILE(x, y) ((void)0)
#  define DEBUGTRACE_CHOKE(x, y) ((void)0)
#  define DEBUGTRACE8_CHOKE(x, y) ((void)0)
#  define DEBUGTRACE_TRACKER(x, y) ((void)0)
#  define DEBUGTRACE8_TRACKER(x, y) ((void)0)
#  define DEBUGTRACE_SOCKET(x, y) ((void)0)
#  define DEBUGTRACE8_SOCKET(x, y) ((void)0)
#  define ENTER_METHOD
#  define LEAVE_METHOD
*/
//#endif

typedef union
{
	BYTE	n[20];
	BYTE	b[20];
} SHAStruct;

typedef int TRISTATE;

#define TS_UNKNOWN	0
#define TS_FALSE	1
#define TS_TRUE		2

template<class T>
class OpP2PList : public OpDesktopList<T>
{
public:
	virtual BOOL Check(T* item)
	{
		T *first = OpDesktopList<T>::Begin();

		while(first != NULL)
		{
			if(first == item)
			{
				return TRUE;
			}
			first = OpDesktopList<T>::Next();
		}
		return FALSE;
	}
	UINT32 GetCount()
	{
		UINT32 cnt = 0;
		T *first = OpDesktopList<T>::Begin();

		while(first != NULL)
		{
			cnt++;
			first = OpDesktopList<T>::Next();
		}
		return cnt;
	}
};

template<class T>
class OpP2PVector : public OpVector<T>
{
public:
	BOOL Check(T* item)
	{
		UINT32 i;

		for(i = 0; i < OpVector<T>::GetCount(); i++)
		{
			if(item == OpVector<T>::Get(i))
			{
				return TRUE;
			}
		}
		return FALSE;
	}
	OP_STATUS AddIfNotExists(T *item)
	{
		if(!Check(item))
		{
			return OpVector<T>::Add(item);
		}
		return OpStatus::OK;
	}
};

//
// GUID
//

typedef union
{
	BYTE	n[16];
	BYTE	b[16];
	UINT32	w[4];
} GGUID;

inline bool operator==(const GGUID& guidOne, const GGUID& guidTwo)
{
   return (
      ((long *)&guidOne)[0] == ((long *)&guidTwo)[0] &&
      ((long *)&guidOne)[1] == ((long *)&guidTwo)[1] &&
      ((long *)&guidOne)[2] == ((long *)&guidTwo)[2] &&
      ((long *)&guidOne)[3] == ((long *)&guidTwo)[3] );
}

inline bool operator!=(const GGUID& guidOne, const GGUID& guidTwo)
{
   return (
      ( (long *)&guidOne)[0] != ((long *)&guidTwo)[0] ||
      ( (long *)&guidOne)[1] != ((long *)&guidTwo)[1] ||
      ( (long *)&guidOne)[2] != ((long *)&guidTwo)[2] ||
      ( (long *)&guidOne)[3] != ((long *)&guidTwo)[3] );
}

class BTSHA
{
// Construction
public:
	BTSHA();
	~BTSHA();

// Attributes
private:
	SSL_Hash_Pointer m_hash;

	unsigned char m_md[SSL_MAX_HASH_LENGTH];

// Operations
public:
	void			Reset();
	OP_STATUS		Add(void *pData, UINT32 nLength);
	OP_STATUS		Finish();
	void			GetHash(SHAStruct* pHash);
	void			GetHashString(OpString& outstr);
public:
	static void		HashToString(const SHAStruct* pHash, OpString& outstr);
	static void		HashToHexString(const SHAStruct* pHash, OpString& outstr);
	static BOOL		HashFromString(char *pszHash, SHAStruct* pHashIn);
	static BOOL		IsNull(SHAStruct* pHash);
};

inline bool operator==(const SHAStruct& sha1a, const SHAStruct& sha1b)
{
    return memcmp( &sha1a, &sha1b, 20 ) == 0;
}

inline bool operator!=(const SHAStruct& sha1a, const SHAStruct& sha1b)
{
    return memcmp( &sha1a, &sha1b, 20 ) != 0;
}

#if defined(BT_FILELOGGING_ENABLED)
class BTFileLogging
{
public:
	BTFileLogging()
		: m_enabled(FALSE)
	{
		prefsManager->GetStringPref(PrefsManager::BTLogfile, m_filename);
		if(m_filename.HasContent())
		{
			OP_STATUS status = OpStatus::ERR;
			FileKind fileType = kFileKindText;
			OpenMode openMode;
			ShareMode shareMode;
			TranslationMode transMode = kTranslateText;

			openMode = kOpenAppendAndRead;
			shareMode = kShareDenyWrite;

			TRAP(status, m_file.ConstructL(m_filename));

			OP_STATUS tmpstatus = m_file.Open(fileType, openMode, shareMode, transMode);

			if(tmpstatus == OpStatus::OK)
			{
				m_enabled = TRUE;
			}
			// silently ignore errors, we can't really deal with errors anyhow
		}
	}
	virtual ~BTFileLogging()
	{
		if(m_file.IsOpen())
		{
			m_file.Close();
		}
	}
	OP_STATUS WriteLogEntry(OpString& entry)
	{
		return m_file.WriteStringL(entry);
	}

	BOOL IsEnabled()
	{
		return m_enabled;
	}
private:
	OpFile m_file;
	OpString m_filename;
	BOOL	m_enabled;
};
#endif // BT_FILELOGGING_ENABLED

#if defined(_DEBUG) && defined (WIN32)
#if defined(DEBUG_BT_RESOURCE_TRACKING)
#include "platforms/windows/win_handy.h"
class BTResourceTracker
{
public:
	BTResourceTracker()
	{
		m_last_checkpoint_printout = op_time(NULL);
	}

	virtual ~BTResourceTracker()
	{
		BTResourceEntry *entry = m_list.Begin();

		if (entry != NULL)
			_DEBUGTRACE8(UNI_L("****** CURRENT BITTORRENT RESOURCES ARE NOT FREED: ******\n"), NULL)

		while(entry != NULL)
		{
			OpString src, name;

			src.Set(entry->m_source);
			name.Set(entry->m_name);

			_DEBUGTRACE8(UNI_L("%s"), (uni_char *)src);
			_DEBUGTRACE8(UNI_L("(%d):"), entry->m_line);
			_DEBUGTRACE8(UNI_L("%s - "), (uni_char *)name);
			_DEBUGTRACE8(UNI_L("0x%08x\n"), entry->m_classptr)

			OP_DELETE(entry);

			entry = m_list.Next();
		}
	}
	void Checkpoint(BOOL force_dump = FALSE)
	{
		time_t now = op_time(NULL);

		if(!force_dump)
		{
			if(now < m_last_checkpoint_printout + 60)
			{
				return;
			}
		}
		m_last_checkpoint_printout = now;

		OpStringHashTable<UINT32> _map;
		UINT32 *value = NULL;
		uni_char *key;
		BTResourceEntry *entry = m_list.Begin();

		while(entry != NULL)
		{
			if(OpStatus::IsSuccess(_map.GetData(entry->m_name, &value)))
			{
				*((UINT32 *)value) = *((UINT32 *)value) + 1;
			}
			else
			{
				UINT32 *int_value = OP_NEW(UINT32, ());

				*int_value = 1;

				_map.Add(entry->m_name, int_value);
			}
			entry = m_list.Next();
		}
		OpHashIterator *iter = _map.GetIterator();

		FILE *file = NULL;
		uni_char buf[1024];
		if(force_dump)
		{
			file = fopen("c:\\bt-debug-log.log", "w+");
		}
		_DEBUGTRACE8(UNI_L("********************* CHECKPOINT *******************\n"), 0);

		if(file)
		{
			fputs("********************* CHECKPOINT *******************", file);
		}

		OP_STATUS ret = iter->First();
		while(ret == OpStatus::OK)
		{
			value = (UINT32 *)iter->GetData();
			key = (uni_char *)iter->GetKey();

			_DEBUGTRACE8(UNI_L("CHECKPOINT - Resource: %s"), (uni_char *)key);
			_DEBUGTRACE8(UNI_L(", number of allocations: %d\n"), *value);

			if(file)
			{
				wsprintf(buf, UNI_L("RES: %s, allocations: %d\n"), (uni_char *)key, *value);
				fputws(buf, file);
			}

			ret = iter->Next();
		}
		OP_DELETE(iter);

		_map.DeleteAll();

		if(file)
		{
			fclose(file);
		}
	}
	BOOL Find(void* classptr)
	{
		BTResourceEntry *entry = m_list.Begin();

		while(entry != NULL)
		{
			if(entry && entry->m_classptr == classptr)
			{
				return TRUE;
			}
			entry = m_list.Next();
		}
		return FALSE;
	}
	void Add(const uni_char *name, void *classptr, const char *source, int line, int class_size = 0)
	{
		if(!Find(classptr))
		{
			BTResourceEntry *new_entry = OP_NEW(BTResourceEntry, (name, classptr, source, line, class_size));
			if (!new_entry)
				return;

			if (OpStatus::IsError(m_list.AddLast(new_entry)))
				OP_DELETE(new_entry);
		}
		else
		{
			OP_ASSERT(FALSE);
		}
	}

	void Remove(void *classptr)
	{
		BTResourceEntry *entry = m_list.Begin();

		while(entry != NULL)
		{
			if(entry && entry->m_classptr == classptr)
			{
				m_list.RemoveCurrentItem();
				OP_DELETE(entry);
				return;
			}
			entry = m_list.Next();
		}
//		OP_ASSERT(FALSE);	// should never get here
	}

	class BTResourceEntry
	{
	public:
		BTResourceEntry(const uni_char *name, void *classptr, const char *source, int line, int class_size)
		{
			m_name = name;
			m_classptr = classptr;
			m_source = source;
			m_line = line;
			m_class_size = class_size;
		}
		virtual ~BTResourceEntry() {}

		const uni_char *m_name;
		void *m_classptr;
		const char *m_source;
		int m_line;
		int m_class_size;
	};

private:
	OpP2PList<BTResourceEntry> m_list;
	time_t	m_last_checkpoint_printout;
};

#endif // DEBUG_BT_RESOURCE_TRACKING

#if defined(DEBUG_BT_FILEPROFILING) || defined(DEBUG_BT_PROFILING)

class BTProfiler
{
public:
	BTProfiler(char *method, BOOL force = FALSE)
	{
		m_method = method;
		m_startticks = g_op_time_info->GetRuntimeMS();
		m_force = force;
	}
	virtual ~BTProfiler()
	{
		double endticks = g_op_time_info->GetRuntimeMS();
		double ms_used = endticks - m_startticks;

		if(m_force || ms_used > 20)
		{
			OpString tmp;

			tmp.Set(m_method);

			_DEBUGTRACE(UNI_L("PROFILING: %s -  "), tmp.CStr());
			_DEBUGTRACE(UNI_L("%f ms\n"), ms_used);
		}
	}

private:
	char	*m_method;
	double	m_startticks;
	BOOL	m_force;
};

#endif // DEBUG_BT_FILEPROFILING
#endif // defined(_DEBUG) && defined (MSWIN)

#define P2P_FLOATING_AVG_SLOTS 200

class P2PTransferSpeed
{
public:
	P2PTransferSpeed();
	virtual ~P2PTransferSpeed();

	double GetKbps() { return m_kibs; }
	void AddPoint(UINT64 bytes);

protected:
	double CalculateAverageSpeed();

private:
	UINT64 m_loaded, m_lastLoaded;
	double m_last_elapsed;
	double m_kibs;
	int m_pointpos;

	class RelTimer
	{
		long last_sec, last_msec;
	public:
		RelTimer() : last_sec(0), last_msec(0){	Get(); }
		double Get();
	} m_timer;

	struct P2PSpeedCheckpoint
	{
		P2PSpeedCheckpoint()
		{
			elapsed = 0;
			bytes = 0;
		}
		double elapsed;
		UINT64 bytes;
		bool used;
	};
	P2PSpeedCheckpoint* m_points;
};

#if defined(DEBUG_BT_FILEPROFILING) && defined(_DEBUG)
  #define BTPROFILE(x)	class BTProfiler btprofile(_METHODNAME_, x)
#else
  #define BTPROFILE(x)
#endif

#if defined(DEBUG_BT_PROFILING) && defined(_DEBUG)
  #define PROFILE(x)	class BTProfiler btprofile(_METHODNAME_, x)
#else
  #define PROFILE(x)
#endif

#endif // BT_UTIL_H
