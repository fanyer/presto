// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef BT_P2PFilePart_H
#define BT_P2PFilePart_H

// ----------------------------------------------------

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"

#include <openssl/evp.h>
#include <openssl/sha.h>

#include "p2p-opfile64.h"
#include "bt-filecache.h"

class PrefsFile;
class P2PFile;
class OpFile64;
class P2PMetaData;

//class BTFileCache;

class P2PVirtualFile
{
public:
	P2PVirtualFile(const OpString& path, UINT64 length, UINT64 offset)
		:	m_length(length),
			m_offset(offset)
	{
		m_path.Set(path);
		m_file = OP_NEW(OpFile64, ());

//		BT_RESOURCE_ADD("P2PVirtualFile", this);
	}
	virtual ~P2PVirtualFile()
	{
		if(m_file && m_file->IsOpen())
		{
			m_file->Close();
		}
		OP_DELETE(m_file);

//		BT_RESOURCE_REMOVE(this);
	}
	OP_STATUS GetPath(OpString& path)
	{
		return path.Set(m_path);
	}
	OP_STATUS GetFullFilePath(OpString& path)
	{
		return path.Set(m_fullpath);
	}
	UINT64 GetLength()
	{
		return m_length;
	}
	UINT64 GetOffset()
	{
		return m_offset;
	}

private:
	OpFile64	*m_file;		// the file pointer
	OpString	m_path;			// the path for this file
	UINT64		m_length;		// the size this file is supposed to be when complete
	UINT64		m_offset;		// where this file starts in relation to the rest of the files
	OpString	m_fullpath;		// full path for this file

	friend class P2PFile;
};

class P2PFilePart
{
// Construction
protected:
	P2PFilePart();
	~P2PFilePart();

// Attributes
public:
	P2PFilePart*	m_previous;
	P2PFilePart*	m_next;

public:
	UINT64			m_offset;
	UINT64			m_length;
	
// Operations
public:
	P2PFilePart*	CreateCopy();
	P2PFilePart*	CreateInverse(UINT64 nSize);
	P2PFilePart*	CreateAnd(P2PFilePart* available);
	INT32			GetCount();
	P2PFilePart*	GetRandom(BOOL breferzero = FALSE);
	P2PFilePart*	GetLargest(OpVector<P2PFilePart> *except = NULL, BOOL bZeroIsLargest = TRUE);
	static UINT64	Subtract(P2PFilePart** ppFragments, P2PFilePart* subtract);
	static UINT64	Subtract(P2PFilePart** ppFragments, UINT64 offset, UINT64 length);
	static void		AddMerge(P2PFilePart** ppFragments, UINT64 offset, UINT64 length);
	
// Inlines
public:
	static P2PFilePart* New(P2PFilePart* previous = NULL, P2PFilePart* next = NULL, UINT64 offset = 0, UINT64 length = 0);
	void		DeleteThis();
	void		DeleteChain();
	
	friend class P2PFilePartPool;
};


class P2PFilePartPool
{
// Construction
public:
	P2PFilePartPool();
	~P2PFilePartPool();

// Attributes
protected:
	P2PFilePart*	m_freefragment;	// chained list of fragments free to be used
	UINT32			m_free;			// how many free fragments do we have
protected:
	OpVector<P2PFilePart> m_pools;

// Operations
protected:
	void	Clear();
	void	NewPool();

public:
	P2PFilePart* New(P2PFilePart* previous = NULL, P2PFilePart* next = NULL, UINT64 offset = 0, UINT64 length = 0);
	void Delete(P2PFilePart* fragment);
};

class P2PFile : public OpBTFileCacheListener
{
// Construction
public:
	P2PFile(OpString& hashkey);
	virtual ~P2PFile();
	
// Attributes
protected:
	OpString	m_hashkey;
	BOOL		m_write;
	UINT32		m_reference;
	UINT64		m_currentoffset;		// updated by SetFilePointer()
	OpVector<P2PVirtualFile> m_files;
	OpString	m_filepath;				// if single file, complete path. Otherwise, complete folder path
	BTFileCache	*m_filecache;
	BOOL		m_is_cache_disabled;
	P2PMetaData *m_metadata;

protected:

// Operations
public:
	void		AddRef();
	void		Release(BOOL write);
	OP_STATUS 	Init(UINT32 blocksize, BTFileCache *cache, P2PMetaData *metadata);
	BOOL		IsOpen();
	BOOL		Read(UINT64 offset, byte *buffer, UINT64 buffersize, UINT64* read, BOOL bypass_cache = FALSE);
	BOOL		Write(UINT64 nOffset, byte *buffer, UINT64 buffersize, UINT64* written, BOOL use_cache = TRUE, UINT32 blocknumber = 0);
	BOOL		SetFilePointer(UINT64 offset, P2PVirtualFile *vfile = NULL);
	BOOL		SetEndOfFile(P2PVirtualFile *vfile = NULL);
	BOOL		FlushBuffers();
	OP_STATUS	AddFile(const OpString& path, UINT64 size, UINT64 offset);
	void		GetFilePath(OpString& path);
	void		SetFilePath(const OpString& path);
	BOOL		Exists(const OpString& path, UINT64 size, UINT64 offset);
	OP_STATUS	ConstructPaths();

	// OpBTFileCacheListener
	OP_STATUS	WriteCacheData(byte *buffer, UINT64 offset, UINT32 buflen);
	void		CacheDisabled(BOOL disabled);
	OpVector<P2PVirtualFile>* GetFiles() { return &m_files; } 

protected:
	BOOL		Open(BOOL write, BOOL create);
	BOOL		EnsureWrite();
	BOOL		CloseWrite();
	BOOL		DumpCache(BOOL offline = FALSE);
	P2PVirtualFile *FindFirstFile(UINT64 offset, UINT64 length);
	BOOL		CanFileProvideAllData(P2PVirtualFile *file, UINT64 offset, UINT64 length);
	void		CloseAllFiles();
	
	friend class P2PFiles;
	
};

class P2PFiles
{
// Construction
public:
	P2PFiles();
	virtual ~P2PFiles();
	
// Attributes
public:
	OpStringHashTable<P2PFile> m_map;
	OpStringHashTable<BTFileCache> m_filecache_map;
	
// Operations
public:
	P2PFile*		Open(OpString& hashkey, BOOL write, BOOL create, OpString& directory, P2PMetaData *metadata, OpVector<P2PVirtualFile> *files = NULL, UINT32 blocksize = 0, BOOL use_cache = TRUE);
	void			Close();

protected:
	void			Remove(P2PFile* file);
	void			Remove(BTFileCache* cache);
	
	friend class P2PFile;
	friend class BTFileCache;
};

class P2PBlockFile
{
// Construction
public:
	P2PBlockFile();
	virtual ~P2PBlockFile();

// Attributes
protected:
	P2PFile*		m_file;
	UINT64			m_total;
	UINT64			m_remaining;
	UINT64			m_unflushed;
	OpString		m_target_directory;
//	P2PFiles		m_P2PFiles;

protected:
	UINT32			m_fragments;
	P2PFilePart*	m_first;
	P2PFilePart*	m_last;
	
// Operations
public:
	BOOL			CreateAll(OpString& hashkey, UINT64 length, P2PMetaData *metadata, OpVector<P2PVirtualFile> *files = NULL, UINT32 blocksize = 0);
	BOOL			Open(OpString& hashkey, P2PMetaData *metadata, OpVector<P2PVirtualFile> *files = NULL, BOOL write = TRUE, UINT32 blocksize = 0);
	BOOL			Flush();
	void			Close();
	void			Clear();
	BOOL			MakeComplete();
	void			SetTotal(UINT64 total) { m_total = total; }
	void			AddFile(OpString& path, UINT64 size, UINT64 offset);
	void			GetFilePath(OpString& path) { m_file->GetFilePath(path); }
	void			SetTargetDirectory(OpString& path) { m_target_directory.Set(path); }
	OpVector<P2PVirtualFile>* GetFiles() { return m_file ? m_file->GetFiles() : NULL; } 

public:
	void			SetEmptyFragments(P2PFilePart* pInput);
	P2PFilePart*	CopyFreeFragments() const;
	P2PFilePart*	CopyFilledFragments() const;
	BOOL			IsPositionRemaining(UINT64 nOffset) const;
	BOOL			DoesRangeOverlap(UINT64 nOffset, UINT64 nLength) const;
	UINT64			GetRangeOverlap(UINT64 nOffset, UINT64 nLength) const;
	BOOL			WriteRange(UINT64 nOffset, void *pData, UINT64 nLength, UINT32 blocknumber);
	BOOL			ReadRange(UINT64 nOffset, void *pData, UINT64 nLength);
	UINT64			InvalidateRange(UINT64 nOffset, UINT64 nLength);
	
// Operations
public:
	inline BOOL IsValid() const
	{
		return ( this != NULL ) && ( m_total > 0 );
	}
	
	inline BOOL IsOpen() const
	{
		return ( this != NULL ) && ( m_file != NULL );
	}
	
	inline UINT64 GetTotal() const
	{
		return m_total;
	}
	
	inline UINT64 GetRemaining() const
	{
		return m_remaining;
	}
	
	inline UINT64 GetCompleted() const
	{
		return IsValid() ? m_total - m_remaining : 0;
	}
	
	inline P2PFilePart* GetFirstEmptyFragment() const
	{
		return m_first;
	}
	
	inline UINT64 GetEmptyFragmentCount() const
	{
		return m_fragments;
	}
	
	inline BOOL IsFlushNeeded() const
	{
		return(m_file != NULL) && (m_unflushed > 0);
	}
};
/*
class P2PFileWriteThread
{
public:
	P2PFileWriteThread();
	virtual ~P2PFileWriteThread();
	
	enum FileThreadSignals
	{
		
	};

	OP_STATUS StartThread(P2PFile *file);
	OP_STATUS EndThread();
	OP_STATUS SignalThread();
};
*/

#define BT_TAG_BLOCK_NUMBER		0x0001 // unsigned, block number
#define BT_TAG_BLOCK_STATUS		0x0002 // unsigned, block status
#define BT_TAG_BLOCK_FILENAME	0x0003 // unsigned char *
#define BT_TAG_BLOCK_FILEDATE	0x0004 // signed, time of last change for this file

#define BT_TAG_CRC_FILE_ENTRY	0x0055	// Record sequence
#define BT_TAG_DATE_FILE_ENTRY	0x0056	// Record with name/last-modified date

class P2PMetaDataFileInfo
{
public:

	P2PMetaDataFileInfo(OpString& filename, time_t last_modified, UINT64 file_len)
	{
		m_filename.Set(filename.CStr());
		m_last_modified = last_modified;
		m_file_len = file_len;

		BT_RESOURCE_ADD("P2PMetaDataFileInfo", this);
	}
	virtual ~P2PMetaDataFileInfo()
	{
		BT_RESOURCE_REMOVE(this);
	}
	OpString8 m_filename;
	time_t m_last_modified;
	UINT64 m_file_len;
};

class P2PMetaData
{
public:
	P2PMetaData();
	virtual ~P2PMetaData();

	OP_STATUS Init(UINT32 block_count, OpString& name);
	OP_STATUS WriteMetaFile(BOOL force = FALSE);
	OP_STATUS ReadMetaFile();

	void SetBlockAsDone(UINT32 block);
	void SetBlockAsDirty(UINT32 block);
	OP_STATUS HasFileChanged(OpString& filename, BOOL& changed, UINT64 file_len);
	BOOL HasMetaData() { return m_has_meta_data; }
	OP_STATUS UpdateFileData(OpString& filename, time_t last_modified, UINT64 file_len);
	BOOL IsBlockDone(UINT32 block);
	OP_STATUS DeleteMetaFile();

protected:
	P2PMetaDataFileInfo* FindFileData(OpString& filename);
	void ClearBlocks();

private:

	UINT32* m_blocks;
	UINT32	m_block_count;
	OpString m_name;
	BOOL	m_changed;
	BOOL	m_has_meta_data;
	time_t	m_last_write;

	OpVector<P2PMetaDataFileInfo> m_files;
};

#endif // BT_P2PFilePart_H
