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

#include "core/pch.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/cookies/url_cm.h"

CookieDomain* CookieDomain::CreateL(const OpStringC8 &dom)
{
	OpStringS8 domain;
	ANCHOR(OpStringS8, domain);
	OpStackAutoPtr<CookiePath> cp(CookiePath::CreateL(NULL));

	domain.SetL(dom.HasContent() ? dom : OpStringC8(""));
	CookieDomain *cd = OP_NEW_L(CookieDomain, (domain, cp.get()));

	cp.release();
	return cd;
}

#include "modules/hardcore/mem/mem_man.h"
#include "modules/util/timecache.h"

#include "modules/formats/uri_escape.h"

#include "modules/url/url_man.h"
#include "modules/formats/url_dfr.h"

#if defined(QUICK_COOKIE_EDITOR_SUPPORT)
#include "adjunct/quick/dialogs/ServerManagerDialog.h"
#endif

#include "modules/olddebug/tstdump.h"


CookieDomain::CookieDomain(OpStringS8 &dom, CookiePath *cookie_path)
{
	domain.TakeOver(dom);
	path_root = cookie_path;
}

CookieDomain::~CookieDomain()
{
#ifdef DEBUG_COOKIE_MEMORY_SIZE
	g_cookieManager->SubCookieMemorySize(domain.Length() + sizeof(CookieDomain));
#endif
	OP_DELETE(path_root);
	path_root = NULL;
}

#ifdef UNUSED_CODE
void CookieDomain::Clear()
{
	domain.Empty();
	path_root->Clear(FALSE);
	Tree::Clear();
}
#endif // UNUSED_CODE

Cookie* CookieDomain::GetLeastRecentlyUsed(time_t last_used, time_t this_time, BOOL current_only)
{
	Cookie *ck_unused = NULL;
	Cookie* ck = path_root->GetLeastRecentlyUsed(last_used, this_time);

	if(current_only)
		return ck;

	if(ck && !ck->GetLastUsed())
	{
		ck_unused = ck;
		ck = NULL;
	}

	time_t new_last_used = (ck) ? ck->GetLastUsed() : last_used;
	CookieDomain* cd = (CookieDomain*) FirstChild();

	while (cd)
	{
		Cookie* new_ck = cd->GetLeastRecentlyUsed(new_last_used, this_time);
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
		cd = cd->Suc();
	}
	return (ck ? ck : ck_unused);
}

int CookieDomain::GetCookiesCount()
{
	return path_root->GetCookiesCount();
}

// dom must not start or end with '.'
CookiePath* CookieDomain::GetCookiePathL(const char* dom, const char* path, 
										BOOL *is_server, 
										BOOL create, CookieDomain* &lowest_domain, BOOL &is_full_path)
{
	lowest_domain = this;
	is_full_path= FALSE;
	if (dom && *dom)
	{
		const char* insert_dom = dom;
			// 28/11/98 YNP : A better fix for IP-number cookies
		char* tmp = (char*) (Parent() != NULL || op_strspn(dom, "0123456789.") != op_strlen(dom) ?
			  op_strrchr(dom, '.') : NULL);
		char save_c = 0;
		if (tmp)
		{
			save_c = *tmp;
			*tmp = '\0';
			insert_dom = tmp+1;
		}

		int test = 1;
		CookieDomain* cd = LastChild();

		while (cd && test>0)
		{
			test = UriUnescape::stricmp(cd->DomainPart().CStr(), insert_dom, UriUnescape::All);
			if (test > 0)
				cd = cd->Pred();
		}

		CookieDomain* next_cd = cd;
		if (test != 0 || !cd)
		{
			if (!create)
			{
				if(is_server)
					*is_server = FALSE;

				if (tmp)
					*tmp = save_c;

				return path_root->GetCookiePathL(path, create, is_full_path); // 04/12/97 YNP
			}

			next_cd = CookieDomain::CreateL(insert_dom);

			if (cd)
				next_cd->Follow(cd);
			else if (FirstChild())
				next_cd->Precede(FirstChild());
			else
				next_cd->Under(this);
		}

		const char* next_dom = (tmp) ? dom : 0;
		CookiePath* return_cp = next_cd->GetCookiePathL(next_dom, path, 
			is_server, 
			create, lowest_domain, is_full_path);

		if (tmp)
			*tmp = save_c;

		return return_cp;
	}
	else
	{
		return path_root->GetCookiePathL(path, create, is_full_path);
	}
}

void CookieDomain::RemoveNonPersistentCookies()
{
	path_root->RemoveNonPersistentCookies();

	CookieDomain* cd = (CookieDomain*) FirstChild();
	while (cd)
	{
		cd->RemoveNonPersistentCookies();
		cd = cd->Suc();
	}
}

void CookieDomain::DeleteAllCookies(BOOL delete_filters)
{
	path_root->DeleteAllCookies();

	CookieDomain* cd = (CookieDomain*) FirstChild();
	CookieDomain* cd1;
	BOOL delete_item;

	while (cd)
	{
		cd->DeleteAllCookies(delete_filters);
		cd1 = cd->Suc();
		delete_item = TRUE;

		if(!cd->Empty())
			delete_item = FALSE;

		ServerName *sn = cd->GetFullDomain();
		if(!delete_filters && delete_item )
		{
			if (sn &&
				(
#if defined(_ASK_COOKIE)
				 sn->GetAcceptCookies()           != COOKIE_DEFAULT ||
				 sn->GetAcceptIllegalPaths() != COOKIE_ILLPATH_DEFAULT ||
				 sn->GetDeleteCookieOnExit() != COOKIE_EXIT_DELETE_DEFAULT ||
#endif // _ASK_COOKIE)
				 sn->GetAcceptThirdPartyCookies() != COOKIE_DEFAULT))
			{
				delete_item = FALSE;
			}
		}
		else if( delete_item && sn)
		{
#if defined(_ASK_COOKIE)
			sn->SetAcceptIllegalPaths(COOKIE_ILLPATH_DEFAULT);
			sn->SetAcceptCookies(COOKIE_DEFAULT);
			sn->SetDeleteCookieOnExit(COOKIE_EXIT_DELETE_DEFAULT);
#endif // _ASK_COOKIE)
			sn->SetAcceptThirdPartyCookies(COOKIE_DEFAULT);
		}

		if(delete_item)
		{
			cd->Out();
			OP_DELETE(cd);
		}

		cd = cd1;
	}
}

#ifdef WIC_COOKIES_LISTENER
void CookieDomain::IterateAllCookies()
{
	path_root->IterateAllCookies();

	CookieDomain* cookieDomain = static_cast<CookieDomain*>( FirstChild() );
	while(cookieDomain)
	{
		cookieDomain->IterateAllCookies();
		cookieDomain = cookieDomain->Suc();
	}
}
#endif // WIC_COOKIES_LISTENER

BOOL CookieDomain::HasCookies(time_t this_time)
{
	if (path_root->HasCookies(this_time))
		return TRUE;

	CookieDomain* cd = (CookieDomain*) FirstChild();
	while (cd)
	{
		if (cd->HasCookies(this_time))
			return TRUE;
		cd = cd->Suc();
	}
#ifdef _ASK_COOKIE
	{
		ServerName *server = GetFullDomain();
		if(server && (server->GetAcceptCookies() != COOKIE_DEFAULT || 
			server->GetAcceptThirdPartyCookies() != COOKIE_DEFAULT ||
			server->GetAcceptIllegalPaths() != COOKIE_ILLPATH_DEFAULT ||
			server->GetDeleteCookieOnExit() != COOKIE_EXIT_DELETE_DEFAULT))
			return TRUE;
	}
#endif
	return FALSE;
}

// Return TRUE if empty
BOOL CookieDomain::FreeUnusedResources()
{
	BOOL ret = path_root->FreeUnusedResources();

	CookieDomain* next_cd = (CookieDomain*) FirstChild();
	while (next_cd)
	{
		CookieDomain* cd =next_cd;
		next_cd = cd->Suc();

		if(cd->FreeUnusedResources())
		{
			cd->Out();
			OP_DELETE(cd);
		}
		else
			ret = FALSE;
	}

#ifdef _ASK_COOKIE
	if(ret)
	{
		ServerName *server = GetFullDomain();
		if(server && (server->GetAcceptCookies() != COOKIE_DEFAULT || 
			server->GetAcceptThirdPartyCookies() != COOKIE_DEFAULT ||
			server->GetAcceptIllegalPaths() != COOKIE_ILLPATH_DEFAULT ||
			server->GetDeleteCookieOnExit() != COOKIE_EXIT_DELETE_DEFAULT))
			ret = FALSE;
	}
#endif

	return ret;
}


#ifdef DISK_COOKIES_SUPPORT
void CookieDomain::ReadCookiesL(DataFile &fp, unsigned long  ver_no)
{
	// Use new format
	if (Parent() && (Parent()->Parent() ||  
		domain.Compare("local") == 0 ||
		domain.SpanOf("0123456789.") == domain.Length()))
		path_root->ReadCookiesL(fp, ver_no);

	int ccount;
	Cookie* ck = NULL;
	time_t this_time = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);

	while ((ccount = GetCookiesCount()) > g_cookieManager->GetMaxCookiesInDomain())
	{
		ck = GetLeastRecentlyUsed(this_time, this_time);
		
		// delete if still necessary
		if (ck && (GetCookiesCount() > g_cookieManager->GetMaxCookiesInDomain()))
		{
			OP_DELETE(ck);
		}
		if(!ck)
			break;
	}

	OpStackAutoPtr<DataFile_Record> rec(0);

	rec.reset(fp.GetNextRecordL());
	while (rec.get() && rec->GetTag() != TAG_COOKIE_DOMAIN_END)
	{
		if(rec->GetTag() == TAG_COOKIE_DOMAIN_ENTRY)
		{
			rec->IndexRecordsL();

			OpString8 dom_name;
			ANCHOR(OpString8, dom_name);

			rec->GetIndexedRecordStringL(TAG_COOKIE_DOMAIN_NAME, dom_name);

			if(dom_name.Compare("/localfile/") == 0)
				dom_name.SetL("$localfile$");

			CookieDomain* pcd = LastChild();
			CookieDomain* cd = CookieDomain::CreateL(dom_name);

			if(pcd && dom_name.HasContent())
			{
				INT32 test = 1;
				
				while (pcd && test>0)
				{
					test = pcd->DomainPart().CompareI(dom_name);
					if (test > 0)
						pcd = pcd->Pred();
				}
				if (pcd)
					cd->Follow(pcd);
				else
					cd->Precede(FirstChild());
			}
			else
				cd->Under(this);

#ifdef _ASK_COOKIE
			BYTE mode;
			ServerName *server = cd->GetFullDomain();

			if(server)
			{
				mode = (BYTE) rec->GetIndexedRecordUIntL(TAG_COOKIE_SERVER_ACCEPT_FLAG);
				if(mode>=1 && mode <= 5)
				{
					COOKIE_MODES cmode = COOKIE_DEFAULT;
					
					switch(mode)
					{
					case 1: 
						cmode = COOKIE_ALL; 
						break;
					case 2: 
						cmode = COOKIE_NONE; 
						break;
					case 3:
						cmode = COOKIE_ALL; 
						break;
					case 4:
						cmode = COOKIE_NONE; 
						break;
					case 5:
						cmode = COOKIE_SEND_NOT_ACCEPT_3P;
						break;
					}
					
					server->SetAcceptCookies(cmode);
				}
				
				mode = (BYTE) rec->GetIndexedRecordUIntL(TAG_COOKIE_SERVER_ACCEPT_THIRDPARTY);
				if(mode>=1 && mode <= 5)
				{
					COOKIE_MODES cmode = COOKIE_DEFAULT;
					
					switch(mode)
					{
					case 1:
						cmode = COOKIE_ACCEPT_THIRD_PARTY; 
						break;
					case 2: 
						cmode = COOKIE_NO_THIRD_PARTY; 
						break;
					case 3: 
						cmode = COOKIE_ACCEPT_THIRD_PARTY; 
						break;
					case 4: 
						cmode = COOKIE_NO_THIRD_PARTY; 
						break;
					case 5:
						cmode = COOKIE_SEND_NOT_ACCEPT_3P;
						break;
					}
					
					server->SetAcceptThirdPartyCookies(cmode);
				}
				
				mode = (BYTE) rec->GetIndexedRecordUIntL(TAG_COOKIE_DOMAIN_ILLPATH);
				if(mode>=1 && mode <= 2)
				{
					COOKIE_ILLPATH_MODE cmode = COOKIE_ILLPATH_DEFAULT;
					
					switch(mode)
					{
					case 1: cmode = COOKIE_ILLPATH_REFUSE; break;
					case 2: cmode = COOKIE_ILLPATH_ACCEPT; break;
					}
					
					server->SetAcceptIllegalPaths(cmode);
				}

				mode = (BYTE) rec->GetIndexedRecordUIntL(TAG_COOKIE_DELETE_COOKIES_ON_EXIT);
				if(mode>=1 && mode <= 2)
				{
					COOKIE_DELETE_ON_EXIT_MODE cmode = COOKIE_EXIT_DELETE_DEFAULT;
					
					switch(mode)
					{
					case 1: cmode = COOKIE_NO_DELETE_ON_EXIT; break;
					case 2: cmode = COOKIE_DELETE_ON_EXIT; break;
					}
					
					server->SetDeleteCookieOnExit(cmode);
				}
			}
#endif

			cd->ReadCookiesL(fp, ver_no);
		}

		rec.reset(fp.GetNextRecordL());
	}
	rec.reset();
}
#endif // DISK_COOKIES_SUPPORT

ServerName *CookieDomain::GetFullDomain()
{
	if(domain_sn == NULL && domain.HasContent())
	{
		ServerName *parent_sn = (Parent() ? Parent()->GetFullDomain() : NULL);

		const char *name = domain.CStr();

		if(parent_sn && parent_sn->Name() && (parent_sn->GetName().Compare("local") != 0 || parent_sn->GetNameComponentCount() > 1))		{
			if(op_strlen(parent_sn->Name()) + domain.Length() +2 >= g_memory_manager->GetTempBuf2kLen())
				return NULL;

			char *temp = (char *) g_memory_manager->GetTempBuf2k();

			temp[0] = 0;
			StrMultiCat(temp, domain.CStr(), ".", parent_sn->Name());
			name = temp;
		}

		domain_sn = g_url_api->GetServerName(name, TRUE);
	}
	return domain_sn;
}

#ifdef COOKIE_ENABLE_FULLDOMAIN2_API
void CookieDomain::GetFullDomain(char *domstring, unsigned int maxlen, BOOL exclude_local_domain)
{
	if(domain.HasContent())
	{
		if(exclude_local_domain && *domstring && Parent() && Parent()->Parent() == NULL && domain.Compare("local") == 0)
		{
			// Do not include .local part if exclude_local_domain == TRUE, but only if there is already a name printed
			return;
		}
		if((op_strlen(domstring) + domain.Length() +2) >= maxlen)
		{
			domstring[0] = '\0';
			return;
		}
		StrMultiCat(domstring, (*domstring ? "." : NULL), domain.CStr());
	}
	if(Parent())
		Parent()->GetFullDomain(domstring,maxlen, exclude_local_domain);
}
#endif // COOKIE_ENABLE_FULLDOMAIN2_API

#ifdef DISK_COOKIES_SUPPORT
size_t CookieDomain::WriteCookiesL(DataFile &fp, time_t this_time, BOOL dry_run)
{
	size_t size = 0;
	if (Parent())
		size = path_root->WriteCookiesL(fp, this_time, dry_run);

	CookieDomain* cd = (CookieDomain*) FirstChild();
	while (cd)
	{
#ifdef _ASK_COOKIE
		ServerName *server = cd->GetFullDomain();

		if(server && server->GetAcceptCookies() == COOKIE_DEFAULT &&
			server->GetAcceptThirdPartyCookies() == COOKIE_DEFAULT && 
			server->GetAcceptIllegalPaths() == COOKIE_ILLPATH_DEFAULT)
			server = NULL;
#endif

#ifdef _ASK_COOKIE
		if (server || cd->HasCookies(this_time))
#else
		if (cd->HasCookies(this_time))
#endif
		{
			DataFile_Record rec(TAG_COOKIE_DOMAIN_ENTRY);
			ANCHOR(DataFile_Record,rec);
			DataFile_Record field(TAG_COOKIE_DOMAIN_NAME);
			ANCHOR(DataFile_Record,field);

			field.SetRecordSpec(fp.GetRecordSpec());
			rec.SetRecordSpec(fp.GetRecordSpec());

			field.AddContentL(cd->DomainPart());
			if (dry_run)
				size += field.CalculateLength();
			else
				field.WriteRecordL(&rec);
#ifdef _ASK_COOKIE
			if(server)
			{
				uint8 val=0;

				switch(server->GetAcceptCookies())
				{
				case COOKIE_ALL:
					val = 1;
					break;
				case COOKIE_NONE:
					val = 2;
					break;
				case COOKIE_SEND_NOT_ACCEPT_3P:
					val = 5; 
					break;
				default:
					break;
				}

				if(val)
					rec.AddRecordL(TAG_COOKIE_SERVER_ACCEPT_FLAG, val);

				val = 0;
				switch(server->GetAcceptThirdPartyCookies())
				{
				case COOKIE_ACCEPT_THIRD_PARTY:
					val = 1; 
					break;
				case COOKIE_NO_THIRD_PARTY:
					val = 2;
					break;
				case COOKIE_SEND_NOT_ACCEPT_3P:
					val = 5; 
					break;
				default: 
					break;
				}

				if(val)
					rec.AddRecordL(TAG_COOKIE_SERVER_ACCEPT_THIRDPARTY, val);

				val = 0;

				switch(server->GetAcceptIllegalPaths())
				{
				case COOKIE_ILLPATH_REFUSE:
					val = 1;
					break;
				case COOKIE_ILLPATH_ACCEPT:
					val = 2;
					break;
				}
				if(val)
					rec.AddRecordL(TAG_COOKIE_DOMAIN_ILLPATH, val);

				val = 0;

				switch(server->GetDeleteCookieOnExit())
				{
				case COOKIE_NO_DELETE_ON_EXIT:
					val = 1;
					break;
				case COOKIE_DELETE_ON_EXIT:
					val = 2;
					break;
				}
				if(val)
					rec.AddRecordL(TAG_COOKIE_DELETE_COOKIES_ON_EXIT, val);
			}
#endif // _ASK_COOKIE
			if (dry_run)
				size += rec.CalculateLength();
			else
				rec.WriteRecordL(&fp);
			size += cd->WriteCookiesL(fp, this_time, dry_run);
		}
		cd = cd->Suc();
	}

	{
		DataFile_Record rec(TAG_COOKIE_DOMAIN_END);
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

#if defined(QUICK_COOKIE_EDITOR_SUPPORT)

#include "adjunct/quick/dialogs/ServerManagerDialog.h"
void CookieDomain::BuildCookieEditorListL()
{
	if( Parent())
	{
		path_root->BuildCookieEditorListL(this);
	}

	CookieDomain *cd = (CookieDomain*)FirstChild();
	while( cd )
	{
		cd->BuildCookieEditorListL();
		cd = cd->Suc();
	}
}

#endif //QUICK_COOKIE_EDITOR_SUPPORT

#ifdef NEED_URL_COOKIE_ARRAY

OP_STATUS CookieDomain::BuildCookieList(Cookie ** cookieArray, int * pos, char * domainstr, char * pathstr, BOOL is_export, BOOL match_subpaths)
{
    BOOL domainfound = FALSE;
    char * domainpos = NULL;
	
	if(domain.HasContent() && domainstr && *domainstr)
    {
		if(domain.Compare(domainstr) != 0)		// check ip-domains directly
		{
			char * lastpart;
			domainpos = op_strrchr(domainstr,'.');
			if(domainpos == NULL) 
				lastpart = domainstr; 
			else
				lastpart = domainpos + 1;

			if(domain.Compare(lastpart) != 0)
				return OpStatus::OK;

			domainfound = TRUE;
		}
    }

    if(domainfound && domainpos) // Remove last part
        *domainpos = '\0';

	if( Parent() )
	{
		path_root->BuildCookieList(cookieArray, pos, pathstr, this, is_export, match_subpaths);
	}

	CookieDomain *cd = (CookieDomain*)FirstChild();
	while( cd )
	{
		cd->BuildCookieList(cookieArray, pos, domainstr, pathstr, is_export, match_subpaths);
		cd = cd->Suc();
	}

    if(domainfound && domainpos) // Return last part
        *domainpos = '.';

    return OpStatus::OK;
}

OP_STATUS CookieDomain::RemoveCookieList(char * domainstr, char * pathstr, char * namestr)
{
    BOOL domainfound = FALSE;
    char * domainpos = NULL;

	if(domain.HasContent() && domainstr && *domainstr)
    {
		if(domain.Compare(domainstr) != 0)		// check ip-domains directly
		{
			char * lastpart;
			domainpos = op_strrchr(domainstr,'.');
			if(domainpos == NULL) 
				lastpart = domainstr; 
			else
				lastpart = domainpos + 1;

			if(domain.Compare(lastpart) != 0)
				return OpStatus::OK;

			domainfound = TRUE;
		}
    }

    if(domainfound && domainpos) // Remove last part
        *domainpos = '\0';

	if( Parent() )
	{
		path_root->RemoveCookieList(pathstr, namestr);
	}

	CookieDomain *cd = (CookieDomain*)FirstChild();
	while( cd )
	{
		cd->RemoveCookieList(domainstr, pathstr, namestr);
		cd = cd->Suc();
	}

    if(domainfound && domainpos) // Return last part
        *domainpos = '.';

    return OpStatus::OK;
}

#endif // NEED_URL_COOKIE_ARRAY

#ifdef _OPERA_DEBUG_DOC_
void CookieDomain::GetMemUsed(DebugUrlMemory &debug)
{
	debug.memory_cookies += sizeof(*this) + domain.Length()+1;

	path_root->GetMemUsed(debug);

	CookieDomain *cd = (CookieDomain *) FirstChild();
	while(cd)
	{
		cd->GetMemUsed(debug);
		cd = cd->Suc();
	}
}
#endif




#ifdef _DEBUG_COOKIES_
void CookieDomain::DebugWriteDomain(FILE* fp)
{
	CookieDomain* cd = (CookieDomain*) Parent();
	if (cd)
		cd->DebugWriteDomain(fp);
	if (cd)
		fprintf(fp, ".%s", DomainPart());
	else
		fprintf(fp, "%s", DomainPart());
}

void CookieDomain::DebugWriteCookies(FILE* fp)
{
	DebugWriteDomain(fp);
#ifdef _ASK_COOKIE

	char *domainname = (char *) g_memory_manager->GetTempBuf();
	domainname[0] = '\0';
	GetFullDomain(domainname, g_memory_manager->GetTempBufLen());

	if(domainname[0])
	{
		ServerName *server = urlManager->GetServerNameNoLocal(domainname,FALSE);
		if(server)
			switch(server->GetAcceptCookies())
			{
				case COOKIE_NONE: fprintf(fp,"   ->   Cookies disabled for server");
					break;
				case COOKIE_ALL: fprintf(fp,"   ->   Cookies always accepted for server");
					break;
			}
	}
#endif
	fprintf(fp, "\n");
	if (Parent())
		path_root->DebugWriteCookies(fp);

	CookieDomain* cd = (CookieDomain*) FirstChild();
	while (cd)
	{
		cd->DebugWriteCookies(fp);
		fprintf(fp, "\n");
		cd = cd->Suc();
	}
}
#endif

