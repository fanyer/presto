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

// Url_cm.cpp

// URL Cookie Management

#include "core/pch.h"

#include "modules/pi/OpSystemInfo.h"
#include "modules/url/url_man.h"
#include "modules/formats/url_dfr.h"
#include "modules/cookies/url_cm.h"
#include "modules/cookies/cookie_common.h"
#include "modules/datastream/opdatastreamsafefile.h"
#include "modules/util/opfile/opmemfile.h"

#ifdef WIC_COOKIE_MANAGER_LISTENER
#include "modules/windowcommander/OpCookieManager.h"
#endif


#ifdef DISK_COOKIES_SUPPORT
void Cookie_Manager::AutoSaveCookies()
{
	if(updated_cookies)
	{
		TRAPD(op_err, WriteCookiesL());
		if(OpStatus::IsMemoryError(op_err)) // Only leave if it was a memory error
			g_memory_manager->RaiseCondition(op_err); 
		updated_cookies = FALSE;
	}
}

#ifdef WIC_COOKIE_MANAGER_LISTENER
/** Convenience class to call listener when WriteCookiesL exits (it may leave) */
class CookieWriteAutoNotify
{
public:
	CookieWriteAutoNotify(BOOL requested_by_platform) : m_success(TRUE), m_requested_by_platform(requested_by_platform) {}

	~CookieWriteAutoNotify()
	{
		if (g_cookie_API)
			g_cookie_API->GetCookieManagerListener()->OnCookieFileSaveFinished(m_success, m_requested_by_platform);
	}

	void SetSuccess() { m_success = TRUE; }

private:
	BOOL m_success;
	BOOL m_requested_by_platform;
};
#endif // WIC_COOKIE_MANAGER_LISTENER

void Cookie_Manager::WriteCookiesL(BOOL requested_by_platform)
{
	CookieContextItem * OP_MEMORY_VAR item = (CookieContextItem *) ContextManagers.First();
#ifdef WIC_COOKIE_MANAGER_LISTENER
	CookieWriteAutoNotify notifier(requested_by_platform);
	ANCHOR(CookieWriteAutoNotify, notifier);
#endif // WIC_COOKIE_MANAGER_LISTENER
	while(item)
	{
		TRAPD(op_err, item->context->WriteCookiesL());
		OpStatus::Ignore(op_err);

		item = (CookieContextItem *) item->Suc();
	}

	if(!cookie_file_read)
	{
#ifdef WIC_COOKIE_MANAGER_LISTENER
		notifier.SetSuccess();
#endif
		return;
	}

#if !(defined EXTERNAL_COOKIE_STORAGE)
	if(cookie_file_location == OPFILE_ABSOLUTE_FOLDER)
		return;
#endif // !EXTERNAL_COOKIE_STORAGE

	DataStream_NonSensitive_ByteArrayL(write_cookies);

#ifdef _DEBUG_COOKIES_
			FILE* fp = fopen("c:\\klient\\dcookie.txt", "a");
			if (fp)
			{
				cookie_root->DebugWriteCookies(fp);
				fclose(fp);
			}
#endif

	time_t this_time;
	this_time = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);

	OP_STATUS op_err = OpStatus::OK;

#if defined EXTERNAL_COOKIE_STORAGE
	OpMemFile* cfp = OP_NEW_L(OpMemFile, ());
	cfp->Open(OPFILE_WRITE);
#else

	OpSafeFile* cfp = OP_NEW(OpDataStreamSafeFile, ());
	ANCHOR_PTR(OpSafeFile, cfp);

	if (cfp)
	{
		op_err = cfp->Construct(CookiesFile, cookie_file_location);
	}
	if (!cfp || OpStatus::IsError(op_err) || OpStatus::IsError(op_err = cfp->Open(OPFILE_WRITE)))
	{
		if(!cfp || OpStatus::IsMemoryError(op_err))
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);

		ANCHOR_PTR_RELEASE(cfp);
		OP_DELETE(cfp);
		return;
	}
#endif

	DataFile c4fp(cfp, COOKIES_FILE_VERSION, 1, 2);

	c4fp.InitL();

#if (COOKIES_MAX_FILE_SIZE > 0) || (CONTEXT_COOKIES_MAX_FILE_SIZE > 0)
	unsigned int cookie_file_max_size = 0;

	if (context_id != 0)
		cookie_file_max_size = CONTEXT_COOKIES_MAX_FILE_SIZE;
	else
		cookie_file_max_size = COOKIES_MAX_FILE_SIZE;

	if (cookie_file_max_size > 0)
	{
		// size of data file header : 
		// (uint32 file_version_number; uint32 app_version_number; uint16 idtag_length; uint16 length_length;)
		const int file_header_size = 4+4+2+2;
		// dry run for estimation of file size
		int file_size = cookie_root->WriteCookiesL(c4fp, this_time, TRUE);
		// removing least recently used cookies until size of file exceeds defined maximum
		while (file_size + file_header_size > cookie_file_max_size)
		{
			Cookie* ck = cookie_root->GetLeastRecentlyUsed(this_time, this_time);
			if (ck)
			{
				if(ck->Persistent(this_time))
				{
					DataFile_Record rec(TAG_COOKIE_ENTRY);
					ANCHOR(DataFile_Record,rec);

					rec.SetRecordSpec(c4fp.GetRecordSpec());

					ck->FillDataFileRecordL(rec);

					size_t cookie_size = rec.CalculateLength();
					file_size -= cookie_size;
				}

				ck->Out();
				delete ck;
			}
			else
			{
				// there aren't any meaning all is protected
				break;
			}
		}
	}
#endif //(COOKIES_MAX_FILE_SIZE > 0) || (CONTEXT_COOKIES_MAX_FILE_SIZE > 0)

	cookie_root->WriteCookiesL(c4fp, this_time);

#if defined EXTERNAL_COOKIE_STORAGE
	if(c4fp.Opened()) // file is closed and deleted on error
	{
		OpFileLength len = cfp->GetFileLength();
		unsigned char* buf = OP_NEWA_L(unsigned char, len);
		ANCHOR_ARRAY(unsigned char, buf);
		cfp->SetFilePos(0);
		OpFileLength read;
		LEAVE_IF_ERROR(cfp->Read(buf, len, &read));
		if (read != len)
			LEAVE(OpStatus::ERR); // Unable to read everything at once?
		extern void WriteExternalCookies(URL_CONTEXT_ID, const unsigned char*, int);
		WriteExternalCookies(context_id, buf, len);
	}
#endif
	ANCHOR_PTR_RELEASE(cfp);
	c4fp.Close();
#ifdef WIC_COOKIE_MANAGER_LISTENER
	notifier.SetSuccess();
#endif
}
#endif // DISK_COOKIES_SUPPORT

