/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DEVICEAPI_JIL_JILFILEFINDER_H
#define DEVICEAPI_JIL_JILFILEFINDER_H

#include "modules/util/simset.h"
#include "modules/hardcore/timer/optimer.h"

class OpFileInfo;

/**
 * Listener class notified about find files operation results
 */
class FilesFoundListener
{
public:
	/**
	 * Notification called when find files operation is finished successfully
	 *
	 * @param file_file<IN> list of found files
	 *
	 * NOTE caller remains the owner of file_list array
	 */
	virtual void OnFilesFound(OpVector<OpString> *file_list) = 0;

	/**
	 * Notification called when some error occured during find files operation
	 *
	 * @param error - error code
	 *
	 */
	virtual void OnError(OP_STATUS error) = 0;
};

/**
 * Class representing single file system entity like file or directory
 */
class FolderEntry : public ListElement<FolderEntry>
{
protected:
	OpString m_path;
	List<FolderEntry> *m_entries;
	FolderEntry *m_parent;

public:
	/**
	 * Type of the entry
	 */
	enum FolderEntryType
	{
		FILE,
		DIR
	};

	/**
	 * State of enumerate operation
	 */
	enum EnumerationState
	{
		ENUMERATION_ERROR = -2, // Some generic error occured
		ENUMERATION_NO_MEMORY_ERROR, // OOM error occured
		ENUMERATION_CONTINUE, // Enumeration of this entry is finished and may be proceeded for next entries
		ENUMERATION_YIELD, // Enumeration operation must be broken immediately. Will continue later
		ENUMERATION_FINISHED // Enumeration of all entries requested to be enumerated was finished
	};

	virtual ~FolderEntry()
	{
		if (m_entries)
			m_entries->Clear();
		OP_DELETE(m_entries);
	}

	FolderEntry()
	: m_entries(NULL)
	, m_parent(NULL)
	{}

	/**
	 * Returns type of this entry
	 */
	virtual FolderEntryType IsA() = 0;

	/**
	 * Gets full path of this entry
	 */
	const OpString &GetFullPath() const { return m_path; }

	/**
	 * Gets this entry's parent
	 */
	FolderEntry *GetParent() const { return m_parent; }

	/**
	 * Enters this entry. It MUST be called before calling Enumerate(). It might be used for some preparations to before Enumerate()
	 *
	 * @param<IN> from - entry this entity was entered from
	 *
	 * @return OK if OK. ERR_NO_MEMORY otherwise
	 */
	virtual OP_STATUS Enter(FolderEntry *from)
	{
		m_parent = from;
		m_entries = OP_NEW(List<FolderEntry>, ());
		RETURN_OOM_IF_NULL(m_entries);
		return OpStatus::OK;
	}

	/**
	 * Enumerates content of this entry
	 *
	 * @param result<IN> - array results of the enumeration are written to
	 * @param pattern<IN> - enumeration pattern
	 * @param case_sensitive<IN> - whether enumeration must take case into account
	 * @param current<OUT> - set to current entry. Should be reused when enumeration continues after break cause by EnumerationState::YIELD
	 * @param max_files<IN> - max number of entries which may be enumerated without yielding
	 * @param recursive<IN> - whether enumeration should be recursive
	 * @param info<IN> - OpFileInfo enumerated entries are compared to
	 *
	 * @return EnumerationState
	 * @see EnumerationState
	 */
	virtual EnumerationState Enumerate(OpVector<OpString> *result, const OpString &pattern, BOOL case_sensitive, FolderEntry *&current, UINT32 max_files, BOOL recursive, OpFileInfo *info = NULL) = 0;
	virtual OP_STATUS Construct(const uni_char *path)
	{
		return this->m_path.Set(path);
	}
};

/**
 * File finder class
 */
class JILFileFinder : public OpTimerListener, public ListElement<JILFileFinder>
{
private:
	OpFileInfo *m_find_files_info;
	FilesFoundListener *m_listener;

	OpVector<OpString> *m_files_found;
	OpString m_path;
	OpString m_pattern;
	OpTimer m_timer;
	FolderEntry *m_start_entry;
	FolderEntry *m_curr_entry;
	BOOL m_recursive;
	BOOL m_case_sensitive;
	UINT32 m_max_files;
	UINT32 m_yield_time;

	/**
	 * Continue paused find operation
	 */
	void Continue();
	void OnFinished()
	{
		m_listener->OnFilesFound(m_files_found);
	}

	void OnError(OP_STATUS status)
	{
		m_listener->OnError(status);
	}

	void Reset()
	{
		m_timer.Stop();
		if (m_files_found)
		{
			m_files_found->DeleteAll();
			OP_DELETE(m_files_found);
			m_files_found = NULL;
		}
		OP_DELETE(m_start_entry);
		m_start_entry = NULL;
	}

public:
	JILFileFinder();
	virtual ~JILFileFinder()
	{
		Reset();
	}

	void OnTimeOut(OpTimer *)
	{
		Continue();
	}

	/**
	 * Finds files fullfilling all given requirements. It calls FilesFoundListener::OnFilesFound with results when finished
	 *
	 * @param path<IN> - path where files should be searched for
	 * @param pattern<IN> - find pattern
	 * @param listener<IN> - instance of FilesFoundListener
	 * @param recursive<IN> - whether search should be recursive
	 * @param info<IN> - OpFileInfo found files are compared to
	 *
	 * NOTE: This is yielding operation. Continue is called to resume from point it was stopped
	 * If next Find() is called before previous find operation is finished - previous operation is cancelled
	 */
	virtual void Find(const uni_char *path, const uni_char *pattern, FilesFoundListener *listener, BOOL recursive, OpFileInfo *find_files_info = NULL);

	/** Sets whether search is case sensitive (it's not case sensitive by default) */
	void SetCaseSensitive(BOOL sensitive) { m_case_sensitive = sensitive; }

	/** Sets max number of files which may be found without yielding */
	void SetMaxFiles(UINT32 max_files) { m_max_files = max_files; }

	/** Sets yield time */
	void SetYieldTime(UINT32 t) { m_yield_time = t; }
};

#endif // DEVICEAPI_JIL_JILFILEFINDER_H
