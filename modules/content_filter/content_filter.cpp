/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifndef FILTER_CAP_MODULE

#ifdef URL_FILTER

#include "modules/logdoc/markup.h"
#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/util/str.h"
#include "modules/util/opstring.h"
#include "modules/dochand/fraud_check.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/rootstore/auto_update_versions.h"

#include "modules/pi/OpSystemInfo.h"

#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/impl/backend/prefssectioninternal.h"

#include "modules/dochand/win.h"

#if defined SUPPORT_VISUAL_ADBLOCK && defined QUICK
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#endif

#include "modules/content_filter/content_filter.h"

#ifdef WEB_TURBO_MODE
#include "modules/util/opstringset.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#endif //WEB_TURBO_MODE

#ifdef SYNC_CONTENT_FILTERS
#include "modules/sync/sync_util.h"
#endif // SYNC_CONTENT_FILTERS

#include "modules/windowcommander/src/WindowCommander.h"

// Faster version #2 (optimized for blocked content too)
/* BOOL URLFilter::MatchUrlPattern(const uni_char *path, const uni_char *pattern)
{
	if (!path || !pattern)
		return FALSE;

	for (; *path && *pattern; ++path, ++pattern)
	{
		// Look for a generic string
		if (*pattern == '*')
		{
			++pattern;

			const uni_char c = *pattern;

			OP_ASSERT(c!='*'); // A double '*' is an error in the sites list, not an interesting case

			if (!c) // The pattern ends with '*', so everything is fine
				return TRUE;

			if (!*path) // The pattern has other characters, so you need at least another character in the path too
				return FALSE;

			// Optimized recursive call


			while (TRUE)
			{
				// Look for the next occurrence of the current pattern character
				while (c != *path && *path)
					path++;

				if (!*path) // Path finished without matches
					return FALSE;

				path++;

				// Fast check: skip characters that are equals
				const uni_char *pattern2=pattern+1;
				const uni_char *path2=path;
				uni_char c2;

				while(c2=*pattern2)
				{
					if(c2=='*' || c2!=*path2)
						break;

					pattern2++;
					path2++;
				}

				// If the first character is a match, check the remaining characters, starting from the second one
				if (c2=='*')
				{
					if(!pattern2[1]) // Pattern finished with '*': match!
						return TRUE;

					if(MatchUrlPattern(path2, pattern2))
						return TRUE;
				}
				else
				{
					// Path and pattern finished: match
					if(!*path2 && !*pattern2)
						return TRUE;
				}
			}
		}

		// Different starting character: no match
		if (*pattern != *path)
			return FALSE;

		// Skip duplicate slashes
		if (pattern[0] == '/')
			while (pattern[1] == '/')
				++pattern;
		if (path[0] == '/')
			while (path[1] == '/')
				++path;
	}

	// Both finished
	if (!*pattern && !*path)
		return TRUE;

	// Pattern not finished: only '*' is allowed
	if (*pattern == '*')
	{
		pattern++;

		OP_ASSERT(*pattern != '*'); // A double '*' is an error in the sites list, not an interesting case

		if (!*pattern)
			return TRUE;
	}

	// Pattern finished, path no, or the opposite
	return FALSE;
}*/

#ifdef CF_BLOCK_INCLUDING_PORT_NUMBER
BOOL URLFilter::PortCompareFilters(char *url1, char *url2)
{
	OpString str1;
	OpString str2;

	if(	OpStatus::IsError(str1.Set(url1)) ||
		OpStatus::IsError(str2.Set(url2)) ||
		OpStatus::IsError(AddPortToUrlFilter(&str1)) ||
		OpStatus::IsError(AddPortToUrlFilter(&str2))
		)
	{
		OP_ASSERT(FALSE);  // This function is just used for testing...

		return FALSE;
	}

	return !str1.Compare(str2);
}

BOOL URLFilter::PortCompareFiltersUnchanged(char *url)
{
	OpString str;

	if(	OpStatus::IsError(str.Set(url)) ||
		OpStatus::IsError(AddPortToUrlFilter(&str))
		)
	{
		OP_ASSERT(FALSE);  // This function is just used for testing...

		return FALSE;
	}

	return !str.Compare(url);
}

OP_STATUS URLFilter::AddPortToUrlFilter(OpString *url)
{
	if( !url)
        return OpStatus::ERR_NULL_POINTER;

	int url_len=url->Length();

    if( url_len<=0)
        return OpStatus::ERR_OUT_OF_RANGE;

    char *domain = NULL;
    char *orig_domain = NULL;
    char *path = NULL;
    OP_STATUS err = OpStatus::OK;

    do
    {
        // Only HTTP and HTTPS accept this modification
        int protocol = 0;
        if( !uni_strncmp( url->CStr(), "http://", sizeof("http://") - 1) )
            protocol = sizeof("http://") - 1;
        else if( !uni_strncmp( url->CStr(), "https://", sizeof("https://") - 1) )
            protocol = sizeof("https://") - 1;

        if( protocol )
        {
			// To be on the safe side, only allow URLs with a '/' (this prevent the port to be misleading, ex: ==>  http://goo*.com* != http://goo*.com:80*)
			uni_char *uni_domain=url->CStr()+protocol;
            uni_char *end = uni_strchr(uni_domain , '/' );

            if(end && *(end-1)!='*')
            {
				int end_pos=end-uni_domain;

				domain = OP_NEWA( char, url_len + 1 + 1 + 5 );    // + "*" and + ":80/"
				if( !domain )
				{
					err = OpStatus::ERR_NO_MEMORY;
					break;
				}
				orig_domain=domain;

				uni_cstrlcpy( domain, url->CStr(), url_len + 1 );

				/*bool haveStarAtEnd = false;
				if( domain[url_len - 1] == '*' )
				{
					domain[url_len - 1] = '\0';
					haveStarAtEnd = true;
				}*/

				domain += protocol;

				char *port=op_strchr(domain,':');
				//char *star=op_strchr(domain,'*');

				if(!port || port-domain>end_pos)
				{
					/*if(end[1]) // Save the path only if there is more than the '/'
					{
						path = OP_NEWA( char, url_len-end_pos + 1 );
						if( !path )
						{
							err = OpStatus::ERR_NO_MEMORY;
							break;
						}
						op_strncpy(path, domain+end_pos+1, url_len-end_pos); // Skip the '/'
					}*/

					// Replica of:
					// op_memmove(domain+end_pos+4, domain+end_pos, url_len-end_pos-1);
					int n=url_len-end_pos-protocol;
					char *domain_end_pos=domain+end_pos;

					while(n>=4)
					{
						UINT32 t=*((UINT32 *)(domain_end_pos+n-3));

						*((UINT32 *)(domain_end_pos+n))=t;
						n-=4;
					}
					while(n>0)
					{
						domain_end_pos[n+3]=domain_end_pos[n];
						n--;
					}

					domain_end_pos[0]=':';
					domain_end_pos[1]='8';
					domain_end_pos[2]='0';
					domain_end_pos[3]='/';
					//if( path )
					//	op_strncpy(domain+end_pos+4, path, url_len-end_pos);

					/*if( haveStarAtEnd )
						op_strcat(domain, "*");*/

					url->Set(orig_domain);
				}
			}
        }
    }
    while(FALSE);

    if( orig_domain )
        OP_DELETEA(orig_domain);
    if( path )
        OP_DELETEA(path);

    return err;
}
#endif//CF_BLOCK_INCLUDING_PORT_NUMBER


// Faster version #1 (optimized for unblocked content)
BOOL URLFilter::MatchUrlPattern(const uni_char *path, const uni_char *pattern)
{
    if (!path || !pattern)
		return FALSE;

	const uni_char *original_path = path;

	for (; *path && *pattern; ++path, ++pattern)
	{
		// Look for a generic string
		if (*pattern == '*' || (*pattern=='|' && pattern[1]=='|'))
		{
			const uni_char *validityLimit = NULL;
			BOOL limit = FALSE;

			if(*pattern=='|')
			{
				limit=TRUE;
				validityLimit = uni_strchr(path, '?'); // After '?' we cannot accept matches
				pattern+=2;
			}
			else
				pattern++;

			const uni_char c = *pattern;

			OP_ASSERT(c!='*'); // A double '*' is an error in the sites list, not an interesting case

			if (!c) // The pattern ends with '*', so everything is fine
				return TRUE;

			if (!*path) // The pattern has other characters, so you need at least another character in the path too
				return FALSE;

			// Optimized recursive call


			while (TRUE)
			{
				// with '||' we cannot match the first character, as we need a separator
				if(limit && path == original_path)
					path++;

				BOOL valid=FALSE;

				do
				{
					// Look for the next occurrence of the current pattern character
					while (c != *path && *path)
						path++;

					// '||' requires that the first character is a separator or a '.'
					if(limit)
					{
						if(IsSeparator(*(path-1)) || *(path-1)=='.')
							valid = TRUE;
						else if(*path)
							path++;
					}
				} while (limit && *path && !valid);

				if (!*path) // Path finished without matches
					return FALSE;

				if(validityLimit && path>validityLimit)
					return FALSE;

				// If the first character is a match, check the remaining characters, starting from the second one
				if (MatchUrlPattern(path + 1, pattern + 1))
					return TRUE;

				path++;
			}
		}

		// Different starting character: no match
		if (*pattern != *path)
		{
			if(*pattern=='^')
			{
				if(!IsSeparator(*path))
					return FALSE;
			}
			else
				return FALSE;
		}

		// Skipping duplicate slashes in the URL is more conservative when '^' is present
		// We no longer double slashes in the patterns
		if (pattern[0] == '/' && path[0]=='/' && (path[1]=='/'))
		{
			while (pattern[1] == '/' && path[1] == '/')
			{
				++pattern;
				++path;
			}
			
			if(pattern[1]!='^')
				while (path[1] == '/')
					++path;
		}
	}

	// Both finished
	if (!*pattern && !*path)
		return TRUE;

	// Pattern not finished: only '*' is allowed
	if (*pattern == '*')
	{
		pattern++;

		OP_ASSERT(*pattern != '*'); // A double '*' is an error in the sites list, not an interesting case

		if (!*pattern)
			return TRUE;
	}

	// '^' matches also the end of line
	while(*pattern=='^')
		pattern++;

	if(!*pattern && !*path)
		return TRUE;

	// Pattern finished, path no, or the opposite
	return FALSE;
}



//*************************************************************************

URLFilter::URLFilter() :
match_array(NULL),
m_filterfile(NULL),
m_exclude_rules(FALSE),
m_include_rules(TRUE),
m_unstoppable_rules(TRUE),
#ifdef CF_BLOCK_OPERA_DEBUG
	m_soft_disabled_rules(FALSE),
#endif
m_exclude_include(TRUE),
m_block_mode(BlockModeNormal),
m_listener(NULL),
m_async_listener(NULL),
alg(FILTER_ADAPTIVE)
#ifdef FILTER_BYPASS_RULES
	, m_bypass_rules(TRUE)
#endif
#ifdef FILTER_REDIRECT_RULES
	, m_redirect_rules(TRUE)
#endif
#ifdef SYNC_CONTENT_FILTERS
	, m_sync_enabled(FALSE)
#endif // SYNC_CONTENT_FILTERS
{
}

//*************************************************************************

URLFilter::~URLFilter()
{
#ifdef SYNC_CONTENT_FILTERS
	g_sync_coordinator->RemoveSyncDataClient(this, OpSyncDataItem::DATAITEM_URLFILTER);
#endif // SYNC_CONTENT_FILTERS
	OP_DELETEA(match_array);
}

//*************************************************************************

#ifdef CF_WRITE_URLFILTER

OP_STATUS URLFilter::WriteL()
{
	OP_STATUS s = OpStatus::OK;
	OpFile tmp; ANCHOR(OpFile, tmp);

	g_pcfiles->GetFileL(PrefsCollectionFiles::UrlFilterFile, tmp);

	m_filterfile = OP_NEW_L(PrefsFile, (OP_NEW_L(URLFilterAccessor, ())));
	OpStackAutoPtr<PrefsFile> file_ap(m_filterfile);

	m_filterfile->ConstructL();
	m_filterfile->SetFileL(&tmp);

	TRAP(s, m_filterfile->DeleteAllSectionsL());

	s = m_filterfile->WriteIntL(UNI_L("prefs"), UNI_L("prioritize excludelist"), m_exclude_include ? 1 : 0);
	if (OpStatus::IsSuccess(s))
	{
		if(m_include_rules.GetCount() == 0)
		{
			FilterURLnode *default_node = OP_NEW_L(FilterURLnode, ());

			default_node->SetIsExclude(FALSE);
			default_node->SetURL(UNI_L("*"));

#ifdef SYNC_CONTENT_FILTERS
			LEAVE_IF_ERROR(GenerateGUIDForFilter(default_node));
#endif // SYNC_CONTENT_FILTERS

			s = m_include_rules.InsertURL(default_node);
			if (OpStatus::IsError(s))
			{
				OP_DELETE(default_node);
				return s;
			}
		}
		s = m_include_rules.SaveNodes(this);
		if (OpStatus::IsSuccess(s))
		{
			s = m_exclude_rules.SaveNodes(this);
		}
	}
	TRAP(s, m_filterfile->CommitL());
	m_filterfile = NULL;

	return s;
}

OP_STATUS URLFilter::SaveSingleNode(FilterURLnode *node)
{
	if(node && m_filterfile && node->GetURL()->HasContent())
	{
			OP_NEW_DBG("URLFilter::SaveSingleNode()", "content_filter");
			OP_DBG((UNI_L("WRITING BLOCKED URL: %s\n"), (const uni_char*)node->GetURL()->CStr()));

#ifdef SYNC_CONTENT_FILTERS
			// usually created already, but let's make sure
			RETURN_IF_ERROR(GenerateGUIDForFilter(node));

			OpString guid;

			if(uni_strncmp(node->GUID(), UNI_L("UUID:"), 5))
			{
				RETURN_IF_ERROR(guid.Set(UNI_L("UUID:")));
			}
			RETURN_IF_ERROR(guid.Append(node->GUID()));
#endif // SYNC_CONTENT_FILTERS

			return m_filterfile->WriteStringL(node->Excluded() ? UNI_L("exclude") : UNI_L("include"), (const uni_char*)node->GetURL()->CStr(),
#ifdef SYNC_CONTENT_FILTERS
				guid.CStr()
#else
				NULL
#endif // SYNC_CONTENT_FILTERS
				);
	}
	return OpStatus::OK;
}
#endif // CF_WRITE_URLFILTER

OP_STATUS URLFilter::CreateExcludeList(OpVector<OpString>& list)
{
	return m_exclude_rules.SaveNodesToList(list);
}

BOOL URLFilter::DeleteURL(FilterURLnode *url, BOOL exclusion, OpGadget *extension)
{
#ifdef CF_BLOCK_INCLUDING_PORT_NUMBER
	AddPortToUrlFilter(&url->m_url);
#endif//CF_BLOCK_INCLUDING_PORT_NUMBER

#ifdef SYNC_CONTENT_FILTERS
	if(!extension && !url->GUID())
	{
		// with sync, we need the real node as seen in the tree as we need the GUID.
		int index = (exclusion)?m_exclude_rules.FindExact(url->GetURL()):m_include_rules.FindExact(url->GetURL());
		if(index > -1)
		{
			url = (exclusion)?m_exclude_rules.Get(index):m_include_rules.Get(index);

			// failing to delete the sync item is not critical
			OP_STATUS status = SyncItem(url, OpSyncDataItem::DATAITEM_ACTION_DELETED);
			OP_ASSERT(OpStatus::IsSuccess(status));
			OpStatus::Ignore(status);
		}
	}
#endif // SYNC_CONTENT_FILTERS
	if(extension) // Delete from the extension
	{
		int pos;
		ExtensionList *list=FindList(extension, pos);

		if(!list)
			return FALSE;

		OP_ASSERT(pos>=0);
		BOOL b=	(exclusion  && list->m_exclude_rules.DeleteURL(url)) ||
				(!exclusion && list->m_include_rules.DeleteURL(url));

		if(!list->m_exclude_rules.GetCount() && !list->m_include_rules.GetCount())
			m_extensions.Delete(pos);

		return b;
	}
	else if((exclusion  && m_exclude_rules.DeleteURL(url)) ||	// Delete from the main exclude list
			(!exclusion && m_include_rules.DeleteURL(url)))		// Delete from the main include list
	{
#ifdef SYNC_CONTENT_FILTERS
		// write it out if we managed to delete it successfully
		TRAPD(status, WriteL());
		OpStatus::Ignore(status);
#endif // SYNC_CONTENT_FILTERS
		return TRUE;
	}
	return FALSE;
}

BOOL URLFilter::RemoveExtension(OpGadget *extension)
{
	if(!extension)
		return FALSE;

	int pos;
	ExtensionList *list=FindList(extension, pos);

	if(!list)
		return FALSE;

	m_extensions.Delete(pos);

	return TRUE;
}

BOOL URLFilter::RemoveAllExtensions()
{
	if(!m_extensions.GetCount())
		return FALSE;

	m_extensions.DeleteAll();

	return TRUE;
}

//*************************************************************************

OP_STATUS URLFilter::InitL(OpString& filter_file)
{
	OP_NEW_DBG("URLFilter::InitL()", "urlfilter");
	OP_DBG((UNI_L("file: %s"), filter_file.CStr()));
	OP_STATUS s;
	FilterURLnode* newone;
	OpString key; ANCHOR(OpString, key);
	OpString value; ANCHOR(OpString, value);
	OpString urlfilterfile; ANCHOR(OpString, urlfilterfile);

	OpFile tmp; ANCHOR(OpFile, tmp);

	RETURN_IF_ERROR(tmp.Construct(filter_file.CStr()));

	BOOL urlfilterfile_exists = FALSE;
	if(OpStatus::IsSuccess(tmp.Exists(urlfilterfile_exists)) && urlfilterfile_exists)
	{
		m_filterfile = OP_NEW_L(PrefsFile, (OP_NEW_L(URLFilterAccessor, ())));
		OpStackAutoPtr<PrefsFile> file_ap(m_filterfile);

		m_filterfile->ConstructL();
		m_filterfile->SetFileL(&tmp);
		TRAP(s, m_filterfile->LoadAllL());
		RETURN_IF_ERROR(s);

		PrefsSection *include = m_filterfile->ReadSectionL(UNI_L("include"));
		OpStackAutoPtr<PrefsSection> section_ap(include);

		const PrefsEntry *ientry = include ? include->Entries() : NULL;
#ifdef PI_EXPAND_ENV_VARIABLES
		OpString expanded;
		ANCHOR(OpString, expanded);
		const int max_expanded_size = 5000;
		expanded.ReserveL(max_expanded_size);
#endif

		while (ientry)
		{
			newone = OP_NEW_L(FilterURLnode, ());

			newone->SetIsExclude(FALSE);

			const uni_char *key = ientry->Key();

			OpAutoPtr<FilterURLnode> node_ap(newone);
#ifdef PI_EXPAND_ENV_VARIABLES
			if (OpStatus::IsSuccess(g_op_system_info->ExpandSystemVariablesInString(key, &expanded)))
			{
				RETURN_IF_ERROR(newone->SetURL(expanded.CStr()));
			}
			else
#endif // PI_EXPAND_ENV_VARIABLES
			{
				RETURN_IF_ERROR(newone->SetURL(key));
			}
#ifdef SYNC_CONTENT_FILTERS
			if(ientry->Value())
			{
				newone->SetGUID(ientry->Value());
			}
			OP_DBG((UNI_L("new include rule: id: '%s'; content: '%s'"), newone->GUID(), newone->GetURL()->CStr()));
#endif // SYNC_CONTENT_FILTERS
			RETURN_IF_ERROR(m_include_rules.InsertURL(newone));
			node_ap.release();
			ientry = (const PrefsEntry *) ientry->Suc();
		}

#ifdef FILTER_BYPASS_RULES
		PrefsSection *bypass = m_filterfile->ReadSectionL(UNI_L("bypass"));
		const PrefsEntry *bentry = bypass ? bypass->Entries() : NULL;
		while (bentry)
		{
			newone = OP_NEW_L(FilterURLnode, ());
			newone->SetIsBypass(TRUE);
			const uni_char *key = bentry->Key();
#ifdef PI_EXPAND_ENV_VARIABLES
			if (OpStatus::IsSuccess(g_op_system_info->ExpandSystemVariablesInString(key, &expanded)))
			{
				RETURN_IF_ERROR(newone->SetURL(expanded.CStr()));
			}
			else
#endif // PI_EXPAND_ENV_VARIABLES
			{
				RETURN_IF_ERROR(newone->SetURL(key));
			}
			m_bypass_rules.InsertURL(newone);
			bentry = (const PrefsEntry *) bentry->Suc();
		}
		OP_DELETE(bypass);
#endif // FILTER_BYPASS_RULES
#ifdef FILTER_REDIRECT_RULES
			PrefsSection *redirect = m_filterfile->ReadSectionL(UNI_L("redirect"));
		    const PrefsEntry *rentry = redirect ? redirect->Entries() : NULL;
		    while (rentry)
		    {
		    	newone = OP_NEW_L(FilterURLnode, ());
		    	newone->SetIsRedirect(TRUE);
		    	const uni_char *key = rentry->Key();
#ifdef PI_EXPAND_ENV_VARIABLES
				if (OpStatus::IsSuccess(g_op_system_info->ExpandSystemVariablesInString(key, &expanded)))
		    	{
		    		RETURN_IF_ERROR(newone->SetURL(expanded.CStr()));
		    	}
		    	else
#endif // PI_EXPAND_ENV_VARIABLES
		    	{
		    		RETURN_IF_ERROR(newone->SetURL(key));
		    	}
		    	m_redirect_rules.InsertURL(newone);
		    	rentry = (const PrefsEntry *) rentry->Suc();
		    }
		    OP_DELETE(redirect);
#endif // FILTER_REDIRECT_RULES

		PrefsSection *exclude = m_filterfile->ReadSectionL(UNI_L("exclude"));
		section_ap.reset(exclude);

		if(!exclude)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		const PrefsEntry *eentry = exclude ? exclude->Entries() : NULL;
		while (eentry)
		{
			const uni_char *key = eentry->Key();
#ifdef CF_DONT_ALLOW_BLOCK_ALL
			if(*key == '*' && *(key + 1) == '\0')
			{
				// url is '*'
			    eentry = (const PrefsEntry *) eentry->Suc();
			    continue;
			}
			else if(!cf_strcmp(key, UNI_L("http://*")))
			{
			    // url is 'http://*'
			    eentry = (const PrefsEntry *) eentry->Suc();
			    continue;
			}
#endif // CF_DONT_ALLOW_BLOCK_ALL

			/*#ifdef CF_DONT_BLOCK_OPERA_PROTOCOLS
				if(!uni_strncmp(key, UNI_L("opera:"), 6) || !uni_strncmp(key, UNI_L("about:"), 6))
			    {
			    	// key is an opera URL
			        eentry = (const PrefsEntry *) eentry->Suc();
			        continue;
			    }
			#endif // CF_DONT_BLOCK_OPERA_PROTOCOLS*/

			newone = OP_NEW_L(FilterURLnode, ());
			newone->SetIsExclude(TRUE);

#ifdef SYNC_CONTENT_FILTERS
			// check if it looks like a valid GUID
			if(eentry->Value())
			{
				newone->SetGUID(eentry->Value());
			}
#endif // SYNC_CONTENT_FILTERS
			//key = eentry->Key();
			OpAutoPtr<FilterURLnode> node_ap(newone);
#ifdef PI_EXPAND_ENV_VARIABLES
			if (OpStatus::IsSuccess(g_op_system_info->ExpandSystemVariablesInString(key, &expanded)))
			{
				RETURN_IF_ERROR(newone->SetURL(expanded.CStr()));
			}
			else
#endif // PI_EXPAND_ENV_VARIABLES
			{
				RETURN_IF_ERROR(newone->SetURL(key));
			}
			RETURN_IF_ERROR(m_exclude_rules.InsertURL(newone));
#ifdef SYNC_CONTENT_FILTERS
			OP_DBG((UNI_L("new exclude rule: id: '%s'; content: '%s'"), newone->GUID(), newone->GetURL()->CStr()));
#endif // SYNC_CONTENT_FILTERS
			node_ap.release();
			eentry = (const PrefsEntry *) eentry->Suc();
		}
		m_exclude_include = m_filterfile->ReadBoolL(UNI_L("prefs"), UNI_L("prioritize excludelist"), TRUE);

		m_filterfile = NULL;	//don't use it anymore, remove it from memory altogether
	}
	OP_DBG(("%d exclude rules", m_exclude_rules.GetCount()));
	OP_DBG(("%d include rules", m_include_rules.GetCount()));
	return OpStatus::OK;
}

OP_STATUS URLFilter::InitL()
{
	OpFile file; ANCHOR(OpFile, file);
	OpString filename; ANCHOR(OpString, filename);

	g_pcfiles->GetFileL(PrefsCollectionFiles::UrlFilterFile, file);

	RETURN_IF_ERROR(filename.Set(file.GetFullPath()));

	OP_STATUS status = InitL(filename);

#ifdef WEB_TURBO_MODE
	OpString bypassURLs; ANCHOR(OpString, bypassURLs);
	g_pcnet->GetStringPrefL(PrefsCollectionNetwork::WebTurboBypassURLs, bypassURLs);
	SetWebTurboBypassURLsL(bypassURLs);
#endif //WEB_TURBO_MODE

#ifdef SYNC_CONTENT_FILTERS
	if (OpStatus::IsSuccess(status))
		status = g_sync_coordinator->SetSyncDataClient(this, OpSyncDataItem::DATAITEM_URLFILTER);
#endif // SYNC_CONTENT_FILTERS

	return status;
}

//*************************************************************************


/*OP_STATUS URLFilter::CheckURL(URL* url, BOOL& urlisok, Window *window, URLFilterListener *listener)
{
	if(m_block_mode == BlockModeOff)
	{
		urlisok = TRUE;
		return OpStatus::OK;
	}

	// Double version, because URL version of peregrine is different than core...
		OpString tt;

		RETURN_IF_ERROR(url->GetAttribute(URL::KUniName_Escaped,0,tt,URL::KFollowRedirect)); // don't filter on username/passwords

		return CheckURL(tt.CStr(), urlisok, window, listener);
}*/

//*************************************************************************
OP_STATUS URLFilter::InitializeHardCodedLists()
{
	// Add "unstoppable URLs", if the list is empty
	if(!m_unstoppable_rules.GetCount())
	{
		// Accept data urls as soon as possible (at least before the "*"-prefixed filters), because they can be long and many and kill performance
		// (Data URLs have no server-name or path anyway, only the content itself, which not normally meant to be searched with these filters)
		RETURN_IF_ERROR(m_unstoppable_rules.InsertURLString(UNI_L("data:*"), FALSE, FALSE));

	#ifdef CF_DONT_BLOCK_OPERA_PROTOCOLS
		RETURN_IF_ERROR(m_unstoppable_rules.InsertURLString(UNI_L("opera:*"), FALSE, FALSE));
		RETURN_IF_ERROR(m_unstoppable_rules.InsertURLString(UNI_L("about:*"), FALSE, FALSE));
	#endif  // CF_DONT_BLOCK_OPERA_PROTOCOLS

	// Browser.JS (is it fine to test a GOGI URL?)
	#ifdef GOGI_SITEPATCH_BROWSERJS_URL
		RETURN_IF_ERROR(m_unstoppable_rules.InsertURLString(GOGI_SITEPATCH_BROWSERJS_URL, FALSE, TRUE));
		// Actually browser JS execute a redirect, so also this URL needs to be allowed
		RETURN_IF_ERROR(m_unstoppable_rules.InsertURLString(UNI_L("||get.geo.opera.com/*"), FALSE, FALSE));
	#endif

	// Auto update server
	#ifdef DEFAULT_AUTOUPDATE_SERVER
		RETURN_IF_ERROR(m_unstoppable_rules.InsertURLString(DEFAULT_AUTOUPDATE_SERVER, FALSE, TRUE));
	#endif

	// Extensions should really not be able to block addons.opera.com
	RETURN_IF_ERROR(m_unstoppable_rules.InsertURLString(UNI_L("||addons.opera.com/*"), FALSE, FALSE));
	// Probably extensions should not block extension-host.opera.com.
	// It's not very clear, maybe it's not used at all, but it is referenced internally as origin domain, so it is safer to just white list it
	RETURN_IF_ERROR(m_unstoppable_rules.InsertURLString(UNI_L("||extension-host.opera.com/*"), FALSE, FALSE));

	// Fraud check
	#ifdef TRUST_RATING
		OpString trust_url;

		// Ensure that there is a protocol
		RETURN_IF_ERROR(trust_url.Set(SITECHECK_HOST));

		if(trust_url.Find(UNI_L("://"))<0)
			RETURN_IF_ERROR(trust_url.Insert(0, "http*://"));

		// Ensure that there is some '/' somewhere
		if(trust_url.FindLastOf('/')<8)
			RETURN_IF_ERROR(trust_url.Append("/"));

		RETURN_IF_ERROR(m_unstoppable_rules.InsertURLString(trust_url.CStr(), FALSE, TRUE));
	#endif // TRUST_RATING

	// OSCP

	// CRL

	// Pub suffix
	#ifdef AUTOUPDATE_SERVER
		OpString8 update_url8;

		// Ensure that there is a protocol
		RETURN_IF_ERROR(update_url8.AppendFormat("%s://%s/*", AUTOUPDATE_SCHEME, AUTOUPDATE_SERVER));

		OpString update_url;
		RETURN_IF_ERROR(update_url.Set(update_url8));

		RETURN_IF_ERROR(m_unstoppable_rules.InsertURLString(update_url.CStr(), FALSE, FALSE));
	#endif // AUTOUPDATE_SERVER
	}


#ifdef CF_BLOCK_OPERA_DEBUG
	// Add the "soft diabled rules", like opera:debug
	if(!m_soft_disabled_rules.GetCount())
	{
		FilterURLnode *url_debug=OP_NEW(FilterURLnode, ());
		OP_STATUS ops=OpStatus::ERR_NO_MEMORY;

		if(!url_debug
		    || OpStatus::IsError(ops=url_debug->SetURL(UNI_L("opera:debug")))
		    || OpStatus::IsError(ops=m_soft_disabled_rules.InsertURL(url_debug)))
		{
			OP_DELETE(url_debug);

			return ops;
		}

		OP_ASSERT(m_soft_disabled_rules.GetCount()==1);
	}
#endif

	return OpStatus::OK;
}

OP_STATUS URLFilter::CheckURL(const uni_char* url_str, BOOL& load, Window *window, HTMLLoadContext *html_ctx, BOOL fire_events)
{
	OP_ASSERT(url_str);

	if(!url_str)
		return OpStatus::ERR_NULL_POINTER;

	if (
#ifdef SUPPORT_VISUAL_ADBLOCK
		(window && !window->IsContentBlockerEnabled()) ||
#endif // SUPPORT_VISUAL_ADBLOCK
		(m_block_mode == BlockModeOff)
		)
	{
		load = TRUE;
		return OpStatus::OK;
	}

	URL url=urlManager->GetURL(url_str);

	OP_STATUS ops = CheckURLKernel(&url, url_str, load, window, html_ctx, fire_events);

	OP_NEW_DBG("URLFilter::CheckURL(URL ...)", "content_filter.full");
	OP_DBG((UNI_L("URL: %s %s"), url_str, (load) ? UNI_L(" CAN load") : UNI_L(" CANNOT load")));

	return ops;
}


OP_STATUS URLFilter::CheckURLKernel(URL *url, const uni_char* url_str, BOOL& load, Window *window, HTMLLoadContext *html_user_ctx, BOOL fire_events)
{
	// Possible optimization: understand if there are patterns with only rules dependent on contexts, and skip those if the context is NULL
	OP_ASSERT(url);
	OP_ASSERT(m_block_mode != BlockModeOff);

	if(!url)
		return OpStatus::ERR_NULL_POINTER;

	// There is no real reason for this method to be called when GetURLBypass() is TRUE
	OP_ASSERT(!html_user_ctx || !html_user_ctx->GetURLBypass());

	OpWindowCommander *wc = window ? window->GetWindowCommander() : NULL;
	HTMLLoadContext *html_ctx = html_user_ctx;

	if(html_ctx && html_ctx->GetDomain()==NULL && html_ctx->GetResource() == RESOURCE_UNKNOWN)
		html_ctx = NULL;  // This will Speed-up things

	// If CF_BLOCK_OPERA_DEBUG is defined, the check must be postponed
#ifndef CF_BLOCK_OPERA_DEBUG
	// We _need_ both an include and exclude list of rules
	if(!m_extensions.GetCount() && (!m_exclude_rules.GetCount() || !m_include_rules.GetCount()))
	{
		load=(m_block_mode != BlockModeRestrictive);

		if(m_listener && !load && fire_events)
			m_listener->URLBlocked(url_str, wc, NULL);

		return OpStatus::OK;
	}
#endif

	// By default we try to allow navigation if there are extensions installed and Opera is in black list mode
	if(m_exclude_include && m_extensions.GetCount() && !m_include_rules.GetCount())
		load=TRUE;

	// Initializations
	OpString value;
	FilterAlgorithm chosen_alg=ChooseBestAlgorithm();

	RETURN_IF_ERROR(value.Set(url_str));
	#ifdef CF_BLOCK_INCLUDING_PORT_NUMBER
		RETURN_IF_ERROR(AddPortToUrlFilter(&value));
	#endif//CF_BLOCK_INCLUDING_PORT_NUMBER

	OP_ASSERT(chosen_alg == FILTER_FAST || chosen_alg == FILTER_SLOW);

	if(chosen_alg != FILTER_SLOW)
		RETURN_IF_ERROR(CreateMatchArray(match_array, &value, TRUE));

	OP_STATUS ops_hard_coded=InitializeHardCodedLists();

	if(OpStatus::IsError(ops_hard_coded))
	{
		// Free the list, so another attempt to fill it with all the required values will be performed in the future
		m_unstoppable_rules.m_list.DeleteAll();

		return ops_hard_coded;
	}

#ifdef CF_BLOCK_OPERA_DEBUG
	BOOL soft_disabled=FALSE;

	// "Soft disabled" has precedence againt "unstoppable"
	if(m_soft_disabled_rules.FindAlg(match_array, url, &value, chosen_alg, html_ctx))
	{
		load=FALSE;

		soft_disabled=TRUE;
	}
	else
#endif // CF_BLOCK_OPERA_DEBUG

	// Check the unstoppable list and give it maximum priority
	if(m_unstoppable_rules.FindAlg(match_array, url, &value, chosen_alg, html_ctx))
	{
		load=TRUE;   // No listener call

		return OpStatus::OK;
	}

#ifdef CF_BLOCK_OPERA_DEBUG
	// We _need_ both an include and exclude list of rules
	if(!m_extensions.GetCount() && (!m_exclude_rules.GetCount() || !m_include_rules.GetCount()))
	{
		if(soft_disabled)
		{
			if(m_listener && !load && fire_events)
				m_listener->URLBlocked(url_str, wc, NULL);

			return OpStatus::OK;  // In this case, the content has already been rejected
		}

		load=(m_block_mode != BlockModeRestrictive);

		if(m_listener && !load && fire_events)
			m_listener->URLBlocked(url_str, wc, NULL);

		return OpStatus::OK;
	}
#endif

	BOOL found = FALSE;
	BOOL unblocked = FALSE;

	// Eliminate unnecessary calls based on the value of load

	// Check main list with high priority
	found=CheckRules(m_exclude_include?m_exclude_rules:m_include_rules, match_array, url, &value, chosen_alg, load, html_ctx);

	UINT32 num;
	BOOL from_widget = html_ctx && html_ctx->IsFromWidget();

	// Check the extensions lists. There are some important considerations:
    // * extensions only work in "black list" mode.
    // * when Opera is in white list mode, extension are checked also after a match in the white list
	// * Each extension can only filter new URLs, cannot allow something blocked by another extension
	// * The white list of each extension is checked only if there is a match on its black list.
	// * All the extensions are always checked, even if there is a match. This is to allow multiple invocations of listeners
    //   if two extensions block the same URL
	// * Extensions are not allowed to block content loaded from other extensions. For example, Ghostery was blocking
	//   our Facebook Speed Dial extension, and we don't want that
	if((!found || load) && !from_widget) // If Opera is in white list mode (load==TRUE even if found==TRUE), we will still check the extensions, to allow them to block some more sites
	{
		for(num=m_extensions.GetCount(); num>0; num--)
		{
			ExtensionList* ext=m_extensions.Get(num-1);

			if(CheckRules(ext->m_exclude_rules, match_array, url, &value, chosen_alg, load, html_ctx))
			{
				// Blocked: verify whitelist
				if(!CheckRules(ext->m_include_rules, match_array, url, &value, chosen_alg, load, html_ctx))
				{
					// Not in white list: blocked!
					OP_ASSERT(load==FALSE);
					found = TRUE;  // Mark as found, but keep going on to eventually notify more extensions
				}
				else
				{
					// in white list: keep checking but signal that it has been allowed
					unblocked = TRUE;
				}
			}
		}
	}

	// Check main list with low priority
	if(!found)
		found=CheckRules(m_exclude_include?m_include_rules:m_exclude_rules, match_array, url, &value, chosen_alg, load, html_ctx);

	if(m_listener && !load && fire_events)
		m_listener->URLBlocked(url_str, wc, NULL);

	if(fire_events)
	{
		for(num=m_extensions.GetCount(); num>0; num--)
		{
			ExtensionList* ext=m_extensions.Get(num-1);

			if(!load)
				ext->SendEvent(ExtensionList::BLOCKED, url_str, wc, html_user_ctx);
			else if(unblocked)
				ext->SendEvent(ExtensionList::UNBLOCKED, url_str, wc, html_user_ctx);
		#ifdef SELFTEST		
			else // allowed: this event is only sent for testing purposes, and it is undocumented
				ext->SendEvent(ExtensionList::ALLOWED, url_str, wc, html_user_ctx);
		#endif // SELFTEST
		}
	}

	return OpStatus::OK;
}

void ExtensionList::SendEvent(EventType evt, const uni_char* url_str, OpWindowCommander *wc, HTMLLoadContext *ctx)
{
	for(int i=0, len=listeners.GetCount(); i<len; i++)
	{
		URLFilterExtensionListener *lsn = listeners.Get(i);

		if(lsn)
		{
			// If there is a listener that wants to override extension listeners, its methods are called and the original
			// listener it is supplied to it
			URLFilterExtensionListenerOverride *lsn_over = (ctx) ? ctx->GetListenerOverride() : NULL;

			if(lsn_over)
			{
				lsn_over->SetOriginalListener(lsn);
				lsn = lsn_over;
			}

			if (evt == BLOCKED)
				lsn->URLBlocked(url_str, wc, NULL);
			else if (evt == UNBLOCKED)
				lsn->URLUnBlocked(url_str, wc, NULL);
		#ifdef SELFTEST
			else if (evt == ALLOWED)
				lsn->URLAllowed(url_str, wc, NULL);
		#endif

			if(lsn_over)
				lsn_over->SetOriginalListener(NULL);
		}
	}
}

#ifdef SELFTEST
OP_STATUS URLFilter::CheckURLSlow(const uni_char* url_str, BOOL& load, Window *window, HTMLLoadContext *html_ctx, BOOL fire_events)
{
#ifdef SUPPORT_VISUAL_ADBLOCK
	if(window && !window->IsContentBlockerEnabled())
	{
		load = TRUE;
		return OpStatus::OK;
	}
#endif // SUPPORT_VISUAL_ADBLOCK
	if(!m_extensions.GetCount() && (!m_exclude_rules.GetCount() || !m_include_rules.GetCount()))	// we _need_ both an include and exclude list of rules
	{
		if (m_block_mode == BlockModeRestrictive)
			load = FALSE;
		else
			load = TRUE;

		return OpStatus::OK;
	}
	if(m_block_mode == BlockModeOff)
	{
		load = TRUE;
		return OpStatus::OK;
	}

	OpString value;
	BOOL found;
	URL url=urlManager->GetURL(url_str);

	RETURN_IF_ERROR(value.Set(url_str));

	#ifdef CF_BLOCK_INCLUDING_PORT_NUMBER
		RETURN_IF_ERROR(AddPortToUrlFilter(&value));
	#endif//CF_BLOCK_INCLUDING_PORT_NUMBER

	if(m_exclude_include)
	{
		RETURN_IF_ERROR(m_include_rules.FindSlow(&url, &value, load, found, html_ctx));
		RETURN_IF_ERROR(m_exclude_rules.FindSlow(&url, &value, load, found, html_ctx));	//exclude has precedence
	}
	else
	{
		RETURN_IF_ERROR(m_exclude_rules.FindSlow(&url, &value, load, found, html_ctx));	//include has precedence
		RETURN_IF_ERROR(m_include_rules.FindSlow(&url, &value, load, found, html_ctx));
	}

	if(m_listener && !load && fire_events)
	{
		OpWindowCommander *wc = window ? window->GetWindowCommander() : NULL;
		m_listener->URLBlocked(url_str, wc, NULL);
	}

	return OpStatus::OK;

}

#endif // SELFTEST

#if defined(SUPPORT_VISUAL_ADBLOCK) && defined(QUICK)
OP_STATUS FilterURLList::SaveNodesToTree(SimpleTreeModel *model)
{
	OP_ASSERT(model);

	if(!model)
		return OpStatus::ERR_NULL_POINTER;

	for(UINT32 i=0; i<GetCount(); i++)
	{
		FilterURLnode *node=m_list.Get(i);

		OP_ASSERT(node);

		if(node)
		{
			if(model->AddItem(node->GetURL()->CStr())<0)
				return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}
#endif // SUPPORT_VISUAL_ADBLOCK and QUICK

OP_STATUS FilterURLList::SaveNodesToList(OpVector<OpString>& list)
{
	for(UINT32 i=0; i<m_list.GetCount(); i++)
	{
		FilterURLnode *node=m_list.Get(i);
		const OpString *url = (node)?node->GetURL():NULL;

		if(url)
		{
			OpString *str=OP_NEW(OpString, ());
			OP_STATUS ops;

			if(!str)
				return OpStatus::ERR_NO_MEMORY;

			if(OpStatus::IsError(ops=str->Set(url->CStr())) || OpStatus::IsError(list.Add(str)))
			{
				OP_DELETE(str);

				return ops;
			}
		}
	}

	return OpStatus::OK;
}

#ifdef CF_WRITE_URLFILTER
// Save all the nodes in a prefernce file
OP_STATUS FilterURLList::SaveNodes(URLFilter *filter)
{
	OP_ASSERT(filter);

	if(!filter)
		return OpStatus::ERR_NULL_POINTER;

	for(UINT32 i=0; i<m_list.GetCount(); i++)
	{
		FilterURLnode *node=m_list.Get(i);

		if(node)
			RETURN_IF_ERROR(filter->SaveSingleNode(node));
	}

	return OpStatus::OK;
}
#endif // CF_WRITE_URLFILTER

#ifdef FILTER_BYPASS_RULES
OP_STATUS URLFilter::CheckBypassURL(const uni_char* url_str, BOOL& load)
{
	OpString value;

	RETURN_IF_ERROR(value.Set(url_str));
	RETURN_IF_ERROR(CreateMatchArray(match_array, &value, TRUE));

	URL url=urlManager->GetURL(url_str);

	if(m_bypass_rules.Find(match_array, &url, &value, NULL))
		load=TRUE;

	return OpStatus::OK;
}

OP_STATUS URLFilter::AddFilterL(const uni_char* url)
{
	FilterURLnode* newone;
	OpString key; ANCHOR(OpString, key);
	OpString value; ANCHOR(OpString, value);
#ifdef PI_EXPAND_ENV_VARIABLES
	OpString expanded;
	ANCHOR(OpString, expanded);
	const int max_expanded_size = 5000;
	expanded.ReserveL(max_expanded_size);
#endif

	newone = OP_NEW_L(FilterURLnode, ());
	newone->SetIsBypass(TRUE);
#ifdef PI_EXPAND_ENV_VARIABLES
	if (OpStatus::IsSuccess(g_op_system_info->ExpandSystemVariablesInString(url, &expanded)))
	{
		RETURN_IF_ERROR(newone->SetURL(expanded.CStr()));
	}
	else
#endif // PI_EXPAND_ENV_VARIABLES
	{
		RETURN_IF_ERROR(newone->SetURL(url));
	}
	m_bypass_rules.InsertURL(newone);
	return OpStatus::OK;
}

OP_STATUS URLFilter::RemoveFilter(const uni_char* url)
{
	for (UINT32 i = 0; i < m_bypass_rules.m_list.GetCount(); ++i)
	{
		FilterURLnode *node = m_bypass_rules.m_list.Get(i);
		if (!node->GetURL()->Compare(url))
		{
			m_bypass_rules.m_list.Remove(i);
			OP_DELETE(node);
			break;
		}
	}
	return OpStatus::OK;
}
#endif // FILTER_BYPASS_RULES

#ifdef FILTER_REDIRECT_RULES
OP_STATUS URLFilter::CheckRedirectURL(const uni_char* url, BOOL& load)
{
	OpString value;

	RETURN_IF_ERROR(value.Set(url));
	RETURN_IF_ERROR(CreateMatchArray(match_array, &value, TRUE));

	if(m_redirect_rules.Find(match_array, &value))
		load=TRUE;

	return OpStatus::OK;
}

OP_STATUS URLFilter::AddRedirectFilterL(const uni_char* url)
{
	FilterURLnode* newone;
	OpString key; ANCHOR(OpString, key);
	OpString value; ANCHOR(OpString, value);
#ifdef PI_EXPAND_ENV_VARIABLES
	OpString expanded;
	ANCHOR(OpString, expanded);
	const int max_expanded_size = 5000;
	expanded.ReserveL(max_expanded_size);
#endif

	newone = OP_NEW_L(FilterURLnode, ());
	newone->SetIsRedirect(TRUE);
#ifdef PI_EXPAND_ENV_VARIABLES
	if (OpStatus::IsSuccess(g_op_system_info->ExpandSystemVariablesInString(url, &expanded)))
	{
		RETURN_IF_ERROR(newone->SetURL(expanded.CStr()));
	}
	else
#endif // PI_EXPAND_ENV_VARIABLES
	{
		RETURN_IF_ERROR(newone->SetURL(url));
	}
	m_redirect_rules.InsertURL(newone);
	return OpStatus::OK;
}
#endif // FILTER_REDIRECT_RULES
//*************************************************************************
void CheckURLAllowedContext::CheckURLAllowedReply(BOOL allowed)
{
	if (!m_is_canceled)
		g_main_message_handler->PostMessage(m_msg,
											m_id,
											allowed);
	OP_DELETE(this);
}

//*************************************************************************
OP_STATUS URLFilter::CheckURLAsync(URL url, OpMessage msg, BOOL& wait)
{
	if (m_async_listener) {

		CheckURLAllowedContext *url_ctx =
			OP_NEW(CheckURLAllowedContext, ((MH_PARAM_1) url.Id(), msg));
		if(!url_ctx) {
			return OpStatus::ERR_NO_MEMORY;
		}
		url_ctx->Into(&m_pending_url_allowed_ctx);

		OpString url_str;
		OpStatus::Ignore(url.GetAttribute(URL::KUniName_Escaped, 0, url_str));

		m_async_listener->OnCheckURLAllowed(url_str.CStr(),
											url_ctx,
											wait);
		if (!wait) {
			// No callback will be sent
			OP_DELETE(url_ctx);
		}
	}
	else {
		wait = FALSE;
	}

	return OpStatus::OK;
}

OP_STATUS URLFilter::CancelCheckURLAsync(URL url)
{
	for (CheckURLAllowedContext* url_ctx =
			 (CheckURLAllowedContext*) m_pending_url_allowed_ctx.First();
		 url_ctx;
		 url_ctx = url_ctx->Suc())
	{
		if (url_ctx->Id() == (MH_PARAM_1)url.Id()) {
			url_ctx->Cancel();
			break;
		}
	}
	return OpStatus::OK;
}

// Find the list associated with the extension
ExtensionList *URLFilter::FindList(OpGadget *extension, int &pos, BOOL create)
{
	pos=-1;

	if(m_extensions.GetCount()>0)
	{
		for(UINT32 i=0, num=m_extensions.GetCount(); i<num ; i++)
		{
			ExtensionList* list=m_extensions.Get(i);

			if(list && list->extension_ptr == extension)
			{
				pos=i;

				return list;
			}
		}
	}

	if(!create)
		return NULL;

	ExtensionList *list = OP_NEW(ExtensionList, (extension, NULL));

	if(!list || OpStatus::IsError(m_extensions.Add(list)))
	{
		OP_DELETE(list);

		return NULL;
	}

	return list;
}

OP_STATUS URLFilter::AddURL(FilterURLnode* newnode, OpGadget *extension, BOOL *node_acquired)
{
	OP_STATUS s = OpStatus::ERR;
	FilterURLList *exclude_rules;
	FilterURLList *include_rules;
	BOOL duplicated=FALSE;

	if(node_acquired)
		*node_acquired=FALSE;

	if(extension)
	{
		int pos;
		ExtensionList *list=FindList(extension, pos, TRUE);

		if(!list)
			return OpStatus::ERR_NO_MEMORY;

		exclude_rules = &list->m_exclude_rules;
		include_rules = &list->m_include_rules;
	}
	else
	{
		exclude_rules = &m_exclude_rules;
		include_rules = &m_include_rules;
	}

	if(newnode->Excluded())
	{
#ifdef CF_DONT_ALLOW_BLOCK_ALL
		const OpString *url = newnode->GetURL();

		if(url && url->HasContent())
		{
			const uni_char *key = url->CStr();

		    if(*key == '*' && *(key + 1) == '\0')
		    {
		    	// url is '*'
		        OP_DELETE(newnode);

				if(node_acquired)
					*node_acquired=TRUE;  // Lie, so the caller will not try to delete the node

		        return OpStatus::OK;
		    }
		    else if(!cf_strcmp(key, UNI_L("http://*")))
		    {
		    	// url is 'http://*'
		        OP_DELETE(newnode);

				if(node_acquired)
					*node_acquired=TRUE;  // Lie, so the caller will not try to delete the node

		        return OpStatus::OK;
		    }
		 }
 #endif // CF_DONT_ALLOW_BLOCK_ALL
		 /*#ifdef CF_DONT_BLOCK_OPERA_PROTOCOLS
		 	if(url && url->HasContent())
		    {
		 		const uni_char *key = url->CStr();
		        if(!uni_strncmp(key, UNI_L("opera:"), 6) || !uni_strncmp(key, UNI_L("about:"), 6))
		        {
		        	// url is an opera url
		            OP_DELETE(newnode);

					if(node_acquired)
						*node_acquired=TRUE;  // Lie, so the caller will not try to delete the node

		            return OpStatus::OK;
		        }
		    }
		 #endif // CF_DONT_BLOCK_OPERA_PROTOCOLS*/

		s = exclude_rules->InsertURL(newnode, FALSE, node_acquired, &duplicated);
	}
	else
	{
		s = include_rules->InsertURL(newnode, FALSE, node_acquired, &duplicated);
	}

#ifdef SYNC_CONTENT_FILTERS
	// Nodes duplicated will not be synchronized
	if(!extension && !duplicated && OpStatus::IsSuccess(s))
	{
		OpSyncDataItem::DataItemStatus sync_status = OpSyncDataItem::DATAITEM_ACTION_ADDED;

		s = SyncItem(newnode, sync_status);
	}
#endif // SYNC_CONTENT_FILTERS

	// If there are not errors, a node is not acquired only when it's duplicated
	OP_ASSERT(OpStatus::IsError(s) || !node_acquired || *node_acquired==!duplicated);

	return s;
}

OP_STATUS URLFilter::AddExtensionListener(OpGadget *extension, URLFilterExtensionListener *listener)
{
	int pos;
	ExtensionList *list = FindList(extension, pos, TRUE);

	if(!list)
		return OpStatus::ERR_NO_MEMORY;

	return list->AddListener(listener);
}

OP_STATUS	URLFilter::RemoveExtensionListener(OpGadget *extension, URLFilterExtensionListener *listener)
{
	int pos;
	ExtensionList *list = FindList(extension, pos, TRUE);

	if(!list)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	return list->RemoveListener(listener);
}


OP_STATUS ExtensionList::AddListener(URLFilterExtensionListener *lsn)
{
	OP_ASSERT(lsn);

	if(!lsn)
		return OpStatus::OK;

	if(listeners.Find(lsn)>=0)
		return OpStatus::OK;  // Listener already present

	return listeners.Add(lsn);
}

OP_STATUS ExtensionList::RemoveListener(URLFilterExtensionListener *lsn)
{
	return listeners.RemoveByItem(lsn);
}

OP_STATUS URLFilter::AddURLString(const uni_char *url, BOOL exclude, OpGadget *extension)
{
	FilterURLnode* node=OP_NEW(FilterURLnode, ());

	RETURN_OOM_IF_NULL(node);
	OP_STATUS ops=node->SetURL(url);

	if(OpStatus::IsError(ops))
	{
		OP_DELETE(node);

		return ops;
	}

	node->SetIsExclude(exclude);

	BOOL node_acquired=FALSE;

	ops=AddURL(node, extension, &node_acquired);

	if(!node_acquired)
		OP_DELETE(node);

	return ops;
}

//#########################################################################
//#########################################################################

//*************************************************************************

FilterURLList::FilterURLList(BOOL loaded)
#ifdef SELFTEST
	:
	m_hash_matches_true(0),
	m_hash_matches(0),
	m_matches(0),
	m_totalcalls(0),
	m_totalcompares(0)
#endif // SELFTEST
{
	m_loaded = loaded;
}

//*************************************************************************

FilterURLList::~FilterURLList()
{
}

int FilterURLList::FindExact(const OpString *url)
{
	OP_ASSERT(url);

	if(!url || !url->CStr())
		return -1;

	for(UINT32 i=0, len=m_list.GetCount(); i<len; i++)
	{
		if(!cf_strcmp(url->CStr(), m_list.Get(i)->m_url.CStr()))
			return i;//m_list.Get(i);
	}
	return -1;
}

#ifdef SYNC_CONTENT_FILTERS
FilterURLnode*	FilterURLList::FindNodeByID(const uni_char *guid)
{
	for(UINT32 i = 0, len = m_list.GetCount(); i < len; i++)
	{
		FilterURLnode *node = m_list.Get(i);

		if(guid && node->GUID() && !cf_strcmp(guid, node->GUID()))
			return node;
	}
	return NULL;
}

// This method must not call any sync methods and should also be called
// as a response to delete commands from the Link server
BOOL FilterURLList::DeleteNodeByID(const uni_char* guid)
{
	for(UINT32 i = 0, len = m_list.GetCount(); i < len; i++)
	{
		FilterURLnode *node = m_list.Get(i);

		if(guid && node->GUID() && !cf_strcmp(guid, node->GUID()))
		{
			m_list.Delete(i);
			return TRUE;
		}
	}
	return FALSE;
}

#endif // SYNC_CONTENT_FILTERS

BOOL FilterURLList::Find(unsigned char *match_array, URL *url, OpString* url_str, HTMLLoadContext *html_ctx)
{
#ifdef SELFTEST
	m_totalcalls++;
#endif // SELFTEST

	for(UINT32 i=0, len=m_list.GetCount(); i<len; i++)
	{
		FilterURLnode* node=m_list.Get(i);

		OP_ASSERT(node->index1<MATCH_ARRAY_SIZE);
		OP_ASSERT(node->index2<MATCH_ARRAY_SIZE);
		OP_ASSERT(node->index3<MATCH_ARRAY_SIZE);

#ifdef SELFTEST
		m_totalcompares++;
#endif // SELFTEST
		// Procede only if it is possibile that there is a match (hash check)
		if(!node->index1 || ((match_array[node->index1]&node->bit1) && (match_array[node->index2]&node->bit2) && (match_array[node->index3]&node->bit3)))
		{
#ifdef SELFTEST
			m_hash_matches++;
			if(node->index1)
				m_hash_matches_true++;
#endif // SELFTEST
			OP_ASSERT(node->GetURL());

			if(URLFilter::MatchUrlPattern(url_str->CStr(), node->GetURL()->CStr()))
			{
#ifdef SELFTEST
				m_matches++;
#endif // SELFTEST

				// If the rules apply, then it's a real match, else it is not
				if(node->CheckNodeRules(url, html_ctx))
					return TRUE;		//no need to continue
			}
		}
	}

	return FALSE;
}

OP_STATUS FilterURLList::FindSlow(URL *url, OpString* url_opstr, BOOL& findresult, BOOL &found, HTMLLoadContext *html_ctx)
{
	found = FALSE;

	uni_char *url_str=url_opstr->CStr();

	for(UINT32 i=0, len=m_list.GetCount(); i<len; i++)
	{
		FilterURLnode* node=m_list.Get(i);

		OP_ASSERT(node->GetURL());

		if(URLFilter::MatchUrlPattern(url_str, node->GetURL()->CStr()))
		{
			// If the rules apply, then it's a real match, else it is not
			if(node->CheckNodeRules(url, html_ctx))
			{
				found = TRUE;
				findresult = node->Excluded() ? FALSE : TRUE;

				return OpStatus::OK;		//no need to continue
			}
		}
	}

	return OpStatus::OK;
}


OP_STATUS FilterURLList::InsertURL(FilterURLnode* newnode, BOOL allow_duplicates, BOOL *node_acquired, BOOL *node_duplicated)
{
	if(node_duplicated)
		*node_duplicated=FALSE;

	OP_ASSERT(newnode);
	int pos=-1;

	#ifdef CF_BLOCK_INCLUDING_PORT_NUMBER
		URLFilter::AddPortToUrlFilter(&newnode->m_url);
	#endif//CF_BLOCK_INCLUDING_PORT_NUMBER

	if( !newnode || !newnode->GetURL() || newnode->GetURL()->IsEmpty() ||
		(!allow_duplicates && FindExact(newnode->GetURL())>=0) )
	{
		// WARNING: if node_acquired is NULL, the node is deleted to avoid memory leaks
		if(node_duplicated)
			*node_duplicated=TRUE;

		if(node_acquired)
			*node_acquired=FALSE;
		else
		{
			if(m_list.Get(pos)!=newnode)  // If the same exact node is already in the list, deleting it is a mistake...
				OP_DELETE(newnode);
		}

		return OpStatus::OK;
	}

	OP_STATUS ops = m_list.Add(newnode);

	if(node_acquired)
		*node_acquired = OpStatus::IsSuccess(ops);

	return ops;
}

OP_STATUS FilterURLList::InsertURLString(const uni_char *pattern, BOOL allow_duplicates, BOOL add_star_at_end)
{
	OpString pattern_star;

	if(add_star_at_end)
	{
		RETURN_IF_ERROR(pattern_star.AppendFormat(UNI_L("%s%c"), pattern, '*'));
	}

	FilterURLnode *node=OP_NEW(FilterURLnode, ());

	if(!node)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ops=OpStatus::ERR_NO_MEMORY;

	if( OpStatus::IsError(ops=node->SetURL((add_star_at_end)?pattern_star.CStr():pattern)) ||
		OpStatus::IsError(ops=InsertURL(node, allow_duplicates)))
	{
		OP_DELETE(node);

		return ops;
	}

	return OpStatus::OK;
}

BOOL FilterURLList::DeleteURL(FilterURLnode *url)
{
	int index = FindExact(url->GetURL());

	if(index>=0)
	{
		m_list.Delete(index);
		return TRUE;
	}
	return FALSE;
}

BOOL FilterURLList::DeleteURL(const OpString& url)
{
	int index = FindExact(&url);

	if(index>=0)
	{
		m_list.Delete(index);
		return TRUE;
	}
	return FALSE;
}

OP_STATUS URLFilter::CreateMatchArray(unsigned char *&match, OpString* url, BOOL clean_array)
{
	if(!match)
		match=OP_NEWA(unsigned char, MATCH_ARRAY_SIZE);

	if(!match)
		return OpStatus::ERR_NO_MEMORY;

	op_memset(match, 0, MATCH_ARRAY_SIZE);

	unsigned char mask[8]={1,2,4,8,16,32,64,128};
	OP_ASSERT(url);

	if(!url)
		return OpStatus::ERR_NULL_POINTER;

	uni_char *p=url->CStr();

	for(int i=0, len=url->Length()-1; i<len; ++i)
	{
		unsigned short index=FilterURLnode::ComputeIndex(p+i);

		match[index>>3]|=mask[index&0x7];
	}

	return OpStatus::OK;
}

UINT32 URLFilter::GetCount(BOOL count_extensions)
{
	UINT32 count_ext=0;

	if(count_extensions)
	{
		for(UINT32 num=m_extensions.GetCount(); num>0; num--)
		{
			ExtensionList* ext=m_extensions.Get(num-1);

			count_ext+=ext->m_exclude_rules.GetCount();
			count_ext+=ext->m_include_rules.GetCount();
		}
	}

	return count_ext + m_exclude_rules.GetCount() + m_include_rules.GetCount();
}


#ifdef WEB_TURBO_MODE
void URLFilter::GetWildcardURLL(const OpStringC &urlStr, OpString &wildcardStr)
{
	wildcardStr.ReserveL(urlStr.Length() + 10);
	wildcardStr.Set(UNI_L("http://"));
	wildcardStr.Append(urlStr);
	wildcardStr.Append(UNI_L("/*"));
}

void URLFilter::SetWebTurboBypassURLsL(const OpStringC &newStr)
{
	if (currBypassURLs.IsEmpty() && newStr.IsEmpty())
		return;

	OpStringSet newSet; ANCHOR(OpStringSet, newSet);
	OpStringSet oldSet; ANCHOR(OpStringSet, oldSet);

	newSet.InitL(newStr);
	oldSet.InitL(currBypassURLs);

	// Get a set of newly added globas and another of newly removed by getting the
	// two relative complements.
	OpStringSet adds; ANCHOR(OpStringSet, adds);
	OpStringSet removes; ANCHOR(OpStringSet, removes);

	adds.RelativeComplementL(newSet, oldSet);
	removes.RelativeComplementL(oldSet, newSet);

	// Update the filters.
	for (int i = 0; i < removes.Size(); ++i)
	{
		OpString wildcardStr; ANCHOR(OpString, wildcardStr);
		GetWildcardURLL(removes.GetString(i), wildcardStr);
		RemoveFilter(wildcardStr.CStr());
	}

	for (int i = 0; i < adds.Size(); ++i)
	{
		OpString wildcardStr; ANCHOR(OpString, wildcardStr);
		GetWildcardURLL(adds.GetString(i), wildcardStr);
		AddFilterL(wildcardStr.CStr());
	}

	// Set the new string to use as a comparison if we change again.
	currBypassURLs.SetL(newStr);
}

#endif //WEB_TURBO_MODE

//#########################################################################
//#########################################################################

//*************************************************************************

FilterURLnode::FilterURLnode(BOOL is_excluded) :
excluded(is_excluded),
//match_offset(0),
index1(0),
index2(0),
index3(0),
bit1(0),
bit2(0),
bit3(0)
#ifdef FILTER_BYPASS_RULES
,m_bypass(FALSE)
#endif // FILTER_BYPASS_RULES
#ifdef FILTER_REDIRECT_RULES
,m_redirect(FALSE)
#endif // FILTER_REDIRECT_RULES
{
}

//*************************************************************************

FilterURLnode::FilterURLnode(FilterURLnode& p)
{
	RETURN_VOID_IF_ERROR(m_url.Set(*p.GetURL()));

	index1=p.index1;
	index2=p.index2;
	index3=p.index3;
	bit1=p.bit1;
	bit2=p.bit2;
	bit3=p.bit3;
	excluded=p.excluded;

#ifdef FILTER_BYPASS_RULES
	m_bypass = p.m_bypass;
#endif // FILTER_BYPASS_RULES
#ifdef FILTER_REDIRECT_RULES
	m_redirect = p.m_redirect;
#endif // FILTER_REDIRECT_RULES
	//SetIsExclude(p.Excluded());
	//SetDeleted(p.IsDeleted());
}

//*************************************************************************
/*
BOOL FilterURLnode::Filter(const uni_char* pattern)
{
	const uni_char *url = m_url.CStr();
	int i = 0;
 	if(!pattern || !url)
	{
		return FALSE;	// null cannot be found in the list
	}
	const uni_char * dot = UNI_L(".");
	while(*pattern)
	{
		switch(*pattern)
		{
		case '?' :
			{
				++i;
				++pattern;		//ignore, can be anything
			}
			break;// matches one
		case '*' :
			{
				++pattern;
				while(m_url[i] != *pattern && m_url[i] != *dot)		//match the rest of the string, or until the separator
				{
					++i;
				}
			}
			break;// matches all
		default :
			{
				if(*pattern++ != m_url[i++])
				{
					return FALSE;
				}
			}
		}
	}

	return !*pattern;	//if we searched through the whole urlstring, then the url wasn't found

}
*/
FilterURLnode FilterURLnode::operator = (FilterURLnode& p)
{
	m_url.Set(*p.GetURL());
	index1=p.index1;
	index2=p.index2;
	index3=p.index3;
	bit1=p.bit1;
	bit2=p.bit2;
	bit3=p.bit3;
	excluded=p.excluded;

	OP_ASSERT(index1<MATCH_ARRAY_SIZE);
	OP_ASSERT(index2<MATCH_ARRAY_SIZE);
	OP_ASSERT(index3<MATCH_ARRAY_SIZE);

	//SetIsExclude(p.Excluded());
	//SetDeleted(p.IsDeleted());

	return *this;
}

OP_STATUS	FilterURLnode::SetURL(const uni_char* url, BOOL remove_trailing_spaces)
{
	int len=(url==NULL) ? 0 : uni_strlen(url);

	if(remove_trailing_spaces)
	{
		while(len>0 && (url[len-1]==' ' || url[len-1]=='\t'))
			len--;
	}

	RETURN_IF_ERROR(m_url.Set(url, len));

	ComputeMatchIndexes();

	return OpStatus::OK;
};

//*************************************************************************
void FilterURLnode::ComputeMatchIndexes()
{
	uni_char *p=m_url.CStr();
	uni_char *start=p;

	//int match_offset=0;

	// Skip to a meaningful location, like the protocol (://), beginning or end, or the first '.'
	// We are tring to optimize also patterns like 'http*://'
	while(*p && *p!='/' && *p!='.' && *p!=':' && !IsCharacterWildCard(*p))
		p++;

	while(*p && (*p=='/' || *p==':' || IsCharacterWildCard(*p)))
		p++;

	// Instead of having always a full match, we could try a "less effective" match; this also helps special cases like "opera:about"
	if(!*p)
		p=m_url.CStr();

	while(*p==':' || *p=='/' || IsCharacterWildCard(*p))
		p++;

	// Skip "www"
	if(*p=='w' && p[1]=='w' && p[2]=='w')
	{
		// Go to the first point
		while(*p && (*p!='.' && !IsCharacterWildCard(*p)))
			p++;

		if(*p=='.')
			p++;

		while(IsCharacterWildCard(*p))
			p++;
	}

	if(p-start<256 && p[0] && p[1] && !IsCharacterWildCard(p[0]) && !IsCharacterWildCard(p[1]))
	{
		// Precomputed mask used for choosing the index
		unsigned char mask[8]={1,2,4,8,16,32,64,128};

		// Compute the indexes
		index1=ComputeIndex(p);

		if(p[2] && p[3] && !IsCharacterWildCard(p[2]) && !IsCharacterWildCard(p[3]))
		{
			index2=ComputeIndex(p+2);
			if(p[4] && p[5] && !IsCharacterWildCard(p[4]) && !IsCharacterWildCard(p[5]))
				index3=ComputeIndex(p+4);
			else
				index3=index2;
		}
		else
			index2=index3=index1;

		// translate the indexes in a "byte index" + "bit mask" pair
		bit1=mask[index1&0x7];	// Bit mask
		bit2=mask[index2&0x7];	// Bit mask
		bit3=mask[index3&0x7];	// Bit mask
		index1>>=3;				// Byte
		index2>>=3;				// Byte
		index3>>=3;				// Byte

		OP_ASSERT(index1<MATCH_ARRAY_SIZE);
		OP_ASSERT(index2<MATCH_ARRAY_SIZE);
		OP_ASSERT(index3<MATCH_ARRAY_SIZE);
	}
}

BOOL FilterURLnode::CheckNodeRules(class URL *url, HTMLLoadContext *html_ctx)
{
	if(!m_rules.GetCount())		// If there are no rules, the match is always valid
		return TRUE;

	// At the moment, we still apply all the rules, but we could evaluate to return FALSE if html_ctx is NULL
	for(int i=0, len=m_rules.GetCount(); i<len; i++)
	{
		FilterRule *rule=m_rules.Get(i);

		OP_ASSERT(rule);

		if(!rule->MatchRule(url, html_ctx))
			return FALSE;
	}

	return TRUE;
}

OP_STATUS FilterURLnode::AddRuleInDomains(OpVector<uni_char> &domains)
{
	// "In domains" are in OR
	RuleGroupOR *group = OP_NEW(RuleGroupOR, ());

	return AddRuleInDomainsKernel(domains, group, TRUE);
}

OP_STATUS FilterURLnode::AddRuleNotInDomains(OpVector<uni_char> &not_domains)
{
	// "Not in domains" are like "in domains", but with the result inverted (by the group)
	RuleGroupNOR *group = OP_NEW(RuleGroupNOR, ());

	return AddRuleInDomainsKernel(not_domains, group, TRUE);
}

OP_STATUS FilterURLnode::AddRuleInDomainsKernel(OpVector<uni_char> &domains, RuleGroup *group, BOOL deep)
{
	if (!group) // The group is the rule
		return OpStatus::ERR_NO_MEMORY;

	for (int i = 0, len = domains.GetCount(); i<len; i++)
	{
		if (domains.Get(i))
		{
			OpString str;

			if (OpStatus::IsError(str.AppendFormat("http://%s", domains.Get(i))))
				return OpStatus::ERR_NO_MEMORY;
			else
			{
				URL url = urlManager->GetURL(str);
				RuleInDomain *rule = (deep) ? OP_NEW(RuleInDomainDeep, (url)) : OP_NEW(RuleInDomain, (url));

				if (!rule || OpStatus::IsError(group->AddRule(rule))) // Rules attached to the group
				{
					OP_DELETE(rule);
					OP_DELETE(group);

					return OpStatus::ERR_NO_MEMORY;
				}
			}
		}
	}

	if (OpStatus::IsError(AddRule(group))) // The group is the rule
	{
		OP_DELETE(group);

		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS FilterURLnode::AddRuleThirdParty()
{
	RuleThirdParty *rule_third = OP_NEW(RuleThirdParty, ());

	if (!rule_third || OpStatus::IsError(AddRule(rule_third)))
	{
		OP_DELETE(rule_third);
		
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS FilterURLnode::AddRuleNotThirdParty()
{
	RuleNotThirdParty *rule_not_third = OP_NEW(RuleNotThirdParty, ());

	if (!rule_not_third || OpStatus::IsError(AddRule(rule_not_third)))
	{
		OP_DELETE(rule_not_third);
		
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS FilterURLnode::AddRuleResources(UINT32 resources)
{
	if(resources == RESOURCE_UNKNOWN)
		return OpStatus::OK;

	// Resources are in OR
	RuleGroupOR *group = OP_NEW(RuleGroupOR, ());

	if (!group) // The group is the rule
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ops = OpStatus::OK;

	if(resources & RESOURCE_OTHER)
		ops = AddRuleResourceToGroup(RESOURCE_OTHER, group);
	if(OpStatus::IsSuccess(ops) && (resources & RESOURCE_SCRIPT))
		ops = AddRuleResourceToGroup(RESOURCE_SCRIPT, group);
	if(OpStatus::IsSuccess(ops) && (resources & RESOURCE_IMAGE))
		ops = AddRuleResourceToGroup(RESOURCE_IMAGE, group);
	if(OpStatus::IsSuccess(ops) && (resources & RESOURCE_STYLESHEET))
		ops = AddRuleResourceToGroup(RESOURCE_STYLESHEET, group);
	if(OpStatus::IsSuccess(ops) && (resources & RESOURCE_OBJECT))
		ops = AddRuleResourceToGroup(RESOURCE_OBJECT, group);
	if(OpStatus::IsSuccess(ops) && (resources & RESOURCE_SUBDOCUMENT))
		ops = AddRuleResourceToGroup(RESOURCE_SUBDOCUMENT, group);
	if(OpStatus::IsSuccess(ops) && (resources & RESOURCE_DOCUMENT))
		ops = AddRuleResourceToGroup(RESOURCE_DOCUMENT, group);
	if(OpStatus::IsSuccess(ops) && (resources & RESOURCE_REFRESH))
		ops = AddRuleResourceToGroup(RESOURCE_REFRESH, group);
	if(OpStatus::IsSuccess(ops) && (resources & RESOURCE_XMLHTTPREQUEST))
		ops = AddRuleResourceToGroup(RESOURCE_XMLHTTPREQUEST, group);
	if(OpStatus::IsSuccess(ops) && (resources & RESOURCE_OBJECT_SUBREQUEST))
		ops = AddRuleResourceToGroup(RESOURCE_OBJECT_SUBREQUEST, group);
	if(OpStatus::IsSuccess(ops) && (resources & RESOURCE_MEDIA))
		ops = AddRuleResourceToGroup(RESOURCE_MEDIA, group);
	if(OpStatus::IsSuccess(ops) && (resources & RESOURCE_FONT))
		ops = AddRuleResourceToGroup(RESOURCE_FONT, group);

	if (OpStatus::IsError(ops) || OpStatus::IsError(ops=AddRule(group))) // The group is the rule
	{
		OP_DELETE(group);

		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS FilterURLnode::AddRuleResourceToGroup(HTMLResource resource, RuleGroup *group)
{
	RuleResource *rule=OP_NEW(RuleResource, (resource));

	if(!rule)
		return OpStatus::ERR_NO_MEMORY;

	return group->AddRule(rule);

}

OP_STATUS FilterURLnode::AddRule(FilterRule *rule)
{
	OP_ASSERT(rule);

	if(!rule)
		return OpStatus::OK;

	return m_rules.Add(rule);
}

OP_STATUS FilterURLnode::RemoveRule(FilterRule *rule, BOOL delete_rule)
{
	OP_ASSERT(rule);

	if(!rule)
		return OpStatus::OK;

	RETURN_IF_ERROR(m_rules.RemoveByItem(rule));

	if(delete_rule)
		OP_DELETE(rule);

	return OpStatus::OK;
}

void FilterURLnode::DeleteAllRules()
{
	m_rules.DeleteAll();
}
#ifdef SELFTEST
void URLFilter::AppendCountersDescription(OpString8 &str)
{
	str.AppendFormat("Exclude Rules - Find() calls: %d - Compares: %d - True Hash: %d - Hash: %d - Matches: %d\n", m_exclude_rules.GetTotalCalls(), m_exclude_rules.GetTotalCompares(), m_exclude_rules.GetTrueHashMatches(), m_exclude_rules.GetHashMatches(), m_exclude_rules.GetMatches());
	str.AppendFormat("Include Rules - Find() calls: %d - Compares: %d - True Hash: %d - Hash: %d - Matches: %d\n", m_include_rules.GetTotalCalls(), m_include_rules.GetTotalCompares(), m_include_rules.GetTrueHashMatches(), m_include_rules.GetHashMatches(), m_include_rules.GetMatches());
	if(m_exclude_rules.GetHashMatches()>0)
		str.AppendFormat("Exclude Rules - Hash Efficiency (Calls / Hash Match): %d - Matches/Hash: %d%%\n", m_exclude_rules.GetTotalCompares()/m_exclude_rules.GetHashMatches(), (int)(100.0*m_exclude_rules.GetMatches()/m_exclude_rules.GetHashMatches()));
	if(m_include_rules.GetHashMatches()>0)
		str.AppendFormat("Include Rules - Hash Efficiency (Calls / Hash Match): %d - Matches/Hash: %d%%\n", m_include_rules.GetTotalCompares()/m_include_rules.GetHashMatches(), (int)(100.0*m_include_rules.GetMatches()/m_include_rules.GetHashMatches()));
}
#endif

#ifdef SELFTEST
BOOL FilterURLnode::VerifyIndexes(unsigned short idx1, unsigned short idx2, unsigned short idx3)
{
	unsigned char mask[8]={1,2,4,8,16,32,64,128};

	if (!index1 && !idx1)
		return TRUE;

	return (idx1>>3) == index1 && mask[idx1&0x7] == bit1 &&
		   (idx2>>3) == index2 && mask[idx2&0x7] == bit2 &&
		   (idx3>>3) == index3 && mask[idx3&0x7] == bit3;
}

BOOL FilterURLnode::VerifyIndexes(const uni_char *str)
{
	int len=uni_strlen(str);

	unsigned short idx1;
	unsigned short idx2;
	unsigned short idx3;

	if(len>=2)
		idx1 = ComputeIndex(str);
	else
		idx1 = 0;

	if(len>=4)
		idx2 = ComputeIndex(str+2);
	else
		idx2 = idx1;

	if(len>=6)
		idx3 = ComputeIndex(str+4);
	else
		idx3 = idx2;

	return VerifyIndexes(idx1, idx2, idx3);
}
#endif

//*************************************************************************

URLFilter::URLFilterAccessor::URLFilterAccessor() : IniAccessor(FALSE), m_escape_section(FALSE)
{

}

OP_STATUS URLFilter::URLFilterAccessor::LoadL(OpFileDescriptor *file, PrefsMap *map)
{
	return IniAccessor::LoadL(file, map);
}

BOOL URLFilter::URLFilterAccessor::ParseLineL(uni_char *key_p, PrefsMap *map)
{
	if (key_p==NULL)
		return TRUE;

	// skip leading whitespace in line
	while (uni_isspace(*key_p)) key_p++;

	if (*key_p == '[')
	{
		// a section
		IniAccessor::ParseSectionL(key_p, map);

		if(!cf_strcmp(m_current_section->HashableName(), UNI_L("exclude")) ||
			!cf_strcmp(m_current_section->HashableName(), UNI_L("include")))
		{
			m_escape_section = TRUE;
		}
		else
		{
			m_escape_section = FALSE;
		}
	}
	else if (*key_p && *key_p != ';' && m_current_section)
	{
		if(*key_p != '"' && m_escape_section)
		{
			OpString url;

			// We always strip the UUID from any urlfilter.ini entries. This is so that a urlfilter.ini can
			// be moved from a platform that writes UUID's to one that does not with no ill effects. See
			// EMO-6402

			// When we append the GUID at the end of the pattern to block, we need some stricter first time verifications. The GUIDs are prepended with "=UUID:"
			// to avoid changing the format of the file.  Synchronization needs the ID.
			uni_char fmt_str[32]; /* ARRAY OK 2010-07-27 pettern */
			uni_char *end_p = uni_strstr(key_p, "=UUID:");
			if(end_p)
			{
				uni_snprintf(fmt_str, 31, UNI_L("\"%%.%ds\"%%s"), end_p - key_p);
				// escape these sections with "" on both sides to make sure it's parsed correctly as an URL
				OpStatus::Ignore(url.AppendFormat(fmt_str, key_p, end_p));
			}
			else
			{
				// escape these sections with "" on both sides to make sure it's parsed correctly as an URL
				OpStatus::Ignore(url.AppendFormat(UNI_L("\"%s\""), key_p));
			}

			// a pair (skip comments and empty lines)
			IniAccessor::ParsePairL(url.CStr());
		}
		else
		{
			// a pair (skip comments and empty lines)
			IniAccessor::ParsePairL(key_p);
		}
	}
	return FALSE;
}

OP_STATUS URLFilter::URLFilterAccessor::StoreL(OpFileDescriptor *file, const PrefsMap *map)
{
	return IniAccessor::StoreL(file, map);
}

#ifdef SYNC_CONTENT_FILTERS

FilterURLnode* URLFilter::FindNodeByID(const uni_char* guid)
{
	FilterURLnode *node = NULL;

	node = m_exclude_rules.FindNodeByID(guid);

	if(!node)
	{
		node = m_include_rules.FindNodeByID(guid);
	}
	return node;
}

OP_STATUS URLFilter::GenerateGUIDForFilter(FilterURLnode* item)
{
	if(!item->GUID())
	{
		OpString guid;

		RETURN_IF_ERROR(SyncUtil::GenerateStringGUID(guid));
		RETURN_IF_ERROR(item->SetGUID(guid));
	}
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS URLFilter::URLFilterItem_to_OpSyncItem(FilterURLnode* item,
												OpSyncItem*& sync_item,
												OpSyncDataItem::DataItemStatus sync_status)
{
	OP_NEW_DBG("URLFilter::URLFilterItem_to_OpSyncItem()", "urlfilter.sync");
	RETURN_IF_ERROR(GenerateGUIDForFilter(item));

	OP_DBG((UNI_L("id: %s"), item->GUID()));
	RETURN_IF_ERROR(g_sync_coordinator->GetSyncItem(&sync_item,
													OpSyncDataItem::DATAITEM_URLFILTER,
													OpSyncItem::SYNC_KEY_ID,
													item->GUID()));

	// Set the action / status
	sync_item->SetStatus(sync_status);

	// Set the other attributes:
	SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_TYPE, item->Excluded() ? UNI_L("exclude") : UNI_L("include")), sync_item);
	OP_DBG(("type: %s", item->Excluded() ? "exclude" : "include"));
	SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_CONTENT, item->GetURL()->CStr()), sync_item);
	OP_DBG((UNI_L("content: %s"), item->GetURL()->CStr()));

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS URLFilter::OpSyncItem_to_URLFilterItem(OpSyncItem* sync_item, FilterURLnode*& item, BOOL enable_update)
{
	OP_NEW_DBG("URLFilter::OpSyncItem_to_URLFilterItem()", "urlfilter.sync");
	BOOL exclude_item = TRUE;	// set a sensible safe default
	OpString type, guid;

	RETURN_IF_ERROR(sync_item->GetData(OpSyncItem::SYNC_KEY_TYPE, type));
	RETURN_IF_ERROR(sync_item->GetData(OpSyncItem::SYNC_KEY_ID, guid));
	OP_DBG((UNI_L("type: %s; id: %s"), type.CStr(), guid.CStr()));

	// server should send a guid
	OP_ASSERT(guid.HasContent());

	if(guid.HasContent())
	{
		if(!type.Compare("exclude"))
		{
			item = m_exclude_rules.FindNodeByID(guid.CStr());
		}
		else if(!type.Compare("include"))
		{
			exclude_item = FALSE;
			item = m_include_rules.FindNodeByID(guid.CStr());
		}
		if (item)
			OP_DBG(("found existing item %p", item));
	}

	OpString item_string;

	RETURN_IF_ERROR(sync_item->GetData(OpSyncItem::SYNC_KEY_CONTENT, item_string));
	// Buggy server sends item with no content, ignore it and continue processing remaining items.
	if (item_string.IsEmpty())
		return OpStatus::OK;
	OP_DBG((UNI_L("content: %s"), item_string.CStr()));

	if(!item)
	{
		OpAutoPtr<FilterURLnode> node_ap(OP_NEW(FilterURLnode, ()));
		if(!node_ap.get())
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		node_ap->SetIsExclude(exclude_item);

		RETURN_IF_ERROR(node_ap->SetURL(item_string.CStr()));
		RETURN_IF_ERROR(node_ap->SetGUID(guid.CStr()));

		item = node_ap.get();

		node_ap.release();
	}
	else if(enable_update)
	{
		RETURN_IF_ERROR(item->SetURL(item_string.CStr()));

		item->SetIsExclude(exclude_item);
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS URLFilter::SyncItem(FilterURLnode* item, OpSyncDataItem::DataItemStatus sync_status, BOOL /*first_sync*/, BOOL is_dirty)
{
	OP_NEW_DBG("URLFilter::SyncItem()", "urlfilter.sync");
	// Syncing is currently not enabled
	if(!m_sync_enabled)
		return OpStatus::OK;

	if(!item)
		return OpStatus::ERR;

	// Create the item with the string as primary key:
	OpSyncItem *sync_item = NULL;
	RETURN_IF_ERROR(URLFilterItem_to_OpSyncItem(item, sync_item, sync_status));

	// Commit the item:
	SYNC_RETURN_IF_ERROR(sync_item->CommitItem(is_dirty, FALSE), sync_item);

	g_sync_coordinator->ReleaseSyncItem(sync_item);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS URLFilter::SyncDataAvailable(OpSyncCollection *new_items, OpSyncDataError& data_error)
{
	OP_NEW_DBG("URLFilter::SyncDataAvailable()", "urlfilter.sync");
	// We should never get an empty list
	if(!new_items || !new_items->First())
	{
		OP_ASSERT(FALSE);
		return OpStatus::OK;
	}

	for (OpSyncItemIterator current(new_items->First()); *current; ++current)
	{
		if(current->GetType() != OpSyncDataItem::DATAITEM_URLFILTER)
			continue;

		RETURN_IF_ERROR(ProcessSyncItem(*current));
	}

	OP_STATUS s;

	TRAP(s, WriteL());

	RETURN_IF_ERROR(s);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS URLFilter::ProcessSyncItem(OpSyncItem* item)
{
	OP_NEW_DBG("URLFilter::ProcessSyncItem()", "urlfilter.sync");
	switch(item->GetStatus())
	{
	case OpSyncDataItem::DATAITEM_ACTION_ADDED:
		return ProcessAddedSyncItem(item, FALSE);
	case OpSyncDataItem::DATAITEM_ACTION_MODIFIED:
		return ProcessAddedSyncItem(item, TRUE);
	case OpSyncDataItem::DATAITEM_ACTION_DELETED:
		return ProcessDeletedSyncItem(item);
	}

	return OpStatus::ERR;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS URLFilter::ProcessAddedSyncItem(OpSyncItem* item, BOOL enable_update)
{
	OP_NEW_DBG("URLFilter::ProcessAddedSyncItem()", "urlfilter.sync");
	FilterURLnode *url_item;
	BOOL node_acquired=FALSE;
	BOOL node_duplicated=FALSE;

	RETURN_IF_ERROR(OpSyncItem_to_URLFilterItem(item, url_item, enable_update));

	if(url_item)
	{
		if(url_item->Excluded())
		{
			// node_acquired prevents the node from being deleted when it is duplicated; there will not be
			// memory leaks, as it has already been retrieved from the internal lists
			RETURN_IF_ERROR(m_exclude_rules.InsertURL(url_item, FALSE, &node_acquired, &node_duplicated));
		}
		else
		{
			// node_acquired prevents the node from being deleted when it is duplicated; there will not be
			// memory leaks, as it has already been retrieved from the internal lists
			RETURN_IF_ERROR(m_include_rules.InsertURL(url_item, FALSE, &node_acquired, &node_duplicated));
		}

		OP_ASSERT(node_acquired!=node_duplicated);
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS URLFilter::ProcessDeletedSyncItem(OpSyncItem* item)
{
	OP_NEW_DBG("URLFilter::ProcessDeletedSyncItem()", "urlfilter.sync");
	OpString item_id;
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_ID, item_id));
	OP_DBG((UNI_L("id: %s"), item_id.CStr()));
	DeleteNodeByID(item_id.CStr());

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS URLFilter::SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty)
{
	OP_NEW_DBG("URLFilter::SyncDataFlush()", "urlfilter.sync");
	if(type != OpSyncDataItem::DATAITEM_URLFILTER)
	{
		OP_ASSERT(FALSE); // We should not get any other notifications
		return OpStatus::OK;
	}
	OP_DBG(("%d exclude rules", m_exclude_rules.GetCount()));
	for(unsigned int i = 0; i < m_exclude_rules.GetCount(); i++)
	{
		RETURN_IF_ERROR(SyncItem(m_exclude_rules.Get(i), OpSyncDataItem::DATAITEM_ACTION_ADDED, first_sync, is_dirty));
	}
	OP_DBG(("%d include rules", m_include_rules.GetCount()));
	for(unsigned int i = 0; i < m_include_rules.GetCount(); i++)
	{
		RETURN_IF_ERROR(SyncItem(m_include_rules.Get(i), OpSyncDataItem::DATAITEM_ACTION_ADDED, first_sync, is_dirty));
	}
	// SyncItem() will generate UUIDs for the items, we need to make sure they are written to disk
	OP_STATUS s;

	TRAP(s, WriteL());

	return s;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS URLFilter::SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support)
{
	OP_NEW_DBG("URLFilter::SyncDataSupportsChanged()", "urlfilter.sync");
	OP_DBG(("%s -> %s", m_sync_enabled?"enabled":"disabled", has_support?"enabled":"disabled"));
	if(type != OpSyncDataItem::DATAITEM_URLFILTER)
	{
		OP_ASSERT(FALSE); // We should not get any other notifications
		return OpStatus::OK;
	}

	m_sync_enabled = has_support;
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS URLFilter::SyncDataInitialize(OpSyncDataItem::DataItemType type)
{
	OP_NEW_DBG("URLFilter::SyncDataInitialize()", "urlfilter.sync");
	if(type != OpSyncDataItem::DATAITEM_URLFILTER)
	{
		OP_ASSERT(FALSE); // We should not get any other notifications
		return OpStatus::OK;
	}
	OP_STATUS status;

	// Force a save
	TRAP(status, WriteL());
	RETURN_IF_ERROR(status);

	// Get the url filter file
	OpFile file;
	TRAP(status, g_pcfiles->GetFileL(PrefsCollectionFiles::UrlFilterFile, file));
	RETURN_IF_ERROR(status);

	RETURN_IF_ERROR(BackupFile(file));

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS URLFilter::BackupFile(OpFile& file)
{
	OP_NEW_DBG("URLFilter::BackupFile()", "urlfilter.sync");
	// Check if the file to backup actually exists
	BOOL exists = FALSE;
	OP_STATUS status = file.Exists(exists);
	if (OpStatus::IsError(status) || !exists)
		return OpStatus::ERR;

	// Create the backupfiles filename
	OpString backupFilename;
	RETURN_IF_ERROR(backupFilename.Set(file.GetFullPath()));
	OP_DBG((UNI_L("file: %s"), backupFilename.CStr()));
	RETURN_IF_ERROR(backupFilename.Append(UNI_L(".pre_sync")));

	exists = FALSE;
	OpFile backupFile;

	RETURN_IF_ERROR(backupFile.Construct( backupFilename.CStr()));
	RETURN_IF_ERROR(backupFile.Exists( exists ));

	if(exists)
		return OpStatus::OK; // File already exists

	return backupFile.CopyContents(&file, TRUE );
}


#endif // SYNC_CONTENT_FILTERS


HTMLResource HTMLLoadContext::ConvertElement(HTML_Element *elem)
{
	if(!elem)
		return RESOURCE_UNKNOWN;

	Markup::Type type = elem->Type();
	NS_Type ns_type = elem->GetNsType();

	// Identify resources only for HTML and SVG space
	if (ns_type != NS_HTML && ns_type != NS_SVG)
		return RESOURCE_OTHER;

	// SVG space is identified only for script, images, stylesheets and SVG fontsx
	if (ns_type == NS_SVG &&
		type != Markup::HTE_SCRIPT &&
		type != Markup::HTE_IMAGE &&
		type != Markup::HTE_IMG &&
		type != Markup::HTE_STYLE && 
		type != Markup::SVGE_FONT_FACE_SRC &&
		type != Markup::SVGE_FONT_FACE_URI)
		return RESOURCE_OTHER;

    switch(type)
    {
		case Markup::HTE_HTML:
		case Markup::HTE_DOC_ROOT:
			return RESOURCE_DOCUMENT;

		case Markup::HTE_SCRIPT:
			return RESOURCE_SCRIPT;

		case Markup::HTE_IMAGE:
		case Markup::HTE_IMG:
			return RESOURCE_IMAGE;

		case Markup::HTE_LINK:
		{
			const uni_char *value = elem->GetAttrValue(UNI_L("rel"));

			if(value && !uni_stricmp(value, "stylesheet"))
				return RESOURCE_STYLESHEET;

			return RESOURCE_OTHER;
		}

		case Markup::HTE_STYLE:
			return RESOURCE_STYLESHEET;

		case Markup::HTE_OBJECT:
			return RESOURCE_OBJECT;

		case Markup::HTE_VIDEO:
		case Markup::HTE_AUDIO:
			return RESOURCE_MEDIA;

		case Markup::HTE_BASEFONT:
		case Markup::HTE_FONT:
		case Markup::SVGE_FONT_FACE_SRC:
		case Markup::SVGE_FONT_FACE_URI:
			return RESOURCE_FONT;

		case Markup::HTE_DOCTYPE:
			return RESOURCE_OTHER;

		case Markup::HTE_UNKNOWN:
			return RESOURCE_UNKNOWN;

	default:
		return RESOURCE_OTHER;
	}
}

// Return the Markup type of the element
Markup::Type DOMLoadContext::GetMarkupType()
{
	return (element == NULL ? Markup::HTE_UNKNOWN : element->Type());
}

HTMLResource HTMLLoadContext::ParseResource(uni_char *elem)
{
	if(!uni_strcmp(elem, "RESOURCE_DOCUMENT"))
		return RESOURCE_DOCUMENT;

	if(!uni_strcmp(elem, "RESOURCE_FONT"))
		return RESOURCE_FONT;

	if(!uni_strcmp(elem, "RESOURCE_IMAGE"))
		return RESOURCE_IMAGE;

	if(!uni_strcmp(elem, "RESOURCE_MEDIA"))
		return RESOURCE_MEDIA;

	if(!uni_strcmp(elem, "RESOURCE_OBJECT"))
		return RESOURCE_OBJECT;

	if(!uni_strcmp(elem, "RESOURCE_OBJECT_SUBREQUEST"))
		return RESOURCE_OBJECT_SUBREQUEST;

	if(!uni_strcmp(elem, "RESOURCE_REFRESH"))
		return RESOURCE_REFRESH;

	if(!uni_strcmp(elem, "RESOURCE_SCRIPT"))
		return RESOURCE_SCRIPT;

	if(!uni_strcmp(elem, "RESOURCE_STYLESHEET"))
		return RESOURCE_STYLESHEET;

	if(!uni_strcmp(elem, "RESOURCE_SUBDOCUMENT"))
		return RESOURCE_SUBDOCUMENT;

	if(!uni_strcmp(elem, "RESOURCE_XMLHTTPREQUEST"))
		return RESOURCE_XMLHTTPREQUEST;

	if(!uni_strcmp(elem, "RESOURCE_OTHER"))
		return RESOURCE_OTHER;

	OP_ASSERT(FALSE && !"Unknown Resource description");

	return RESOURCE_UNKNOWN;
}

BOOL RuleNotInDomainDeep::MatchRule(URL *url, HTMLLoadContext *html_ctx)
{
	if(!html_ctx || !html_ctx->GetDomain())
		return FALSE;

	ServerName *domain = html_ctx->GetDomain();

	while(domain)
	{
		if(domain == server_name)
			return FALSE;

		domain = domain->GetParentDomain();
	}

	return TRUE;
}

BOOL RuleInDomainDeep::MatchRule(URL *url, HTMLLoadContext *html_ctx)
{
	if(!html_ctx || !html_ctx->GetDomain())
		return FALSE;

	ServerName *domain = html_ctx->GetDomain();

	while(domain)
	{
		if(domain == server_name)
			return TRUE;

		domain = domain->GetParentDomain();
	}

	return FALSE;
}

ServerName *RuleThirdParty::GetAncestorDomain(ServerName *domain)
{
	OP_ASSERT(domain);

	if(!domain || !domain->GetParentDomain())  // Manages domains like "localhost" without triggering the assert
		return domain;

	while(domain && domain->GetParentDomain())
	{
		ServerName *parent_domain = domain->GetParentDomain();

		OP_ASSERT(parent_domain);

		if(parent_domain->GetDomainTypeASync() == ServerName::DOMAIN_TLD || parent_domain->GetDomainTypeASync() == ServerName::DOMAIN_REGISTRY)
			return domain;

		domain = parent_domain;
	}

	OP_ASSERT( !"Unexpected: please contact the module owner as we are possibly skipping some domains");

	return domain;
}

BOOL RuleThirdParty::MatchRule(URL *url, HTMLLoadContext *html_ctx)
{
	if(!html_ctx || !html_ctx->GetDomain())
		return FALSE;

	ServerName *ancestor_sn=GetAncestorDomain(url->GetServerName());
	ServerName *context_sn=GetAncestorDomain(html_ctx->GetDomain());

	if(ancestor_sn!=context_sn)  // It is considered 3rd party when the ancestor is different
		return TRUE;

	return FALSE;
}

BOOL RuleGroupAND::MatchRule(URL *url, HTMLLoadContext *html_ctx)
{
	if(empty_ctx_means_false && !html_ctx)
		return FALSE;

	for(int i=0, len=rules.GetCount(); i<len; i++)
	{
		FilterRule *rule = rules.Get(i);

		if(rule && !rule->MatchRule(url, html_ctx))
			return FALSE;
	}

	return TRUE;
}

BOOL RuleGroupOR::MatchRule(URL *url, HTMLLoadContext *html_ctx)
{
	if(empty_ctx_means_false && !html_ctx)
		return FALSE;

	for(int i=0, len=rules.GetCount(); i<len; i++)
	{
		FilterRule *rule = rules.Get(i);

		if(rule && rule->MatchRule(url, html_ctx))
			return TRUE;
	}

	return FALSE;
}

BOOL RuleGroupNOR::MatchRule(URL *url, HTMLLoadContext *html_ctx)
{
	if(empty_ctx_means_false && !html_ctx)
		return FALSE;

	return !RuleGroupOR:: MatchRule(url, html_ctx);
}


OP_STATUS RuleGroup::RemoveRule(FilterRule *rule, BOOL delete_rule)
{
	OP_ASSERT(rule);

	if(!rule)
		return OpStatus::OK;

	RETURN_IF_ERROR(rules.RemoveByItem(rule));

	if(delete_rule)
		OP_DELETE(rule);

	return OpStatus::OK;
}

//*************************************************************************
//*************************************************************************
//*************************************************************************
//*************************************************************************
//*************************************************************************
#endif // URL_FILTER

#endif // FILTER_CAP_MODULE
