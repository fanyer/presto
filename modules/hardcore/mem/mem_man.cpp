/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/hardcore/mem/mem_man.h"

#include "modules/img/image.h"
#include "modules/url/url_man.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/display/FontAtt.h"

#include "modules/doc/frm_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/util/timecache.h"

#include "modules/util/str.h"

#include "modules/ecmascript/ecmascript.h"

#if (defined OPSYSTEMINFO_GETAVAILMEM || defined OPSYSTEMINFO_GETPHYSMEM) && defined PREFS_AUTOMATIC_RAMCACHE
#include "modules/pi/OpSystemInfo.h"
#endif

#define USE_HOST_ADDR_CACHE

#ifdef EPOC
#define MEM_MAN_CHECK_URSRC_SEC 2
#define MEM_MAN_CHECK_URSRC_INT 0
#else
#define MEM_MAN_CHECK_URSRC_SEC 20
#define MEM_MAN_CHECK_URSRC_INT 20
#endif

MemoryManager::MemoryManager()
	: hard_max_doc_mem(0)
	, soft_max_doc_mem(0)
	, reported_doc_mem_used(0)
	, url_mem_used(0)
	, max_img_mem(0)
	, tmp_buf(NULL)
	, tmp_buf2(NULL)
	, tmp_buf2k(NULL)
	, tmp_buf_uni(NULL)
	, doc_check_ursrc_count(0)
	, urlrsrc_freed(0)
	, legacy_message_pool(NULL)
#ifdef SELFTEST
	, selftest_test_pool(NULL)
#endif // SELFTEST
	, pseudo_persistent_elem_cnt(0)
{
}

OP_STATUS MemoryManager::Init()
{
	tmp_buf = OP_NEWA(char,MEM_MAN_TMP_BUF_LEN+UNICODE_SIZE(1));
	RETURN_OOM_IF_NULL(tmp_buf);
	tmp_buf2 = OP_NEWA(char,MEM_MAN_TMP_BUF_LEN+UNICODE_SIZE(1));
	RETURN_OOM_IF_NULL(tmp_buf2);
	tmp_buf2k = OP_NEWA(char,MEM_MAN_TMP_BUF_2K_LEN+UNICODE_SIZE(1));
	RETURN_OOM_IF_NULL(tmp_buf2k);
	tmp_buf_uni = OP_NEWA(char,MEM_MAN_TMP_BUF_LEN+UNICODE_SIZE(1));
	RETURN_OOM_IF_NULL(tmp_buf_uni);
	legacy_message_pool = OP_NEW(OpLegacyMessagePool, ());
	RETURN_OOM_IF_NULL(legacy_message_pool);
#ifdef SELFTEST
	selftest_test_pool = OP_NEW(TestPool::PoolType, ());
	RETURN_OOM_IF_NULL(selftest_test_pool);
#endif // SELFTEST

	return OpStatus::OK;
}

MemoryManager::~MemoryManager()
{
	MMDocElm* dle = (MMDocElm*) doc_list.First();
	while (dle)
	{
		Remove(dle);
		OP_DELETE(dle);
		dle = (MMDocElm*) doc_list.First();
	}

	OP_DELETEA(tmp_buf);
	OP_DELETEA(tmp_buf2);
	OP_DELETEA(tmp_buf2k);
	OP_DELETEA(tmp_buf_uni);

	OP_ASSERT(reported_doc_mem_used == 0);
	OP_ASSERT(url_mem_used == 0);

	OP_DELETE(legacy_message_pool);
	legacy_message_pool = NULL;
#ifdef SELFTEST
	OP_DELETE(selftest_test_pool);
	selftest_test_pool = NULL;
#endif // SELFTEST
}

void MemoryManager::IncDocMemory(size_t size, BOOL check)
{
	reported_doc_mem_used += size;
	if (check)
	{
		CheckDocMemorySize();
	}
}

void MemoryManager::DecDocMemory(size_t size)
{
	OP_ASSERT(size <= reported_doc_mem_used);

	reported_doc_mem_used -= size;
}
/* static */ void MemoryManager::IncDocMemoryCount(size_t size, BOOL check)
{
	if (g_memory_manager)
		g_memory_manager->IncDocMemory(size, check);
}

/* static */ void MemoryManager::DecDocMemoryCount(size_t size)
{
	if (g_memory_manager)
		g_memory_manager->DecDocMemory(size);
}

size_t MemoryManager::DocMemoryUsed() const
{
	size_t total_doc_mem_used = reported_doc_mem_used;
	if (g_ecmaManager)
		total_doc_mem_used += g_ecmaManager->GetCurrHeapSize();
	return total_doc_mem_used;
}

void MemoryManager::SetMaxImgMemory(size_t m)
{
	max_img_mem = m;

	if (
#ifdef PREFS_AUTOMATIC_RAMCACHE
		!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticRamCache) &&
#endif
		!g_pccore->GetIntegerPref(PrefsCollectionCore::RamCacheFigs))
	{
		max_img_mem = 0;
	}
	if (imgManager)
	{
		imgManager->SetCacheSize(max_img_mem, IMAGE_CACHE_POLICY_SOFT); // FIXME:IMG-KILSMO
	}
}

void MemoryManager::SetMaxDocMemory(size_t m)
{
	hard_max_doc_mem = m;
	soft_max_doc_mem = ComputeSoftMaxDocMem(hard_max_doc_mem);
	CheckDocMemorySize();
}

void MemoryManager::CheckDocMemorySize()
{
	size_t doc_mem_used = DocMemoryUsed();

	if (
#ifdef PREFS_AUTOMATIC_RAMCACHE
	    !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticRamCache) &&
#endif
	    !g_pccore->GetIntegerPref(PrefsCollectionCore::RamCacheDocs))
		FreeDocMemory(doc_mem_used, TRUE);
	else if ((soft_max_doc_mem < doc_mem_used) && (hard_max_doc_mem > doc_mem_used))
		FreeDocMemory(doc_mem_used - soft_max_doc_mem, FALSE);
	else if (hard_max_doc_mem < doc_mem_used)
		FreeDocMemory(doc_mem_used - hard_max_doc_mem, TRUE);
}

BOOL MemoryManager::FreeDocMemory(size_t m, BOOL force /*= FALSE*/)
{
	MMDocElm* dle;
	UINT num_docs;
	BOOL deleted = FALSE;
	time_t t;
	size_t doc_mem_used = DocMemoryUsed();
	size_t target_doc_mem_used = (doc_mem_used > m) ? doc_mem_used - m : 0;

	dle = (MMDocElm*) doc_list.First();
	num_docs = doc_list.Cardinal();

	/* Postpone deleting edited documents to be deleted as late as possible
	 * When force = TRUE we want to push them to the back but when fore = FALSE they refuse to be deleted
	 * so move them to the end in any case
	 */
	if (pseudo_persistent_elem_cnt > 0 && num_docs != (unsigned int) pseudo_persistent_elem_cnt)
	{
		MMDocElm* curr = dle;
		MMDocElm* next = curr;

		/* List may change as something migth be taken from some pos and be put at the end so loop proper number of times */
		for (unsigned int item_num = 0; item_num < num_docs; item_num++)
		{
			OP_ASSERT(curr); // Otherwise something is wrong with Cardinal()

			// Save the next element in case curr is moved to the end
			next = curr->Suc();

			/* This might slightly increase a chance, cache will be unloaded and documentedit
			 * will be still kept
			 */
			if (curr->ShouldBeKeptInMemoryAsLongAsPossible())
			{
				/* Move the documentedit at the end of the list */
				Remove(curr);
				Insert(curr);
			}

			curr = next;
		}
	}

	while (dle)
	{
		//If not force and document should be kept as long as possible it will refuse to be deleting - break here
		if (!force && dle->ShouldBeKeptInMemoryAsLongAsPossible())
			break;

		dle->Doc()->Free(TRUE, (force) ? FramesDocument::FREE_IMPORTANT : FramesDocument::FREE_NORMAL);

		doc_mem_used = DocMemoryUsed();
		if (doc_mem_used < target_doc_mem_used)
			goto finish;

		Remove(dle);  // element must be taken out of list before freeing document,
									// in case the free routine does calls to memory manager.

		if (dle->Doc()->Free(FALSE, (force) ? FramesDocument::FREE_IMPORTANT : FramesDocument::FREE_NORMAL))
		{
			OP_DELETE(dle);
			deleted = TRUE;
		}
		else
		{
			InsertLocked(dle);
		}

		g_ecmaManager->PurgeDestroyedHeaps();

		doc_mem_used = DocMemoryUsed();
		if (doc_mem_used < target_doc_mem_used)
			goto finish;

		dle = (MMDocElm*) doc_list.First();
	}

	t = g_timecache->CurrentTime();
	if (doc_check_ursrc_count++	> MEM_MAN_CHECK_URSRC_INT && t - urlrsrc_freed > MEM_MAN_CHECK_URSRC_SEC)
	{
    	doc_check_ursrc_count = 0;
		urlrsrc_freed = t;
		if (urlManager)
			urlManager->FreeUnusedResources();
	}

finish:

	dle = (MMDocElm*) locked_doc_list.First();

	while (dle)
	{
		RemoveLocked(dle);
		Insert(dle);
		dle = (MMDocElm*) locked_doc_list.First();
	}

#ifdef AGGRESSIVE_ECMASCRIPT_GC
	// Garbage collect javascript objects
	if (deleted)
	{
		g_ecmaManager->GarbageCollect();
	}
#endif // AGGRESSIVE_ECMASCRIPT_GC

	return deleted;
}

MMDocElm* MemoryManager::GetMMDocElm(FramesDocument* doc)
{
	for (MMDocElm* dle = (MMDocElm*) doc_list.First(); dle; dle = dle->Suc())
		if (dle->Doc() == doc)
			return dle;

	return 0;
}

MMDocElm* MemoryManager::GetLockedMMDocElm(FramesDocument* doc)
{
	for (MMDocElm* dle = (MMDocElm*) locked_doc_list.First(); dle; dle = dle->Suc())
		if (dle->Doc() == doc)
			return dle;

	return 0;
}

void MemoryManager::DisplayedDoc(FramesDocument* doc)
{
	MMDocElm* dle = GetMMDocElm(doc);
	if (dle)
	{
		Remove(dle);
		OP_DELETE(dle);
	}

	/* Is it possible the doc is on both lists? 
	 * If not why this does not return after deleting proper DocElm? 
	 */

	// Remove from temporary locked list as well
	dle = GetLockedMMDocElm(doc);
	if (dle)
	{
		Remove(dle);
		OP_DELETE(dle);
	}
}

OP_STATUS MemoryManager::UndisplayedDoc(FramesDocument* doc, BOOL keep_as_long_as_possible)
{
	MMDocElm* dle = GetMMDocElm(doc);
	if (dle)
	{
		Remove(dle);
		dle->SetKeepInMemory(keep_as_long_as_possible);		
		Insert(dle);
	}
	else
	{
		dle = OP_NEW(MMDocElm,(doc, keep_as_long_as_possible));

		if (dle == NULL)
		{
			// We are out of memory.
			return OpStatus::ERR_NO_MEMORY;
		}

		Insert(dle);
	}

	return OpStatus::OK;
}

#ifdef _OPERA_DEBUG_DOC_
unsigned int MemoryManager::GetCachedDocumentCount()
{
	return doc_list.Cardinal() + locked_doc_list.Cardinal();
}
#endif // _OPERA_DEBUG_DOC_


void MemoryManager::RaiseCondition( OP_STATUS type )
{
	if (type == OpStatus::ERR_NO_MEMORY)
		g_main_message_handler->PostOOMCondition(TRUE);
	else if (type == OpStatus::ERR_SOFT_NO_MEMORY)
		g_main_message_handler->PostOOMCondition(FALSE);
	else if (type == OpStatus::ERR_NO_DISK)
		g_main_message_handler->PostOODCondition();
	else
		OP_ASSERT(0);
}

#ifdef PREFS_AUTOMATIC_RAMCACHE
void MemoryManager::GetRAMCacheSizesDesktop(UINT32 available_mem, size_t& fig_cache_size, size_t& doc_cache_size)
{
	// Try to take up to 10% of system memory for images and 15% for documents (including scripts).
	fig_cache_size = (available_mem * 1024) / 10;
	doc_cache_size = (available_mem * 1024) / 7;
}

#if (AUTOMATIC_RAM_CACHE_STRATEGY != 0)
void MemoryManager::GetRAMCacheSizesSmartphone(UINT32 available_mem, size_t& fig_cache_size, size_t& doc_cache_size)
{
	const UINT32 EMERGENCY_LOW_THRESHOLD = 16;
	const UINT32 LOW_THRESHOLD = 32;
	const UINT32 MED_THRESHOLD = 128;
	const size_t EMERGENCY_LOW_DOC = 200;
	const size_t EMERGENCY_LOW_FIG = 2000;
	const int DOC_PERCENTAGE = 8;
	const int FIG_PERCENTAGE = 12;

	OP_ASSERT(EMERGENCY_LOW_THRESHOLD < LOW_THRESHOLD);
	OP_ASSERT(LOW_THRESHOLD < MED_THRESHOLD);
	OP_ASSERT(EMERGENCY_LOW_DOC <= LOW_THRESHOLD * 1024 * DOC_PERCENTAGE / 100);
	OP_ASSERT(EMERGENCY_LOW_FIG <= LOW_THRESHOLD * 1024 * FIG_PERCENTAGE / 100);
	OP_ASSERT(DOC_PERCENTAGE > 0);
	OP_ASSERT(FIG_PERCENTAGE > 0);

	if (available_mem < 3)
	{
		// "too low to handle"
		OP_ASSERT(!"You shouldn't try to run with this little memory!");
		fig_cache_size = 0;
		doc_cache_size = 0;
	}
	else if (available_mem < EMERGENCY_LOW_THRESHOLD)
	{
		// "emergency low"
		fig_cache_size = EMERGENCY_LOW_FIG;
		doc_cache_size = EMERGENCY_LOW_DOC;
	}
	else if (available_mem < LOW_THRESHOLD)
	{
		// Increase from values for < 16, to values for 32
		UINT32 doc_upper = LOW_THRESHOLD * 1024 * DOC_PERCENTAGE / 100;
		UINT32 doc_lower = EMERGENCY_LOW_DOC;
		UINT32 fig_upper = LOW_THRESHOLD * 1024 * FIG_PERCENTAGE / 100;
		UINT32 fig_lower = EMERGENCY_LOW_FIG;
		doc_cache_size = (available_mem - EMERGENCY_LOW_THRESHOLD) * (doc_upper - doc_lower) / (LOW_THRESHOLD - EMERGENCY_LOW_THRESHOLD) + doc_lower;
		fig_cache_size = (available_mem - EMERGENCY_LOW_THRESHOLD) * (fig_upper - fig_lower) / (LOW_THRESHOLD - EMERGENCY_LOW_THRESHOLD) + fig_lower;
	}
	else if (available_mem < MED_THRESHOLD)
	{
		// Give a bit more to image cache
		doc_cache_size = available_mem * 1024 * DOC_PERCENTAGE / 100;
		fig_cache_size = available_mem * 1024 * FIG_PERCENTAGE / 100;
	}
	else
	{
		GetRAMCacheSizesDesktop(available_mem, fig_cache_size, doc_cache_size);
	}
}
#endif // (AUTOMATIC_RAM_CACHE_STRATEGY != 0)

void MemoryManager::GetRAMCacheSizes(UINT32 available_mem, size_t& fig_cache_size, size_t& doc_cache_size)
{
#if (AUTOMATIC_RAM_CACHE_STRATEGY == 0)
	GetRAMCacheSizesDesktop(available_mem, fig_cache_size, doc_cache_size);
#else
	GetRAMCacheSizesSmartphone(available_mem, fig_cache_size, doc_cache_size);
#endif
}
#endif // PREFS_AUTOMATIC_RAMCACHE

void MemoryManager::ApplyRamCacheSettings()
{
	size_t fig_cache_size = g_pccore->GetIntegerPref(PrefsCollectionCore::ImageRAMCacheSize);
	size_t doc_cache_size = g_pccore->GetIntegerPref(PrefsCollectionCore::DocumentCacheSize);

#ifdef PREFS_AUTOMATIC_RAMCACHE
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticRamCache))
	{
		INT32 mem_MB = -1;
#if defined(OPSYSTEMINFO_GETAVAILMEM) && (AUTOMATIC_RAM_CACHE_STRATEGY == 1)
		UINT32 available_physical_MB;
		if (OpStatus::IsSuccess(g_op_system_info->GetAvailablePhysicalMemorySizeMB(&available_physical_MB)))
		{
			mem_MB = (UINT32)available_physical_MB;
		}
		else
#endif // OPSYSTEMINFO_GETAVAILMEM && (AUTOMATIC_RAM_CACHE_STRATEGY == 1)
		{
#ifdef OPSYSTEMINFO_GETPHYSMEM
			mem_MB = g_op_system_info->GetPhysicalMemorySizeMB();
#endif // OPSYSTEMINFO_GETPHYSMEM
		}

		// For OOM we would like to use lowest levels, but "0" can also mean unsupported
		if (mem_MB > 0)
		{
			GetRAMCacheSizes(mem_MB, fig_cache_size, doc_cache_size);
		}

		// Cache should be at least MIN_CACHE_RAM_FIGS_SIZE / MIN_CACHE_RAM_SIZE
		fig_cache_size = MAX(fig_cache_size, (size_t) MIN_CACHE_RAM_FIGS_SIZE);
		doc_cache_size = MAX(doc_cache_size, (size_t) MIN_CACHE_RAM_SIZE);

		// Cache should be at most MAX_CACHE_RAM_FIGS_SIZE / MAX_CACHE_RAM_SIZE
		fig_cache_size = MIN(fig_cache_size, (size_t) MAX_CACHE_RAM_FIGS_SIZE);
		doc_cache_size = MIN(doc_cache_size, (size_t) MAX_CACHE_RAM_SIZE);
	}
#endif // PREFS_AUTOMATIC_RAMCACHE

	g_memory_manager->SetMaxImgMemory(fig_cache_size * 1024);
	g_memory_manager->SetMaxDocMemory(doc_cache_size * 1024);
}

#if defined(SELFTEST) && defined(PREFS_AUTOMATIC_RAMCACHE)
void TestGetRAMCacheSizes(UINT32 available_mem, size_t& fig_cache_size, size_t& doc_cache_size)
{
	MemoryManager::GetRAMCacheSizes(available_mem, fig_cache_size, doc_cache_size);
}
#endif // SELFTEST && PREFS_AUTOMATIC_RAMCACHE

#ifdef SELFTEST
OP_USE_MEMORY_POOL_IMPL(g_memory_manager->GetSelftestPool(), TestPool::TestPoolElement);
#endif // SELFTEST
