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

// URL Cookie Management

#include "core/pch.h"

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/pi/OpSystemInfo.h"

#include "modules/formats/uri_escape.h"

#include "modules/url/url_man.h"
#include "modules/formats/url_dfr.h"
#include "modules/cookies/cookie_common.h"
#include "modules/cookies/url_cm.h"

#if defined(QUICK_COOKIE_EDITOR_SUPPORT)
#include "adjunct/quick/dialogs/ServerManagerDialog.h"
#endif

#include "modules/olddebug/tstdump.h"

CookiePath::CookiePath(OpStringS8 &pth)
{
	path.TakeOver(pth);
}

CookiePath*
CookiePath::CreateL(const OpStringC8 &pth)
{
	OP_STATUS op_err = OpStatus::OK;

	CookiePath* item;

	item = CookiePath::Create(pth, op_err);

	LEAVE_IF_ERROR(op_err);

	return item;
}

CookiePath* CookiePath::Create(const OpStringC8 &pth, OP_STATUS &op_err)
{
	OpStringS8 path;
	CookiePath* item;

	op_err = path.Set(pth.HasContent() ? pth : OpStringC8(""));
	if(OpStatus::IsError(op_err))
		return NULL;

	op_err = OpStatus::OK;
	item = OP_NEW(CookiePath, (path));
	if(item == NULL)
	{
		op_err = OpStatus::ERR_NO_MEMORY;
		return NULL;
	}

	return item;
}

CookiePath::~CookiePath()
{
#ifdef DEBUG_COOKIE_MEMORY_SIZE
	urlManager->SubCookieMemorySize(path.Length() + sizeof(CookiePath));
#endif
}

void CookiePath::Clear()
{
	cookie_list.Clear();
	path.Empty();
	Tree::Clear();
}

/**
 * Appends the content of str (if not NULL) and moves the pointer 'p' forward.
 */
static void BufferAppend(char*&p, const char* str, int len = -1)
{
	if (str)
	{
		if (len == -1)
			len = op_strlen(str);
		op_memcpy(p, str, len);
		p += len;
		*p = '\0';
	}
}


int	CookiePath::GetCookieRequest(
		time_t this_time,
		BOOL is_secure,
		BOOL is_server,
		unsigned short port,
		int &version,
		int &max_version,
		BOOL third_party_only,
		BOOL already_have_password,
		BOOL already_have_authentication,
		BOOL &have_password,
		BOOL &have_authentication,
		BOOL is_full_path,
		char* buf, int buf_len,
		BOOL for_http,
		BOOL allow_dollar_cookies,
		BOOL& seen_cookie2)
{
	int buf_used = 0;
	char* buf_p = buf;
	Cookie* ck = (Cookie*) cookie_list.First();
	while (ck)
	{
		Cookie* next_ck = ck->Suc();
		if (ck->Expires() && ck->Expires() < this_time)
		{
			OP_DELETE(ck);
		}
		else if ((is_secure || !ck->Secure())  &&
				(for_http || !ck->HTTPOnly()) &&
				(is_full_path || !ck->FullPathOnly())
				 && (is_server || !ck->SendOnlyToServer()) &&
				 (ck->Version() == 0 || ck->CheckPort(port)) &&
				 (!third_party_only || ck->GetAcceptedAsThirdParty()) &&
				 (allow_dollar_cookies || *ck->Name().CStr() != '$')
		)
		{

			int name_len = ck->Name().Length();
			int value_len = ck->Value().Length();

			int ver = ck->Version();
			if(version > ver)
				version = ver;

			if(ver > max_version)
				max_version = ver;

			if (ck->GetCookieType() == COOKIE2_COOKIE)
				seen_cookie2 = TRUE;

			int dom_len = (ver ? ck->Received_Domain().Length() : 0) ;
			int path_len = (ver && ck->Received_Path().HasContent() ? ck->Received_Path().Length()+1:  0) ;
			int port_len = (ver ? (ck->Port().HasContent() ? ck->Port().Length() : (ck->PortReceived() ? 1 : 0)) : 0) ;
			if ((name_len + value_len + 3 +
			    (dom_len ? dom_len + 10 : 0) +
			    (path_len ? path_len + 8 : 0) +
			    (port_len ? port_len + 8 : 0)) < buf_len - buf_used)
			{
				if (buf_used)
					BufferAppend(buf_p, "; ", 2);
				BufferAppend(buf_p, ck->Name().CStr());

				if (value_len > 0 || ck->Assigned())
				{
					*buf_p++ = '=';
					BufferAppend(buf_p, ck->Value().CStr());
				}

				if(ver > 0)
				{
					if(path_len)
					{
						BufferAppend(buf_p, "; $Path=\"");
						BufferAppend(buf_p, ck->Received_Path().CStr());
						*buf_p++ = '"';
						*buf_p = '\0';
					}
					if(dom_len)
					{
						BufferAppend(buf_p, "; $Domain=");
						BufferAppend(buf_p, ck->Received_Domain().CStr());
					}
					if(ck->PortReceived())
					{
						if(ck->Port().CStr())
						{
							BufferAppend(buf_p, "; $Port=\"");
							BufferAppend(buf_p, ck->Port().CStr());
							*buf_p++ = '"';
							*buf_p = '\0';
						}
						else
							BufferAppend(buf_p, "; $Port");
					}
				}
				if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TagUrlsUsingPasswordRelatedCookies))
				{
					if(ck->GetHavePassword())
						have_password = TRUE;
					if(ck->GetHaveAuthentication())
						have_authentication = TRUE;
					if(already_have_password)
						ck->SetHavePassword(TRUE);
					if(already_have_authentication)
						ck->SetHaveAuthentication(TRUE);
				}
				buf_used = buf_p - buf;

				ck->SetLastUsed(this_time);
			}
		}
		ck = next_ck;
	}

	return buf_used;
}



Cookie* CookiePath::GetLeastRecentlyUsed(time_t last_used, time_t this_time)
{
	time_t new_last_used = last_used;
	Cookie* ck = 0;
	Cookie *ck_unused = NULL;
	Cookie* new_ck = (Cookie*) cookie_list.First();
	while (new_ck)
	{
		Cookie* next_ck = new_ck->Suc();
		if (new_ck->Expires() && new_ck->Expires() < this_time)
		{
			OP_DELETE(new_ck);
		}
		else if (new_ck->GetLastUsed() <= new_last_used && !new_ck->ProtectedCookie())
		{
			if(new_ck->GetLastUsed())
			{
				new_last_used = new_ck->GetLastUsed();
				ck = new_ck;
			}
			else if(!ck_unused)
				ck_unused = new_ck;
		}
		new_ck = next_ck;
	}

	CookiePath* cp = (CookiePath*) FirstChild();
	while (cp)
	{
		new_ck = cp->GetLeastRecentlyUsed(new_last_used, this_time);
		if (new_ck)
		{
			if(new_ck->GetLastUsed())
			{
				ck = new_ck;
				new_last_used = ck->GetLastUsed();
			}
			else
				ck_unused = new_ck;
		}
		cp = cp->Suc();
	}
	return (ck ? ck : ck_unused);
}

int CookiePath::GetCookiesCount()
{
	int count = cookie_list.Cardinal();
	CookiePath* cp = (CookiePath*) FirstChild();
	while (cp)
	{
		count += cp->GetCookiesCount();
		cp = cp->Suc();
	}
	return count;
}

// pth must not start with '/'
CookiePath* CookiePath::GetCookiePathL(const char* pth, BOOL create, BOOL &is_full_path)
{
	if (pth && *pth)
	{
		is_full_path = FALSE;
		char* tmp = (char*) (*pth != '?' ? op_strpbrk(pth, "/?") : NULL);
		char save_c = 0;
		if (tmp)
		{
			save_c = *tmp;
			*tmp = '\0';
		}

		int test = 1;
		BOOL is_prefix = FALSE;
		CookiePath* cp = LastChild();

		if(create)
		{
			while (cp && test>0)
			{
				test = UriUnescape::strcmp(cp->PathPart().CStr(), pth, UriUnescape::Safe);
				if (test > 0)
					cp = cp->Pred();
			}
		}
		else
		{
			int len1 = op_strlen(pth);
			int clen;

			while(cp && test>0)
			{
				clen = cp->PathPart().Length();

				if(clen < len1 && UriUnescape::isstrprefix(cp->PathPart().CStr(), pth, UriUnescape::All))
				{
					is_prefix = TRUE;
					test = 0;
				}
				else
					test = UriUnescape::strcmp(cp->PathPart().CStr(), pth, UriUnescape::Safe);
				if (test > 0)
					cp = cp->Pred();

			}
		}

		CookiePath* next_cp = cp;
		if (test != 0 || !cp)
		{
			if (!create)
			{
				if (tmp)
					*tmp = save_c;

				return this;
			}

			next_cp = CookiePath::CreateL(pth);

			if (cp)
				next_cp->Follow(cp);
			else if (FirstChild())
				next_cp->Precede(FirstChild());
			else
				next_cp->Under(this);
		}

		BOOL local_is_full_path = FALSE;
		if (tmp)
		{
			*tmp = save_c;
			if(save_c == '/')
				local_is_full_path = TRUE;
		}

		const char* next_path = (tmp && !is_prefix) ? (save_c == '?' ? tmp : tmp+1) : 0;
		CookiePath* return_cp = next_cp->GetCookiePathL(next_path, create, is_full_path);

		if(return_cp == next_cp && tmp && local_is_full_path && !is_prefix)
			is_full_path = TRUE;

		return return_cp;
	}
	else
	{
		return this;
	}
}

CookiePath *CookiePath::GetNextPrefix()
{
	OpStringC8 pth( PathPart());
	if(pth.IsEmpty())
		return NULL;

	CookiePath *cp = Pred();

	int len1 = pth.Length();
	int clen;
	int test = 1;
	
	while(cp && test!=0)
	{
		clen = cp->PathPart().Length();
		if(clen < len1 && UriUnescape::isstrprefix(cp->PathPart().CStr(), pth.CStr(), UriUnescape::All))
			test = 0;
		else
			test = UriUnescape::strcmp(cp->PathPart().CStr(), pth.CStr(), UriUnescape::Safe);
		if (test != 0)
			cp = cp->Pred();
	}

	return (test == 0 ? cp : NULL);
}

Cookie* CookiePath::GetCookieL(Cookie_Item_Handler *params, BOOL create)
{
	Cookie* ck = LocalGetCookie(params->name);
	if (!create)
		return ck;

	if (ck)
	{
#ifdef _ASK_COOKIE
		ServerName *servername = (ServerName *) params->url->GetAttribute(URL::KServerName, (const void*) 0);
		COOKIE_DELETE_ON_EXIT_MODE disc_mode =  (servername ? servername->GetDeleteCookieOnExit(TRUE) : COOKIE_EXIT_DELETE_DEFAULT);
		BOOL default_disc_mode = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AcceptCookiesSessionOnly);

		if(
			((default_disc_mode  && disc_mode != COOKIE_NO_DELETE_ON_EXIT) ||
			 (!default_disc_mode  && disc_mode == COOKIE_DELETE_ON_EXIT))&&
		   !params->flags.discard_at_exit && !ck->DiscardAtExit())
			params->flags.discard_at_exit= TRUE;
#endif
		if(!params->flags.accept_updates)
			params->flags.accept_updates = ck->AcceptUpdates();
		if(!params->flags.accepted_as_third_party)
			params->flags.accepted_as_third_party = ck->GetAcceptedAsThirdParty();
		if(!params->flags.have_password)
			params->flags.have_password = ck->GetHavePassword();
		if(!params->flags.have_authentication)
			params->flags.have_authentication = ck->GetHaveAuthentication();
		OP_DELETE(ck);
		// *** Don't save an expired cookie.
		if(params->expire && params->expire <
			(time_t) (g_op_time_info->GetTimeUTC()/1000.0)
			)
			return NULL;
	}
	
	ck = Cookie::CreateL(params);

	ck->Into(&cookie_list);
	
	return ck;
}

Cookie* CookiePath::LocalGetCookie(const OpStringC8 &nme)
{
	if (nme.IsEmpty())
		return 0;

	Cookie* ck = (Cookie*) cookie_list.First();
	while (ck && ck->Name().Compare(nme) != 0)
		ck = ck->Suc();

	return ck;
}


BOOL CookiePath::HasCookies(time_t this_time)
{
	Cookie* ck = (Cookie*) cookie_list.First();
	while (ck)
	{
		if (!ck->DiscardAtExit() && ck->Expires() > this_time)
			return TRUE;
		ck = ck->Suc();
	}
	CookiePath* cp = (CookiePath*) FirstChild();
	while (cp)
	{
		if (cp->HasCookies(this_time))
			return TRUE;
		cp = cp->Suc();
	}
	return FALSE;
}

// Return TRUE if empty
BOOL CookiePath::FreeUnusedResources()
{
	BOOL ret = cookie_list.Empty();

	CookiePath* next_cp = (CookiePath*) FirstChild();
	while (next_cp)
	{
		CookiePath* cp =next_cp;
		next_cp = cp->Suc();

		if(cp->FreeUnusedResources())
		{
			cp->Out();
			OP_DELETE(cp);
		}
		else
			ret = FALSE;
	}

	return ret;
}


#ifdef DISK_COOKIES_SUPPORT
void CookiePath::ReadCookiesL(DataFile &fp, unsigned long  ver_no)
{
	Cookie_Item_Handler loaded_cookie;
	ANCHOR(Cookie_Item_Handler, loaded_cookie);
	OpStackAutoPtr<DataFile_Record> rec(0);

	rec.reset(fp.GetNextRecordL());
	while (rec.get() && rec->GetTag() != TAG_COOKIE_PATH_END)
	{
		switch(rec->GetTag())
		{
		case TAG_COOKIE_PATH_ENTRY:
			{
				rec->IndexRecordsL();

				OpString8 pth_name;
				ANCHOR(OpString8, pth_name);

				rec->GetIndexedRecordStringL(TAG_COOKIE_PATH_NAME, pth_name);

				BOOL is_full=FALSE;

				CookiePath* cp = GetCookiePathL(pth_name.CStr(), TRUE, is_full);

				if(cp == NULL)
				{
					LEAVE(OpStatus::ERR_NO_MEMORY);
				}

				cp->ReadCookiesL(fp, ver_no);
			}
			break;
		case TAG_COOKIE_ENTRY:
			{
				loaded_cookie.Clear();
				
				rec->IndexRecordsL();
				
				rec->GetIndexedRecordStringL(TAG_COOKIE_NAME, loaded_cookie.name);
				rec->GetIndexedRecordStringL(TAG_COOKIE_VALUE, loaded_cookie.value);
				
				loaded_cookie.expire = (time_t) rec->GetIndexedRecordUInt64L(TAG_COOKIE_EXPIRES);
				loaded_cookie.last_used = (time_t) rec->GetIndexedRecordUInt64L(TAG_COOKIE_LAST_USED);
				loaded_cookie.last_sync = (time_t) rec->GetIndexedRecordUInt64L(TAG_COOKIE_LAST_SYNC);

				loaded_cookie.flags.secure = rec->GetIndexedRecordBOOL(TAG_COOKIE_SECURE);
				loaded_cookie.flags.httponly = rec->GetIndexedRecordBOOL(TAG_COOKIE_HTTP_ONLY);
				
				loaded_cookie.flags.full_path_only = rec->GetIndexedRecordBOOL(TAG_COOKIE_NOT_FOR_PREFIX);
				loaded_cookie.flags.only_server = rec->GetIndexedRecordBOOL(TAG_COOKIE_ONLY_SERVER);
				loaded_cookie.flags.assigned = rec->GetIndexedRecordBOOL(TAG_COOKIE_ASSIGNED);
				loaded_cookie.flags.protected_cookie = rec->GetIndexedRecordBOOL(TAG_COOKIE_PROTECTED);
				
				loaded_cookie.version = rec->GetIndexedRecordUIntL(TAG_COOKIE_VERSION);

				switch (loaded_cookie.version)
				{
				case 0:
					loaded_cookie.cookie_type = NETSCAPE_COOKIE;
					break;
				case 1:
					loaded_cookie.cookie_type = COOKIE2_COOKIE;
					break;
				default:
					loaded_cookie.cookie_type = UNKNOWN_COOKIE;
				}

				rec->GetIndexedRecordStringL(TAG_COOKIE_COMMENT, loaded_cookie.comment);
				rec->GetIndexedRecordStringL(TAG_COOKIE_COMMENT_URL, loaded_cookie.comment_URL);
				rec->GetIndexedRecordStringL(TAG_COOKIE_RECVD_DOMAIN, loaded_cookie.domain);
				if(fp.AppVersion() >COOKIES_FILE_VERSION_BUGGY_RECVD_PATH)
				{
					rec->GetIndexedRecordStringL(TAG_COOKIE_RECVD_PATH, loaded_cookie.recvd_path);
				}
				else
				{
					OpStringS8 temp_path;
					ANCHOR(OpStringS8, temp_path);

					rec->GetIndexedRecordStringL(TAG_COOKIE_RECVD_PATH, temp_path);

					if(temp_path.Compare("/") == 0)
						loaded_cookie.recvd_path.TakeOver(temp_path);
					else
						LEAVE_IF_ERROR(loaded_cookie.recvd_path.SetConcat("/",temp_path));
				}
				rec->GetIndexedRecordStringL(TAG_COOKIE_PORT, loaded_cookie.port);
				
				loaded_cookie.flags.have_password = rec->GetIndexedRecordBOOL(TAG_COOKIE_HAVE_PASSWORD);
				loaded_cookie.flags.have_authentication= rec->GetIndexedRecordBOOL(TAG_COOKIE_HAVE_AUTHENTICATION);
				loaded_cookie.flags.accepted_as_third_party = rec->GetIndexedRecordBOOL(TAG_COOKIE_ACCEPTED_AS_THIRDPARTY);
				
				Cookie* ck = Cookie::CreateL(&loaded_cookie);
				
				ck->Into(&cookie_list);
			}
		}

		rec.reset(fp.GetNextRecordL());
	}
	rec.reset();
}
#endif // DISK_COOKIES_SUPPORT

#ifdef _DEBUG_COOKIES_
void CookiePath::DebugWritePath(FILE* fp)
{
	CookiePath* cp = (CookiePath*) Parent();
	if (cp)
	{
		cp->DebugWritePath(fp);
		fprintf(fp, "/%s", PathPart());
	}
	else
		fprintf(fp, "%s", PathPart());
}

void CookiePath::DebugWriteCookies(FILE* fp)
{
	fprintf(fp, "   ");
	DebugWritePath(fp);
	fprintf(fp, ": \n");
	Cookie* ck = (Cookie*) cookie_list.First();
	while (ck)
	{
		fprintf(fp, "      %s=%s; %lu; %d; %lu %s\n", ck->Name(), ck->Value(), ck->Expires(), ck->Secure(), ck->GetLastUsed(), (ck->DiscardAtExit() ? "; Discard on exit" : ""));
		ck = ck->Suc();
	}

	CookiePath* cp = (CookiePath*) FirstChild();
	while (cp)
	{
		cp->DebugWriteCookies(fp);
		cp = cp->Suc();
	}
}
#endif

#ifdef DISK_COOKIES_SUPPORT
size_t CookiePath::WriteCookiesL(DataFile &fp, time_t this_time, BOOL dry_run)
{
	size_t size = 0;
	Cookie* ck = (Cookie*) cookie_list.First();
	while (ck)
	{
		if(ck->Persistent(this_time))
		{
			DataFile_Record rec(TAG_COOKIE_ENTRY);
			ANCHOR(DataFile_Record,rec);

			rec.SetRecordSpec(fp.GetRecordSpec());

			ck->FillDataFileRecordL(rec);

			if (dry_run)
				size += rec.CalculateLength();
			else
				rec.WriteRecordL(&fp);
		}
		ck = ck->Suc();
	}

	CookiePath* cp = (CookiePath*) FirstChild();
	while (cp)
	{
		if (cp->HasCookies(this_time))
		{
			DataFile_Record rec(TAG_COOKIE_PATH_ENTRY);
			ANCHOR(DataFile_Record,rec);


			rec.SetRecordSpec(fp.GetRecordSpec());
			rec.AddRecordL(TAG_COOKIE_PATH_NAME, cp->PathPart());
			if (dry_run)
				size += rec.CalculateLength();
			else
				rec.WriteRecordL(&fp);

			size += cp->WriteCookiesL(fp, this_time, dry_run);
		}
		cp = cp->Suc();
	}

	{
		DataFile_Record rec(TAG_COOKIE_PATH_END);
		// spec is a pointer to existing field of object fp, there is no need to assert that it's not null
		const DataRecord_Spec *spec = fp.GetRecordSpec();
		rec.SetRecordSpec(spec);
		if (dry_run)
			size += ((rec.GetTag() & MSB_VALUE) == MSB_VALUE) ? spec->idtag_len : rec.CalculateLength();
		else
			rec.WriteRecordL(&fp);
	}
	return size;
}
#endif // DISK_COOKIES_SUPPORT

void CookiePath::RemoveNonPersistentCookies()
{
	time_t this_time = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);
	Cookie* ck = (Cookie*) cookie_list.First();

	while (ck)
	{
		Cookie *ck1 = ck->Suc();
		if (!ck->Persistent(this_time))
		{
			OP_DELETE(ck);
		}
		ck = ck1;
	}
	CookiePath* cp = (CookiePath*) FirstChild();
	while (cp)
	{
		cp->RemoveNonPersistentCookies();
		cp = cp->Suc();
	}
}

void CookiePath::DeleteAllCookies()
{
	Cookie* ck = (Cookie*) cookie_list.First();

	while (ck)
	{
		Cookie *ck1 = ck->Suc();

		OP_DELETE(ck);

		ck = ck1;
	}
	CookiePath* cp = (CookiePath*) FirstChild();
	while (cp)
	{
		cp->DeleteAllCookies();
		cp = cp->Suc();
	}
}

#ifdef WIC_COOKIES_LISTENER
void CookiePath::IterateAllCookies()
{
	Cookie* ck = static_cast<Cookie*>( cookie_list.First() );
	OpCookieIteratorListener* listener = g_cookie_API->GetCookieIteratorListener();
	while( ck )
	{
		listener->OnIterateCookies(*ck);
		ck = ck->Suc();
	}

	CookiePath* cp = static_cast<CookiePath*>( FirstChild() );
	while (cp)
	{
		cp->IterateAllCookies();
		cp = cp->Suc();
	}
}
#endif // WIC_COOKIES_LISTENER

#ifdef _SSL_USE_SMARTCARD_
BOOL CookiePath::CleanSmartCardAuthenticatedCookies()
{
	Cookie *next_ck = (Cookie *) cookie_list.First();

	while(next_ck )
	{
		Cookie *ck = next_ck;
		next_ck =  next_ck->Suc();

		if(ck->GetHaveSmartCardAuthentication())
		{
			OP_DELETE(ck);
		}
	}

	CookiePath *next_cp = LastChild();

	while(next_cp)
	{
		CookiePath *cp = next_cp;
		next_cp = next_cp->Pred();

		if(cp->CleanSmartCardAuthenticatedCookies())
		{
			cp->Out();
			OP_DELETE(cp);
		}
	}

	return cookie_list.Empty() && Empty();
}
#endif


#if defined(QUICK_COOKIE_EDITOR_SUPPORT)
#include "adjunct/quick/dialogs/ServerManagerDialog.h"

void CookiePath::BuildCookieEditorListL(CookieDomain* domain)
{
	Cookie *ck = (Cookie*)cookie_list.First();
	if( ck )
	{
		ck->BuildCookieEditorListL(domain, this);
	}
	
	CookiePath *cp = (CookiePath*)FirstChild();
	while(cp)
	{
		cp->BuildCookieEditorListL(domain);
		cp = cp->Suc();
	}
}

#endif //QUICK_COOKIE_EDITOR_SUPPORT

#ifdef NEED_URL_COOKIE_ARRAY
OP_STATUS CookiePath::BuildCookieList(Cookie ** cookieArray, int * pos, char * pathstr, CookieDomain *domainHolder, BOOL is_export, BOOL match_subpaths/*=FALSE*/)
{
	char* pathstrcopy = pathstr;
	if(!match_subpaths && path.HasContent() && pathstr && *pathstr)
	{
		int p_len = path.Length();
		if(path.Compare(pathstr,p_len) != 0)
			return OpStatus::OK;

		pathstr += p_len; // Remove first part, if path found
		if(*pathstr == '/') pathstr++;
	}

	Cookie *ck = (Cookie*)cookie_list.First();
	if( ck )
	{
		if(match_subpaths && pathstrcopy && *pathstrcopy)	//if a path was specified
		{
			OpString8 fullpath;
			RETURN_IF_ERROR(GetFullPath(fullpath));	// get the full path
			//the internal root part does not start with /, so get rid of it
			fullpath.Delete(0,1);
			int pathlen = fullpath.Length();
			if(!pathlen || (pathlen && fullpath.Compare(pathstrcopy, pathlen) == 0))	//compare against the full path length since we want subpaths, root has null length
				ck->BuildCookieList(cookieArray, pos, domainHolder, this, is_export);
		}
		else
			ck->BuildCookieList(cookieArray, pos, domainHolder, this, is_export);
	}
	
	CookiePath *cp = (CookiePath*)FirstChild();
	while(cp)
	{
		cp->BuildCookieList(cookieArray, pos, pathstr, domainHolder, is_export, match_subpaths);
		cp = cp->Suc();
	}

    return OpStatus::OK;
}

OP_STATUS CookiePath::RemoveCookieList(char * pathstr, char * namestr)
{
	char* pathstrcopy = pathstr;

	if(path.HasContent() && pathstr && *pathstr)
	{
		int p_len = path.Length();
		if(path.Compare(pathstr,p_len) != 0)
			return OpStatus::OK;

		pathstr += p_len; // Remove first part, if path found
		if(*pathstr == '/') pathstr++;
	}

	Cookie *ck = (Cookie*)cookie_list.First();
	while(ck)
	{
		Cookie * next = ck->Suc();
		if(pathstr && *pathstr)	// if a path was specified test against it, otherwise delete it
		{
			OpString8 fullpath;
			RETURN_IF_ERROR(GetFullPath(fullpath));	// get the full path
			//the internal root part does not start with /, so get rid of it
			fullpath.Delete(0,1);
			int pathlen = fullpath.Length();
			if(pathlen && fullpath.Compare(pathstrcopy, pathlen) == 0)	//compare against the full path length since we want subpaths
			{
				OP_DELETE(ck);
			}
		}
		else if((!namestr || !*namestr) || (ck->Name().Compare(namestr) == 0))	// remove it if no Name value was set or if the Name value equals 
		{
			ck->Out();
			OP_DELETE(ck);
		}
		ck = next;
	}
	
	CookiePath *cp = (CookiePath*)FirstChild();
	while(cp)
	{
		cp->RemoveCookieList(pathstr, namestr);
		cp = cp->Suc();
	}
    return OpStatus::OK;
}

#endif // NEED_URL_COOKIE_ARRAY

#ifdef _OPERA_DEBUG_DOC_
void CookiePath::GetMemUsed(DebugUrlMemory &debug)
{
	debug.memory_cookies += sizeof(*this) + path.Length();

	Cookie *ck = (Cookie *) cookie_list.First();
	while(ck)
	{
		ck->GetMemUsed(debug);
		ck = ck->Suc();
	}

	CookiePath *cp = (CookiePath *) FirstChild();
	while(cp)
	{
		cp->GetMemUsed(debug);
		cp = cp->Suc();
	}
}

#endif

OP_STATUS CookiePath::GetFullPath(OpString8& fullpath) const
{
	const CookiePath* cp = this;

	while (cp)
	{
		RETURN_IF_ERROR(fullpath.Insert(0, cp->PathPart()));
		cp = cp->Parent();
		if(cp && cp->PathPart().HasContent())
			RETURN_IF_ERROR(fullpath.Insert(0, "/"));
	}
	RETURN_IF_ERROR(fullpath.Insert(0, "/"));	// root is "/"

	return OpStatus::OK;
}

