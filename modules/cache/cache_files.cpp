/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/cache/url_cs.h"

#include "modules/url/url2.h"
#include "modules/cache/multimedia_cache.h"

#ifndef RAMCACHE_ONLY

CacheFile_Storage::CacheFile_Storage(URL_Rep *url_rep, URLCacheType ctyp)
: File_Storage(url_rep, ctyp)
{
}

OP_STATUS CacheFile_Storage::Construct(URL_Rep *url_rep, const OpStringC &afilename)
{
	return File_Storage::Construct(afilename, NULL);
}


#ifndef SEARCH_ENGINE_CACHE
OP_STATUS CacheFile_Storage::Construct(FileName_Store &filenames, OpFileFolder folder, const OpStringC &afilename, OpFileLength file_len)
{
	return File_Storage::Construct(afilename, &filenames, folder, file_len);
}
#else
OP_STATUS CacheFile_Storage::Construct(OpFileFolder folder, const OpStringC &afilename)
{
	return File_Storage::Construct(afilename, NULL, folder);
}
#endif

#ifdef UNUSED_CODE
CacheFile_Storage* CacheFile_Storage::Create(URL_Rep *url_rep, const OpStringC &afilename, URLCacheType ctyp)
{
	CacheFile_Storage* cfs = OP_NEW(CacheFile_Storage, (url_rep, ctyp));
	if (!cfs)
		return NULL;
	if (OpStatus::IsError(cfs->Construct(url_rep, afilename)))
	{
		OP_DELETE(cfs);
		return NULL;
	}
	return cfs;
}


#ifndef SEARCH_ENGINE_CACHE
CacheFile_Storage* CacheFile_Storage::Create(URL_Rep *url_rep, FileName_Store &filenames, OpFileFolder folder, const OpStringC &afilename, URLCacheType ctyp)
{
	CacheFile_Storage* cfs = OP_NEW(CacheFile_Storage, (url_rep, ctyp));
	if (!cfs)
		return NULL;
	if (OpStatus::IsError(cfs->Construct(filenames, folder, afilename)))
	{
		OP_DELETE(cfs);
		return NULL;
	}
	return cfs;
}
#endif
#endif


#ifdef CACHE_RESOURCE_USED
OpFileLength CacheFile_Storage::ResourcesUsed(ResourceID resource)
{
	if(resource == MEMORY_RESOURCE)
		return Cache_Storage::ResourcesUsed(resource);

	return FileLength();
}
#endif

Session_Only_Storage::Session_Only_Storage(URL_Rep *url_rep)
	: CacheFile_Storage(url_rep, URL_CACHE_TEMP)
{
}

OP_STATUS Session_Only_Storage::Construct(URL_Rep *url_rep)
{
	return CacheFile_Storage::Construct(url_rep);
}

Session_Only_Storage* Session_Only_Storage::Create(URL_Rep *url_rep)
{
	Session_Only_Storage* sos = OP_NEW(Session_Only_Storage, (url_rep));
	if (!sos)
		return NULL;
	if (OpStatus::IsError(sos->Construct(url_rep)))
	{
		OP_DELETE(sos);
		return NULL;
	}
	return sos;	
}

#ifdef CACHE_RESOURCE_USED
OpFileLength Session_Only_Storage::ResourcesUsed(ResourceID resource)
{
	if(resource == MEMORY_RESOURCE || resource == TEMP_DISK_RESOURCE)
		return CacheFile_Storage::ResourcesUsed(resource);

	return 0;
}
#endif

Persistent_Storage::Persistent_Storage(URL_Rep *url_rep)
: CacheFile_Storage(url_rep, URL_CACHE_DISK)
{
}

Persistent_Storage::~Persistent_Storage()
{
}

OP_STATUS Persistent_Storage::Construct(const OpStringC &afilename)
{
	return CacheFile_Storage::Construct(NULL,afilename);
}

#ifndef SEARCH_ENGINE_CACHE
OP_STATUS Persistent_Storage::Construct(FileName_Store &filenames, OpFileFolder folder, const OpStringC &afilename, OpFileLength file_len)
{
	return CacheFile_Storage::Construct(filenames, folder, afilename, file_len);
}
#else
OP_STATUS Persistent_Storage::Construct(OpFileFolder folder, const OpStringC &afilename)
{
	return CacheFile_Storage::Construct(folder, afilename);
}
#endif

Persistent_Storage* Persistent_Storage::Create(URL_Rep *url_rep, const OpStringC &afilename)
{
	Persistent_Storage* ps = OP_NEW(Persistent_Storage, (url_rep));
	if (!ps)
		return NULL;
	if (OpStatus::IsError(ps->Construct(afilename)))
	{
			OP_DELETE(ps);
			return NULL;
	}
	return ps;	
}

#ifndef SEARCH_ENGINE_CACHE
Persistent_Storage* Persistent_Storage::Create(URL_Rep *url_rep, FileName_Store &filenames, OpFileFolder folder, const OpStringC &afilename, OpFileLength file_len)
{
	Persistent_Storage* ps = OP_NEW(Persistent_Storage, (url_rep));
	if (!ps)
		return NULL;
	if (OpStatus::IsError(ps->Construct(filenames, folder, afilename, file_len)))
	{
		OP_DELETE(ps);
		return NULL;
	}
	return ps;	
}
#else
Persistent_Storage* Persistent_Storage::Create(URL_Rep *url_rep, OpFileFolder folder, const OpStringC &afilename)
{
	Persistent_Storage* ps = OP_NEW(Persistent_Storage, (url_rep));
	if (!ps)
		return NULL;
	if (OpStatus::IsError(ps->Construct(folder, afilename)))
	{
		OP_DELETE(ps);
		return NULL;
	}
	return ps;	
}
#endif

#ifdef CACHE_RESOURCE_USED
OpFileLength Persistent_Storage::ResourcesUsed(ResourceID resource)
{
	if(resource == MEMORY_RESOURCE || resource == DISK_RESOURCE)
		return CacheFile_Storage::ResourcesUsed(resource);

	return 0;
}
#endif
#endif  // !RAMCACHE_ONLY

#ifdef URL_ENABLE_ASSOCIATED_FILES
AssociatedFile *Cache_Storage::CreateAssociatedFile(URL::AssociatedFileType type)
{
	OpString fname;
	AssociatedFile *f;
	OpFileFolder folder_assoc;

	if (AssocFileName(fname, type, folder_assoc, TRUE) != OpStatus::OK)
		return NULL;

	if ((f = OP_NEW(AssociatedFile, ())) == NULL)
		return NULL;

	if (f->Construct(fname, folder_assoc) != OpStatus::OK)
	{
		OP_DELETE(f);
		return NULL;
	}

	if (f->Open(OPFILE_WRITE | OPFILE_SHAREDENYREAD | OPFILE_SHAREDENYWRITE) != OpStatus::OK)
	{
		OP_DELETE(f);
		return NULL;
	}

	SetPurgeAssociatedFiles(type);

	return f;
}

AssociatedFile *Cache_Storage::OpenAssociatedFile(URL::AssociatedFileType type)
{
	OpString fname;
	AssociatedFile *f;
	OpFileFolder folder_assoc;

	if (AssocFileName(fname, type, folder_assoc, FALSE) != OpStatus::OK)
		return NULL;

	if ((f = OP_NEW(AssociatedFile, ())) == NULL)
		return NULL;

	if (f->Construct(fname, folder_assoc) != OpStatus::OK)
	{
		OP_DELETE(f);
		return NULL;
	}

	if (f->Open(OPFILE_READ | OPFILE_SHAREDENYWRITE) != OpStatus::OK)
	{
		OP_DELETE(f);
		return NULL;
	}

	return f;
}

#endif  // URL_ENABLE_ASSOCIATED_FILES
