/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_MEM_MEM_MAN_H
#define MODULES_HARDCORE_MEM_MEM_MAN_H

class HTMLayoutElm;

#include "modules/hardcore/base/opstatus.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/util/excepts.h"
#include "modules/util/simset.h"
#include "modules/hardcore/component/OpMemoryPool.h"
#include "modules/hardcore/src/generated/g_message_hardcore_legacy.h"
typedef OpMemoryPool<sizeof(OpLegacyMessage)> OpLegacyMessagePool;
#ifdef SELFTEST
# include "modules/hardcore/tests/testmemorypool.h"
#endif // SELFTEST

class FrameDisplayElm;
class CSS_DataTmp;
class MMDocElm;
class Image;

#ifdef USE_POOLING_MALLOC
//
// By enabeling this code, the REPORT_ macros designed to allocate/deallocate
// and count total usage will allocate and deallocate using a pooling malloc.
// These macros are used by the document code to keep track of the total
// memory usage for document data.  Pooling the document objects may give
// significant performance improvements if the system allocator is slow,
// or the heap gets fragmented over time.
//
// The functions to do the allocation/deallocation needs to be implemented
// in the various platform layers that intends to use this functionality.
//
// NOTE: The REPORT_MEMMAN_CHECK_NEW is not supported as it seems to have
// fallen into disuse, at least on platforms where pooling is used.
//

extern "C" void* OpPooledAlloc(size_t size);
extern "C" void OpPooledFree(void* ptr);

#define REPORT_MEMMAN_NEW(s) \
do { \
	void* ptr = OpPooledAlloc(s); \
	if (ptr) \
		MemoryManager::IncDocMemoryCount(s, FALSE); \
	return ptr; \
} while (0)

#define REPORT_MEMMAN_DELETE(ptr, s) \
do { \
	if (ptr) { \
		MemoryManager::DecDocMemoryCount(s); \
		OpPooledFree(ptr); \
	} \
} while(0)

#define REPORT_MEMMAN_NEW_L(s, l) \
do { \
    (void)l; \
	void* ptr = OpPooledAlloc(s); \
    LEAVE_IF_NULL(ptr); \
    MemoryManager::IncDocMemoryCount(s, FALSE); \
	return ptr; \
} while (0)

#else
//
// Regular REPORT_MEMMAN that uses global allocator and counts usage.
//

#define REPORT_MEMMAN_NEW(s) \
do { \
	void* ptr = ::operator new(s); \
	if (ptr) \
		MemoryManager::IncDocMemoryCount(s, FALSE); \
	return ptr; \
} while (0)

#define REPORT_MEMMAN_CHECK_NEW(s) \
do { \
	void* ptr = ::operator new(s); \
	if (ptr) \
	    MemoryManager::IncDocMemoryCount(s); \
	return ptr; \
} while (0)

#define REPORT_MEMMAN_NEW_L(s, l) \
do { \
	void* ptr = ::operator new(s, l); \
	MemoryManager::IncDocMemoryCount(s, FALSE); \
	return ptr; \
} while (0)

#define REPORT_MEMMAN_DELETE(ptr, s) \
do { \
	if (ptr) { \
	    MemoryManager::DecDocMemoryCount(s); \
		::operator delete(ptr); \
	} \
} while(0)

#endif // USE_POOLING_MALLOC

#define REPORT_MEMMAN_INC(s) do { MemoryManager::IncDocMemoryCount(s, FALSE); } while(0)

#define REPORT_MEMMAN_DEC(s) do { MemoryManager::DecDocMemoryCount(s); } while(0)

class FramesDocument;

class MMDocElm
  : public Link
{
private:

	FramesDocument* doc;
	BOOL keep_in_memory_as_long_as_possible;

public:

	MMDocElm(FramesDocument* d, BOOL keep_as_long_as_possible) 
	: doc(d)
	, keep_in_memory_as_long_as_possible(keep_as_long_as_possible)
	
	{
	}

	MMDocElm*		Suc() const {return (MMDocElm*)Link::Suc();}
	MMDocElm*		Pred() const {return (MMDocElm*)Link::Pred();}
	FramesDocument*	Doc() const { return doc; }
	/**
	 * Indicates whether this document should be tried to be kept in memory as long as it's possible \n
	 * e.g. due to user changes in doc's tree
	 */
	BOOL ShouldBeKeptInMemoryAsLongAsPossible() { return keep_in_memory_as_long_as_possible; }
	void SetKeepInMemory(BOOL keep) { keep_in_memory_as_long_as_possible = keep; }
};


class MemoryManager
{
#if defined(SELFTEST) && defined(PREFS_AUTOMATIC_RAMCACHE)
	friend void TestGetRAMCacheSizes(UINT32 available_mem, size_t& fig_cache_size, size_t& doc_cache_size);
#endif // SELFTEST && PREFS_AUTOMATIC_RAMCACHE

private:
	Head			locked_doc_list; // Temporary list used when
	                                 // freeing doc memeory.

	Head			doc_list;	// list of undisplayed documents.
  								// first in list is first to be deleted

	size_t	hard_max_doc_mem;
	size_t	soft_max_doc_mem;
	size_t	reported_doc_mem_used;
	size_t	url_mem_used;
	size_t	max_img_mem;

	MMDocElm*		GetMMDocElm(FramesDocument* doc);
	MMDocElm*       GetLockedMMDocElm(FramesDocument* doc);

	char*			tmp_buf;
	char*			tmp_buf2;
	char*			tmp_buf2k;
	char*			tmp_buf_uni;

	int				doc_check_ursrc_count;
	time_t			urlrsrc_freed;

	OpLegacyMessagePool* legacy_message_pool;
#ifdef SELFTEST
	TestPool::PoolType* selftest_test_pool;
#endif // SELFTEST

	size_t ComputeSoftMaxDocMem(size_t hard_limit) { return static_cast<size_t>(hard_limit * ((double)MEM_MAN_DOCS_CACHE_SOFT_LIMIT_LEVEL / 100.0)); }

	int pseudo_persistent_elem_cnt;

	void Insert(Head *list, MMDocElm *elem)
	{
		if (elem->ShouldBeKeptInMemoryAsLongAsPossible())
			++pseudo_persistent_elem_cnt;

		elem->Into(list);
	}

	void Remove(MMDocElm *elem)
	{
		if (elem->ShouldBeKeptInMemoryAsLongAsPossible())
			--pseudo_persistent_elem_cnt;

		OP_ASSERT(pseudo_persistent_elem_cnt >= 0);

		elem->Out();
	}

	void RemoveLocked(MMDocElm *elem)
	{
		Remove(elem);
	}

	void Insert(MMDocElm *elem)
	{
		Insert(&doc_list, elem);
	}

	void InsertLocked(MMDocElm *elem)
	{
		Insert(&locked_doc_list, elem);
	}

#ifdef PREFS_AUTOMATIC_RAMCACHE
	/**
	 * Calculate cache sizes for doc and image caches, depending on amount
	 * of available memory and algorithm for calculating size.
	 *
	 * @param[in] available amount of memory to use for calculations, in MB
	 * @param[out] fig_cache_size suggested size of image cache, in kB
	 * @param[out] doc_cache_size suggested size of document cache, in kB
	 */
	void static GetRAMCacheSizes(UINT32 available_mem, size_t& fig_cache_size, size_t& doc_cache_size);

	/**
	 * Desktop algorithm for calculating RAM cache sizes.
	 */
	void static GetRAMCacheSizesDesktop(UINT32 available_mem, size_t& fig_cache_size, size_t& doc_cache_size);

#if (AUTOMATIC_RAM_CACHE_STRATEGY != 0)
	/**
	 * Smartphone algorithm for calculating RAM cache sizes.
	 */
	void static GetRAMCacheSizesSmartphone(UINT32 available_mem, size_t& fig_cache_size, size_t& doc_cache_size);
#endif
#endif // PREFS_AUTOMATIC_RAMCACHE

public:

					MemoryManager();
					~MemoryManager();

    void            InitL() { LEAVE_IF_ERROR(Init()); }
	OP_STATUS		Init();

	void*			GetTempBuf() { return (void*)tmp_buf; }
	/**< Returns a shared temporary buffer of size MEM_MAN_TMP_BUF_LEN bytes. */
	unsigned int	GetTempBufLen() { return MEM_MAN_TMP_BUF_LEN; }
	/**< Returns the size of the buffer returned by GetTempBuf() in bytes. */

	void*			GetTempBuf2() { return (void*)tmp_buf2; }
	/**< Returns a shared temporary buffer of size MEM_MAN_TMP_BUF_LEN bytes. */
	unsigned int	GetTempBuf2Len() { return MEM_MAN_TMP_BUF_LEN; }
	/**< Returns the size of the buffer returned by GetTempBuf2() in bytes. */

	void*			GetTempBuf2k() { return (void*)tmp_buf2k; }
	/**< Returns a shared temporary buffer of size MEM_MAN_TMP_BUF_2K_LEN bytes. */
	unsigned int	GetTempBuf2kLen() { return MEM_MAN_TMP_BUF_2K_LEN; }
	/**< Returns the size of the buffer returned by GetTempBuf2k() in bytes. */

	void*			GetTempBufUni() { return (void*)tmp_buf_uni; }
	/**< Returns a shared temporary buffer of size MEM_MAN_TMP_BUF_LEN bytes. */
	unsigned int	GetTempBufUniLenUniChar() { return UNICODE_DOWNSIZE(MEM_MAN_TMP_BUF_LEN); }
	/**< Returns the size of the buffer returned by GetTempBufUni() in uni_chars. */

	OpLegacyMessagePool* GetLegacyMessagePool() const { return legacy_message_pool; }
	/**< Returns the OpMemoryPool for the OpLegacyMessage. */

#ifdef SELFTEST
	TestPool::PoolType* GetSelftestPool() const { return selftest_test_pool; }
	/**< Returns the OpMemoryPool for the opmemorypool.ot selftest. */
#endif // SELFTEST

	uni_char*		CopyString(const uni_char* str);
	uni_char*		CopyStringN(const uni_char* str, unsigned int len);

	void			SetImgString(char** str_ptr, const char* new_val);

	void			IncDocMemory(size_t size, BOOL check = TRUE);
	void			DecDocMemory(size_t size);

	void			IncURLMemoryCount(unsigned int size) { url_mem_used += size; }
	void			DecURLMemoryCount(unsigned int size) { url_mem_used -= size; }

	size_t	DocMemoryUsed() const;

	static void		IncDocMemoryCount(size_t size, BOOL check = TRUE);
	static void		DecDocMemoryCount(size_t size);

	size_t	MaxDocMemory() const { return hard_max_doc_mem; }

	void			SetMaxImgMemory(size_t m);
	size_t	GetMaxImgMemory() { return max_img_mem; }
	void			SetMaxDocMemory(size_t m);

	/**
	 * Frees document cache from undisplayed documents.
	 *
	 * @param[in] m Amount of document memory to free.
	 * @param[in] force When this argument is FALSE (default), the memory of
	 *  some documents (e.g. documents with a tree that was changed by the user)
	 *  will not not be freed. Those documents are kept in memory as long as
	 *  possible. If the value is TRUE, then changed documents may be freed as
	 *  well if the specified amount of memory was not freed before reaching
	 *  them.
	 *
	 * @retval value indicating whether memory was freed or not
	 */
	BOOL			FreeDocMemory(size_t m, BOOL force = FALSE);

	/**
	 * Checks whether memory limits are exceeded and calls FreeDocMemory() if that's the case.
	 * There are 2 memory limits. Crossing soft limit will cause only non edited documents will be deleted.
	 * Crossing the Hard limit will cause memory will be freed until proper amount was freed or all 
	 * documents were freed no matter whether they were edited or not
	 */
	void			CheckDocMemorySize();

	OP_STATUS		UndisplayedDoc(FramesDocument* doc, BOOL keep);
	void			DisplayedDoc(FramesDocument* doc);

	void			RaiseCondition(OP_STATUS type);

	const MMDocElm*	GetDocumentCache() const { return (const MMDocElm*)doc_list.First(); }

#ifdef _OPERA_DEBUG_DOC_
	unsigned int				GetCachedDocumentCount();
#endif // _OPERA_DEBUG_DOC_

	/** Retrieve values for RAM cache and set
	 * memory manager settings accordingly.
	 */
	void ApplyRamCacheSettings();
};

#ifdef SELFTEST
/**
 * Helper function that exposes internal
 * MemoryManager::GetRAMCacheSizes(), used for testing purposes.
 *
 * @param available [in] amount of memory to use for calculations, in MB
 * @param fig_cache_size [out] suggested size of image cache, in kB
 * @param doc_cache_size [out] suggested size of document cache, in kB
 */
void TestGetRAMCacheSizes(UINT32 available_mem, size_t& fig_cache_size, size_t& doc_cache_size);
#endif

#endif // !MODULES_HARDCORE_MEM_MEM_MAN_H
