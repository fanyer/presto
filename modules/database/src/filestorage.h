/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/


#ifndef _MODULES_DATABASE_FILESTORAGE_H
#define _MODULES_DATABASE_FILESTORAGE_H

#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND

#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/util/opfile/opsafefile.h"

class MessageHandler;
class WebStorageValueInfo;
class WebStorageValueInfoTable;

class FileStorageLoadingCallback
{
public:
	/**
	 * This callback function is called when a key/value pair is found.
	 * Callee takes over responsability for the object regardless of what happens.
	 */
	virtual OP_STATUS OnValueFound(WebStorageValueInfo*) = 0;

	virtual void		OnLoadingFinished() = 0;

	/**
	 * Possible values passed:
	 * PS_Status::ERR_CORRUPTED_FILE;
	 * OpStatus::ERR_NO_MEMORY;
	 */
	virtual void		OnLoadingFailed(OP_STATUS) = 0;
};

class FileStorageSavingCallback
{
public:
	virtual void		OnSavingFinished() = 0;
	virtual void		OnSavingFailed(OP_STATUS) = 0;
};

class FileStorageLoader : public XMLParser::Listener, public XMLTokenHandler
{
private:
	enum FslState
	{
		FSL_STATE_IN_ENTRIES,
		FSL_STATE_IN_ENTRY,
		FSL_STATE_IN_KEY,
		FSL_STATE_IN_VALUE,
		FSL_STATE_ROOT,
		FSL_STATE_FAILED
	};

	MessageHandler*             m_mh;
	XMLParser*                  m_parser;
	WebStorageValueInfo*        m_current_value;
	FileStorageLoadingCallback* m_callback;

	FslState m_state;
	BOOL     m_caller_notified;

	void		SetState(FslState new_state);
	void		Finish(OP_STATUS err_val);

public:
	FileStorageLoader(FileStorageLoadingCallback* callback)
		: m_mh(NULL),
		  m_parser(NULL),
		  m_current_value(NULL),
		  m_callback(callback),
		  m_state(FSL_STATE_ROOT),
		  m_caller_notified(FALSE) {};

	~FileStorageLoader();

	OP_STATUS		Load(const uni_char* file_path);

	// Implementation of the XMLParser::Listener interface
	virtual void	Continue(XMLParser *parser);
	virtual void	Stopped(XMLParser *parser);

	// Implementation of the XMLTokenHandler interface
	virtual XMLTokenHandler::Result
					HandleToken(XMLToken &token);
	virtual void	ParsingStopped(XMLParser *parser);
};

class FileStorageSaver
#ifdef WEBSTORAGE_SIMPLE_BACKEND_ASYNC
	: public OpFileListener
#else // WEBSTORAGE_SIMPLE_BACKEND_ASYNC
	: public MessageObject
#endif // WEBSTORAGE_SIMPLE_BACKEND_ASYNC
{
private:
	enum FssState
	{
		FSS_STATE_PRE_START,
		FSS_STATE_BEFORE_KEY,
		FSS_STATE_KEY_VALUE,
		FSS_STATE_AFTER_KEY,
		FSS_STATE_VALUE_VALUE,
		FSS_STATE_AFTER_VALUE,
		FSS_STATE_AFTER_ENTRIES,
		FSS_STATE_FINISHED,
		FSS_STATE_FAILED
	};

	char*				m_buffer;
	unsigned			m_used_len;
	unsigned			m_data_offset;
	unsigned			m_buffer_len;
	char*				m_base64_buffer;
	int					m_base64_len;
#ifdef DEBUG_ENABLE_OPASSERT
	unsigned			m_commit_count;
#endif // DEBUG_ENABLE_OPASSERT
	FssState			m_state;
	BOOL				m_caller_notified;
	BOOL				m_synchronous;
	MessageHandler*		m_mh;
	OpSafeFile			m_out_file;
	const OpVector<WebStorageValueInfo>* m_vector;
	unsigned			m_vector_index;
	FileStorageSavingCallback* m_callback;

	void		SetState(FssState new_state);
	BOOL		Append(const char *str, unsigned str_len, BOOL apply_base64);
	void		Commit();
	void		WriteNextBlock();
	void		Finish(OP_STATUS err_val);

public:
	FileStorageSaver(MessageHandler *mh, FileStorageSavingCallback* callback, BOOL synchronous)
		: m_buffer(NULL),
		  m_used_len(0),
		  m_data_offset(0),
		  m_buffer_len(0),
		  m_base64_buffer(NULL),
		  m_base64_len(0),
#ifdef DEBUG_ENABLE_OPASSERT
		  m_commit_count(0),
#endif // DEBUG_ENABLE_OPASSERT
		  m_state(FSS_STATE_PRE_START),
		  m_caller_notified(FALSE),
		  m_synchronous(synchronous),
		  m_mh(mh),
		  m_vector(NULL),
		  m_vector_index(0),
		  m_callback(callback) {};
	~FileStorageSaver();

	void Clean();

	OP_STATUS Save(const uni_char* file_path, const OpVector<WebStorageValueInfo>* vector);

	// The OpFileListener interface
	virtual void	OnDataWritten(OpFile* file, OP_STATUS result, OpFileLength len);

	/**
	 * Synchronously writes all remaining data to disk
	 */
	void		Flush();

	inline BOOL IsStateFinished() const { return m_state == FSS_STATE_FINISHED || m_state == FSS_STATE_FAILED; }
	inline BOOL HasCompleted() const { return m_caller_notified; }

#ifndef WEBSTORAGE_SIMPLE_BACKEND_ASYNC
	// The MessageObject interface
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
#endif // WEBSTORAGE_SIMPLE_BACKEND_ASYNC
};

#endif // WEBSTORAGE_ENABLE_SIMPLE_BACKEND

#endif // _MODULES_DATABASE_FILESTORAGE_H
