/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/


#ifndef CACHE_COMMON_H
#define CACHE_COMMON_H

#define CACHE_TEMP_FILENAME_LEN	256
// Number of files for each generation. Remember that each folder can have more than this number of files
#define GEN_CACHE_FILES_PER_GENERATION 128
// Number of generations (== directory). A number too high can stress the file system, a number too low is sub-optimal
#define GEN_CACHE_GENERATIONS 128
// Name of the directory that will contain the files used for "View Source"
// The cache will delete the files during the startup
#define CACHE_VIEW_SOURCE_FOLDER UNI_L("source")
// Name of the directory that will contain the session files
// The cache will delete the files during the startup
#define CACHE_SESSION_FOLDER UNI_L("sesn")

#ifndef DEFAULT_CACHE_FLUSH_THRESHOLD_LIMIT
// How long should unchacheable elements be kept in cache even if they are no longer used by a document?
// Default is 5 minutes, for SDK 1 minute, after being loaded. this prevents immedate reload of documents
#if !defined(SDK) && !defined(WIN_CE)
#define DEFAULT_CACHE_FLUSH_THRESHOLD_LIMIT 5
#else
#define DEFAULT_CACHE_FLUSH_THRESHOLD_LIMIT 1
#endif
#endif


#ifndef MAX_MEMORY_CONTENT_SIZE
#define MAX_MEMORY_CONTENT_SIZE (256*1024)
#endif

#ifndef MIN_FORCE_DISK_STORAGE
#define MIN_FORCE_DISK_STORAGE (64*1024)
#endif

#ifndef SEARCH_ENGINE_CACHE
#define DiskCacheFile UNI_L("dcache4.url")
#define DiskCacheFileNew UNI_L("dcache4.new")
#define DiskCacheFileOld UNI_L("dcache4.old")
#endif
#define VlinkFile UNI_L("vlink4.dat")
#define VlinkFileNew UNI_L("vlink4.new")
#define VlinkFileOld UNI_L("vlink4.old")
#define CURRENT_CACHE_VERSION	0x00020000

#endif
