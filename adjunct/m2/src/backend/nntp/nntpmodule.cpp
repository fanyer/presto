/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/backend/nntp/nntpmodule.h"

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/store.h"

#include "adjunct/m2/src/backend/nntp/nntp-protocol.h"
#include "adjunct/m2/src/backend/nntp/nntprange.h"

#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/qp.h"
#include "adjunct/m2/src/util/misc.h"

#include "adjunct/desktop_util/prefs/PrefsUtils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/m2_ui/dialogs/AskMaxMessagesDownloadDialog.h"
#include "adjunct/desktop_util/string/stringutils.h"

#include "modules/locale/oplanguagemanager.h"

#include <assert.h>
#include <time.h>

#define DEFAULT_CHARSET_STR "iso-8859-1"
#define NEWSRC_HASH_SIZE    256
#define RFC850_DATE         423273600L
#define DEFAULT_MAX_DOWNLOAD 250

// ----------------------------------------------------

OP_STATUS NewsRCItem::GetUnicodeName(OpString& name, NntpBackend* backend) const
{
    name.Empty();
    if (newsgroup.IsEmpty())
        return OpStatus::OK;

    OP_STATUS ret;
	if ((ret=name.SetFromUTF8(newsgroup.CStr())) != OpStatus::OK)
        return ret;

	if (name.HasContent() && uni_strchr(name.CStr(), 0xfffd)!=NULL)
	{
		OpString8 override_charset;
		Account* account_ptr = backend ? backend->GetAccountPtr() : NULL;
		if (account_ptr && account_ptr->GetCharset(override_charset)==OpStatus::OK)
		{
			OpStatus::Ignore(MessageEngine::GetInstance()->GetInputConverter().ConvertToChar16(override_charset, newsgroup, name));
			backend->AlertNewsgroupEncodingOverride();
		}
	}

    if (uni_strchr(name.CStr(), '?') != NULL)
    {
        OpString8 downconverted_name;
        if ((ret=downconverted_name.Set(name.CStr())) != OpStatus::OK)
            return ret;

        BOOL warning, error;
        return OpQP::Decode(downconverted_name, name, DEFAULT_CHARSET_STR, warning, error);
    }

    return OpStatus::OK;
}

// ----------------------------------------------------

NntpBackend::NntpBackend(MessageDatabase& database)
: ProtocolBackend(database),
  m_connections_ptr(NULL),
  m_open_connections(0),
  m_max_connection_count(4),
  m_current_connections_count(0),
  m_accept_new_connections(TRUE),
  m_command_list(NULL),
  m_subscribed_list(NULL),
  m_last_updated(0),
  m_last_update_request(0),
  m_newsrc_file_changed(FALSE),
  m_checked_newgroups(FALSE),
  m_download_count(0),
  m_max_download(DEFAULT_MAX_DOWNLOAD),
  m_ask_for_max_download(TRUE),
  m_signal_new_message(FALSE),
  m_send_queued_when_done(FALSE),
  m_has_alerted_newsgroup_encoding_override(FALSE),
  m_in_session(FALSE),
  m_report_to_ui_list(NULL)
{
}

NntpBackend::~NntpBackend()
{
    if (m_report_to_ui_list)
    {
        StopFolderLoading();
    }

    if (m_subscribed_list)
    {
        WriteRCFile(m_subscribed_list);
        m_subscribed_list->Clear(); //The list _should_ be cleared in the above call, but better safe than sorry
        OP_DELETE(m_subscribed_list);
    }

#if defined NNTP_LOG_QUEUE
    Log("Before NntpBackend::~NntpBackend", "");
    LogQueue();
#endif

    if (m_command_list)
    {
        m_command_list->Clear();
        OP_DELETE(m_command_list);
    }

    if (m_connections_ptr)
    {
	    int i;
        for (i=0; i<4; i++)
		{
			if (m_connections_ptr[i])
			{
				OP_DELETE(m_connections_ptr[i]);
				m_current_connections_count--;
			}
		}
    }

    OP_DELETEA(m_connections_ptr);
}

OP_STATUS NntpBackend::Init(Account* account)
{
    if (!account)
        return OpStatus::ERR_NULL_POINTER;

    m_account = account;

    OP_STATUS ret;
    if ((ret=ReadSettings()) != OpStatus::OK)
        return ret;

    if (!m_subscribed_list || m_newsrc_file_changed)
    {
        if (m_subscribed_list)
        {
            m_subscribed_list->Clear();
            OP_DELETE(m_subscribed_list);
        }
        m_subscribed_list = OP_NEW(Head, ());
        if (!m_subscribed_list)
            return OpStatus::ERR_NO_MEMORY;

        if ((ret=ReadRCFile(m_subscribed_list, TRUE, 0)) != OpStatus::OK)
            return ret;
    }

    if (!m_command_list)
        m_command_list = OP_NEW(Head, ());

    if (!m_command_list)
        return OpStatus::ERR_NO_MEMORY;

    m_accept_new_connections = TRUE;
    return OpStatus::OK;
}

OP_STATUS NntpBackend::ReadSettings()
{
    OpString filename;
    OP_STATUS ret = GetOptionsFile(filename);
    if (ret != OpStatus::OK)
        return ret;

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    PrefsFile* prefs_file = glue_factory->CreatePrefsFile(filename);
    if (!prefs_file)
        return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, "settings", "last_updated", RFC850_DATE, m_last_updated));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, "settings", "newsrc_file", m_newsrc_file));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, "settings", "max_download", DEFAULT_MAX_DOWNLOAD, m_max_download));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, "settings", "ask_for_max_download", TRUE, m_ask_for_max_download));

	glue_factory->DeletePrefsFile(prefs_file);
	RETURN_IF_ERROR(ret);

	// if file doesn't exist, recreate path to allow for easy moving of mail folders
	if (m_newsrc_file.HasContent())
	{
		OpFile* file = glue_factory->CreateOpFile(m_newsrc_file);
		if (!file)
			return OpStatus::ERR_NO_MEMORY;

		BOOL file_exists;
		ret = file->Exists(file_exists);
		glue_factory->DeleteOpFile(file);
		if (ret==OpStatus::OK && !file_exists)
		{
			m_newsrc_file.Empty();
		}
	}
	if (m_newsrc_file.IsEmpty())
		RETURN_IF_ERROR(CreateNewsrcFileName());

    return OpStatus::OK;
}

OP_STATUS NntpBackend::WriteSettings()
{
    OpString filename;
    OP_STATUS ret = GetOptionsFile(filename);
    if (ret != OpStatus::OK)
        return ret;

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    PrefsFile* prefs_file = glue_factory->CreatePrefsFile(filename);
    if (!prefs_file)
        return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, "settings", "last_updated", m_last_updated));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, "settings", "newsrc_file", m_newsrc_file));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, "settings", "max_download", m_max_download));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, "settings", "ask_for_max_download", m_ask_for_max_download));

	TRAP(ret, prefs_file->CommitL());

	glue_factory->DeletePrefsFile(prefs_file);

	return ret;
}

OP_STATUS NntpBackend::CreateNewsrcFileName()
{
	if (m_newsrc_file.IsEmpty())
	{
        OpString servername;
		RETURN_IF_ERROR(GetServername(servername));
		RETURN_IF_ERROR(MailFiles::GetNewsrcFileName(servername, m_newsrc_file));
		return WriteSettings();
	}
	return OpStatus::OK;
}

OP_STATUS NntpBackend::ReadRCFile(OUT Head* hashed_list, BOOL only_subscribed, UINT16 hash_size)
{
    if (!hashed_list)
        return OpStatus::ERR_NULL_POINTER;

    if (hash_size==0)
    {
        hashed_list->Clear();
    }
    else
    {
        UINT16 i;
        for (i=0; i<hash_size; i++)
            hashed_list[i].Clear();
    }

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    OpFile* file = glue_factory->CreateOpFile(m_newsrc_file);
    if (!file)
        return OpStatus::ERR_NO_MEMORY;

    BOOL file_exists;
	OP_STATUS ret = file->Exists(file_exists);
    if (ret==OpStatus::OK && !file_exists)
    {
		glue_factory->DeleteOpFile(file);
        return OpStatus::OK; //'File not found' is not an error
    }

    if ((ret=file->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE)) != OpStatus::OK)
    {
		glue_factory->DeleteOpFile(file);
        return ret;
    }

    OpString8 parse_buffer;
    char*     parse_ptr = NULL;
    NewsRCItem* newsrc_item;
    BOOL done = FALSE;
    while (!done)
    {
        newsrc_item = OP_NEW(NewsRCItem, ());
        if (!newsrc_item)
        {
			glue_factory->DeleteOpFile(file);
            return OpStatus::ERR_NO_MEMORY;
        }

        if ((ret=ParseNextNewsRCItem(file, only_subscribed, parse_buffer, parse_ptr, newsrc_item)) != OpStatus::OK)
        {
            OP_DELETE(newsrc_item);
			glue_factory->DeleteOpFile(file);
            return ret;
        }

        if ((done = newsrc_item->newsgroup.IsEmpty()) == FALSE)
        {
            if (hash_size==0)
            {
                newsrc_item->Into(hashed_list);
            }
            else
            {
                UINT16 hash = CalculateHash(newsrc_item->newsgroup, hash_size);
                newsrc_item->Into(&hashed_list[hash]);
            }
        }
        else
        {
            OP_DELETE(newsrc_item); //This item wasn't used
        }
    }
	glue_factory->DeleteOpFile(file);

    /* Note: This code will not catch new groups that has been subscribed. When subscribing to a group,
    ** the .newsrc-file should (or even better: must) be saved to disk immediately */
    if (!only_subscribed) //Add new newsgroups (that hasn't been saved yet) to list
    {
        NewsRCItem* newsrc_item = (NewsRCItem*)(m_subscribed_list->First());
        NewsRCItem* exists_item;
        UINT16    hash = 0;
        while (newsrc_item)
        {
            if (newsrc_item->status == NewsRCItem::NEW_NEWSGROUP)
            {
                if (hash_size==0)
                {
                    exists_item = (NewsRCItem*)(hashed_list->First());
                }
                else
                {
                    hash = CalculateHash(newsrc_item->newsgroup, hash_size);
                    exists_item = (NewsRCItem*)(hashed_list[hash].First());
                }

                while (exists_item && exists_item->newsgroup.Compare(newsrc_item->newsgroup)!=0)
                {
                    exists_item = (NewsRCItem*)(exists_item->Suc());
                }

                if (!exists_item)
                {
                    NewsRCItem* new_item = OP_NEW(NewsRCItem, ());
                    if ( ((ret=new_item->newsgroup.Set(newsrc_item->newsgroup)) != OpStatus::OK) ||
                         ((ret=new_item->read_range.Set(newsrc_item->read_range)) != OpStatus::OK) ||
                         ((ret=new_item->kept_range.Set(newsrc_item->kept_range)) != OpStatus::OK) )
                    {
                        OP_DELETE(new_item);
                        return ret;
                    }
                    new_item->status = newsrc_item->status;
                    if (hash_size==0)
                    {
                        new_item->Into(hashed_list);
                    }
                    else
                    {
                        new_item->Into(&hashed_list[hash]);
                    }
                }
            }

            newsrc_item = (NewsRCItem*)(newsrc_item->Suc());
        }
    }

    return OpStatus::OK;
}

OP_STATUS NntpBackend::WriteRCFile(IN Head* list)
{
    if (!m_newsrc_file_changed || list->Empty()) //No need to rewrite if nothing is changed
        return OpStatus::OK;

    Head* all_groups_hashed = OP_NEWA(Head, NEWSRC_HASH_SIZE);
    if (!all_groups_hashed)
        return OpStatus::ERR_NO_MEMORY;

    OP_STATUS ret;
    //Read in the entire file
    if ((ret=ReadRCFile(all_groups_hashed, FALSE, NEWSRC_HASH_SIZE)) != OpStatus::OK)
    {
        FreeRCHashedList(all_groups_hashed, NEWSRC_HASH_SIZE);
        return ret;
    }

    //Update items
    NewsRCItem* tmp_item = (NewsRCItem*)(list->First());
    NewsRCItem* next_tmp_item;
    NewsRCItem* rc_item;
    NNTPRange merged_range;
    UINT16 hash;
    while (tmp_item)
    {
        next_tmp_item = (NewsRCItem*)(tmp_item->Suc());
        hash = CalculateHash(tmp_item->newsgroup, (UINT16)NEWSRC_HASH_SIZE);

        //Note: To avoid duplicates, we will have to search for items marked as NEW_NEWSGROUP, too
        rc_item = (NewsRCItem*)(all_groups_hashed[hash].First());
        while (rc_item && rc_item->newsgroup.Compare(tmp_item->newsgroup)!=0)
        {
            rc_item = (NewsRCItem*)(rc_item->Suc());
        }

        if (!rc_item) //tmp_item is not found in old .newsrc-file, so we add it to the list
        {
            tmp_item->Out();
            tmp_item->Into(&all_groups_hashed[hash]);
        }
        else
        {
            if ( ((ret=merged_range.SetReadRange(tmp_item->read_range)) != OpStatus::OK) ||
                 ((ret=merged_range.AddRange(rc_item->read_range)) != OpStatus::OK) ||
                 ((ret=merged_range.GetReadRange(rc_item->read_range)) != OpStatus::OK) ||
                 ((ret=merged_range.SetReadRange(tmp_item->kept_range)) != OpStatus::OK) ||
                 ((ret=merged_range.AddRange(rc_item->kept_range)) != OpStatus::OK) ||
                 ((ret=merged_range.GetReadRange(rc_item->kept_range)) != OpStatus::OK) )
            {
                FreeRCHashedList(all_groups_hashed, NEWSRC_HASH_SIZE);
                return ret;
            }

            if (tmp_item->status != NewsRCItem::NEW_NEWSGROUP)
                rc_item->status = tmp_item->status;

            tmp_item->Out();
            OP_DELETE(tmp_item);
        }

        tmp_item = next_tmp_item;
    }

    //Generate tmp-filename
    OpString tmp_filename;
    if ( ((ret=tmp_filename.Set(m_newsrc_file)) != OpStatus::OK) ||
         ((ret=tmp_filename.Append(".tmp")) != OpStatus::OK) )
    {
        FreeRCHashedList(all_groups_hashed, NEWSRC_HASH_SIZE);
        return ret;
    }

    //Create tmp-file
	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    OpFile* tmp_file = glue_factory->CreateOpFile(tmp_filename);
    if (!tmp_file)
    {
        FreeRCHashedList(all_groups_hashed, NEWSRC_HASH_SIZE);
        return OpStatus::ERR_NO_MEMORY;
    }

    if ((ret=tmp_file->Open(OPFILE_WRITE|OPFILE_SHAREDENYREAD|OPFILE_SHAREDENYWRITE))!=OpStatus::OK)
    {
        FreeRCHashedList(all_groups_hashed, NEWSRC_HASH_SIZE);
		glue_factory->DeleteOpFile(tmp_file);
        return OpStatus::ERR_NO_DISK;
    }

    //Write tmp-file
    UINT16 i;
    for (i=0; i<NEWSRC_HASH_SIZE; i++)
    {
        tmp_item = (NewsRCItem*)(all_groups_hashed[i].First());
        BOOL unvisited_group;
        while (tmp_item)
        {
            unvisited_group = tmp_item->read_range.IsEmpty();

            if ( tmp_file->Write(tmp_item->newsgroup.CStr(), tmp_item->newsgroup.Length())!=OpStatus::OK ||
				tmp_file->Write((tmp_item->status==NewsRCItem::SUBSCRIBED ? ":" : "!"), 1)!=OpStatus::OK ||
				(!unvisited_group && tmp_file->Write(" ", 1)!=OpStatus::OK) ||
				(!tmp_item->read_range.IsEmpty() && tmp_file->Write(tmp_item->read_range.CStr(), tmp_item->read_range.Length())!=OpStatus::OK) ||
				(!tmp_item->kept_range.IsEmpty() && tmp_file->Write("/", 1)!=OpStatus::OK) ||
				(!tmp_item->kept_range.IsEmpty() && tmp_file->Write(tmp_item->kept_range.CStr(), tmp_item->kept_range.Length())!=OpStatus::OK) ||
				tmp_file->Write("\r\n", 2)!=OpStatus::OK )
            {
                FreeRCHashedList(all_groups_hashed, NEWSRC_HASH_SIZE);
				glue_factory->DeleteOpFile(tmp_file);
                return OpStatus::ERR;
            }
            tmp_item = (NewsRCItem*)(tmp_item->Suc());
        }
    }
    FreeRCHashedList(all_groups_hashed, NEWSRC_HASH_SIZE);
    if ((ret=tmp_file->Flush())!=OpStatus::OK ||
        (ret=tmp_file->Close())!=OpStatus::OK)
    {
		glue_factory->DeleteOpFile(tmp_file);
        return ret;
    }

    if ((ret=WriteSettings()) != OpStatus::OK) //Store settings. Last_updated might have been changed.
    {
		glue_factory->DeleteOpFile(tmp_file);
        return ret;
    }

    //Replace original file
    OpFile* rc_file = glue_factory->CreateOpFile(m_newsrc_file);
    if (!rc_file)
    {
		glue_factory->DeleteOpFile(tmp_file);
        return OpStatus::ERR_NO_MEMORY;
    }

    ret = rc_file->SafeReplace(tmp_file);

	glue_factory->DeleteOpFile(tmp_file);
	glue_factory->DeleteOpFile(rc_file);

    m_newsrc_file_changed = FALSE; //newsrc-file is now up to date.

    return ret;
}

void NntpBackend::FreeRCHashedList(Head* list, UINT16 hash_size) const
{
    if (list)
    {
        if (hash_size>0)
        {
            for (; hash_size>0; hash_size--)
                list[hash_size-1].Clear();

            OP_DELETEA(list);
        }
        else
        {
            list->Clear();
            OP_DELETE(list);
        }
    }
}

OP_STATUS NntpBackend::ParseNextNewsRCItem(OpFile* file, BOOL only_subscribed, OpString8& parse_buffer, char*& parse_ptr, NewsRCItem* newsrc_item) const
{
    if (!file || !newsrc_item)
        return OpStatus::ERR_NULL_POINTER;

    OP_STATUS ret;
    char*   subscribed_char;
    char*   keep_char;
    char*   new_line;
    char*   crlf;
    UINT8 subscribed;

    BOOL done = FALSE;
    while (!done)
    {
        if (!parse_ptr || !*parse_ptr) //No more data to parse. Read more data from file
        {
            if ((done=file->Eof()) == FALSE) //When the end of a buffer is at the end of a file, this test is needed
            {
                OpString8 read_buffer;
                if (!read_buffer.Reserve(8*1024+1)) //Is 8Kb a reasonable buffer?
                    return OpStatus::ERR_NO_MEMORY;

				OpFileLength bytes_read = 0;
                if ((ret=file->Read(read_buffer.CStr(), read_buffer.Capacity()-1, &bytes_read)) != OpStatus::OK)
                {
                    OP_ASSERT(0);
                    return ret;
                }
                *((char*)(read_buffer.CStr()+bytes_read)) = 0; //Need to manually terminate the string
                done = file->Eof();

                if (parse_ptr!= NULL && (ret=parse_buffer.Set(parse_ptr)) != OpStatus::OK) //Keep unparsed part of buffer
                    return ret;

                if ( ((ret=parse_buffer.Append(read_buffer)) != OpStatus::OK) ||
                     (done && ((ret=parse_buffer.Append("\r\n")) != OpStatus::OK)) )
                {
                    return ret;
                }

                parse_ptr = parse_buffer.CStr();
            }
        }

        while (parse_ptr && (*parse_ptr=='\r' || *parse_ptr=='\n')) //Skip any leading linefeeds
            parse_ptr++;

        while (parse_ptr && *parse_ptr)
        {
            new_line = strchr(parse_ptr, '\r'); //Find the first CR or LF
            crlf = strchr(parse_ptr, '\n');
            if (!new_line || (new_line && crlf && crlf<new_line))
                new_line = crlf;

            if (!new_line) //We have parsed all lines in buffer. Break out of inner loop to read more data from file
            {
                if ((ret=parse_buffer.Set(parse_ptr)) != OpStatus::OK)
                    return ret;

                parse_ptr = NULL;
                break;
            }

            subscribed_char = strchr(parse_ptr, ':');
            if (new_line && new_line<subscribed_char) //Is the subscribed char belonging to this line?
                subscribed_char = NULL;

            subscribed = (subscribed_char!=NULL);
            if (!subscribed_char)
            {
                if (only_subscribed)
                {
                    parse_ptr = new_line;
                    while (parse_ptr && (*parse_ptr=='\r' || *parse_ptr=='\n'))
                        parse_ptr++;

                    continue; //Skip non-subscribed groups
                }

                subscribed_char = strchr(parse_ptr, '!'); //This is actually 'unsubscribed_char'...
                if (new_line && new_line<subscribed_char)
                    subscribed_char = NULL;
            }

            newsrc_item->status = subscribed ? NewsRCItem::SUBSCRIBED : NewsRCItem::NOT_SUBSCRIBED;
            if (!subscribed_char)
            {
                if ((ret=newsrc_item->newsgroup.Set(parse_ptr, new_line ? new_line-parse_ptr : (int)KAll)) != OpStatus::OK)
                    return ret;
            }
            else
            {
                if ((ret=newsrc_item->newsgroup.Set(parse_ptr, subscribed_char-parse_ptr)) != OpStatus::OK)
                    return ret;

                ++subscribed_char; //Skip subscribed_char (':' or '!')
                while (*subscribed_char==' ') subscribed_char++; //Skip whitespace

                keep_char = strchr(subscribed_char, '/');
                if (new_line && new_line<keep_char) //Is the keep char belonging to this line?
                    keep_char = NULL;

                if (!keep_char)
                {
                    if ((ret=newsrc_item->read_range.Set(subscribed_char, new_line ? new_line-subscribed_char : (int)KAll)) != OpStatus::OK)
                        return ret;
                }
                else
                {
                    int subscribed_length = keep_char-subscribed_char;

                    ++keep_char; //Skip keep_char ('/')
                    while (*keep_char==' ') keep_char++; //Skip whitespace

                    if ( ((ret=newsrc_item->read_range.Set(subscribed_char, subscribed_length)) != OpStatus::OK) ||
                        ((ret=newsrc_item->kept_range.Set(keep_char, new_line ? new_line-keep_char : (int)KAll)) != OpStatus::OK) )
                    {
                        return ret;
                    }
                }
            }

            parse_ptr = new_line;
            while (parse_ptr && (*parse_ptr=='\r' || *parse_ptr=='\n'))
                parse_ptr++;

            if (!parse_ptr || !*parse_ptr)
			{
                parse_buffer.Empty(); //We have parsed exactly the whole string
				parse_ptr = NULL;
			}

            return OpStatus::OK;
        }
    }
    newsrc_item->newsgroup.Empty(); //End of file. Return an empty (invalid) newsgroup-name
    return OpStatus::OK;
}

OP_STATUS NntpBackend::AnyNewsgroup(OpString8& newsgroup)
{
    if (m_subscribed_list && m_subscribed_list->First())
    {
        return newsgroup.Set( ((NewsRCItem*)m_subscribed_list->First())->newsgroup );
    }

    return newsgroup.Set("junk");
}

NewsRCItem* NntpBackend::FindNewsgroupItem(const OpStringC8& newsgroup, BOOL allow_filesearch) const
{
	OP_ASSERT(newsgroup.HasContent());
	if (newsgroup.IsEmpty())
		return NULL;

    NewsRCItem* newsgroup_item = m_subscribed_list ? (NewsRCItem*)(m_subscribed_list->First()) : NULL;
    while (newsgroup_item)
    {
        if (newsgroup_item->newsgroup.Compare(newsgroup)==0)
            break;

        newsgroup_item = (NewsRCItem*)(newsgroup_item->Suc());
    }

    //If not found in memory, search in the .newsrc-file (if this kind of search is allowed)
    if (!newsgroup_item && allow_filesearch)
    {
        OpString8 tmp_newsgroup, dummy_range;
        if (tmp_newsgroup.Set(newsgroup) != OpStatus::OK) //FindNewsgroupRange isn't const by design
            return NULL;

		OP_ASSERT(tmp_newsgroup.HasContent());
		BOOL dummy_flag;
        FindNewsgroupRange(tmp_newsgroup, dummy_range, dummy_flag); //This will find the range, and put the newsgroup in the m_subscribed_list if not already there
        return FindNewsgroupItem(newsgroup, FALSE);
    }

    return newsgroup_item;
}

OP_STATUS NntpBackend::FindNewsgroupRange(IO OpString8& parameter, OUT OpString8& range, BOOL& set_ignore_rcfile_flag) const
{
    OP_STATUS ret;
    range.Empty();
	set_ignore_rcfile_flag = FALSE;

	OP_ASSERT(parameter.HasContent());
	if (parameter.IsEmpty()) //A newsgroup must always be given
		return OpStatus::ERR;

    char* newsgroup_seperator = strchr(parameter.CStr(), '/');
    if (newsgroup_seperator && *(newsgroup_seperator+1)!=0) //We have a range embedded in the newsgroup parameter
    {
		set_ignore_rcfile_flag = TRUE;
        *(newsgroup_seperator) = 0;
        return range.Append(++newsgroup_seperator);
    }

    //Search for newsgroup in the subscribed-list
    NewsRCItem* subscribed_item = FindNewsgroupItem(parameter, FALSE);
    if (subscribed_item)
    {
        return range.Set(subscribed_item->read_range);
    }

    //Group is not in the subscribed-list. Search for it in the .newsrc-file
	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    OpFile* file = glue_factory->CreateOpFile(m_newsrc_file);
    if (!file)
        return OpStatus::ERR_NO_MEMORY;

    BOOL file_exists;
	ret = file->Exists(file_exists);
    if (ret==OpStatus::OK && !file_exists)
    {
		glue_factory->DeleteOpFile(file);
        return OpStatus::OK; //'File not found' is not an error
    }

    if ((ret=file->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE)) != OpStatus::OK)
    {
		glue_factory->DeleteOpFile(file);
        return ret;
    }

    //Allocate new item (it _might_ be put into the subscribed-list)
    NewsRCItem* newsrc_item = OP_NEW(NewsRCItem, ());
    if (!newsrc_item)
    {
		glue_factory->DeleteOpFile(file);
        return OpStatus::ERR_NO_MEMORY;
    }

    OpString8 parse_buffer;
    char*     parse_ptr = NULL;
    BOOL done = FALSE;
    while (!done)
    {
        if ((ret=ParseNextNewsRCItem(file, FALSE, parse_buffer, parse_ptr, newsrc_item)) != OpStatus::OK)
        {
            OP_DELETE(newsrc_item);
			glue_factory->DeleteOpFile(file);
            return ret;
        }

        if (((done = newsrc_item->newsgroup.IsEmpty()) == FALSE) &&
            newsrc_item->newsgroup.Compare(parameter)==0) //We have found the newsgroup in the .newsrc-file
        {
            if ((ret=range.Set(newsrc_item->read_range)) == OpStatus::OK)
            {
                newsrc_item->Into(m_subscribed_list); //Insert it into the subscribed-list (it is not subscribed, but we might need to update its range in the .newsrc-file)
            }
            else
            {
                OP_DELETE(newsrc_item);
            }
			glue_factory->DeleteOpFile(file);
            return ret;
        }
    }
    OP_DELETE(newsrc_item);
	glue_factory->DeleteOpFile(file);
    return OpStatus::OK;
}

OP_STATUS NntpBackend::ParseXref(IN const OpStringC8& xref, OUT OpString8& message_id, OUT OpString8& newsserver, OUT OpString8& newsgroup, OUT int& nntp_number) const
{
    message_id.Empty();
    newsserver.Empty();
    newsgroup.Empty();
    nntp_number = 0;
    if (xref.IsEmpty())
        return OpStatus::OK; //Nothing to do

    OP_STATUS ret;
    char* start_message_id = const_cast<char*>(xref.CStr());
    while (*start_message_id == ' ') start_message_id++;
    char* end_message_id = start_message_id;
    if (*start_message_id == '<')
    {
        end_message_id = start_message_id;
        while (*end_message_id && (*end_message_id!=' ' && *end_message_id!='>')) end_message_id++;
        if ((ret=message_id.Set(start_message_id, end_message_id-start_message_id+1)) != OpStatus::OK)
            return ret;
    }

    char* start_newsserver = end_message_id;
    while (*start_newsserver=='>' || *start_newsserver==' ') start_newsserver++;
    char* end_newsserver = start_newsserver;
    while (*end_newsserver && *end_newsserver!=' ') end_newsserver++;
    if ((ret=newsserver.Set(start_newsserver, end_newsserver-start_newsserver)) != OpStatus::OK)
        return ret;

    char* start_newsgroup = end_newsserver;
    while (*start_newsgroup && *start_newsgroup==' ') start_newsgroup++;
    char* end_newsgroup = start_newsgroup;
    while (*end_newsgroup && (*end_newsgroup!=' ' && *end_newsgroup!=':')) end_newsgroup++;
    if (*start_newsgroup && (ret=newsgroup.Set(start_newsgroup, end_newsgroup-start_newsgroup))!=OpStatus::OK)
        return ret;

    char* start_nntp = end_newsgroup;
    while (*start_nntp && (*start_nntp==' ' || *start_nntp==':')) start_nntp++;
    while (*start_nntp>='0' && *start_nntp<='9') nntp_number = 10*nntp_number+(*(start_nntp++)-'0');

    return OpStatus::OK;
}

OP_STATUS NntpBackend::AddNewNewsgroups(OpString8& newsgroups)
{
    char* newsgroup_start = (char*)(newsgroups.CStr());
    if (!newsgroup_start || *newsgroup_start==0)
    {
        m_last_updated = max(m_last_updated, m_last_update_request);
        return OpStatus::OK; //Nothing to do
    }

    char* newsgroup_end;
    char* line_end;
    while (*newsgroup_start)
    {
        while (*newsgroup_start=='\r' || *newsgroup_start=='\n') newsgroup_start++; //Skip linefeeds

        newsgroup_end = newsgroup_start;
        while (*newsgroup_end && *newsgroup_end!=' ') newsgroup_end++;

        line_end = newsgroup_end;
        while (*line_end && (*line_end!='\r' && *line_end!='\n')) line_end++;

        if (line_end!=newsgroup_end && (*line_end=='\r' || *line_end=='\n')) //We have a complete line
        {
            NewsRCItem* newsrc_item = OP_NEW(NewsRCItem, ());
            if (!newsrc_item)
                return OpStatus::ERR_NO_MEMORY;

            OP_STATUS ret;
            if ((ret=newsrc_item->newsgroup.Set(newsgroup_start, newsgroup_end-newsgroup_start)) != OpStatus::OK)
            {
                OP_DELETE(newsrc_item);
                newsgroups.Set(line_end);
                return ret;
            }

            if (newsrc_item->newsgroup.Compare(".")==0) //end-of-text marker?
            {
                OP_DELETE(newsrc_item);
                newsgroups.Set(line_end);
                m_last_updated = m_last_update_request;
                return OpStatus::OK;
            }

            newsrc_item->status = NewsRCItem::NEW_NEWSGROUP;
            newsrc_item->Into(m_subscribed_list);
            m_newsrc_file_changed = TRUE;

            OpString16 newsgroup, path;

			if ( ((ret=newsrc_item->GetUnicodeName(newsgroup, this)) !=OpStatus::OK) ||
                 ((ret=path.Set(newsrc_item->newsgroup)) !=OpStatus::OK))
            {
                return ret;
            }

			GetAccountPtr()->OnFolderAdded(newsgroup, path, FALSE, 0);
        }
        else
        {
            break; //We have an incomplete line.
        }

        newsgroup_start = line_end;
    }

    return newsgroups.Set(newsgroup_start);
}

OP_STATUS NntpBackend::AddReadRange(const OpStringC8& newsgroup, const OpStringC8& range)
{
    OP_STATUS ret;
	OP_ASSERT(newsgroup.HasContent());
	if (newsgroup.IsEmpty())
		return OpStatus::ERR;

    //Find newsgroup item
    NewsRCItem* newsgroup_item = FindNewsgroupItem(newsgroup, TRUE);
    if (!newsgroup_item)
        return OpStatus::OK; //Nothing to do

    //Set original range
    NNTPRange new_range;
    if ((ret=new_range.SetReadRange(newsgroup_item->read_range)) != OpStatus::OK)
        return ret;

    //Add the given read range
    OpString8 new_read_range;
    if ( ((ret=new_range.AddRange(range)) != OpStatus::OK) ||
         ((ret=new_range.GetReadRange(new_read_range)) != OpStatus::OK)  )
    {
        return ret;
    }

    //Is the new range different than the old one?
    if (new_read_range.Compare(newsgroup_item->read_range) != 0)
    {
        if ((ret=newsgroup_item->read_range.Set(new_read_range)) != OpStatus::OK)
            return ret;

        m_newsrc_file_changed = TRUE;
    }
    return OpStatus::OK;
}

OP_STATUS NntpBackend::AddCommands(int count, ...)
{
    OP_STATUS ret = OpStatus::OK;

    va_list command_list;
    va_start(command_list, count);

    Head* tmp_list = OP_NEW(Head, ());
    if (!tmp_list)
        return OpStatus::ERR_NO_MEMORY;

    //Generate list of commands
	int i;
    for (i=0; i<count; i++)
    {
        CommandItem* command_item = OP_NEW(CommandItem, ());
        CommandItem::Commands command = (CommandItem::Commands)va_arg(command_list, int);
        OpString8* param1 = va_arg(command_list, OpString8*);
        OpString8* param2 = va_arg(command_list, OpString8*);
		UINT8 flags = (UINT8)va_arg(command_list, int);

        if (!command_item ||
            (param1 && !param1->IsEmpty() && ((ret=command_item->m_param1.Set(*param1))!=OpStatus::OK)) ||
            (param2 && !param2->IsEmpty() && ((ret=command_item->m_param2.Set(*param2))!=OpStatus::OK)) )
        {
            tmp_list->Clear();
            OP_DELETE(tmp_list);
            va_end(command_list);
            if (command_item)
            {
                OP_DELETE(command_item);
            }
            else
                ret = OpStatus::ERR_NO_MEMORY;

            return ret;
        }
        command_item->m_command = command;
		command_item->m_flags = flags;
        command_item->Into(tmp_list);
    }
    va_end(command_list);

    //Insert list into an available connection list, or the queue list
    ret = OpStatus::OK;
    if (!tmp_list->Empty())
    {
        Head* destination_list = m_command_list;

        NNTP* connection_ptr = GetConnectionPtr(GetAvailableConnectionId());
        if (connection_ptr)
        {
            destination_list = connection_ptr->GetCommandListPtr();
        }

        UINT32 article_count = 0;
        ret = destination_list ? NntpBackend::MoveQueuedCommand(tmp_list, destination_list, article_count, FALSE) : OpStatus::ERR_NULL_POINTER;

        if (ret==OpStatus::OK && connection_ptr)
        {
            connection_ptr->IncreaseTotalCount(article_count);
            connection_ptr->SendQueuedMessages();
        }
    }

    tmp_list->Clear();
    OP_DELETE(tmp_list);

#if defined NNTP_LOG_QUEUE
    Log("After NntpBackend::AddCommands", "");
    LogQueue();
#endif

    return ret;
}

int NntpBackend::GetConnectionId(const NNTP* connection) const
{
	int i;
    for (i=0; i<4; i++)
    {
        if (m_connections_ptr && m_connections_ptr[i]==connection)
            return i;
    }
    return -1; //Connection requested is not among the 4 existing connections
}

NNTP* NntpBackend::GetConnectionPtr(int id)
{
    if (id<0 || id>3)
        return NULL;

    if (!m_connections_ptr)
    {
        m_connections_ptr = OP_NEWA(NNTP*, 4);
        if (!m_connections_ptr)
            return NULL;

        int i;
        for (i=0; i<4; i++)
            m_connections_ptr[i] = NULL;

		//Reset counters
		m_max_connection_count = 4;
		m_current_connections_count = 0;
    }

    if (!m_connections_ptr[id] && m_accept_new_connections && m_current_connections_count<m_max_connection_count)
    {
        m_connections_ptr[id] = OP_NEW(NNTP, (this));
        if (m_connections_ptr[id])
        {
            if (m_connections_ptr[id]->Init() != OpStatus::OK)
            {
                OP_DELETE(m_connections_ptr[id]);
                m_connections_ptr[id] = NULL;
            }

			//Connection is active
			m_current_connections_count++;
        }
    }

    return m_connections_ptr[id];
}

BOOL NntpBackend::GetMaxDownload(int available_messages, const OpStringC8& group, BOOL force_dont_ask, int& max_download)
{
	BOOL ok = TRUE;
	if (available_messages > m_max_download && m_ask_for_max_download && !force_dont_ask)
	{
		AskMaxMessagesDownloadDialog* dlg = OP_NEW(AskMaxMessagesDownloadDialog, (m_max_download, m_ask_for_max_download, available_messages, group, ok));
		if (dlg)
			dlg->Init(g_application->GetActiveDesktopWindow());
	}

	if (ok)
		max_download = m_max_download == 0 ? available_messages : m_max_download; // 0 means all available messages

	return ok;
}

BOOL NntpBackend::IHave(const OpStringC8& message_id) const
{
    if (message_id.IsEmpty())
    {
        OP_ASSERT(0);
        return FALSE;
    }

    Message message;
    if (OpStatus::IsError(MessageEngine::GetInstance()->GetMessage(message, message_id, FALSE)))
        return FALSE;

    return (!message.IsFlagSet(Message::IS_OUTGOING));
}

void NntpBackend::AuthenticationFailed(const NNTP* current_connection)
{
    m_accept_new_connections = FALSE;
    KillAllConnections(current_connection);
}

void NntpBackend::ConnectionDenied(NNTP* current_connection)
{
	if (!m_connections_ptr || !current_connection)
		return;

	int id = GetConnectionId(current_connection);
	if (id==-1)
		return;

	if (IsInSession(current_connection))
		SignalEndSession(current_connection);

	current_connection->Disconnect();
	autodelete(current_connection);
	m_connections_ptr[id] = NULL;

	if (id<2 && m_current_connections_count>2) //Move lower-priority connections to higher-priority connections when possible
	{
		OP_ASSERT(0);
		int i;
		for (i=2; i<4; i++)
		{
			if (m_connections_ptr[i])
			{
				m_connections_ptr[id] = m_connections_ptr[i];
				m_connections_ptr[i] = NULL;

				OpString8 log_heading;
				if (log_heading.Reserve(19)!=NULL)
				{
					sprintf(log_heading.CStr(), "NNTP SWAP [#%01d->#%01d]", i, id);
					OpStatus::Ignore(Log(log_heading, ""));
				}

				break;
			}
		}
	}

	m_current_connections_count--;
	if (m_max_connection_count>1) //Always allow 1 connection
	{
		m_max_connection_count--;
    }
}

Account* NntpBackend::GetNextSearchAccount(BOOL first_search) const
{
    UINT16 current_account_id = m_account->GetAccountId();

    AccountManager* account_manager = (AccountManager*)MessageEngine::GetInstance()->GetAccountManager();
    if (!account_manager)
        return NULL;

    UINT16 search_account_id = (UINT16)-1;
    Account* search_account = NULL;
    Account* tmp_account = account_manager->GetFirstAccount();
    OpString8 protocol;
    while (tmp_account)
    {
        if (tmp_account->GetIncomingProtocol() == AccountTypes::NEWS)
        {
            UINT16 tmp_account_id = tmp_account->GetAccountId();
            if (tmp_account_id<search_account_id && (tmp_account_id>current_account_id || first_search))
            {
                search_account = tmp_account;
                search_account_id = tmp_account_id;
            }
        }

        tmp_account = (Account*)(tmp_account->Suc());
    }

    return search_account;
}

void NntpBackend::AlertNewsgroupEncodingOverride()
{
	if (m_has_alerted_newsgroup_encoding_override) //Only alert once each session
		return;

    OpString errorstring;
	OpStatus::Ignore(g_languageManager->GetString(Str::S_WRONG_NNTP_GROUP_ENCODING, errorstring));

	if (errorstring.HasContent())
		OnError(errorstring);

	m_has_alerted_newsgroup_encoding_override = TRUE;
}

#if defined NNTP_LOG_QUEUE
void NntpBackend::LogQueue()
{
    OpString8 text;
    Head* list;
    CommandItem* command;
    char header[100]; /* ARRAY OK 2003-10-28 frodegill */
    int i;
    for (i=0; i<5; i++)
    {
        sprintf(header, "\nList %d\n", i);
        text.Append(header);
        list = (i<4) ? (m_connections_ptr && m_connections_ptr[i] ? m_connections_ptr[i]->GetCommandListPtr() : NULL) : m_command_list;
        command = list ? (CommandItem*)(list->First()) : NULL;
        while (command)
        {
            switch (command->m_command)
            {
            case CommandItem::MODE_READER: strcpy(header, "MODE_READER"); break;
            case CommandItem::AUTHINFO_GENERIC: strcpy(header, "AUTHINFO_GENERIC"); break;
            case CommandItem::AUTHINFO_USER: strcpy(header, "AUTHINFO_USER"); break;
            case CommandItem::LIST: strcpy(header, "LIST"); break;
            case CommandItem::NEWGROUPS: strcpy(header, "NEWGROUPS"); break;
            case CommandItem::GROUP: strcpy(header, "GROUP"); break;
            case CommandItem::HEAD: strcpy(header, "HEAD"); break;
            case CommandItem::HEAD_LOOP: strcpy(header, "HEAD_LOOP"); break;
            case CommandItem::XOVER: strcpy(header, "XOVER"); break;
            case CommandItem::XOVER_LOOP: strcpy(header, "XOVER_LOOP"); break;
            case CommandItem::ARTICLE: strcpy(header, "ARTICLE"); break;
            case CommandItem::ARTICLE_LOOP: strcpy(header, "ARTICLE_LOOP"); break;
            case CommandItem::POST: strcpy(header, "POST"); break;
            case CommandItem::POST_MESSAGE: strcpy(header, "POST_MESSAGE"); break;
            case CommandItem::QUIT: strcpy(header, "QUIT"); break;
            case CommandItem::END_OF_COMMAND: strcpy(header, "END_OF_COMMAND"); break;
            default: sprintf(header, "<unknown %d>", command->m_command); break;
            }

			OpString8 line_log;
			line_log.AppendFormat("%s (%s) (%s) (%u)\n", header,
											   command->m_param1?command->m_param1:"",
											   command->m_param2?command->m_param2:"",
											   (unsigned int)command->m_flags);
			OpStatus::Ignore(text.Append(line_log));

            command = (CommandItem*)(command->Suc());
        }
        text.Append("\n");
    }
    Log("", text);
}
#endif

OP_STATUS NntpBackend::KillAllConnections(const NNTP* current_connection)
{
//#pragma PRAGMAMSG("FG: What if M2 is downloading messages when this is happening?  [20020617]")
    if (m_connections_ptr)
    {
	    int i;
        for (i=0; i<4; i++)
        {
            if (m_connections_ptr[i])
            {
                m_connections_ptr[i]->Disconnect();
                if (m_connections_ptr[i] == current_connection)
                {
                    autodelete(m_connections_ptr[i]);
                }
                else
                {
                    OP_DELETE(m_connections_ptr[i]);
                }

                m_connections_ptr[i] = NULL;
				m_current_connections_count--;
            }
        }
    }

    if (m_command_list)
    {
#if defined NNTP_LOG_QUEUE
        Log("Before NntpBackend::KillAllConnections Clear", "");
        LogQueue();
#endif

        m_command_list->Clear();
    }

    OP_STATUS ret;
    if ((ret=WriteSettings()) != OpStatus::OK)
        return ret;

	m_checked_newgroups = FALSE;
    return OpStatus::OK;
}

OP_STATUS NntpBackend::MoveQueuedCommand(Head* source_list, Head* destination_list, UINT32& article_count, BOOL only_first_command)
{
    if (!source_list || !destination_list)
        return OpStatus::ERR_NULL_POINTER;

    CommandItem* quit_command = (CommandItem*)(destination_list->Last());
    while (quit_command && (quit_command->m_command == CommandItem::END_OF_COMMAND))
        quit_command = (CommandItem*)(quit_command->Pred()); //Skip end_of_command if present
    if (quit_command && (quit_command->m_command != CommandItem::QUIT))
        quit_command = NULL; //Only keep address if it is the Quit-command (for anything else, commands should be inserted at the end)

    CommandItem* item = (CommandItem*)(source_list->First());
    CommandItem* next_item;
    while (item)
    {
        next_item = (CommandItem*)(item->Suc());
        item->Out();
        if (quit_command)
        {
            item->Precede(quit_command);
        }
        else
        {
            item->Into(destination_list);
        }

        if (item->m_command==CommandItem::HEAD ||
            item->m_command==CommandItem::ARTICLE ||
            item->m_command==CommandItem::POST)
            article_count++;

        if (only_first_command && item->m_command == CommandItem::END_OF_COMMAND)
            break;

        item = next_item;
    }

    return OpStatus::OK;
}


BOOL NntpBackend::CommandExistsInQueue(const OpStringC8& newsgroup, CommandItem::Commands command, OpString8* parameter)
{
    Head* queue;
    int i;
    for (i=0; i<5; i++)
    {
        queue = m_command_list;
        OpString8 queue_newsgroup;
        if (i<4)
        {
            queue = NULL;
            if (m_connections_ptr && m_connections_ptr[i])
            {
                if (m_connections_ptr[i]->CurrentCommandMatches(newsgroup, command, parameter))
                    return TRUE;

                queue = m_connections_ptr[i]->GetCommandListPtr();

                if (!newsgroup.IsEmpty())
                {
                    if (m_connections_ptr[i]->GetCurrentNewsgroup(queue_newsgroup) != OpStatus::OK)
                        return FALSE;
                }
            }
        }

        if (queue && CommandExistsInQueue(queue, newsgroup, queue_newsgroup, command, parameter))
            return TRUE;
    }

    return FALSE;
}

BOOL NntpBackend::CommandExistsInQueue(Head* queue, const OpStringC8& newsgroup, const OpStringC8& current_queue_newsgroup,
                                       CommandItem::Commands command, OpString8* parameter)
{
    if (!queue)
        return FALSE;

    char* req_parameter = parameter ? parameter->CStr() : NULL;

    if (newsgroup.IsEmpty())
    {
        BOOL command_match;
        CommandItem* list_item = (CommandItem*)(queue->First());
        while (list_item)
        {
            command_match = (list_item->m_command == command);
            if (!command_match && list_item->m_command==CommandItem::HEAD && command==CommandItem::ARTICLE) //Special case. Article should replace Head
                command_match = TRUE;

            char* list_parameter1 = !list_item->m_param1.IsEmpty() ? list_item->m_param1.CStr() : NULL;

            if (command_match && req_parameter && (list_parameter1==NULL || op_stricmp(list_parameter1, req_parameter)==0))
            {
                if (list_item->m_command != command) //Replace Head with Article
                    list_item->m_command = command;

                return TRUE;
            }

            list_item = (CommandItem*)(list_item->Suc());
        }
    }
    else
    {
        BOOL in_correct_newsgroup = (newsgroup.CompareI(current_queue_newsgroup)==0);
        CommandItem* list_item = (CommandItem*)(queue->First());
        while (list_item)
        {
            if (list_item->m_command == CommandItem::GROUP)
            {
                in_correct_newsgroup = (newsgroup.CompareI(list_item->m_param1)==0);

                do //Skip Group- and any EndOfCommand-items
                {
                    list_item = (CommandItem*)(list_item->Suc());
                } while (list_item && list_item->m_command==CommandItem::END_OF_COMMAND);


                if (list_item->m_command == CommandItem::GROUP) //Should be rare, but this prevents missing a new Group
                    continue;
            }

            if (in_correct_newsgroup)
            {
                BOOL command_match = (list_item->m_command == command);
                if (!command_match &&
                     ((list_item->m_command==CommandItem::HEAD && command==CommandItem::ARTICLE) ||
                      (list_item->m_command==CommandItem::XOVER_LOOP && command==CommandItem::HEAD_LOOP) ||
                      (list_item->m_command==CommandItem::XOVER_LOOP && command==CommandItem::ARTICLE_LOOP) ||
                      (list_item->m_command==CommandItem::HEAD_LOOP && command==CommandItem::ARTICLE_LOOP))) //Special cases where the new command should replace the old command
                    command_match = TRUE;

                char* list_parameter1 = !list_item->m_param1.IsEmpty() ? list_item->m_param1.CStr() : NULL;

                if (command_match && (!req_parameter || (list_parameter1 && op_stricmp(list_parameter1, req_parameter)==0)))
                {
                    if (list_item->m_command != command) //Update old item
                        list_item->m_command = command;

                    return TRUE;
                }
            }

            list_item = (CommandItem*)(list_item->Suc());
        }
    }

    return FALSE;
}


INT8 NntpBackend::GetAvailableConnectionId()
{
    NNTP* connection_ptr;
    int i;
    for (i=0; i<4; i++)
    {
        connection_ptr = GetConnectionPtr(i);
        if ((!connection_ptr && m_current_connections_count<m_max_connection_count) ||
			(connection_ptr && !connection_ptr->IsBusy()))
		{
            return i;
		}
    }
    return -1;
}

BOOL NntpBackend::IsMessageId(OpString8& string) const
{
    if (string.IsEmpty())
        return FALSE;

    BOOL is_messageid = (strchr(string.CStr(), '@') != NULL); //Is this a too simple test?
    if (is_messageid)
    {
        unsigned char* tmp_ptr = (unsigned char*)string.CStr();
        if (*tmp_ptr != '<')
        {
            string.Insert(0, "<");
        }

        tmp_ptr = (unsigned char*)(string.CStr()+string.Length()-1);
        if (*tmp_ptr != '>')
        {
            string.Append(">");
        }
    }

    return is_messageid;
}

BOOL NntpBackend::IsXref(const OpStringC8& string) const
{
    if (string.IsEmpty())
        return FALSE;

    char* marker = const_cast<char*>(string.CStr()); //Skip message_id that might precede xref
    char* at_marker = strchr(marker, '@');
    char* end_messageid = at_marker ? strchr(at_marker, '>') : marker;

    BOOL is_xref = (strchr(end_messageid ? end_messageid : marker, ':') != NULL); //Is this a too simple test?
    return is_xref;
}

OP_STATUS NntpBackend::SettingsChanged(BOOL startup)
{
	if (startup)
		return OpStatus::OK;

    OpStatus::Ignore(KillAllConnections(NULL));
    return Init(m_account);
}

BOOL NntpBackend::IsBusy() const
{
	int i;
    for (i=0; i<4; i++)
    {
        if (m_connections_ptr && m_connections_ptr[i] && m_connections_ptr[i]->IsBusy())
			return TRUE;
    }
	return FALSE;
}

OP_STATUS NntpBackend::PrepareToDie()
{
    m_accept_new_connections = FALSE;
    KillAllConnections(NULL);

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    OpFile* file = glue_factory->CreateOpFile(m_newsrc_file);
    if (file)
    {
        file->Delete();
    }
	glue_factory->DeleteOpFile(file);

    return OpStatus::OK;
}

OP_STATUS NntpBackend::AddSubscribedFolder(OpString& path)
{
    OP_STATUS ret;
    OpString8 newsgroup;
    if ((ret=newsgroup.Set(path.CStr())) != OpStatus::OK)
        return ret;

    NewsRCItem* newsgroup_item = FindNewsgroupItem(newsgroup, TRUE);
    if (!newsgroup_item)
    {
        OpString8 newsgroup_line;
        if ( ((ret=newsgroup_line.Set(newsgroup)) != OpStatus::OK) ||
             ((ret=newsgroup_line.Append(" \r\n")) != OpStatus::OK) ||
             ((ret=AddNewNewsgroups(newsgroup_line)) != OpStatus::OK) )
            return ret;

        newsgroup_item = FindNewsgroupItem(newsgroup, TRUE);
    }

    if (newsgroup_item && (newsgroup_item->status != NewsRCItem::SUBSCRIBED))
    {
        newsgroup_item->status = NewsRCItem::SUBSCRIBED;
        m_newsrc_file_changed = TRUE;
    }

    return OpStatus::OK;
}


OP_STATUS NntpBackend::RemoveSubscribedFolder(UINT32 index_id)
{
	Index*    index		= MessageEngine::GetInstance()->GetIndexById(index_id);

	if (!index)
		return OpStatus::ERR_NULL_POINTER;

	IndexSearch* search = index->GetSearch();
	if (!search)
		return OpStatus::ERR;

	OpString path;
	RETURN_IF_ERROR(path.Set(search->GetSearchText()));

	// Remove both the messages inside this folder and the folder
	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetIndexer()->RemoveMessagesInIndex(index));
	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetIndexer()->RemoveIndex(index));

	// Remove subscription to newsgroup
	OpString8 newsgroup;
	RETURN_IF_ERROR(newsgroup.Set(path.CStr()));

	NewsRCItem* newsgroup_item = FindNewsgroupItem(newsgroup, TRUE);
	if (newsgroup_item)
	{
		newsgroup_item->status = NewsRCItem::NOT_SUBSCRIBED;
		m_newsrc_file_changed = TRUE;
	}

	//Remove from queues
	if (m_connections_ptr)
	{
		for (INT8 i=0; i<4; i++)
		{
			if (m_connections_ptr[i])
			{
				m_connections_ptr[i]->HandleUnsubscribe(path);
			}
		}
	}

	return HandleUnsubscribe(path, m_command_list, UNI_L(""));
}

OP_STATUS NntpBackend::HandleUnsubscribe(const OpStringC& newsgroup, Head* queue, const OpStringC& current_newsgroup)
{
	if (!queue || newsgroup.IsEmpty())
		return OpStatus::OK; //Nothing to do

	BOOL in_unsubscribed_group = (newsgroup.CompareI(current_newsgroup)==0);

	CommandItem* item = (CommandItem*)(queue->First()), *next_item;
	while (item)
	{
		next_item = (CommandItem*)(item->Suc());

		switch (item->m_command)
		{
		case CommandItem::GROUP:
			{
				in_unsubscribed_group = (!item->m_param1.IsEmpty() && newsgroup.CompareI(item->m_param1)==0);
			} //Fallthrough
        case CommandItem::HEAD: //Fallthrough, all of these commands should be removed for an unsubscribed group
        case CommandItem::HEAD_LOOP:
        case CommandItem::XOVER:
        case CommandItem::XOVER_LOOP:
        case CommandItem::ARTICLE:
        case CommandItem::ARTICLE_LOOP:
        case CommandItem::END_OF_COMMAND:
			{
			  if (in_unsubscribed_group)
			  {
				item->Out();
				OP_DELETE(item);
			  }
			  break;
			}

		default: in_unsubscribed_group = FALSE;
		}

		item = next_item;
	}
	return OpStatus::OK;
}

OP_STATUS NntpBackend::CommitSubscribedFolders()
{
    OP_STATUS ret;
    if ((ret=WriteRCFile(m_subscribed_list)) != OpStatus::OK)
        return ret;

    return ReadRCFile(m_subscribed_list, TRUE, 0); //Read in the fresh list
}


void NntpBackend::GetAllFolders()
{
    m_accept_new_connections = TRUE;
    if (!m_report_to_ui_list)
    {
        m_report_to_ui_list = OP_NEW(Head, ());
		if (!m_report_to_ui_list)
		{
            StopFolderLoading();
			return;
		}
	}

	if (m_report_to_ui_list->Empty())
	{
        if (ReadRCFile(m_report_to_ui_list, FALSE, 0)!=OpStatus::OK)
        {
            StopFolderLoading();
            return;
        }

        if (m_report_to_ui_list->Empty() || !m_checked_newgroups)
        {
			m_checked_newgroups = FALSE; //In case the first newgroups-call failed (timeout etc)
			OpStatus::Ignore(FetchNNTPNewGroups()); //No newsgroups? Find some!
        }
    }

    NewsRCItem* newsrc_item = (NewsRCItem*)(m_report_to_ui_list->First());
    NewsRCItem* next_newsrc_item;
    OpString16 newsgroup, path;
    while (newsrc_item) //Should probably be made asyncronus
    {
        next_newsrc_item = (NewsRCItem*)(newsrc_item->Suc());
        if ( newsrc_item->GetUnicodeName(newsgroup, this)!=OpStatus::OK ||
            path.Set(newsrc_item->newsgroup)!=OpStatus::OK)
        {
            StopFolderLoading();
            return;
        }

		INT32 subscribed = (newsrc_item->status == NewsRCItem::SUBSCRIBED) ? 1 : 0;
		GetAccountPtr()->OnFolderAdded(newsgroup, path, newsrc_item->status==NewsRCItem::SUBSCRIBED, subscribed);

        newsrc_item = next_newsrc_item;
    }

	if (m_report_to_ui_list->Empty())
	{
		OP_DELETE(m_report_to_ui_list);
		m_report_to_ui_list = NULL;
	}
}

void NntpBackend::StopFolderLoading()
{
    if (m_report_to_ui_list)
    {
		Account* account = GetAccountPtr();
		if (account)
			account->OnFolderLoadingCompleted();

        m_report_to_ui_list->Clear();
        OP_DELETE(m_report_to_ui_list);
        m_report_to_ui_list = NULL;
    }
}

OP_STATUS NntpBackend::Connect()
{
	m_max_connection_count = 4; //In case this has been wrongly detected, reset it and detect again

    OP_STATUS ret;
    NNTP* connection_ptr;
	int i;
    for (i=0; i<2; i++)
    {
        connection_ptr = GetConnectionPtr(i);
        if (connection_ptr && ((ret=connection_ptr->Connect()) != OpStatus::OK) ) //Only connect the two first connections. The other two will connect when needed
        {
            Disconnect();
            return ret;
        }
    }
    return OpStatus::OK;
}

OP_STATUS NntpBackend::Disconnect()
{
    if (m_command_list)
        m_command_list->Clear();

    OP_STATUS ret = OpStatus::OK;
	int i;
    for (i=0; i<4; i++)
    {
        OP_STATUS temp_ret;
        if (m_connections_ptr && m_connections_ptr[i] && ((temp_ret=m_connections_ptr[i]->Disconnect()) != OpStatus::OK) )
            ret = temp_ret;
    }
	return ret;
}

OP_STATUS NntpBackend::SignalStartSession(const NNTP* connection)
{
	if (!connection || !m_account)
		return OpStatus::ERR_NULL_POINTER;

	INT8 connection_id = GetConnectionId(connection);
	if (connection_id==-1)
		return OpStatus::ERR;

	OP_ASSERT((m_open_connections&(1<<connection_id))==0);

	if ((m_open_connections&(1<<connection_id))!=0) //Is the connection already marked as connected?
		return OpStatus::ERR;

	m_open_connections |= 1<<connection_id;
	if (!m_in_session)
	{
		m_in_session = TRUE;
		return m_account->SignalStartSession(this);
	}
	return OpStatus::OK;
}

OP_STATUS NntpBackend::SignalEndSession(const NNTP* connection)
{
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	INT8 connection_id = connection ? GetConnectionId(connection) : -1;
	if (connection && connection_id==-1)
		return OpStatus::ERR;

	OP_ASSERT(m_in_session && (!connection || (m_open_connections&(1<<connection_id))!=0));

	if (!connection) //End session, no matter how many connections are open
	{
		m_open_connections = 0;
	}
	else
	{
		if ((m_open_connections&(1<<connection_id))==0) //Is the connection already marked as disconnected?
			return OpStatus::ERR;

		m_open_connections &= ~(1<<connection_id);
	}

	m_in_session = m_open_connections!=0; //Are there more active connections?
	if (m_in_session)
		return OpStatus::OK;

	/*
	* Last connection is down. End session
	*/

	//Write rc-file
    CommitSubscribedFolders();

	//Send queued messages
	if (m_send_queued_when_done && m_account->GetSendQueuedAfterChecking())
	{
		OpStatus::Ignore(MessageEngine::GetInstance()->SendMessages(m_account->GetAccountId()));
		m_send_queued_when_done = FALSE;
	}

	//End session
	OpStatus::Ignore(MessageEngine::GetInstance()->GetMasterProgress().NotifyReceived(GetAccountPtr()));
	return m_progress.EndCurrentAction(FALSE);
}

BOOL NntpBackend::IsInSession(const NNTP* connection) const
{
	if (!connection)
		return m_in_session;

	INT8 connection_id = GetConnectionId(connection);
	if (connection_id == -1)
		return FALSE;

	return (m_open_connections&(1<<connection_id)) != 0;
}

void NntpBackend::OnProgressChanged(const ProgressInfo& progress)
{
	m_progress.Reset();

	if (!m_connections_ptr)
		return;

	for (int i = 0; i < 4; i++)
	{
		if (m_connections_ptr[i])
			m_progress.Merge(m_connections_ptr[i]->GetProgress());
	}

	m_progress.AlertListeners();
}

OP_STATUS NntpBackend::Fetched(Message* message, BOOL headers_only)
{
	OP_STATUS ret = GetAccountPtr()->Fetched(*message, headers_only);
	if (OpStatus::IsSuccess(ret))
		m_download_count++;

	return ret;
}

OP_STATUS NntpBackend::FetchMessages(BOOL enable_signalling)
{
    m_accept_new_connections = TRUE;
	m_send_queued_when_done |= enable_signalling;

	OP_STATUS ret;
    NewsRCItem* newsgroup_item = m_subscribed_list ? (NewsRCItem*)(m_subscribed_list->First()) : NULL;

	OpStatus::Ignore(FetchNNTPNewGroups()); //Not important if this fails

    if (newsgroup_item)
    {
        while (newsgroup_item)
        {
            if (newsgroup_item->status == NewsRCItem::SUBSCRIBED)
		    {
                if ((ret=GetDownloadBodies() ? FetchNNTPMessages(newsgroup_item->newsgroup, enable_signalling) : FetchNNTPHeaders(newsgroup_item->newsgroup, enable_signalling)) != OpStatus::OK)
				    return ret;
		    }

            newsgroup_item = (NewsRCItem*)(newsgroup_item->Suc());
        }
    }

	return OpStatus::OK;
}

OP_STATUS NntpBackend::SelectFolder(UINT32 index_id)
{
	Index* index = MessageEngine::GetInstance()->GetIndexById(index_id);

	if (index)
	{
		OpString8 folder8;
		IndexSearch* search = index->GetM2Search();

		if (search)
		{
			RETURN_IF_ERROR(folder8.Set(search->GetSearchText().CStr()));
		}
		FetchMessage(folder8);
	}

	return OpStatus::OK;
}

OP_STATUS NntpBackend::StopFetchingMessages()
{
	return Disconnect();
}

OP_STATUS NntpBackend::SendMessage(Message& message, BOOL anonymous)
{
	return PostMessage(message.GetId());
}

OP_STATUS NntpBackend::FetchMessage(message_index_t index, BOOL user_request, BOOL force_complete)
{
    if (user_request)
        m_accept_new_connections = TRUE;

	Message message;
	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessage(message, index));

    OpString8 internet_location;
	RETURN_IF_ERROR(message.GetInternetLocation(internet_location));

	if (internet_location.IsEmpty())
	{
		RETURN_IF_ERROR(message.GetHeaderValue(Header::MESSAGEID, internet_location));

		if (internet_location.IsEmpty())
			return OpStatus::ERR_OUT_OF_RANGE;
	}

    return FetchNNTPMessage(internet_location, index, user_request);
}

OP_STATUS NntpBackend::FetchHeaders(const OpStringC8& internet_location, message_index_t index)
{
    m_accept_new_connections = TRUE;
    OP_STATUS ret;
    OpString8 parameter;
    if ((ret=parameter.Set(internet_location)) != OpStatus::OK) //Make backup of it. We don't want it const all the time...
        return ret;

    if (IsXref(parameter))
    {
        return FetchNNTPMessageByLocation(TRUE, parameter);
    }
    else if (IsMessageId(parameter))
    {
        return FetchNNTPMessageById(TRUE, parameter);
    }
    else
	{
        return GetDownloadBodies() ? FetchNNTPMessages(parameter, FALSE) : FetchNNTPHeaders(parameter, FALSE);
	}
}

OP_STATUS NntpBackend::FetchMessage(const OpStringC8& internet_location, message_index_t index)
{
    return FetchNNTPMessage(internet_location, index, TRUE);
}

OP_STATUS NntpBackend::FetchNNTPMessage(const OpStringC8& internet_location, message_index_t index, BOOL user_request)
{
    if (user_request)
        m_accept_new_connections = TRUE;

    OpString8 store_id;
    if (index != 0)
    {
	    if (!store_id.Reserve(11))
		    return OpStatus::ERR_NO_MEMORY;

	    sprintf(store_id.CStr(), "%d", index);
    }

    OP_STATUS ret;
    OpString8 parameter;
    if ((ret=parameter.Set(internet_location)) != OpStatus::OK) //Make backup of it. We don't want it const all the time...
        return ret;

    if (IsXref(parameter))
    {
        return FetchNNTPMessageByLocation(FALSE, parameter, store_id);
    }
    else if (IsMessageId(parameter))
    {
        return FetchNNTPMessageById(FALSE, parameter, store_id);
    }
    else
	{
        return GetDownloadBodies() ? FetchNNTPMessages(parameter, FALSE) : FetchNNTPHeaders(parameter, FALSE);
	}
}

OP_STATUS NntpBackend::FetchNNTPNewGroups()
{
    if (m_checked_newgroups)
        return OpStatus::OK; //We have already fetched new groups this session

    m_last_update_request = time(NULL); //Store current time

	// Setting this boolean prevents the refreshing of newsgroups in this Opera session. See bug #160267 (arjanl)
    // m_checked_newgroups = TRUE; //We set it to true here. If something fails after this, it will also fail next time this session

    if (m_last_updated==0 || m_last_updated==RFC850_DATE) //First time we should send a LIST, as NEWGROUPS don't return very old groups
    {
        return AddCommands(2, (int)CommandItem::LIST, NULL, NULL, (int)0,
                              (int)CommandItem::END_OF_COMMAND, NULL, NULL, (int)0);
    }
    else
    {
		struct tm* time = op_gmtime(&m_last_updated);

		OpString8 last_checked;
        if (!last_checked.Reserve(18))
            return OpStatus::ERR_NO_MEMORY;

        sprintf(last_checked.CStr(), "%02u%02u%02u %02u%02u%02u GMT",
                time->tm_year%100, (time->tm_mon%12)+1, max(1,time->tm_mday%32),
                time->tm_hour%24, time->tm_min%60, time->tm_sec%60);

        return AddCommands(2, (int)CommandItem::NEWGROUPS, &last_checked, NULL, (int)0,
                              (int)CommandItem::END_OF_COMMAND, NULL, NULL, (int)0);
    }
}

OP_STATUS NntpBackend::FetchNNTPHeaders(OpString8& newsgroup, BOOL enable_signalling)
{
	m_send_queued_when_done |= enable_signalling;

    OP_STATUS ret;

    if (CommandExistsInQueue(newsgroup, CommandItem::XOVER_LOOP, NULL))
        return OpStatus::OK;

    OpString8 range;
	BOOL ignore_rcfile;
    if ((ret=FindNewsgroupRange(newsgroup, range, ignore_rcfile)) != OpStatus::OK)
        return ret;

	int article_flag = ignore_rcfile*1<<CommandItem::IGNORE_RCFILE |
					   enable_signalling*1<<CommandItem::ENABLE_SIGNALLING;

    return AddCommands(3, (int)CommandItem::GROUP, &newsgroup, NULL, (int)0,
						  (int)CommandItem::XOVER_LOOP, &range, NULL, article_flag,
                          (int)CommandItem::END_OF_COMMAND, NULL, NULL, (int)0);
}

OP_STATUS NntpBackend::FetchNNTPMessages(OpString8& newsgroup, BOOL enable_signalling)
{
	m_send_queued_when_done |= enable_signalling;

    OP_STATUS ret;

    if (CommandExistsInQueue(newsgroup, CommandItem::ARTICLE_LOOP, NULL))
        return OpStatus::OK;

    OpString8 range;
	BOOL ignore_rcfile;
    if ((ret=FindNewsgroupRange(newsgroup, range, ignore_rcfile)) != OpStatus::OK)
        return ret;

	int article_flag = ignore_rcfile*1<<CommandItem::IGNORE_RCFILE |
					   enable_signalling*1<<CommandItem::ENABLE_SIGNALLING;

    return AddCommands(3, (int)CommandItem::GROUP, &newsgroup, NULL, (int)0,
                          (int)CommandItem::ARTICLE_LOOP, &range, NULL, article_flag,
                          (int)CommandItem::END_OF_COMMAND, NULL, NULL, (int)0);
}

OP_STATUS NntpBackend::FetchNNTPMessageByLocation(BOOL headers_only, const OpStringC8& internet_location, const OpStringC8& store_id)
{
    if (internet_location.IsEmpty())
        return OpStatus::OK; //Nothing to fetch

    OP_STATUS ret;
    int nntp_number;
    OpString8 dummy_message_id, dummy_newsserver, newsgroup, nntp_number_str;
    if ((ret=ParseXref(internet_location, dummy_message_id, dummy_newsserver, newsgroup, nntp_number)) != OpStatus::OK)
        return ret;

    if (newsgroup.IsEmpty())
        return OpStatus::OK; //Nothing to do

	if (!nntp_number_str.Reserve(11))
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	sprintf(nntp_number_str.CStr(), "%d", nntp_number);

    if (CommandExistsInQueue(newsgroup, headers_only ? CommandItem::HEAD : CommandItem::ARTICLE, &nntp_number_str))
        return OpStatus::OK;

    return AddCommands(3, (int)CommandItem::GROUP, &newsgroup, NULL, (int)0,
                          headers_only ? (int)CommandItem::HEAD : (int)CommandItem::ARTICLE, &nntp_number_str, store_id.HasContent() ? &store_id : NULL, (int)0,
                          (int)CommandItem::END_OF_COMMAND, NULL, NULL, (int)0);
}

OP_STATUS NntpBackend::FetchNNTPMessageById(BOOL headers_only, const OpStringC8& message_id, const OpStringC8& store_id)
{
    if (message_id.IsEmpty())
        return OpStatus::OK; //Nothing to fetch

    OP_STATUS ret;
    OpString8 tmp_message_id;
    if ((ret=tmp_message_id.Set(message_id)) != OpStatus::OK)
        return ret;

    if (CommandExistsInQueue("", headers_only ? CommandItem::HEAD : CommandItem::ARTICLE, &tmp_message_id))
        return OpStatus::OK;

    OpString8 newsgroup;
    if ((ret=AnyNewsgroup(newsgroup)) != OpStatus::OK)
        return ret;

    return AddCommands(3, (int)CommandItem::GROUP, &newsgroup, NULL, (int)0,
                          headers_only ? (int)CommandItem::HEAD : (int)CommandItem::ARTICLE, &message_id, store_id.HasContent() ? &store_id : NULL, (int)0,
                          (int)CommandItem::END_OF_COMMAND, NULL, NULL, (int)0);
}

OP_STATUS NntpBackend::PostMessage(message_gid_t id)
{
    m_accept_new_connections = TRUE;

	Account* account = GetAccountPtr();
	if (!account)
		return OpStatus::ERR_NULL_POINTER;

	Message message;
	RETURN_IF_ERROR(account->PrepareToSendMessage(id, FALSE, message));

    OpString8 raw_message;
    RETURN_IF_ERROR(message.GetRawMessage(raw_message, FALSE, FALSE, FALSE));

	OpString8 index_str;
	if (!index_str.Reserve(11))
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	sprintf(index_str.CStr(), "%d", message.GetId());

    //Put the message into the queue
    return AddCommands(3, (int)CommandItem::POST, NULL, NULL, (int)0,
                       (int)CommandItem::POST_MESSAGE, &raw_message, &index_str, (int)0,
                       (int)CommandItem::END_OF_COMMAND, NULL, NULL, (int)0);
}

#endif //M2_SUPPORT
