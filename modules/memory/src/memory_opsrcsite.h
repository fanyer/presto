/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef MEMORY_OPSRCSITE_H
#define MEMORY_OPSRCSITE_H

/** \file
 *
 * \brief Include file for OpSrcSite object
 *
 * The \c OpSrcSite class identifies a unique source code site,
 * typically obtained through \c __FILE__ and \c __LINE__ macros.
 */

#ifdef ENABLE_MEMORY_DEBUGGING

/**
 * \brief Identify a given source code location
 *
 * The OpSrcSite class identifies a unique source code location,
 * typically obtained through \c __FILE__ and \c __LINE__ macros.
 * The method \c GetSite() is the method to call to translate a
 * file/line pair into a unique OpSrcSite object.  Two identical
 * file/line pairs will yield pointers to the same OpSrcSite
 * object.  The OpSrcSite also has an ID for unique identification.
 *
 */
class OpSrcSite
{
public:
	/**
	 * \brief Create an OpSrcSite object with the given data
	 *
	 * This method is public for technical reasons, but it should
	 * not be called except by the class itself!  See \c GetSite()
	 * for info on how to find/create an \c OpSrcSite.
	 *
	 * \b NOTE: It is the responsibility of the caller to make sure
	 * this method is not called simultaneously on different threads.
	 * This means that the caller must use the same mutex for protection
	 * everywhere this method is called.
	 *
	 * \param file The filename of the file, full path possibly
	 * \param line The line number in the file
	 * \param id   The ID to be assigned to this (new) OpSrcSite
	 */
	OpSrcSite(const char* file, int line, int id);

	/**
	 * \brief Find a unique OpSrcSite by file and line-number
	 *
	 * Find and return a unique OpSrcSite object that identifies the
	 * file and line number pair.  There is a special OpSrcSite object
	 * returned in case of OOM: \c g_mem_opsrcsite_oom_site.
	 *
	 * \param file The filename of the file, full path possibly
	 * \param line The line number in the file
	 * \returns A pointer to an OpSrcSite object
	 */
	static OpSrcSite* GetSite(const char* file, int line);

	/**
	 * \brief Find a unique OpSrcSite by site-id
	 *
	 * Return the OpSrcSite with the given site-id.  The site-id must
	 * exist, or else the behaviour is undefined.
	 *
	 * \param siteid The site-id of the OpSrcSite object to locate
	 * \returns A pointer to an OpSrcSite object
	 */
	static OpSrcSite* GetSite(UINT32 siteid);

	/**
	 * \brief Get the filename of the source site
	 *
	 * If you have an OpSrcSite, this function will return a
	 * (pretty-printed) path-name of the source code file.
	 *
	 * \returns The filename of the soruce code file
	 */
	const char* GetFile(void) const;

	/**
	 * \brief Get the line number of the soruce site
	 *
	 * Same as \c GetFilename() but return the line number.
	 *
	 * \returns The line number of the soruce code file
	 */
	int GetLine(void) const { return line; }

	/**
	 * \brief Get the unique ID for the source site object
	 *
	 * All created \c OpSrcSite objects has a unique ID that
	 * can be used to identify the OpSrcSite.  This method
	 * returns this unique site source ID.
	 *
	 * \returns The unique site source ID for this \c OpSrcSite
	 */
	int GetId(void) const { return id; }

	/**
	 * \name Operator new/delete for this class to avoid recursive debugging
	 *
	 * Since \c OpSrcSite objects may be allocated during allocation
	 * operations, we make sure these objects don't call the regular
	 * (debugging) allocators, to avoid infinite recursion.
	 *
	 * @{
	 */
	void* operator new(size_t size) OP_NOTHROW { return op_lowlevel_malloc(size); }
	void* operator new[](size_t size) OP_NOTHROW { return op_lowlevel_malloc(size); }
	void operator delete(void* ptr) { op_lowlevel_free(ptr); }
	void operator delete[](void* ptr) { op_lowlevel_free(ptr); }
	/* @} */

#ifdef MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
	void IncMemUsed(size_t size) { OP_ASSERT(memused + size >= memused); memused += size; m_total_mem_used += size; }
	void DecMemUsed(size_t size) { OP_ASSERT(memused >= size); memused -= size; m_total_mem_used -= size; }
	size_t GetMemUsed(void) const { return memused; }
	static void PrintLiveAllocations(void);
	static int GetTotalMemUsed(void) { return m_total_mem_used; }
	static void ClearCallCounts();
#endif // MEMORY_LOG_USAGE_PER_ALLOCATION_SITE

#ifdef MEMORY_FAIL_ALLOCATION_ONCE
	bool HasFailed() { return hasfailed; }
	void SetHasFailed() { hasfailed = true; }
	int GetCallCount() { return callcount; }
	void SetCallCount( int n ) { callcount = n; }
	void IncCallCount() { callcount++; }
#endif // MEMORY_FAIL_ALLOCATION_ONCE

private:
	const char* file;
	OpSrcSite* next;
	INT32 line;
	UINT32 id;
#ifdef MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
	size_t memused;
#endif // MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
#ifdef MEMORY_FAIL_ALLOCATION_ONCE
	int callcount;
#endif // MEMORY_FAIL_ALLOCATION_ONCE
	static size_t m_total_mem_used;

#ifdef MEMORY_FAIL_ALLOCATION_ONCE
	bool hasfailed;
#endif // MEMORY_FAIL_ALLOCATION_ONCE
};

#endif // ENABLE_MEMORY_DEBUGGING

#endif // MEMORY_OPSRCSITE_H
