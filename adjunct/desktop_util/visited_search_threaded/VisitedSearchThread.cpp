/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Patricia Aas (psmaas)
 */

#include "core/pch.h"
#include "adjunct/desktop_util/visited_search_threaded/VisitedSearchThread.h"
#include "modules/pi/OpThreadTools.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

#ifdef VISITED_PAGES_SEARCH

template <typename T> class LockIterator :
	public SearchIterator<T>
{
public:
	LockIterator(DesktopMutex& mutex, SearchIterator<T> *it) : m_lock(mutex), m_iterator(it) {}

	virtual ~LockIterator() { OP_DELETE(m_iterator); }

	virtual BOOL Next(void) { return m_iterator->Next(); }

	virtual BOOL Prev(void) { return m_iterator->Prev(); }

	virtual OP_STATUS Error(void) const { return m_iterator->Error(); }

	virtual int Count(void) const { return m_iterator->Count(); }

	virtual BOOL End(void) const { return m_iterator->End(); }

	virtual BOOL Beginning(void) const { return m_iterator->Beginning(); }

	virtual const T &Get() { return m_iterator->Get(); }

private:
	DesktopMutexLock  m_lock;
	SearchIterator<T> *m_iterator;
};


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
AsyncVisitedSearch * AsyncVisitedSearch::Create()
{
	return OP_NEW(VisitedSearchThread, ());
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
VisitedSearchThread::VisitedSearchThread()
  : m_is_open(FALSE)
  , m_next_free_handle(reinterpret_cast<VisitedSearch::RecordHandle>(1)) // 'fake' or proxy handle for dealing with the outside world, see CreateRecord()
  , m_initialized(FALSE)
{
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::SetMaxSize(OpFileLength max_size)
{
    DesktopMutexLock mutex(m_search_mutex);

    m_search.SetMaxSize(max_size);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::SetMaxItems(int max_items)
{
    DesktopMutexLock mutex(m_search_mutex);

    m_search.SetMaxItems(max_items);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS VisitedSearchThread::Open(const uni_char *directory)
{
    // Open the directory
    RETURN_IF_ERROR(m_search.Open(directory));
    m_is_open = TRUE;

	return StartThread();
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS VisitedSearchThread::StartThread()
{
	OP_PROFILE_METHOD("Started visited search thread");

	if (!m_initialized)
	{
		// Initialize mutex
		RETURN_IF_ERROR(m_search_mutex.Init());
	
		// Initialize thread
		RETURN_IF_ERROR(DesktopThread::Init());

		m_initialized = TRUE;
	}

	// Set callbacks
	OpMessage msg[] = {MSG_VISITEDSEARCH_PREFLUSH, MSG_VISITEDSEARCH_FLUSH, MSG_VISITEDSEARCH_COMMIT};
	RETURN_IF_ERROR(g_main_message_handler->SetCallBackList(this, (MH_PARAM_1)&m_search, msg, 3));

	// Start the thread
	return Start();
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS VisitedSearchThread::Close(BOOL force_close)
{
	OP_PROFILE_METHOD("Stopped visited search thread");

	// Stop the thread
	StopThread();

	// Close the search
	RETURN_IF_ERROR(m_search.Close(force_close));
	m_is_open = FALSE;

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::StopThread()
{
	 // Stop the thread
	OpStatus::Ignore(Stop());

	// Unset callbacks
	g_main_message_handler->UnsetCallBacks(this);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
BOOL VisitedSearchThread::IsOpen()
{
    return m_is_open;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS VisitedSearchThread::Clear(BOOL reopen)
{
	if (!IsOpen())
		return OpStatus::OK;

	// Stop the thread
	StopThread();

	// Do the clearing
	RETURN_IF_ERROR(m_search.Clear(reopen));

	m_is_open = reopen;
	if (!m_is_open)
		return OpStatus::OK;

	// If reopened, restart thread
	return StartThread();
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
VisitedSearch::RecordHandle VisitedSearchThread::CreateRecord(const char *url,
															  const uni_char *title)
{
	OpAutoPtr<NewRecord> new_record (OP_NEW(NewRecord, (m_next_free_handle, url, title)));

    if(!new_record.get())
		return 0;

    RETURN_VALUE_IF_ERROR(PostMessage(MSG_VPS_CREATE_RECORD, reinterpret_cast<MH_PARAM_1>(new_record.get()), 0), 0);
	new_record.release();

	// m_next_free_handle is the unique handle by which this record will be
	// known to the caller. An actual handle for this record will be created
	// when MSG_VPS_CREATE_RECORD is received in the thread, and the mapping
	// will be saved for later reference.
	return m_next_free_handle++;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS VisitedSearchThread::AddTitle(VisitedSearch::RecordHandle handle,
										const uni_char *title)
{
	OpAutoPtr<OpString> title_copy (OP_NEW(OpString, ()));

	if(!title_copy.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(title_copy->Set(title));
    RETURN_IF_ERROR(PostMessage(MSG_VPS_ADD_TITLE, (MH_PARAM_1) handle, (MH_PARAM_2) title_copy.get()));
	title_copy.release();

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS VisitedSearchThread::AddTextBlock(VisitedSearch::RecordHandle handle,
					    const uni_char *text,
					    float ranking,
						BOOL is_continuation)
{
    OpAutoPtr<TextRecord> text_record (OP_NEW(TextRecord, (handle, text, ranking, is_continuation)));

    if(!text_record.get())
		return OpStatus::ERR_NO_MEMORY;

    RETURN_IF_ERROR(PostMessage(MSG_VPS_ADD_TEXT_BLOCK, (MH_PARAM_1) text_record.get(), 0));
	text_record.release();

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS VisitedSearchThread::CloseRecord(VisitedSearch::RecordHandle handle)
{
    return PostMessage(MSG_VPS_CLOSE_RECORD, (MH_PARAM_1) handle, 0);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::AbortRecord(VisitedSearch::RecordHandle handle)
{
    PostMessage(MSG_VPS_ABORT_RECORD, (MH_PARAM_1) handle, 0);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
SearchIterator<VisitedSearch::Result>* VisitedSearchThread::Search(const uni_char *text,
																   VisitedSearch::Sort sort_by,
																   int flags)
{
	DesktopMutexLock lock(m_search_mutex);

	SearchIterator<VisitedSearch::Result>* it = m_search.Search(text, sort_by);
	if (!it)
		return 0;

	LockIterator<VisitedSearch::Result>* lock_iterator = OP_NEW(LockIterator<VisitedSearch::Result>, (m_search_mutex, it));
	if (!lock_iterator)
		OP_DELETE(it);

	return lock_iterator;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS VisitedSearchThread::Search(const uni_char *text,
									  VisitedSearch::Sort sort_by,
									  int max_items,
									  int excerpt_max_chars,
									  const OpStringC & excerpt_start_tag,
									  const OpStringC & excerpt_end_tag,
									  int excerpt_prefix_ratio,
									  MessageObject * callback)
{
	if(!IsOpen())
	{
		return g_thread_tools->PostMessageToMainThread(MSG_VPS_SEARCH_RESULT,
										  (MH_PARAM_1) callback,
										  (MH_PARAM_2) 0);
	}

    OpAutoPtr<VisitedSearch::SearchSpec> search_spec(OP_NEW(VisitedSearch::SearchSpec, ()));

    if(!search_spec.get())
		return OpStatus::ERR_NO_MEMORY;

    RETURN_IF_ERROR(search_spec->query.Set(text));
    RETURN_IF_ERROR(search_spec->start_tag.Set(excerpt_start_tag.CStr()));
    RETURN_IF_ERROR(search_spec->end_tag.Set(excerpt_end_tag.CStr()));

    search_spec->sort_by      = sort_by;
	search_spec->max_items    = max_items;
    search_spec->max_chars    = excerpt_max_chars;
    search_spec->prefix_ratio = excerpt_prefix_ratio;

	RETURN_IF_ERROR(AddSearch(callback));

	OP_STATUS status = PostMessage(MSG_VPS_SEARCH, (MH_PARAM_1) search_spec.release(), (MH_PARAM_2) callback);

    if(OpStatus::IsError(status))
		OpStatus::Ignore(RemoveSearch(callback));

    return status;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS VisitedSearchThread::InvalidateUrl(const char *url)
{
	if(!IsOpen())
		return OpStatus::OK;

    char * url_copy = strdup(url);
    return PostMessage(MSG_VPS_INVALIDATE_URL, (MH_PARAM_1) url_copy, 0);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS VisitedSearchThread::InvalidateUrl(const uni_char *url)
{
    OpString8 utf8_str;
    RETURN_IF_ERROR(utf8_str.SetUTF8FromUTF16(url));
    return InvalidateUrl(utf8_str.CStr());
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	// Forwarding the message to the thread

	if(&m_search == (MessageObject *) par1)
		OpStatus::Ignore(PostMessage(msg, par1, par2));
}

// --------------------------- PRIVATE ---------------------------------------------

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS VisitedSearchThread::AddSearch(MessageObject * callback)
{
	DesktopMutexLock mutex(m_table_mutex);

	SearchCounter * search_counter = NULL;

	if(OpStatus::IsError(m_current_searches.GetData(callback, &search_counter)))
	{
		search_counter = OP_NEW(SearchCounter, ());
		if(!search_counter)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<SearchCounter> anchor(search_counter);

		RETURN_IF_ERROR(m_current_searches.Add(callback, search_counter));

		anchor.release();
	}

	search_counter->counter++;

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS VisitedSearchThread::RemoveSearch(MessageObject * callback)
{
	DesktopMutexLock mutex(m_table_mutex);

	SearchCounter * search_counter = NULL;

	RETURN_IF_ERROR(m_current_searches.GetData(callback, &search_counter));

	OP_ASSERT(search_counter);

	if(search_counter == NULL)
		return OpStatus::ERR;

	search_counter->counter--;

	if(search_counter->counter == 0)
	{
		RETURN_IF_ERROR(m_current_searches.Remove(callback, &search_counter));
		OP_DELETE(search_counter);
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
BOOL VisitedSearchThread::ContinueSearch(MessageObject * callback)
{
	DesktopMutexLock mutex(m_table_mutex);

	SearchCounter * search_counter = NULL;

	RETURN_IF_ERROR(m_current_searches.GetData(callback, &search_counter));

	OP_ASSERT(search_counter);

	if(search_counter == NULL)
		return TRUE;

	return search_counter->counter == 1;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::HandleMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
    DesktopMutexLock lock(m_search_mutex);

    switch(msg)
    {
	case MSG_VPS_CREATE_RECORD:
	{
		HandleCreateRecord((NewRecord *) par1);
		break;
	}
    case MSG_VPS_ADD_TITLE:
    {
		HandleAddTitle((VisitedSearch::RecordHandle) par1, (OpString *) par2);
		break;
    }
    case MSG_VPS_ADD_TEXT_BLOCK:
    {
		HandleAddTextBlock((TextRecord *) par1);
		break;
    }
    case MSG_VPS_CLOSE_RECORD:
    {
		HandleCloseRecord((VisitedSearch::RecordHandle) par1);
		break;
    }
    case MSG_VPS_ABORT_RECORD:
    {
		HandleAbortRecord((VisitedSearch::RecordHandle) par1);
		break;
    }
    case MSG_VPS_SEARCH:
    {
		HandleSearch((VisitedSearch::SearchSpec *) par1, (MessageObject *) par2);
		RemoveSearch((MessageObject *) par2);
		break;
    }
    case MSG_VPS_INVALIDATE_URL:
    {
		HandleInvalidateUrl((char *) par1);
		break;
    }
	case MSG_VISITEDSEARCH_PREFLUSH:
	case MSG_VISITEDSEARCH_FLUSH:
	case MSG_VISITEDSEARCH_COMMIT:
	{
		// Forwarding the message to the object in the thread
		m_search.HandleCallback(msg, par1, par2);
		break;
	}
    default:
    {
		OP_ASSERT(!"Unknown message received!");
		return;
    }
    }
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::HandleCreateRecord(NewRecord * new_record)
{
	// Create the record and get a 'real' handle for m_search
	VisitedSearch::RecordHandle real_handle = m_search.CreateRecord(new_record->url.CStr(), new_record->title.CStr());

	// Save the handle mapping
	if (real_handle)
		OpStatus::Ignore(m_record_map.Add(new_record->handle, real_handle));

	OP_DELETE(new_record);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::HandleAddTitle(VisitedSearch::RecordHandle handle,
										 OpString * title)
{
	VisitedSearch::RecordHandle real_handle = GetRealHandle(handle);

	if (real_handle)
    	OpStatus::Ignore(m_search.AddTitle(real_handle, title->CStr()));

    OP_DELETE(title);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::HandleAddTextBlock(TextRecord * text_record)
{
	VisitedSearch::RecordHandle real_handle = GetRealHandle(text_record->handle);

	if (real_handle)
    	OpStatus::Ignore(m_search.AddTextBlock(real_handle, text_record->text.CStr(), text_record->ranking, text_record->is_continuation));

    OP_DELETE(text_record);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::HandleCloseRecord(VisitedSearch::RecordHandle handle)
{
	VisitedSearch::RecordHandle real_handle = GetRealHandle(handle);

	if (real_handle)
		OpStatus::Ignore(m_search.CloseRecord(real_handle));

	OpStatus::Ignore(m_record_map.Remove(handle, &real_handle));
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::HandleAbortRecord(VisitedSearch::RecordHandle handle)
{
	VisitedSearch::RecordHandle real_handle = GetRealHandle(handle);

	if (real_handle)
		m_search.AbortRecord(real_handle);

	OpStatus::Ignore(m_record_map.Remove(handle, &real_handle));
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::HandleSearch(VisitedSearch::SearchSpec * search_spec, MessageObject * callback)
{
    OpAutoPtr< VisitedSearch::SearchSpec > search_spec_keeper(search_spec);

	if(!ContinueSearch(callback))
		return;

#if 0
    OpAutoPtr< SearchIterator< VisitedSearch::Result > > result(m_search.Search(search_spec->query,
																				search_spec->sort_by));
#else
    OpAutoPtr< SearchIterator< VisitedSearch::Result > > result(m_search.FastPrefixSearch(search_spec->query.CStr()));
#endif

    if(!result.get())
		return;

    if(OpStatus::IsError(result->Error()))
		return;

    OpAutoPtr< OpAutoVector<VisitedSearch::SearchResult> > items(OP_NEW(OpAutoVector<VisitedSearch::SearchResult>, ()));

    if(!items.get())
		return;

    if(result->Empty())
	{
		OpStatus::Ignore(g_thread_tools->PostMessageToMainThread(MSG_VPS_SEARCH_RESULT,
													(MH_PARAM_1) callback,
													(MH_PARAM_2) items.release()));
		return;
	}

	int max = search_spec->max_items;
	int count = 0;

    do
    {
		if(!ContinueSearch(callback))
			return;

		OpAutoPtr<VisitedSearch::SearchResult> item(OP_NEW(VisitedSearch::SearchResult, ()));

		if(!item.get())
			return;

		RETURN_VOID_IF_ERROR(item->CopyFrom(result->Get(), search_spec));
		RETURN_VOID_IF_ERROR(items->Add(item.get()));

		item.release();
		count++;

    } while (result->Next() && (max == KAll || count < max));

    OpStatus::Ignore(g_thread_tools->PostMessageToMainThread(MSG_VPS_SEARCH_RESULT,
												(MH_PARAM_1) callback,
												(MH_PARAM_2) items.release()));
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void VisitedSearchThread::HandleInvalidateUrl(char *url)
{
    OpStatus::Ignore(m_search.InvalidateUrl(url));
    free(url);
}

/***********************************************************************************
 ** Map a handle given to the outside world to a real, internal search_engine
 ** record handle
 **
 ** @return A search_engine record handle, or 0 if it couldn't be found
 ***********************************************************************************/
VisitedSearch::RecordHandle VisitedSearchThread::GetRealHandle(VisitedSearch::RecordHandle handle)
{
	VisitedSearch::RecordHandle real_handle = 0;

	if (OpStatus::IsSuccess(m_record_map.GetData(handle, &real_handle)))
		return real_handle;

	return 0;
}


#endif  // VISITED_PAGES_SEARCH
