/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
#include "modules/util/gen_math.h"
#include "modules/util/opfile/opmemfile.h"
#include "adjunct/m2/src/engine/engine.h"
//#include "irc-message.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
# include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/formats/url_dfr.h"

#include "adjunct/bittorrent/bt-util.h"
#include "adjunct/bittorrent/bt-globals.h"
#include "adjunct/bittorrent/p2p-fileutils.h"

P2PFilePart::P2PFilePart()
{
	BT_RESOURCE_ADD("P2PFilePart", this);
};
P2PFilePart::~P2PFilePart()
{
	BT_RESOURCE_REMOVE(this);
};

P2PFilePart* P2PFilePart::New(P2PFilePart* previous, P2PFilePart* next, UINT64 offset, UINT64 length)
{
	return g_P2PFilePartPool->New( previous, next, offset, length );
}

void P2PFilePart::DeleteThis()
{
	if(this == NULL) return;
	g_P2PFilePartPool->Delete( this );
}

void P2PFilePart::DeleteChain()
{
	if(this == NULL) return;

	for ( P2PFilePart* fragment = m_next ; fragment ; )
	{
		P2PFilePart* next = fragment->m_next;
		g_P2PFilePartPool->Delete( fragment );
		fragment = next;
	}
	g_P2PFilePartPool->Delete(this);
}

//////////////////////////////////////////////////////////////////////
// P2PFilePart copy fragments

P2PFilePart* P2PFilePart::CreateCopy()
{
	P2PFilePart* first = NULL;
	P2PFilePart* last = NULL;

	for (P2PFilePart* fragment = this ; fragment ; fragment = fragment->m_next)
	{
		P2PFilePart* copy = New(last, NULL, fragment->m_offset, fragment->m_length);
		if (!first) first = copy;
		if (last) last->m_next = copy;
		last = copy;
	}
	return first;
}

//////////////////////////////////////////////////////////////////////
// P2PFilePart invert fragments

P2PFilePart* P2PFilePart::CreateInverse(UINT64 size)
{
	P2PFilePart* first	= NULL;
	P2PFilePart* last	= NULL;
	UINT64 nLast = 0;

	for (P2PFilePart* fragment = this ; fragment ; fragment = fragment->m_next)
	{
		if (fragment->m_offset > nLast)
		{
			P2PFilePart* copy = New(last, NULL, nLast, fragment->m_offset - nLast);
			if (!first) first = copy;
			if (last) last->m_next = copy;
			last = copy;
		}

		nLast = fragment->m_offset + fragment->m_length;
	}

	if (size > nLast)
	{
		P2PFilePart* copy = New(last, NULL, nLast, size - nLast);
		if (!first) first = copy;
		if (last) last->m_next = copy;
		last = copy;
	}
	return first;
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile and fragments

P2PFilePart* P2PFilePart::CreateAnd(P2PFilePart* available)
{
	P2PFilePart* first	= NULL;
	P2PFilePart* last	= NULL;

	for(P2PFilePart* fragment = this ; fragment ; fragment = fragment->m_next)
	{
		for(P2PFilePart *other = available ; other ; other = other->m_next)
		{
			UINT64 offset, length;

			if(other->m_offset <= fragment->m_offset &&
				 other->m_offset + other->m_length >= fragment->m_offset + fragment->m_length)
			{
				offset = fragment->m_offset;
				length = fragment->m_length;
			}
			else if(other->m_offset > fragment->m_offset &&
					other->m_offset + other->m_length < fragment->m_offset + fragment->m_length)
			{
				offset = other->m_offset;
				length = other->m_length;
			}
			else if(other->m_offset + other->m_length > fragment->m_offset &&
						other->m_offset + other->m_length < fragment->m_offset + fragment->m_length)
			{
				offset = fragment->m_offset;
				length = other->m_length - (fragment->m_offset - other->m_offset);
			}
			else if(other->m_offset > fragment->m_offset &&
						other->m_offset < fragment->m_offset + fragment->m_length)
			{
				offset = other->m_offset;
				length = fragment->m_offset + fragment->m_length - other->m_offset;
			}
			else
			{
				continue;
			}
			P2PFilePart* copy = New(last, NULL, offset, length);
			if (first == NULL) first = copy;
			if (last != NULL) last->m_next = copy;
			last = copy;
		}
	}
	return first;
}

//////////////////////////////////////////////////////////////////////
// P2PFilePart count the fragments in the list

INT32 P2PFilePart::GetCount()
{
	INT32 nCount = 0;
	for(P2PFilePart* count = this ; count ; count = count->m_next)
	{
		nCount++;
	}
	return nCount;
}

//////////////////////////////////////////////////////////////////////
// P2PFilePart find random fragment

P2PFilePart* P2PFilePart::GetRandom(BOOL bPreferZero)
{
	if (this == NULL) return NULL;

	INT32 nCount = 0;
	P2PFilePart* count;
	for (count = this ; count ; count = count->m_next)
	{
		if (bPreferZero && count->m_offset == 0) return count;
		nCount++;
	}

	OP_ASSERT(nCount > 0);

	nCount = rand() % nCount;

	for (count = this ; count ; count = count->m_next)
	{
		if (nCount-- == 0) return count;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////
// P2PFilePart find largest fragment

P2PFilePart* P2PFilePart::GetLargest(OpVector<P2PFilePart> *except, BOOL bZeroIsLargest)
{
	if (this == NULL) return NULL;

	if (bZeroIsLargest && m_offset == 0)
	{
		if (!except || except->Find(this) == -1) return this;
	}

	P2PFilePart* largest = NULL;

	for(P2PFilePart* fragment = this ; fragment ;)
	{
		if(!largest || fragment->m_length > largest->m_length)
		{
			if(!except || except->Find(fragment) == -1) largest = fragment;
		}
		fragment = fragment->m_next;
	}
	return largest;
}

//////////////////////////////////////////////////////////////////////
// P2PFilePart subtract fragments

UINT64 P2PFilePart::Subtract(P2PFilePart** ppFragments, P2PFilePart* subtract)
{
	UINT64 nCount = 0;

	for (; subtract ; subtract = subtract->m_next)
	{
		if (ppFragments == NULL || *ppFragments == NULL) break;
		nCount += Subtract(ppFragments, subtract->m_offset, subtract->m_length);
	}
	return nCount;
}

UINT64 P2PFilePart::Subtract(P2PFilePart** ppFragments, UINT64 offset, UINT64 length)
{
	if (ppFragments == NULL || *ppFragments == NULL) return 0;

	UINT64 nCount = 0;

	for(P2PFilePart* fragment = *ppFragments ; fragment ;)
	{
		P2PFilePart* next = fragment->m_next;

		if (offset <= fragment->m_offset &&
			 offset + length >= fragment->m_offset + fragment->m_length)
		{
			if(fragment->m_previous != NULL)
			{
				fragment->m_previous->m_next = next;
			}
			else
			{
				*ppFragments = next;
			}
			if(next != NULL)
			{
				next->m_previous = fragment->m_previous;
			}
			nCount += fragment->m_length;

			fragment->DeleteThis();
		}
		else if (offset > fragment->m_offset &&
					offset + length < fragment->m_offset + fragment->m_length)
		{
			P2PFilePart* newfragment = New(fragment, next);
			if (newfragment)
			{
				newfragment->m_offset	= offset + length;
				newfragment->m_length	= fragment->m_offset + fragment->m_length - newfragment->m_offset;

				fragment->m_length	= offset - fragment->m_offset;
				fragment->m_next	= newfragment;
				nCount				+= length;

				if (next)
				{
					next->m_previous = newfragment;
				}
			}
			break;
		}
		else if (offset + length > fragment->m_offset &&
					offset + length < fragment->m_offset + fragment->m_length)
		{
			UINT64 nFragment = length - (fragment->m_offset - offset);

			fragment->m_offset	+= nFragment;
			fragment->m_length	-= nFragment;
			nCount				+= nFragment;
		}
		else if (offset > fragment->m_offset &&
					offset < fragment->m_offset + fragment->m_length)
		{
			UINT64 nFragment	= fragment->m_offset + fragment->m_length - offset;

			fragment->m_length	-= nFragment;
			nCount				+= nFragment;
		}
		fragment = next;
	}

	return nCount;
}

//////////////////////////////////////////////////////////////////////
// P2PFilePart add and merge a fragment

void P2PFilePart::AddMerge(P2PFilePart** ppFragments, UINT64 offset, UINT64 length)
{
	if(ppFragments == NULL || length == 0) return;
	P2PFilePart* fragment;
	for(fragment = *ppFragments ; fragment ; fragment = fragment->m_next)
	{
		if(fragment->m_offset + fragment->m_length == offset)
		{
			fragment->m_length += length;
			break;
		}
		else if(offset + length == fragment->m_offset)
		{
			fragment->m_offset -= length;
			fragment->m_length += length;
			break;
		}
	}
	if(fragment == NULL)
	{
		P2PFilePart* fragment = New(NULL, *ppFragments, offset, length);
		if (fragment)
		{
			if (*ppFragments != NULL) (*ppFragments)->m_previous = fragment;
			*ppFragments = fragment;
		}
		return;
	}
	for(P2PFilePart* other = *ppFragments ; other ; other = other->m_next)
	{
		if (fragment == other)
		{
			continue;
		}
		else if (fragment->m_offset + fragment->m_length == other->m_offset)
		{
			fragment->m_length += other->m_length;
		}
		else if (other->m_offset + other->m_length == fragment->m_offset)
		{
			fragment->m_offset -= other->m_length;
			fragment->m_length += other->m_length;
		}
		else
		{
			continue;
		}

		if(other->m_previous)
		{
			other->m_previous->m_next = other->m_next;
		}
		else
		{
			*ppFragments = other->m_next;
		}
		if(other->m_next)
		{
			other->m_next->m_previous = other->m_previous;
		}
		other->DeleteThis();
		break;
	}
}


//////////////////////////////////////////////////////////////////////
// P2PFilePartPool construction

P2PFilePartPool::P2PFilePartPool()
:	m_freefragment(NULL),
	m_free(0)
{
	BT_RESOURCE_ADD("P2PFilePartPool", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PFilePartPool::~P2PFilePartPool()"
#endif

P2PFilePartPool::~P2PFilePartPool()
{
	Clear();

	DEBUGTRACE_RES(UNI_L("*** DESTRUCTOR for %s ("), UNI_L(_METHODNAME_));
	DEBUGTRACE_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

//////////////////////////////////////////////////////////////////////
// P2PFilePartPool clear

void P2PFilePartPool::Clear()
{
	// can't use DeleteAll here, need delete[] !!
	for(UINT32 idx = 0; idx < m_pools.GetCount(); idx++)
	{
		P2PFilePart* pool = (P2PFilePart*)m_pools.Get(idx);
		OP_DELETEA(pool);
	}
	m_pools.Clear();
	m_freefragment = NULL;
	m_free = 0;
}

//////////////////////////////////////////////////////////////////////
// P2PFilePartPool new pool setup

void P2PFilePartPool::NewPool()
{
	int size = 2048;

	P2PFilePart* pool = OP_NEWA(P2PFilePart, size);
	if (!pool)
		return;

	m_pools.Add(pool);

	while(size-- > 0)
	{
		pool->m_next = m_freefragment;
		m_freefragment = pool++;
		m_free++;
	}
}

P2PFilePart* P2PFilePartPool::New(P2PFilePart* previous, P2PFilePart* next, UINT64 offset, UINT64 length)
{
	if (m_free == 0)
		NewPool();

	if (m_free == 0)	// OOM
		return NULL;

	P2PFilePart* fragment = m_freefragment;
	m_freefragment = m_freefragment->m_next;
	m_free --;

	fragment->m_previous	= previous;
	fragment->m_next		= next;
	fragment->m_offset		= offset;
	fragment->m_length		= length;

	return fragment;
}

void P2PFilePartPool::Delete(P2PFilePart* fragment)
{
	fragment->m_previous = NULL;
	fragment->m_next = m_freefragment;
	m_freefragment = fragment;
	m_free ++;
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile construction

P2PBlockFile::P2PBlockFile()
:	m_file(NULL),
	m_total(0),
	m_remaining(0),
	m_unflushed(0),
	m_fragments(0),
	m_first(NULL),
	m_last(NULL)
{

	BT_RESOURCE_ADD("P2PBlockFile", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PBlockFile::~P2PBlockFile()"
#endif

P2PBlockFile::~P2PBlockFile()
{
	Clear();

	DEBUGTRACE_RES(UNI_L("*** DESTRUCTOR for %s ("), UNI_L(_METHODNAME_));
	DEBUGTRACE_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile create

BOOL P2PBlockFile::CreateAll(OpString& hashkey, UINT64 length, P2PMetaData *metadata, OpVector<P2PVirtualFile> *files, UINT32 blocksize)
{
	if(m_file != NULL /*|| m_total > 0 */) return FALSE;
	if(length == 0) return FALSE;

	OP_ASSERT(m_target_directory.HasContent());

	m_file = g_P2PFiles->Open(hashkey, TRUE, TRUE, m_target_directory, metadata, files, blocksize);
	if (m_file == NULL) return FALSE;

	m_remaining = m_total = length;
	m_fragments = 1;

	m_first = m_last = P2PFilePart::New(NULL, NULL, 0, m_total);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile open

BOOL P2PBlockFile::Open(OpString& hashkey, P2PMetaData *metadata, OpVector<P2PVirtualFile> *files, BOOL write, UINT32 blocksize)
{
	if(m_file != NULL || m_total == 0) return FALSE;

	OP_ASSERT(m_target_directory.HasContent());

	m_file = g_P2PFiles->Open(hashkey, write, FALSE, m_target_directory, metadata, files, blocksize);

	if(m_file == NULL) return FALSE;

	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile flush

BOOL P2PBlockFile::Flush()
{
	if(m_unflushed == 0) return FALSE;
	if(m_file == NULL || ! m_file->IsOpen()) return FALSE;
	m_file->FlushBuffers();
	m_unflushed = 0;
	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile close

void P2PBlockFile::Close()
{
	if(m_file)
	{
		m_file->Release(TRUE);
	}
	m_file = NULL;
	m_unflushed = 0;
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile clear

void P2PBlockFile::Clear()
{
	Close();

	m_first->DeleteChain();

	m_total = m_remaining = m_unflushed = m_fragments = 0;
	m_first = m_last = NULL;
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile make complete

BOOL P2PBlockFile::MakeComplete()
{
	if (m_total == 0 || m_remaining == 0) return FALSE;

	m_first->DeleteChain();
	m_remaining = m_fragments = 0;
	m_first = m_last = NULL;

	if (m_file != NULL)
	{
		// set pointer to the end of file
		if(m_file->SetFilePointer(m_total))
		{
			// set end of file. The file from last position to the
			// end will be filled with zeros.
			m_file->SetEndOfFile();
		}
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile simple fragment operations

void P2PBlockFile::SetEmptyFragments(P2PFilePart* input)
{
	m_first->DeleteChain();

	m_first		= m_last = input;
	m_fragments	= 0;
	m_remaining	= 0;

	for (; input ; input = input->m_next)
	{
		m_fragments ++;
		m_remaining += input->m_length;
		m_last = input;
	}
}

P2PFilePart* P2PBlockFile::CopyFreeFragments() const
{
	return m_first->CreateCopy();
}

P2PFilePart* P2PBlockFile::CopyFilledFragments() const
{
	return m_first->CreateInverse(m_total);
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile simple intersections

BOOL P2PBlockFile::IsPositionRemaining(UINT64 offset) const
{
	if (m_remaining == 0 || m_fragments == 0) return FALSE;
	if (offset >= m_total) return FALSE;

	for(P2PFilePart* fragment = m_first ; fragment ;)
	{
		if (offset >= fragment->m_offset && offset < fragment->m_offset + fragment->m_length)
			return TRUE;

		fragment = fragment->m_next;
	}
	return FALSE;
}

BOOL P2PBlockFile::DoesRangeOverlap(UINT64 offset, UINT64 length) const
{
	if (m_remaining == 0 || m_fragments == 0) return FALSE;
	if (length == 0) return FALSE;

	for (P2PFilePart* fragment = m_first ; fragment ; fragment = fragment->m_next)
	{
		if (offset <= fragment->m_offset &&
			 offset + length >= fragment->m_offset + fragment->m_length)
		{
			if (fragment->m_length) return TRUE;
		}
		else if (offset > fragment->m_offset &&
				offset + length < fragment->m_offset + fragment->m_length)
		{
			return TRUE;
		}
		else if(offset + length > fragment->m_offset &&
				offset + length < fragment->m_offset + fragment->m_length)
		{
			if (length - (fragment->m_offset - offset)) return TRUE;
		}
		else if(offset > fragment->m_offset &&
				offset < fragment->m_offset + fragment->m_length)
		{
			if (fragment->m_offset + fragment->m_length - offset) return TRUE;
		}
	}
	return FALSE;
}

UINT64 P2PBlockFile::GetRangeOverlap(UINT64 offset, UINT64 length) const
{
	if (m_remaining == 0 || m_fragments == 0) return 0;
	if (length == 0) return 0;

	UINT64 overlap = 0;

	for (P2PFilePart* fragment = m_first ; fragment ; fragment = fragment->m_next)
	{
		if(offset <= fragment->m_offset &&
			 offset + length >= fragment->m_offset + fragment->m_length)
		{
			overlap += fragment->m_length;
		}
		else if(offset > fragment->m_offset &&
				offset + length < fragment->m_offset + fragment->m_length)
		{
			overlap += length;
			break;
		}
		else if(offset + length > fragment->m_offset &&
				offset + length < fragment->m_offset + fragment->m_length)
		{
			overlap += length - (fragment->m_offset - offset);
		}
		else if(offset > fragment->m_offset &&
				offset < fragment->m_offset + fragment->m_length)
		{
			overlap += fragment->m_offset + fragment->m_length - offset;
		}
	}
	return overlap;
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile write some data to a range

BOOL P2PBlockFile::WriteRange(UINT64 offset, void *pData, UINT64 length, UINT32 blocknumber)
{
	P2PFilePart* fragment;

	if (m_file == NULL) return FALSE;
	if (m_remaining == 0 || m_fragments == 0) return FALSE;
	if (length == 0) return TRUE;

	UINT64 result, processed = 0;

	for (fragment = m_first ; fragment ;)
	{
		P2PFilePart* next = fragment->m_next;

		if(offset <= fragment->m_offset &&
			 offset + length >= fragment->m_offset + fragment->m_length)
		{
			byte * source = (byte *)pData + (fragment->m_offset - offset);

			if(!m_file->Write(fragment->m_offset, source, fragment->m_length, &result, TRUE, blocknumber))
			{
				return FALSE;
			}
			processed		+= fragment->m_length;
			m_remaining		-= fragment->m_length;

			if (fragment->m_previous)
				fragment->m_previous->m_next = next;
			else
				m_first = next;

			if (next)
				next->m_previous = fragment->m_previous;
			else
				m_last = fragment->m_previous;

			fragment->DeleteThis();
			m_fragments --;
		}
		else if(offset > fragment->m_offset &&
				offset + length < fragment->m_offset + fragment->m_length)
		{
			if(!m_file->Write(offset, (byte *)pData, length, &result, TRUE, blocknumber))
			{
				return FALSE;
			}
			processed		+= length;
			m_remaining		-= length;

			P2PFilePart* pNew = P2PFilePart::New(fragment, next);
			if (pNew)
			{
				pNew->m_offset	= offset + length;
				pNew->m_length	= fragment->m_offset + fragment->m_length - pNew->m_offset;

				fragment->m_length	= offset - fragment->m_offset;
				fragment->m_next	= pNew;

				if(next)
				{
					next->m_previous = pNew;
				}
				else
				{
					m_last = pNew;
				}
				m_fragments++;
			}
			break;
		}
		else if(offset + length > fragment->m_offset &&
				offset + length < fragment->m_offset + fragment->m_length)
		{
			byte *source	= (byte *)pData + (fragment->m_offset - offset);
			UINT64 nFragment = length - (fragment->m_offset - offset);

			if(!m_file->Write(fragment->m_offset, source, nFragment, &result, TRUE, blocknumber))
			{
				return FALSE;
			}
			processed		+= nFragment;
			m_remaining		-= nFragment;

			fragment->m_offset	+= nFragment;
			fragment->m_length	-= nFragment;
		}
		else if(offset > fragment->m_offset &&
				offset < fragment->m_offset + fragment->m_length)
		{
			UINT64 nFragment	= fragment->m_offset + fragment->m_length - offset;

			if(!m_file->Write(offset, (byte *)pData, nFragment, &result, TRUE, blocknumber))
			{
				return FALSE;
			}

			processed		+= nFragment;
			m_remaining		-= nFragment;

			fragment->m_length	-= nFragment;
		}
		fragment = next;
	}
	m_unflushed += processed;
	return processed > 0;
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile read some data from a range

BOOL P2PBlockFile::ReadRange(UINT64 offset, void *pData, UINT64 length)
{
	if(m_file == NULL) return FALSE;
	if(length == 0) return TRUE;

	if(DoesRangeOverlap(offset, length)) return FALSE;

	UINT64 read = 0;
	m_file->Read(offset, (byte *)pData, length, &read);

	return (BOOL)(read == length);
}

//////////////////////////////////////////////////////////////////////
// P2PBlockFile invalidate a range

UINT64 P2PBlockFile::InvalidateRange(UINT64 offset, UINT64 length)
{
	P2PFilePart* full = CopyFilledFragments();
	UINT64 count = P2PFilePart::Subtract(&full, offset, length);
	SetEmptyFragments(full->CreateInverse(m_total));
	full->DeleteChain();

	return count;
}

void P2PBlockFile::AddFile(OpString& path, UINT64 size, UINT64 offset)
{
	OP_ASSERT(m_file != NULL);

	m_file->AddFile(path, size, offset);
}

//////////////////////////////////////////////////////////////////////
// P2PFiles construction

P2PFiles::P2PFiles()
{
	BT_RESOURCE_ADD("P2PFiles", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PFiles::~P2PFiles()"
#endif

P2PFiles::~P2PFiles()
{
	Close();

	DEBUGTRACE_RES(UNI_L("*** DESTRUCTOR for %s ("), UNI_L(_METHODNAME_));
	DEBUGTRACE_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

//////////////////////////////////////////////////////////////////////
// P2PFiles open a file

P2PFile* P2PFiles::Open(OpString& hashkey, BOOL write, BOOL create, OpString& target_directory, P2PMetaData *metadata, OpVector<P2PVirtualFile> *files, UINT32 blocksize, BOOL use_cache)
{
	P2PFile* file = NULL;
	OpString tmphash;

	tmphash.Set(hashkey);

	if(write)
	{
		tmphash.Append("-");
		tmphash.Append("write");
	}
	if(m_map.GetData(tmphash.CStr(), &file) == OpStatus::OK)
	{
		if(write && !file->EnsureWrite())
			return NULL;

		BTFileCache *filecache = NULL;
#ifdef USE_BT_DISKCACHE
		if(m_filecache_map.GetData(hashkey.CStr(), &filecache) != OpStatus::OK)
		{
			filecache = OP_NEW(BTFileCache, ());
			if (!filecache)
			{
				return NULL;
			}
			filecache->Init(blocksize);
	 		m_filecache_map.Add(hashkey.CStr(), filecache);
			filecache->m_hashkey.Set(hashkey.CStr());
		}
#endif // USE_BT_DISKCACHE
	}
	else
	{
		BTFileCache *filecache = NULL;
		file = OP_NEW(P2PFile, (tmphash));
		if(!file)
		{
			return NULL;
		}
		file->SetFilePath(target_directory);
		if(files)
		{
			for(UINT32 pos = 0; pos < files->GetCount(); pos++)
			{
				P2PVirtualFile *vfile = files->Get(pos);

				if(vfile != NULL)
				{
					OpString path;

					vfile->GetPath(path);

					if(!file->Exists(path, vfile->GetLength(), vfile->GetOffset()))
					{
						file->AddFile(path, vfile->GetLength(), vfile->GetOffset());
					}
				}
			}
		}
		if(!file->Open(write, create))
		{
			OP_DELETE(file);
			return NULL;
		}
#ifdef USE_BT_DISKCACHE
		if(m_filecache_map.GetData(hashkey.CStr(), &filecache) != OpStatus::OK)
		{
			filecache = OP_NEW(BTFileCache, ());
			if(!filecache)
			{
				OP_DELETE(file);
				return NULL;
			}
			filecache->Init(blocksize);
	 		m_filecache_map.Add(hashkey.CStr(), filecache);
			filecache->m_hashkey.Set(hashkey.CStr());
		}
#endif // USE_BT_DISKCACHE

		if(OpStatus::IsError(file->Init(blocksize, filecache, metadata)))
		{
			OP_DELETE(file);
			return NULL;
		}
		// FIX: Check if Add will replace matching keys
 		m_map.Add(file->m_hashkey.CStr(), file);

		DEBUGTRACE_FILE(UNI_L("FILE ADDED TO MAP 0x%08x (new map count: "), file);
		DEBUGTRACE_FILE(UNI_L("%d)\n"), m_map.GetCount());
	}
	file->AddRef();
	file->CacheDisabled(use_cache ? FALSE : TRUE);
	DEBUGTRACE_FILE(UNI_L("FILE CACHE: %s\n"), use_cache ? "ENABLED" : "DISABLED");

	return file;
}

//////////////////////////////////////////////////////////////////////
// P2PFiles close all files

void P2PFiles::Close()
{
	OP_STATUS ret;
	BTFileCache *cache;
	P2PFile *file;
	OpHashIterator *iter;

	iter = m_filecache_map.GetIterator();

	ret = iter->First();
	while(ret == OpStatus::OK)
	{
		cache = (BTFileCache *)iter->GetData();
		cache->WriteCache();

		ret = iter->Next();
	}
	OP_DELETE(iter);

	m_filecache_map.RemoveAll();

	iter = m_map.GetIterator();

	ret = iter->First();
	while(ret == OpStatus::OK)
	{
		file = (P2PFile *)iter->GetData();

		file->CloseAllFiles();

		ret = iter->Next();
	}
	OP_DELETE(iter);

	m_map.RemoveAll();
}

//////////////////////////////////////////////////////////////////////
// P2PFiles remove a single file

void P2PFiles::Remove(P2PFile* file)
{
	P2PFile *tmp;
#ifdef DEBUG_ENABLE_OPASSERT
	OP_STATUS stat =
#endif
        m_map.Remove(file->m_hashkey.CStr(), &tmp);

	OP_ASSERT(stat == OpStatus::OK);

	DEBUGTRACE_FILE(UNI_L("FILE REMOVED FROM MAP 0x%08x (new map count: "), file);
	DEBUGTRACE_FILE(UNI_L("%d)\n"), m_map.GetCount());
}

void P2PFiles::Remove(BTFileCache* cache)
{
	BTFileCache *tmp;
#ifdef DEBUG_ENABLE_OPASSERT
	OP_STATUS stat =
#endif
        m_filecache_map.Remove(cache->m_hashkey.CStr(), &tmp);

	OP_ASSERT(stat == OpStatus::OK);

	DEBUGTRACE_FILE(UNI_L("CACHE REMOVED FROM MAP 0x%08x (new map count: "), cache);
	DEBUGTRACE_FILE(UNI_L("%d)\n"), m_filecache_map.GetCount());
}

//////////////////////////////////////////////////////////////////////
// P2PFile construction

P2PFile::P2PFile(OpString& hashkey)
:	m_write(FALSE),
	m_reference(0),
	m_currentoffset(0),
	m_filecache(NULL),
	m_is_cache_disabled(FALSE),
	m_metadata(NULL)
{
	m_hashkey.Set(hashkey);

	BT_RESOURCE_ADD("P2PFile", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PFile::~P2PFile()"
#endif

P2PFile::~P2PFile()
{
	if(IsOpen())
	{
		CloseAllFiles();
	}
	for(UINT32 pos = 0; pos < m_files.GetCount(); pos++)
	{
		P2PVirtualFile *file = m_files.Get(pos);

		if(file != NULL)
		{
			BT_RESOURCE_REMOVE(file);
			OP_DELETE(file);
		}
	}
	m_files.Clear();
	if(m_filecache)
	{
		if(m_write)
		{
			m_filecache->SetListener(NULL);
		}
		m_filecache->DecRef();
	}

	DEBUGTRACE_RES(UNI_L("*** DESTRUCTOR for %s ("), UNI_L(_METHODNAME_));
	DEBUGTRACE_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

OP_STATUS P2PFile::Init(UINT32 blocksize, BTFileCache *filecache, P2PMetaData *metadata)
{
	m_filecache = filecache;

	if(m_filecache)
	{
		if(m_write)
		{
			m_filecache->SetListener(this);
		}
		m_filecache->IncRef();
	}
	m_metadata = metadata;

	return OpStatus::OK;
}

OP_STATUS P2PFile::WriteCacheData(byte *buffer, UINT64 offset, UINT32 buflen)
{
	UINT64 written;

	if(Write(offset, buffer, buflen, &written, FALSE, (UINT32)(offset / m_filecache->GetBlocksize())))
	{
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

void P2PFile::CacheDisabled(BOOL disabled)
{
	m_is_cache_disabled = disabled;
}

BOOL P2PFile::Exists(const OpString& path, UINT64 size, UINT64 offset)
{
	UINT32 pos;

	for(pos = 0; pos < m_files.GetCount(); pos++)
	{
		P2PVirtualFile *vfile = m_files.Get(pos);

		if(vfile != NULL)
		{
			OpString path2;

			vfile->GetPath(path2);

			if(path2.Compare(path) == 0 && size == vfile->GetLength() && offset == vfile->GetOffset())
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

OP_STATUS P2PFile::AddFile(const OpString& path, UINT64 size, UINT64 offset)
{
	P2PVirtualFile *file = OP_NEW(P2PVirtualFile, (path, size, offset));

	if(file != NULL)
	{
		BT_RESOURCE_ADD("P2PVirtualFile-AddFile", file);

		m_files.Add(file);
		return OpStatus::OK;
	}
	return OpStatus::ERR_NO_MEMORY;
}

P2PVirtualFile * P2PFile::FindFirstFile(UINT64 offset, UINT64 length)
{
	for(UINT32 pos = 0; pos < m_files.GetCount(); pos++)
	{
		P2PVirtualFile *file = m_files.Get(pos);

		if(file->GetLength() == 0)
		{
			continue;
		}
		if(offset >= file->m_offset)		// above offset
		{
			if(offset <= file->m_offset + file->m_length - 1)	// but below threshold
			{
				return file;
			}
		}
	}
	return NULL;
}

/*

TEST 1 (success):

file : offset 0, length 10000
input: offset 5000, length 5000

if(5000 >= 0)
{
	if(5000 - 5000 < 10000 - 0)
	{
		if(0 + 10000 >= 5000 + 5000)
		{
			// success
		}
	}
}

TEST 2 (failure):

file : offset 0, length 10000
input: offset 11000, length 5000

if(11000 >= 0)
{
	if(5000 - 11000 < 10000 - 0)
	{
		if(0 + 10000 >= 11000 + 5000)
		{
			// fail
		}
	}
}

*/

BOOL P2PFile::CanFileProvideAllData(P2PVirtualFile *file, UINT64 offset, UINT64 length)
{
	// above offset
	if(offset >= file->m_offset)
	{
		// but size does not exceed file range
		if((INT64)(length - offset) < (INT64)(file->m_length - file->m_offset))
		{
			if((INT64)(file->m_offset) + file->m_length >= offset + length)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////
// P2PFile reference counts

void P2PFile::AddRef()
{
	m_reference++;

	DEBUGTRACE_FILE(UNI_L("FILE ADDREF 0x%08x (new ref: "), this);
	DEBUGTRACE_FILE(UNI_L("%d)\n"), m_reference);
}

void P2PFile::Release(BOOL write)
{
	m_reference--;

	DEBUGTRACE_FILE(UNI_L("FILE RELEASE 0x%08x (new ref: "), this);
	DEBUGTRACE_FILE(UNI_L("%d)\n"), m_reference);

	if(!m_reference)
	{
		g_P2PFiles->Remove(this);
		OP_DELETE(this);
		return;
	}
	if(m_write && write)
	{
		CloseWrite();
	}
}

//////////////////////////////////////////////////////////////////////
// P2PFile handle

BOOL P2PFile::IsOpen()
{
	BOOL isopen = FALSE;

	for(UINT32 pos = 0; pos < m_files.GetCount(); pos++)
	{
		P2PVirtualFile *file = m_files.Get(pos);

		if(file != NULL)
		{
			isopen |= file->m_file->IsOpen();
		}
	}
	return isopen;
}

void P2PFile::GetFilePath(OpString& path)
{
	path.Set(m_filepath);
}

void P2PFile::SetFilePath(const OpString& path)
{
	m_filepath.Set(path);
}

//////////////////////////////////////////////////////////////////////
// P2PFile construct paths

OP_STATUS P2PFile::ConstructPaths()
{
	BOOL singlefile = (m_files.GetCount() == 1);
	OP_MEMORY_VAR OP_STATUS status = OpStatus::OK;

	for(OP_MEMORY_VAR UINT32 pos = 0; pos < m_files.GetCount(); pos++)
	{
		P2PVirtualFile *file = m_files.Get(pos);

		if(file != NULL && file->GetLength() != 0)
		{
			OpString path;
			int seppos;

			seppos = file->m_path.FindFirstOf(PATHSEPCHAR);
			if(seppos != KNotFound || singlefile)
			{
				path.Set(file->m_path);
			}
			else
			{
				path.Set(m_hashkey);
				path.Append(PATHSEP);
				path.Append(file->m_path);
			}

			// path: dir\file

			if(m_filepath.HasContent())
			{
				TRAP(status, file->m_file->ConstructL(m_filepath, path));
				file->m_file->GetFullPath(file->m_fullpath);
			}
			else
			{
				OpString tmp_storage;
				const uni_char * download_folder_unichar = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_DOWNLOAD_FOLDER, tmp_storage);
				TRAP(status, file->m_file->ConstructL(download_folder_unichar, path));
				file->m_file->GetFullPath(file->m_fullpath);
			}
		}
	}
	return status;
}

//////////////////////////////////////////////////////////////////////
// P2PFile open

BOOL P2PFile::Open(BOOL write, BOOL create)
{
	OP_ASSERT(m_filepath.HasContent());

	if(IsOpen() || m_files.GetCount() == 0)
	{
		return FALSE;
	}
	FileKind fileType = kFileKindBinary;
	OP_MEMORY_VAR OpenMode openMode;
	ShareMode shareMode;
	TranslationMode transMode = kTranslateBinary;

	openMode = kOpenRead;
	shareMode = kShareFull;

	if(create)
	{
		openMode = kOpenReadAndWriteNew;
	}
	else
	{
		openMode = write ? kOpenReadAndWrite : kOpenRead;
	}
	OP_MEMORY_VAR OP_STATUS status = OpStatus::OK;

	status = ConstructPaths();
	if(OpStatus::IsError(status))
	{
		return FALSE;
	}
	for(OP_MEMORY_VAR UINT32 pos = 0; pos < m_files.GetCount(); pos++)
	{
		P2PVirtualFile *file = m_files.Get(pos);

		if(file != NULL && file->GetLength() != 0)
		{
			OpString loc;

			file->GetFullFilePath(loc);

			OpFile64::MakePath(loc.CStr());

			DEBUGTRACE_FILE(UNI_L("Opening file: %s, mode: "), (uni_char *)loc.CStr());

			OpString mode;

			if(write)
			{
				mode.Append("write ");
			}
			if(create)
			{
				mode.Append("create");
			}
			DEBUGTRACE_FILE(UNI_L("%s\n"), mode.CStr());

			OP_STATUS tmpstatus = file->m_file->Open(fileType, openMode, shareMode, transMode);

			if(tmpstatus != OpStatus::OK && write && !create)
			{
				// we want to write, but file isn't there. Force a create
				tmpstatus = file->m_file->Open(fileType, kOpenReadAndWriteNew, shareMode, transMode);

				OP_ASSERT(tmpstatus == OpStatus::OK);
			}
			status |= tmpstatus;
			if(file->m_file->IsOpen())
			{
				m_write = write;
			}
		}
	}
	return status == OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////
// P2PFile write access management

void P2PFile::CloseAllFiles()
{
	DumpCache();

	for(UINT32 pos = 0; pos < m_files.GetCount(); pos++)
	{
		P2PVirtualFile *file = m_files.Get(pos);

		if(file != NULL)
		{
			OpString path;

			file->GetPath(path);

			DEBUGTRACE_FILE(UNI_L("Closing file: %s\n"), path.CStr());
			file->m_file->Close();
		}
	}
}

BOOL P2PFile::EnsureWrite()
{
	if(IsOpen() == FALSE) return FALSE;
	if(m_write) return TRUE;

	CloseAllFiles();

	if(Open(TRUE, FALSE)) return TRUE;

	Open(FALSE, FALSE);

	return FALSE;
}

BOOL P2PFile::CloseWrite()
{
	if(IsOpen() == FALSE) return FALSE;
	if(!m_write) return TRUE;

	if(DumpCache() == FALSE)
	{
		OP_ASSERT(FALSE);
		return FALSE;
	}

	CloseAllFiles();

	BOOL success = Open(FALSE, FALSE);

	if(success)
	{
		m_write = FALSE;
	}
	return success;
}

BOOL P2PFile::SetEndOfFile(P2PVirtualFile *file)
{
	OpFileLength len;

	if(file != NULL)
	{
		if(file->m_file->IsOpen() == FALSE)
		{
			return FALSE;
		}
	}
	else
	{
		if(IsOpen() == FALSE)
		{
			return FALSE;
		}
		file = FindFirstFile(m_currentoffset, 1000);
		if(file == NULL)
		{
			return FALSE;
		}
	}
	if(!m_write) return TRUE;

	OpFileLength current = file->m_file->GetFilePos();

	file->m_file->Length(&len);

	if(len > current)
	{
		char *tmpbuf = OP_NEWA(char, 4096);

		OP_ASSERT(FALSE);		// verify this code!
		if(tmpbuf)
		{
			memset(tmpbuf, 0, 4096);

			while(len > current)
			{
				OpFileLength writelen = MIN(len - current, 4096);

				if(file->m_file->Write(tmpbuf, writelen))
				{
					current += writelen;
				}
			}
			OP_DELETEA(tmpbuf);
		}
	}
	return TRUE;
}

BOOL P2PFile::SetFilePointer(UINT64 offset, P2PVirtualFile *vfile)
{
	if(vfile == NULL)
	{
		vfile = FindFirstFile(offset, 100);	// second argument not really used
		if(vfile == NULL)
		{
			return FALSE;
		}
	}
	if(vfile->m_file->IsOpen() == FALSE) return FALSE;

	m_currentoffset = offset;

	// get the offset relatively within this file
	offset = GET_RELATIVE_OFFSET(offset, vfile->m_offset);

	vfile->m_file->SetFilePos(offset, SEEK_FROM_START);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// P2PFile read

BOOL P2PFile::Read(UINT64 offset, byte *buffer, UINT64 bufferCount, UINT64* pnRead, BOOL bypass_cache)
{
	byte *bufferptr = buffer;

	*pnRead = 0;

	if(IsOpen() == FALSE)
	{
		return FALSE;
	}
#ifdef USE_BT_DISKCACHE
	if(!m_is_cache_disabled)
	{
		if(!bypass_cache)
		{
			UINT64 buf_len = bufferCount;
			if(m_filecache->GetCacheData(buffer, buf_len, offset))
			{
				// cache could provide all the data
				*pnRead = buf_len;
				return TRUE;
			}
			else
			{
				if(bufferCount == m_filecache->GetBlocksize())
				{
					// invalidate the whole block
					m_filecache->InvalidateBlock((UINT32)(offset / m_filecache->GetBlocksize()));
				}
				else
				{
					m_filecache->InvalidateRange(offset, bufferCount);
				}
			}
		}
		else
		{
			if(bufferCount == m_filecache->GetBlocksize())
			{
				// invalidate the whole block
				m_filecache->InvalidateBlock((UINT32)(offset / m_filecache->GetBlocksize()));
			}
			else
			{
				m_filecache->InvalidateRange(offset, bufferCount);
			}
		}
	}
#endif // USE_BT_DISKCACHE
	P2PVirtualFile *vfile = FindFirstFile(offset, bufferCount);
	if(vfile != NULL)
	{
		while(bufferCount > 0 && vfile != NULL)
		{
			if(CanFileProvideAllData(vfile, offset, bufferCount))
			{
				// we can keep it all within the same file
				if(SetFilePointer(offset, vfile))
				{
					if(vfile->m_file->Read(bufferptr, bufferCount))
					{
						*pnRead += bufferCount;
#ifdef USE_BT_DISKCACHE
						if(!m_is_cache_disabled && !bypass_cache)
						{
							// let's update the cache with the newly read data
							if(OpStatus::IsError(m_filecache->AddCacheEntry(buffer, (UINT32)*pnRead, offset, (UINT32)(offset / m_filecache->GetBlocksize()), TRUE)))
							{
								m_is_cache_disabled = TRUE;
							}
						}
#endif // USE_BT_DISKCACHE
						return TRUE;
					}
					else
					{
						return FALSE;
					}
				}
				else
				{
					break;
				}
			}
			else
			{
				// SetFilePointer will adjust for the relative offset for the file
				if(SetFilePointer(offset, vfile))
				{
					// max of what this file can provide
					UINT64 realoffset = GET_RELATIVE_OFFSET(offset, vfile->m_offset);
					UINT64 readlen = min(bufferCount, vfile->m_length - realoffset);

					if(bufferCount > vfile->m_length - realoffset)
					{
						readlen = vfile->m_length - realoffset;
					}
					else
					{
						readlen = bufferCount;
					}
					if(!vfile->m_file->Read(bufferptr, readlen))
					{
						return FALSE;
					}
					*pnRead += readlen;
					bufferCount -= readlen;
					offset += readlen;
					bufferptr = &bufferptr[readlen];

					vfile = FindFirstFile(offset, bufferCount);
				}
				else
				{
					break;
				}
			}
		}
		if(bufferCount == 0)
		{
#ifdef USE_BT_DISKCACHE
			if(!m_is_cache_disabled && !bypass_cache)
			{
				// let's update the cache with the newly read data
				if(OpStatus::IsError(m_filecache->AddCacheEntry(buffer, (UINT64)*pnRead, offset, (UINT32)(offset / m_filecache->GetBlocksize()), TRUE)))
				{
					m_is_cache_disabled = TRUE;
				}
			}
#endif // USE_BT_DISKCACHE
			return TRUE;
		}
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////
// P2PFile write (with deferred extension)

BOOL P2PFile::Write(UINT64 offset, byte *buffer, UINT64 bufferCount, UINT64 * pnWritten, BOOL use_cache, UINT32 blocknumber)
{
	byte *bufferptr = buffer;

	*pnWritten = 0;

	if(IsOpen() == FALSE)
	{
		return FALSE;
	}
	if(!m_write)
	{
		return FALSE;
	}
	P2PVirtualFile *vfile = FindFirstFile(offset, bufferCount);
	if(vfile != NULL)
	{
#ifdef USE_BT_DISKCACHE
		if(!m_is_cache_disabled && use_cache)
		{
			// let's update the cache with the newly read data
			if(OpStatus::IsError(m_filecache->AddCacheEntry(buffer, (UINT64)bufferCount, offset, (UINT32)(offset / m_filecache->GetBlocksize()), FALSE)))
			{
				m_is_cache_disabled = TRUE;
			}
			else
			{
				return TRUE;
			}
		}
#endif // USE_BT_DISKCACHE

		while(bufferCount > 0 && vfile != NULL)
		{
			if(CanFileProvideAllData(vfile, offset, bufferCount))
			{
				// we can keep it all within the same file
				if(SetFilePointer(offset, vfile))
				{
					if(vfile->m_file->Write(bufferptr, bufferCount))
					{
						*pnWritten += bufferCount;
						if(m_metadata)
						{
							OpString path;
							vfile->GetFullFilePath(path);

							m_metadata->UpdateFileData(path, op_time(NULL), vfile->m_length);
						}
						return TRUE;
					}
					else
					{
						return FALSE;
					}
				}
				else
				{
					break;
				}
			}
			else
			{
				// scenario:
				// file ptr is at 10000   -> realoffset
				// file size is 15000     -> vfile->m_length
				// we need to write 7000  -> bufferCount
				//
				// calculation
				//
				// vfile->m_length(15000) - realoffset(10000) = writelen(5000)

				// SetFilePointer will adjust for the relative offset for the file
				if(SetFilePointer(offset, vfile))
				{
					// max of what this file can provide
					UINT64 realoffset = GET_RELATIVE_OFFSET(offset, vfile->m_offset);
					UINT64 writelen;

					if(bufferCount > vfile->m_length - realoffset)
					{
						writelen = vfile->m_length - realoffset;
					}
					else
					{
						writelen = bufferCount;
					}

					if(!vfile->m_file->Write(bufferptr, writelen))
					{
						return FALSE;
					}
					*pnWritten += writelen;
					bufferCount -= writelen;
					offset += writelen;
					bufferptr = &bufferptr[writelen];

					if(m_metadata)
					{
						OpString path;
						vfile->GetFullFilePath(path);

						m_metadata->UpdateFileData(path, op_time(NULL), vfile->GetLength());
					}

					vfile = FindFirstFile(offset, bufferCount);
				}
				else
				{
					break;
				}
			}
		}
		if(bufferCount == 0)
		{
			return TRUE;
		}
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////
// P2PFile deferred writes

BOOL P2PFile::DumpCache(BOOL bOffline)
{
#ifdef USE_BT_DISKCACHE
	if(!IsOpen() || !m_write || !m_filecache)
	{
		return FALSE;
	}
	if(OpStatus::IsError(m_filecache->WriteCache()))
	{
		return FALSE;
	}

#endif // USE_BT_DISKCACHE

	return TRUE;
}

BOOL P2PFile::FlushBuffers()
{
	for(UINT32 pos = 0; pos < m_files.GetCount(); pos++)
	{
		P2PVirtualFile *file = m_files.Get(pos);

		if(file != NULL)
		{
			file->m_file->Flush();
		}
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// P2PMetaData for storing which blocks have ok CRC etc.

P2PMetaData::P2PMetaData()
	:	m_blocks(NULL),
		m_changed(FALSE),
		m_has_meta_data(FALSE),
		m_last_write(0)
{
	BT_RESOURCE_ADD("P2PMetaData", this);
}

P2PMetaData::~P2PMetaData()
{
	OP_DELETEA(m_blocks);

	m_files.DeleteAll();

	BT_RESOURCE_REMOVE(this);
}

OP_STATUS P2PMetaData::Init(UINT32 block_count, OpString& name)
{
	if(m_blocks)
	{
		OP_DELETEA(m_blocks);
	}
	m_blocks = OP_NEWA(UINT32, block_count);
	if(!m_blocks)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	m_block_count = block_count;

	ClearBlocks();

	m_files.DeleteAll();

	OpString tmp_storage;
	const OpStringC opdir(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_BITTORRENT_METADATA_FOLDER, tmp_storage));

	m_name.Set(opdir.CStr());

	if (opdir[opdir.Length()-1] != PATHSEPCHAR)
		m_name.Append(PATHSEP);

	m_name.Append(name);
	m_name.Append(".dat");

	m_last_write = op_time(NULL);

	OP_STATUS s = ReadMetaFile();
	if(s != OpStatus::ERR_FILE_NOT_FOUND)
	{
		return s;
	}
	return OpStatus::OK;
}

void P2PMetaData::ClearBlocks()
{
	OP_MEMORY_VAR unsigned int i;

	for(i = 0; i < m_block_count; i++)
	{
		m_blocks[i] = 0;
	}
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PMetaData::DeleteMetaFile()"
#endif

OP_STATUS P2PMetaData::DeleteMetaFile()
{
	if(m_name.HasContent())
	{
		OpStackAutoPtr<OpFile> dlfp(OP_NEW(OpFile, ()));
		if (!dlfp.get())
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(dlfp->Construct(m_name.CStr()));
		RETURN_IF_ERROR(dlfp->Delete());
	}
	return OpStatus::OK;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PMetaData::WriteMetaFile()"
#endif

OP_STATUS P2PMetaData::WriteMetaFile(BOOL force)
{
	OP_STATUS ret;

	if(m_name.IsEmpty())
	{
		// init not called?
		return OpStatus::ERR_NULL_POINTER;
	}

	if(!force)
	{
		if(!m_changed)
		{
			return OpStatus::OK;
		}
		time_t now = op_time(NULL);

		if(now < m_last_write + 10)		// write max every 10 seconds
		{
			return OpStatus::OK;
		}
		m_last_write = now;
	}
	PROFILE(TRUE);

	OpStackAutoPtr<OpFile> dlfp(OP_NEW(OpFile, ()));
	if (!dlfp.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(dlfp->Construct(m_name.CStr()));
	RETURN_IF_ERROR(dlfp->Open(OPFILE_WRITE));

	// dload_rescue_file takes ownership of dlfp !!!! DO NOT DELETE !!!!
	DataFile dload_rescue_file(dlfp.release(), 1, 1, 2);
	OpStackAutoPtr<DataFile_Record> url_rec(NULL);

	TRAP(ret, dload_rescue_file.InitL());
	RETURN_IF_ERROR(ret);

	OP_MEMORY_VAR unsigned int i;

	for(i = 0; i < m_files.GetCount(); i++)
	{
		P2PMetaDataFileInfo *data = m_files.Get(i);
		if(data)
		{
			data->m_last_modified = op_time(NULL);

			url_rec.reset(OP_NEW(DataFile_Record, (BT_TAG_DATE_FILE_ENTRY)));
			if (!url_rec.get())
				return OpStatus::ERR_NO_MEMORY;

			url_rec->SetRecordSpec(dload_rescue_file.GetRecordSpec());

			TRAP(ret, url_rec->AddRecordL(BT_TAG_BLOCK_FILENAME, (char *)data->m_filename.CStr()); \
				url_rec->AddRecordL(BT_TAG_BLOCK_FILEDATE, (unsigned int)data->m_last_modified));

			RETURN_IF_ERROR(ret);

			TRAP(ret, url_rec->WriteRecordL(&dload_rescue_file));

			RETURN_IF_ERROR(ret);

			url_rec.reset();
		}
	}
	DEBUGTRACE_METAFILE(UNI_L("Writing blocks:\n"), UNI_L(""));
	for(i = 0; i < m_block_count; i++)
	{
		if(m_blocks[i])
		{
			DEBUGTRACE_METAFILE(UNI_L("Block: %d\n"), i);

			url_rec.reset(OP_NEW(DataFile_Record, (BT_TAG_CRC_FILE_ENTRY)));
			if (!url_rec.get())
				return OpStatus::ERR_NO_MEMORY;

			url_rec->SetRecordSpec(dload_rescue_file.GetRecordSpec());

			TRAP(ret, url_rec->AddRecordL(BT_TAG_BLOCK_NUMBER, i); \
				url_rec->AddRecordL(BT_TAG_BLOCK_STATUS, m_blocks[i]));

			RETURN_IF_ERROR(ret);

			TRAP(ret, url_rec->WriteRecordL(&dload_rescue_file));

			RETURN_IF_ERROR(ret);

			url_rec.reset();

			m_has_meta_data = TRUE;
		}
	}
	m_changed = FALSE;

	// dload_rescue_file destructor destroys dlfp
	return OpStatus::OK;
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "P2PMetaData::ReadMetaFile()"
#endif

OP_STATUS P2PMetaData::ReadMetaFile()
{
	PROFILE(TRUE);

	OP_STATUS ret;

	OpStackAutoPtr<OpFile> dlfp(OP_NEW(OpFile, ()));
	if (!dlfp.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(dlfp->Construct(m_name.CStr()));

    // OpFile::Open() shall not return ERR_FILE_NOT_FOUND (bug #209114)
    BOOL exists;
    RETURN_IF_ERROR(dlfp->Exists(exists));
    if( !exists )
        return OpStatus::ERR_FILE_NOT_FOUND;

	RETURN_IF_ERROR(dlfp->Open(OPFILE_READ));

	DataFile dload_rescue_file(dlfp.release());

	TRAP(ret, dload_rescue_file.InitL());
	RETURN_IF_ERROR(ret);

	OpStackAutoPtr<DataFile_Record> url_rec(NULL);

	ClearBlocks();

	while(TRUE)
	{
		TRAP(ret, url_rec.reset(dload_rescue_file.GetNextRecordL()));
		RETURN_IF_ERROR(ret);

		if (!url_rec.get())
		{
			break;
		}
		url_rec->SetRecordSpec(dload_rescue_file.GetRecordSpec());
		TRAP(ret, url_rec->IndexRecordsL());
		RETURN_IF_ERROR(ret);

		if(url_rec->GetTag() == BT_TAG_DATE_FILE_ENTRY)
		{
			OpString8 temp;
			OpString filename;

			TRAP(ret, url_rec->GetIndexedRecordStringL(BT_TAG_BLOCK_FILENAME, temp));
			RETURN_IF_ERROR(ret);

			RETURN_IF_ERROR(filename.Set(temp));

			time_t last_modified = (time_t)url_rec->GetIndexedRecordUIntL(BT_TAG_BLOCK_FILEDATE);

			if(filename.HasContent())
			{
				RETURN_IF_ERROR(UpdateFileData(filename, last_modified, 0));
			}
		}
		else if(url_rec->GetTag() == BT_TAG_CRC_FILE_ENTRY)
		{
			unsigned int block_number = url_rec->GetIndexedRecordUIntL(BT_TAG_BLOCK_NUMBER);

			if(block_number < m_block_count)
			{
				m_blocks[block_number] = url_rec->GetIndexedRecordUIntL(BT_TAG_BLOCK_STATUS);
			}
		}
	}
	// dload_rescue_file destructor destroys dlfp

	return OpStatus::OK;
}

void P2PMetaData::SetBlockAsDone(UINT32 block)
{
	OP_ASSERT(block < m_block_count);

	if(block < m_block_count)
	{
		m_blocks[block] = 1;
		m_changed = TRUE;
	}
#ifdef _DEBUG
	unsigned int i;

	for(i = 0; i < m_block_count; i++)
	{
		DEBUGTRACE_METAFILE(UNI_L("%s"), m_blocks[i] ? UNI_L("*") : UNI_L("."));
	}
	DEBUGTRACE_METAFILE(UNI_L("%s"), UNI_L("\n"));

#endif // _DEBUG
}

void P2PMetaData::SetBlockAsDirty(UINT32 block)
{
	OP_ASSERT(block < m_block_count);

	if(block < m_block_count)
	{
		m_blocks[block] = 0;
		m_changed = TRUE;
	}
}


BOOL P2PMetaData::IsBlockDone(UINT32 block)
{
	OP_ASSERT(block < m_block_count);

	if(block < m_block_count)
	{
		if(m_blocks[block])
		{
			return TRUE;
		}
	}
	return FALSE;
}

P2PMetaDataFileInfo* P2PMetaData::FindFileData(OpString& filename)
{
	UINT32 cnt;

	for(cnt = 0; cnt < m_files.GetCount(); cnt++)
	{
		P2PMetaDataFileInfo *data = m_files.Get(cnt);
		if(data)
		{
			OpString8 tmp;

			tmp.Set(filename.CStr());

			if(data->m_filename.CompareI(tmp) == 0)
			{
				return data;
			}
		}
	}
	return NULL;
}

OP_STATUS P2PMetaData::HasFileChanged(OpString& filename, BOOL& changed, UINT64 file_len)
{
	OpFile file;

	if(m_files.GetCount() == 0)
	{
		// no meta info exists for any file
		changed = FALSE;
		return OpStatus::ERR_FILE_NOT_FOUND;
	}
	P2PMetaDataFileInfo *data = FindFileData(filename);
	if(data)
	{
		BOOL exists = FALSE;
		time_t last_modified = 0;

		RETURN_IF_ERROR(file.Construct(filename.CStr()));
		file.Exists(exists);
		if(!exists)
		{
			ClearBlocks();
			changed = TRUE;
			return OpStatus::ERR_FILE_NOT_FOUND;
		}
		if(OpStatus::IsError(file.GetLastModified(last_modified)))
		{
			changed = FALSE;
			return OpStatus::ERR_FILE_NOT_FOUND;
		}
/*
		OpFileLength len = file.GetFileLength();
		if(len != (OpFileLength)file_len)
		{
			ClearBlocks();
			changed = TRUE;
			return OpStatus::OK;
		}
*/
		// 10 seconds grace period
//		if(last_modified - 10 > data->m_last_modified)
		if(last_modified != data->m_last_modified)
		{
			changed = TRUE;
			return OpStatus::OK;
		}
		else
		{
			changed = FALSE;
			return OpStatus::OK;
		}
	}
	changed = FALSE;
	return OpStatus::ERR_FILE_NOT_FOUND;
}

OP_STATUS P2PMetaData::UpdateFileData(OpString& filename, time_t last_modified, UINT64 file_len)
{
	P2PMetaDataFileInfo *data = FindFileData(filename);
	if(data)
	{
		data->m_last_modified = last_modified;
		return OpStatus::OK;
	}
	ClearBlocks();
	P2PMetaDataFileInfo *meta_info = OP_NEW(P2PMetaDataFileInfo, (filename, last_modified, file_len));
	if (!meta_info)
		return OpStatus::ERR_NO_MEMORY;

	return m_files.Add(meta_info);
}


#endif // _BITTORRENT_SUPPORT_

#endif //M2_SUPPORT
