/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/htmlify.h"
#include "modules/formats/uri_escape.h"

#include "modules/url/url2.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_stor.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpLocale.h"
#include "modules/cache/cache_utils.h"
#include "modules/cache/cache_list.h"

#include "modules/cache/url_cs.h"

#ifdef CACHE_ADVANCED_VIEW
	#include "modules/util/tempbuf.h"
	#include "modules/cache/url_cs.h"
#endif

#ifdef _DEBUG
#ifdef YNP_WORK
#include "modules/olddebug/tstdump.h"
#define DEBUG_CACHE
#endif
#endif

#ifndef NO_URL_OPERA
class urlsort_item: public B23Tree_Item
{
private:
	URL_Rep *url_rep;

private:
	OP_STATUS WriteURL(URL &target);

public:
	urlsort_item(URL_Rep *url_item):url_rep(url_item){};
	virtual~urlsort_item(){url_rep = NULL;}

	URL_Rep *GetURL_Rep(){return url_rep;}

	/** return: 
	 *		negative if search item is less than this object
	 *		zero(0) if search item is equal to this object
	 *		positive if search item is greater than this object
	 */
	virtual int SearchCompare(void *search_param);
	/** Return TRUE if travers should continue with next element */
	virtual BOOL TraverseActionL(uint32 action, void *params);
};

class urlsort_st : public B23Tree_Store
{
public:
	urlsort_st();
	OP_STATUS AddRecord(URL_Rep *);
	OP_STATUS WriteRecords(URL &target, BOOL reverse);
};

OP_STATUS urlsort_item::WriteURL(URL &target)
{
	if(url_rep)
	{
		{
#ifndef RAMCACHE_ONLY
			OpStringC filename(url_rep->GetAttribute(URL::KFileName));
			if (filename.HasContent())
			{
				target.WriteDocumentDataUniSprintf(UNI_L("<tr><td>%s</td>\r\n"), filename.CStr());
			}
			else
#endif // !RAMCACHE_ONLY
			{
				OpString memstring;
				TRAPD(err, g_languageManager->GetStringL(Str::SI_CACHE_MEMORY_TEXT, memstring));
				target.WriteDocumentDataUniSprintf(UNI_L("<tr><td>%s</td>\r\n"), memstring.CStr());
			}
		}

		OpFileLength registered_len=0;
		url_rep->GetAttribute(URL::KContentLoaded, &registered_len);

		{
			OpString8 temp_len_str;
			OpString len_str;

			OpStatus::Ignore(g_op_system_info->OpFileLengthToString(registered_len, &temp_len_str));
			OpStatus::Ignore(len_str.Set(temp_len_str));

			target.WriteDocumentDataUniSprintf(UNI_L("<td>%s</td>\r\n<td>"), len_str.CStr());
		}

		uni_char *temp = HTMLify_string(url_rep->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped).CStr());
		if (temp == 0)
		{
			return OpStatus::ERR_NO_MEMORY;//FIXME:OOM7 - Do we need more advanced error handling? We have already written some data to the url...
		}
		target.WriteDocumentDataUniSprintf(UNI_L("<a href=\"%s\">%s</a>"), temp, temp);
		OP_DELETEA(temp);

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && !defined __OEM_OPERATOR_CACHE_MANAGEMENT
		BOOL neverflush = url_rep->GetAttribute(URL::KNeverFlush);
		uni_char *expdate =(uni_char *) g_memory_manager->GetTempBuf();
		if (neverflush)
		{
			time_t visited = 0;

			url_rep->GetAttribute(URL::KVLocalTimeVisited, &visited);

			time_t expires = visited +
				g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NeverFlushExpirationTimeDays)*24*60*60;
			struct tm *exp_tm = op_localtime(&expires);
			g_oplocale->op_strftime(expdate, 64, UNI_L("%x %X"), exp_tm);
		}
		else
		{
			expdate[0] = '-';
			expdate[1] = 0;
		}
		target.WriteDocumentDataUniSprintf(UNI_L("<td>%s"), expdate);
#endif
		
		target.WriteDocumentData(URL::KNormal, UNI_L("</td></tr>\r\n\r\n"));

		/*
		Name: url_rep->Name()
		Filename: url_rep->FileName()
		Mime type: http_url->MimeType()
		Url date: http_url->LoadDate()
		Local time loaded: url_rep->LocalTimeLoaded()
		Expire time: url_rep->Expires()
		Content size: url_rep->ContentSize()
		Loaded size: url_rep->ContentLoaded()
		Loading status: url_rep->GetStatus()
		Content type: url_rep->GetContentType()
		*/
	}


	return OpStatus::OK;
}

int compare_domain(const uni_char *string1, const uni_char *string2)
{
	uni_char *topdom1 = (uni_char*) uni_strrchr(string1,'.');
	uni_char *topdom2 = (uni_char*) uni_strrchr(string2,'.');

	int comp;

	if(topdom1 == NULL && topdom2 == NULL)
		return uni_strcmp(string1, string2);

	if(topdom1 == NULL)
	{
		comp = uni_strcmp(string1, topdom2);
		if(comp == 0)
			comp = -1;
		return comp;
	}
	if(topdom2 == NULL)
	{
		comp = uni_strcmp(topdom1, string2);
		if(comp == 0)
			comp = 1;
		return comp;
	}

	comp = uni_strcmp(topdom1, topdom2);
	if(comp == 0)
	{
		*topdom1 = '\0';
		*topdom2 = '\0';
		comp = compare_domain(string1, string2);
		*topdom1 = '.';
		*topdom2 = '.';
	}

	return comp;
}

int compare_ip(const uni_char *string1, const uni_char *string2)
{
	uni_char *topdom1 = (uni_char*) uni_strchr(string1,'.');
	uni_char *topdom2 = (uni_char*) uni_strchr(string2,'.');

	int comp;

	if(topdom1 == NULL && topdom2)
	{
		return -1;
	}
	if(topdom2 == NULL && topdom1)
	{
		return 1;
	}

	unsigned int v1,v2;

	if(topdom1)
		*topdom1 = '\0';
	v1 = uni_strtoul(string1,NULL, (*string1 == '0' ? 8 : 10));
	if(topdom1)
		*topdom1 = '.';

	if(topdom2)
		*topdom2 = '\0';
	v2 = uni_strtoul(string2,NULL,(*string2 == '0' ? 8 : 10));
	if(topdom2)
		*topdom2 = '.';

	if(v1 == v2)
	{
		if(topdom1 && topdom2)
			comp = compare_ip(topdom1+1, topdom2+1);
		else
			comp = 0;
	}
	else
		comp = (v1 < v2 ? -1 : 1);

	return comp;
}

/** return positive if first is greater than second 
 *	similar to op_strcmp(first, second);
 */
int urlsort_item_compare(const B23Tree_Item *first, const B23Tree_Item *second)
{
	urlsort_item *item1 = (urlsort_item *)  first;
	urlsort_item *item2 = (urlsort_item *)  second;

	URL_Rep *url1 = item1->GetURL_Rep();
	URL_Rep *url2 = item2->GetURL_Rep();

	if(url1 == url2)
		return 0; // same URL

	ServerName *s1 = (ServerName *) url1->GetAttribute(URL::KServerName, NULL);
	ServerName *s2 = (ServerName *) url2->GetAttribute(URL::KServerName, NULL);

	int comp = 0;
	if(s1 != s2)
	{
		const uni_char *s1n = s1->UniName();
		const uni_char *s2n = s2->UniName();
		if(s1n == NULL)
			comp = (s2n == NULL ? 0 : 1);
		else if(s2n == NULL)
			comp = -1;
		else
		{
			BOOL s1_ip = (uni_strspn(s1n,UNI_L("0123456789.")) == uni_strlen(s1n) ? TRUE : FALSE);
			BOOL s2_ip = (uni_strspn(s2n,UNI_L("0123456789.")) == uni_strlen(s2n) ? TRUE : FALSE);

			if(s1_ip)
			{
				if(s2_ip)
				{
					comp = compare_ip(s1n,s2n);
				}
				else
					comp = -1;
			}
			else if(!s1_ip && s2_ip)
				comp = 1;
			else
				comp = compare_domain(s1n, s2n);
		}

		if(comp)
			return comp;
	}

	if(url1->GetAttribute(URL::KRealType) != url2->GetAttribute(URL::KRealType))
	{
		OpStringC8 prot1= url1->GetAttribute(URL::KProtocolName);
		OpStringC8 prot2= url2->GetAttribute(URL::KProtocolName);
		
		comp = prot1.Compare(prot2);

		if(comp)
			return comp;
	}

	{
		unsigned short port1 = (unsigned short) url1->GetAttribute(URL::KResolvedPort);
		unsigned short port2 = (unsigned short) url2->GetAttribute(URL::KResolvedPort);

		if(port1 > port2)
			return 1;
		if(port1 < port2)
			return -1;

		port1 = (unsigned short) url1->GetAttribute(URL::KServerPort);
		port2 = (unsigned short) url2->GetAttribute(URL::KServerPort);

		if(port1 && !port2)
			return 1; // No port is listed first
		if(!port1 && port2)
			return -1; // No port is listed first

		// same port field
	}

	{
		OpStringC8 path1(url1->GetAttribute(URL::KProtocolName));
		OpStringC8 path2= url2->GetAttribute(URL::KProtocolName);

		if(path1.CStr() != NULL  && path2.CStr() != NULL)
			comp = UriUnescape::strcmp(path1.CStr(), path2.CStr(), UriUnescape::Safe);
		else if(path1.CStr() != NULL)
			comp = 1;
		else if(path2.CStr() != NULL)
			comp = -1;
	}
	if(comp == 0)
	{
		// Same path components, must be unique, comparing on ID (address)

		if((unsigned long) url1 > (unsigned long) url2)
			comp = 1;
		else if((unsigned long) url1 < (unsigned long) url2)
			comp = -1;
	}

	return comp;
}

int urlsort_item::SearchCompare(void *search_param)
{
	return urlsort_item_compare((urlsort_item *) search_param, this);
}


BOOL urlsort_item::TraverseActionL(uint32 action, void *params)
{
	if(action == 1)
	{
		URL *target = (URL *) params;

		if(!target || OpStatus::IsError(WriteURL(*target)))
			return FALSE;
	}

	return TRUE;
}

urlsort_st::urlsort_st()
: B23Tree_Store(urlsort_item_compare)
{
}

OP_STATUS urlsort_st::AddRecord(URL_Rep *url)
{
	if(url == NULL)
		return OpStatus::OK;

	urlsort_item search_item(url);

	urlsort_item * OP_MEMORY_VAR item = (urlsort_item *) Search(&search_item);

	if(item)
		return OpStatus::OK;

	item = OP_NEW(urlsort_item, (url));

	if(!item)
		return OpStatus::ERR_NO_MEMORY;

	TRAPD(op_err, InsertL(item));
	return op_err;
}

OP_STATUS urlsort_st::WriteRecords(URL &target, BOOL reverse)
{
	TRAPD(op_err, TraverseL(1, &target));
	return op_err;
}

#ifdef CACHE_STATS
void CacheStatistics::CreateStatsURLList(OpAutoVector<OpString> *list, OpString &out)
{
	if(out.CStr())
		out.CStr()[0]=0;
		
	out.AppendFormat(UNI_L("<span style=\"cursor:hand\" onclick=\"urlList%x.style.display='inline'\"><b>show</b><span style=\"display:none\" name=\"urlList%x\" id=\"urlList%x\"><br />"), list, list, list);
	for(UINT32 i=0; i<list->GetCount(); i++)
		out.AppendFormat(UNI_L("<div>%s</div>"), list->Get(i)->CStr());
	out.Append(UNI_L("</span></span>"));
}
#endif // CACHE_STATS

CacheListWriter::CacheListWriter(Context_Manager *ctx)
{
	ctx_man=NULL; 
#ifdef CACHE_ADVANCED_VIEW
	export_listener=NULL;
#endif
}

OP_STATUS CacheListWriter::WriteStatistics(CacheListStats &list_stats, URL& url)
{
	OP_ASSERT(ctx_man);

#if defined(CACHE_STATS) || defined(SELFTEST)
	OpString stats;
	
	// Configuration of the cache
	#ifdef SELFTEST
		stats.Set("<h3>DEBUG-only Configuration</h3>");
		stats.Append("<table><thead><th>TWEAK</th><th>State</th></thead><tbody>");

			OpString folder_path;

			g_folder_manager->GetFolderPath(ctx_man->GetCacheLocationForFiles(), folder_path);
			stats.AppendFormat(UNI_L("<tr><td>Cache Path</td><td>%s</td></tr>"), folder_path.CStr());

			g_folder_manager->GetFolderPath(ctx_man->GetCacheLocationForFilesCorrected(), folder_path);
			stats.AppendFormat(UNI_L("<tr><td>Cache Path Corrected</td><td>%s</td></tr>"), folder_path.CStr());

		stats.AppendFormat(UNI_L("<tr><td>TWEAK - Fast Index</td><td>%s</td></tr>"),
		#ifdef CACHE_FAST_INDEX
				UNI_L("Enabled")
		#else
			UNI_L("Disabled")
		#endif
		);
		
		stats.AppendFormat(UNI_L("<tr><td>TWEAK - Embedded files</td><td>%s</td></tr>"),
		#if CACHE_SMALL_FILES_SIZE>0
				UNI_L("Enabled")
		#else
			UNI_L("Disabled")
		#endif
		);
		
		stats.AppendFormat(UNI_L("<tr><td>TWEAK - Multiple folders</td><td>%s</td></tr>"),
		#ifdef CACHE_MULTIPLE_FOLDERS
			UNI_L("Enabled")
		#else
			UNI_L("Disabled")
		#endif
		);
		
		#ifdef CACHE_CONTAINERS_ENTRIES
				stats.AppendFormat(UNI_L("<tr><td>TWEAK - Cache containers</td><td>Enabled: %d</td></tr>"), CACHE_CONTAINERS_ENTRIES);
		#else
			stats.AppendFormat(UNI_L("<tr><td>TWEAK - Cache containers</td><td>Disabled</td></tr>"));
		#endif
		
		stats.AppendFormat(UNI_L("<tr><td>TWEAK - Search Engine cache</td><td>%s</td></tr>"),
		#ifdef SEARCH_ENGINE_CACHE
			UNI_L("YES")
		#else
			UNI_L("NO")
		#endif
		);
		
		stats.AppendFormat(UNI_L("<tr><td>FEATURE - Disk Cache Support</td><td>%s</td></tr>"),
		#ifdef DISK_CACHE_SUPPORT
			UNI_L("YES")
		#else
			UNI_L("NO")
		#endif
		);
		
		stats.AppendFormat(UNI_L("<tr><td>TWEAK - Associated files enabled</td><td>%s</td></tr>"),
		#ifdef URL_ENABLE_ASSOCIATED_FILES
			UNI_L("YES")
		#else
			UNI_L("NO")
		#endif
		);

		stats.AppendFormat(UNI_L("<tr><td>TWEAK - Pub Suffix enabled</td><td>%s</td></tr>"),
		#ifdef PUBSUFFIX_ENABLED
			UNI_L("YES")
		#else
			UNI_L("NO")
		#endif
		);
		stats.AppendFormat(UNI_L("<tr><td>Disk Cache Size</td><td>%d</td></tr>"), (int) ctx_man->GetCacheSize());
		stats.AppendFormat(UNI_L("<tr><td>Disk Cache Used</td><td>%d</td></tr>"), (int)  ctx_man->size_disk.GetUsed());
		stats.AppendFormat(UNI_L("<tr><td>RAM Cache Size</td><td>%d</td></tr>"), (int)  ctx_man->size_ram.GetSize());
		stats.AppendFormat(UNI_L("<tr><td>RAM Cache Used</td><td>%d</td></tr>"), (int)  ctx_man->size_ram.GetUsed());
		stats.Append("</tbody></table><br />");
		
		WriteCacheListStats(stats, list_stats, TRUE);
		stats.Append("<br />");
	#endif // SELFTEST
		
	#ifdef CACHE_STATS
		stats.Append("<h3>Statistics</h3>");
		stats.Append("<table><thead><th>Variable</th><th>Value</th><th>Details</th></thead>");

		#if CACHE_SMALL_FILES_SIZE>0
			stats.AppendFormat(UNI_L("<tr><td>Embedded files:</td><td>max size: %u</td><td>files embedded: %u</td></tr>"), CACHE_SMALL_FILES_SIZE, urlManager->GetEmbeddedFiles());
			stats.AppendFormat(UNI_L("<tr><td>Embedded files:</td><td>mem used: %u %s</td><td>mem max: %u KB</td></tr>"), (urlManager->GetEmbeddedSize()>4192)?urlManager->GetEmbeddedSize()/1024:urlManager->GetEmbeddedSize(), (urlManager->GetEmbeddedSize()>4192)?UNI_L("KB"):UNI_L("Bytes"),CACHE_SMALL_FILES_LIMIT/1024);
		#else
			stats.AppendFormat(UNI_L("<tr><td>Embedded files:</td><td>DISABLED</td><td></td></tr>"));
		#endif
		
		stats.AppendFormat(UNI_L("<tr><td>Index Read Time</td><td>%u ms</td><td></td></tr>"), ctx_man->stats.index_read_time);
		stats.AppendFormat(UNI_L("<tr><td>Index Read Max Time</td><td>%u ms</td><td></td></tr>"), ctx_man->stats.max_index_read_time);
		stats.AppendFormat(UNI_L("<tr><td>Index Number of Read</td><td>%u</td><td></td></tr>"), ctx_man->stats.index_num_read);
		stats.AppendFormat(UNI_L("<tr><td>Index Init Time</td><td>%u ms</td><td></td></tr>"), ctx_man->stats.index_init);
		stats.AppendFormat(UNI_L("<tr><td>Sync Time</td><td>%u + %u ms</td><td></td></tr>"), ctx_man->stats.sync_time1, ctx_man->stats.sync_time2);
		stats.AppendFormat(UNI_L("<tr><td>Number of URLs at the startup</td><td>%u</td><td></td></tr>"), ctx_man->stats.urls);
		stats.AppendFormat(UNI_L("<tr><td>Number of Cache request</td><td>%u</td><td></td></tr>"), ctx_man->stats.req);
		stats.AppendFormat(UNI_L("<tr><td>Files in a container</td><td>%u</td><td></td></tr>"), ctx_man->stats.container_files);
	
		stats.Append("</table><br /><br />");
	
		ctx_man->stats.StatsReset(FALSE);
	#endif // CACHE_STATS
	
	return url.WriteDocumentData(URL::KNormal, stats.CStr());
#else
	return OpStatus::OK;
	
#endif  // CACHE_STATS
}

#ifdef CACHE_ADVANCED_VIEW
// Class used to construct the list of the domains presents in the cache
class DomainSummary
{
private:
	OpString domain;
	OpString stripped_domain;
	UINT32 num_url;

public:
	DomainSummary(): num_url(0){ }
	// Set the domain
	OP_STATUS SetDomain(const uni_char *url, URL_Rep *rep)
	{
		OP_ASSERT(url);
		
		RETURN_IF_ERROR(domain.Set(url));
		/*return stripped_domain.Set(domain);*/
		
		uni_char *cur=domain.CStr();
		int len_domain=domain.Length();
		int last_dot=-1;
		int last_but_one_dot=-1;
		int last_but_two_dot=-1;
		
		for(int k=0; k<len_domain; k++)
		{
			if(cur[k]=='.')
			{
				last_but_two_dot=last_but_one_dot;
				last_but_one_dot=last_dot;
				last_dot=k;
			}
		}
		
		if(last_but_one_dot>=0)
		{
			// 	CORE-31136: IPs are no longer truncated. As numeric TLD don't exist, checking for a number
			// seems to be the quickest way.
			if(cur[last_but_one_dot+1]>='0' && cur[last_but_one_dot+1]<='9')
				return stripped_domain.Set(domain);

			// Check public suffix; if the module is not enabled, only a bunch of them are checked
			BOOL pub_suffix=FALSE;	// TRUE if it is a public suffix
			BOOL pub_suffix_checked=FALSE;	// TRUE if the check has been performed by the pubsuffix module

		#ifdef PUBSUFFIX_ENABLED
			OpString8 host8;
			if(OpStatus::IsSuccess(host8.Set(cur+last_but_one_dot+1)))
			{
				ServerName *sn=g_url_api->GetServerName(host8,TRUE);

				if(sn)
				{
					ServerName::DomainType dt=sn->GetDomainTypeASync();

					if(dt!=ServerName::DOMAIN_WAIT_FOR_UPDATE && dt!=ServerName::DOMAIN_UNKNOWN) // If the public suffix list is not ready, just check the fallback list
					{
						pub_suffix_checked=TRUE;

						if(dt==ServerName::DOMAIN_REGISTRY)
							pub_suffix=TRUE;
					}
				}
			}
		#endif

			OP_ASSERT(!pub_suffix || (pub_suffix && pub_suffix_checked));

			// (Very tiny) fallback list
			if(!pub_suffix_checked && (
					!uni_strcmp(cur+last_but_one_dot+1, UNI_L("operaunite.com")) ||
					!uni_strcmp(cur+last_but_one_dot+1, UNI_L("co.uk")) ||
					!uni_strcmp(cur+last_but_one_dot+1, UNI_L("co.jp")) ||
					!uni_strcmp(cur+last_but_one_dot+1, UNI_L("ac.jp")) ))
				pub_suffix=TRUE;

			return stripped_domain.Set(cur+1+ (pub_suffix?last_but_two_dot:last_but_one_dot) );
		}
		else
			return stripped_domain.Set(domain);
	}
	
	// Return the domain stripped (www.opera.com ==> opera.com)
	const uni_char* GetStrippedDomain() { return stripped_domain.CStr(); }
	// Return the domain
	const uni_char* GetDomain() { return domain.CStr(); }
	// Return the number of URL contained in the domain
	UINT32 GetNumberOfURLs() { return num_url; }
	// Increase the number of URLs
	void SetNumberOfURLs(int num) { num_url=num; }
};

#ifdef CRYPTO_API_SUPPORT
	#include "modules/libcrypto/include/OpRandomGenerator.h"
#else
	#include "modules/libssl/sslrand.h"
#endif

OP_STATUS CacheListWriter::ComputeValidation(const OpString8 &action, OpString &validation)
{
	OpString *random;
	validation.Empty();
	
	if(!action.CompareI("export"))
		random=&random_key_priv;
	else
		return OpStatus::OK; // No validation required for list and preview
	
	// Create the random key
	if(random->IsEmpty())
	{
		UINT32 rnd;
		
		#ifdef CRYPTO_API_SUPPORT
			rnd=g_libcrypto_random_generator->GetUint32();
		#else
			SSL_RND(&rnd, sizeof(rnd));
		#endif
		
		RETURN_IF_ERROR(random->AppendFormat(UNI_L("%d_%d"), rnd, (UINT32) (g_op_time_info->GetTimeUTC()/1000.0)));
	}
	
	return validation.Set(random->CStr());
	
/*#ifdef CRYPTO_HASH_MD5_SUPPORT
	OpAutoPtr<CryptoHash> hash(CryptoHash::CreateMD5());
	if (!hash.get())
		return OpStatus::ERR_NO_MEMORY;
#else			
	SSL_Hash_Pointer hash(digest_fields.digest);
#endif

	hash->InitHash();
	//hash->CalculateHash();*/
}

// Check the validation code to authorize the action
OP_STATUS CacheListWriter::CheckValidation(const OpString8 &action, const OpString &validation, BOOL &ok)
{
	OpString correct_validation;
	
	ok=FALSE;
	RETURN_IF_ERROR(ComputeValidation(action, correct_validation));
	
	if(correct_validation.IsEmpty() || !correct_validation.Compare(validation))
		ok=TRUE;
	
	return OpStatus::OK;
}


void CacheListWriter::SortDomains(OpVector<DomainSummary> *domains)
{
	OP_ASSERT(domains);
	
	if(!domains)
		return;
		
	/// Shell sort of the domains
	int size=domains->GetCount();
	int i, j, increment=size/2;
	
	#ifdef _DEBUG
		int cur=0;
		
		// Check that the stripped domain matches with the domain...
		for(cur=0; cur<size; cur++)
			OP_ASSERT(uni_strstr(domains->Get(cur)->GetDomain(), domains->Get(cur)->GetStrippedDomain()));
	#endif
 
	while (increment > 0)
	{
		for (i = increment; i < size; ++i)
		{
			j = i;
      
			DomainSummary *tempElem=domains->Get(i);
			OP_ASSERT(tempElem);
	
			if(tempElem)
			{
				while (	j >= increment && 
						domains->Get(j-increment) && 
						uni_strcmp(domains->Get(j-increment)->GetStrippedDomain(), tempElem->GetStrippedDomain()) > 0)
				{
					domains->Replace(j, domains->Get(j-increment));
					j-=increment;
				}
			}
			domains->Replace(j, tempElem);
		}
 
		if (increment == 2)
			increment = 1;
		else 
			//increment = (int) (increment / 2.2f);
			increment = (int) (increment * 0.454545f);
	}
	
	#ifdef _DEBUG
		// Check that the stripped domain still matches with the domain...
		for(cur=0; cur<size; cur++)
			OP_ASSERT(uni_strstr(domains->Get(cur)->GetDomain(), domains->Get(cur)->GetStrippedDomain()));
	#endif
}

#define ADD_MIME_CLICK(desc) \
RETURN_IF_ERROR(dom_list.Append("<span style=\"cursor:hand\" onclick=\"var ck=mime_"));	\
RETURN_IF_ERROR(dom_list.AppendUnsignedLong(mime_id));	\
RETURN_IF_ERROR(dom_list.Append("; ck.checked=(ck.checked?false:true); \">" desc "</span>"));

// First part of ADD_MIME
#define _ADD_MIME_1()	\
RETURN_IF_ERROR(dom_list.Append("<td><input name=\"mime\" id=\"mime_"));	\
RETURN_IF_ERROR(dom_list.AppendUnsignedLong(++mime_id));					\
RETURN_IF_ERROR(dom_list.Append("\" type=\"checkbox\" value=\""));

// Second part of ADD_MIME
#define _ADD_MIME_2(desc)	\
RETURN_IF_ERROR(dom_list.Append("\"/> "));		\
ADD_MIME_CLICK(desc);							\
RETURN_IF_ERROR(dom_list.Append("</td>"));

// Add mime with long value
#define _ADD_MIME(desc, value)							\
_ADD_MIME_1()											\
RETURN_IF_ERROR(dom_list.AppendUnsignedLong(value));	\
_ADD_MIME_2(desc)

// Add mime with string value
#define _ADD_MIME_STRING(desc, value)		\
_ADD_MIME_1()								\
RETURN_IF_ERROR(dom_list.Append(value));	\
_ADD_MIME_2(desc)


#define _TR_START() RETURN_IF_ERROR(dom_list.Append(UNI_L("<tr>")));
#define _TR_END() RETURN_IF_ERROR(dom_list.Append(UNI_L("</tr>")));
#define _TD_START_END() RETURN_IF_ERROR(dom_list.Append(UNI_L("<td> </td>")));

OP_STATUS CacheListWriter::WriteAdvancedViewSummary(URL& url, OpStringHashTable<DomainSummary> *domains)
{
	OP_ASSERT(ctx_man);

	OpString str_min_size;
	OpString str_max_size;

	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CACHE_MIN_SIZE, str_min_size));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CACHE_MAX_SIZE, str_max_size));
	
#ifdef _DEBUG
	// Demo export functionality
	if(!export_listener)
	{
		OpString temp;
	
		OpString tmp_storage;
		temp.AppendFormat(UNI_L("%s%s"), g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_CACHE_FOLDER, tmp_storage), UNI_L("export"));
		save_view.SetDirectory(&temp);
	
		export_listener=&save_view;
	}
#endif

	// Show the domains available
	OpHashIterator *iterator=domains->GetIterator();
	OpAutoPtr<OpHashIterator> ap(iterator);
	OpVector<DomainSummary> domain_list; // Tee memory is taken from the hash table...
	TempBuffer dom_list;
	OpString8 action_empty; // The action for now it is not important, because the random number does not change
	OpString validation;

	RETURN_IF_ERROR(ComputeValidation(action_empty, validation));

#ifdef SELFTEST
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<script>function ListPending() { filter_form.target='_blank'; filter_form.list.value='listPending'; filter_form.domain.value=''; filter_form.validation.value='")));
	RETURN_IF_ERROR(dom_list.Append(validation.CStr()));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("'; filter_form.submit(); } </script>")));

	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<script>function ListUnique() { filter_form.target='_blank'; filter_form.list.value='listUnique'; filter_form.domain.value=''; filter_form.validation.value='")));
	RETURN_IF_ERROR(dom_list.Append(validation.CStr()));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("'; filter_form.submit(); } </script>")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<script>function ListAll() { filter_form.target='_blank'; filter_form.list.value='listAll'; filter_form.domain.value=''; filter_form.validation.value='")));
	RETURN_IF_ERROR(dom_list.Append(validation.CStr()));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("'; filter_form.submit(); } </script>")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<script>function ListAllNoFiles() { filter_form.target='_blank'; filter_form.list.value='listAllNoFiles'; filter_form.domain.value=''; filter_form.validation.value='")));
	RETURN_IF_ERROR(dom_list.Append(validation.CStr()));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("'; filter_form.submit(); } </script>")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<script>function ListHTTPS() { filter_form.target='_blank'; filter_form.list.value='listAllHTTPS'; filter_form.domain.value=''; filter_form.validation.value='")));
	RETURN_IF_ERROR(dom_list.Append(validation.CStr()));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("'; filter_form.submit(); } </script>")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<script>function ListUsed(num) { filter_form.target='_blank'; filter_form.list.value='listUsed'; filter_form.num.value=num; filter_form.domain.value=''; filter_form.validation.value='")));
	RETURN_IF_ERROR(dom_list.Append(validation.CStr()));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("'; filter_form.submit(); } </script>")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<script>function ListRef(num) { filter_form.target='_blank'; filter_form.list.value='listRef'; filter_form.num.value=num; filter_form.domain.value=''; filter_form.validation.value='")));
	RETURN_IF_ERROR(dom_list.Append(validation.CStr()));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("'; filter_form.submit(); } </script>")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<script>function ListType(num) { filter_form.target='_blank'; filter_form.list.value='listAllType'; filter_form.num.value=num; filter_form.domain.value=''; filter_form.validation.value='")));
	RETURN_IF_ERROR(dom_list.Append(validation.CStr()));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("'; filter_form.submit(); } </script>")));
	
#endif


	// Form used to filter the content as requested by the user
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<iframe name=\"hidden_frame\" style=\"display:none\"></iframe><form target=\"_blank\" action=\"opera:cache\" method=\"get\" name=\"filter_form\" id=\"filter_form\"><input type=\"hidden\" name=\"domain\" /><input type=\"hidden\" name=\"validation\" /><input type=\"hidden\" name=\"list\" /><input type=\"hidden\" name=\"num\" /><span class=\"label\">")));
	RETURN_IF_ERROR(dom_list.Append(str_min_size.CStr()));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("</span><input name=\"min_size\" type=\"number\" maxlength=\"6\"/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class=\"label\">")));
	RETURN_IF_ERROR(dom_list.Append(str_max_size.CStr()));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("</span><input name=\"max_size\" type=\"number\" maxlength=\"6\"/><br /><br />")));

#ifdef SELFTEST
	RETURN_IF_ERROR(dom_list.Append(UNI_L("HTTP Response code: <input name=\"http_response\"><br /><br />")));
#endif


	if(OpStatus::IsSuccess(iterator->First()))
	{
		OpString temp;
		DomainSummary *summary;
		OpString validation_export;
		OpString8 action_export; // The action for now it is not important, because the random number does not change
		
		action_export.Set("export");
		
		RETURN_IF_ERROR(ComputeValidation(action_export, validation_export));
		
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<script>function PrevDomain(host) { filter_form.target='_blank'; filter_form.list.value='preview'; filter_form.domain.value=host; filter_form.validation.value='")));
		RETURN_IF_ERROR(dom_list.Append(validation.CStr()));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("'; filter_form.submit(); } </script>")));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<script>function ListDomain(host) { filter_form.target='_blank'; filter_form.list.value='list'; filter_form.domain.value=host; filter_form.validation.value='")));
		RETURN_IF_ERROR(dom_list.Append(validation.CStr()));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("'; filter_form.submit(); } </script>")));

		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<script>function ExportDomain(host) { filter_form.target='hidden_frame'; filter_form.list.value='export'; filter_form.domain.value=host; filter_form.validation.value='")));
		RETURN_IF_ERROR(dom_list.Append(validation_export.CStr()));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("'; filter_form.submit(); } </script>")));
		
		//RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<table><thead><th>Images</th><th>Video</th><th>Audio</th><th>Web</th></thead><tbody>")));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<table><tbody>")));
		
		_TR_START();
		
		int mime_id=0;
		
		_ADD_MIME("image/jpeg", URL_JPG_CONTENT);
		_ADD_MIME("video/mp4", URL_MP4_CONTENT);
		_ADD_MIME("audio/x-wav", URL_WAV_CONTENT);
		_ADD_MIME("text/html", URL_HTML_CONTENT);
		
		_TR_END();
		_TR_START();
		
		_ADD_MIME("image/gif", URL_GIF_CONTENT);
		_ADD_MIME("video/mpeg", URL_MPG_CONTENT);
		_ADD_MIME("audio/midi", URL_MIDI_CONTENT);
		_ADD_MIME("text/css", URL_CSS_CONTENT);
		
		_TR_END();
		_TR_START();
		
		_ADD_MIME("image/png", URL_PNG_CONTENT);
		// Problem: '/' ==> %2F... So '/' is converted to '_'...
		_ADD_MIME_STRING("video/flv", "video_flv");
		_ADD_MIME_STRING("audio/mpeg", "audio_mpeg");
		//_TD_START_END();
		_ADD_MIME("application/x-javascript", URL_X_JAVASCRIPT);
		
		_TR_END();
		_TR_START();
		
		_ADD_MIME("image/bmp", URL_BMP_CONTENT);
		// Problem: '/' ==> %2F... So '/' is converted to '_'...
		_ADD_MIME_STRING("video/x-flv", "video_x-flv");
		_ADD_MIME_STRING("audio/mp3", "audio_mp3");
		_ADD_MIME("application/xml", URL_XML_CONTENT);
		
		_TR_END();
		_TR_START();
		
		_ADD_MIME("image/webp", URL_WEBP_CONTENT);
		_TD_START_END();
		_TD_START_END();
		_ADD_MIME("text/plain", URL_TEXT_CONTENT);
		
		_TR_END();
		_ADD_MIME("image/svg+xml", URL_SVG_CONTENT);
		_TD_START_END();
		_TD_START_END();
		_TD_START_END();
		_TR_END();
		_TR_START();
		
		#ifdef ICO_SUPPORT
			_TR_START();
			_ADD_MIME("image/x-icon", URL_ICO_CONTENT);
			_TD_START_END();
			_TD_START_END();
			_TD_START_END();
			_TR_END();
		#endif // ICO_SUPPORT
		
		
		
		
		//_ADD_MIME("image/x-quicktime", );
		
		
		//MP3 _ADD_MIME("audio/mpeg", );
		
		//_ADD_MIME("video/x-msvideo", );
		//_ADD_MIME("application/ogg", );
		//_ADD_MIME("video/quicktime", );
		
		
		
		
		//_ADD_MIME("text/plain", URL_TEXT_CONTENT);
		
		//_ADD_MIME("application/xml-dtd", );
		//_ADD_MIME("application/xslt+xml", );
		
		
		/*_ADD_MIME("application/octet-stream", );
		_ADD_MIME("application/zip", );
		_ADD_MIME("application/x-tar", );
		_ADD_MIME("application/pdf", );
		_ADD_MIME("application/x-shockwave-flash", );*/
		
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n</tbody></table>")));

		RETURN_IF_ERROR(dom_list.Append(UNI_L("</form><br /><br />")));
		
		OpString str_preview_all;
		OpString str_list_all;
		OpString str_export_all;
		OpString str_preview;
		OpString str_list;
		OpString str_export;
		OpString str_domain;
		OpString str_urls;

		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CACHE_PREVIEW_ALL, str_preview_all));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CACHE_LIST_ALL, str_list_all));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CACHE_EXPORT_ALL, str_export_all));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CACHE_PREVIEW, str_preview));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CACHE_LIST, str_list));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CACHE_EXPORT, str_export));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CACHE_DOMAIN, str_domain));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CACHE_NUMBER_URL, str_urls));
		
		
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<br /><a class=\"label\" href=\"javascript: PrevDomain(''); \">")));
		RETURN_IF_ERROR(dom_list.Append(str_preview_all.CStr())); // "Preview all domains"
		RETURN_IF_ERROR(dom_list.Append(UNI_L("</a>\n<br /><a class=\"label\" href=\"javascript: ListDomain(''); \">")));
		RETURN_IF_ERROR(dom_list.Append(str_list_all.CStr()));		// "List all domains"
		RETURN_IF_ERROR(dom_list.Append(UNI_L("</a>")));

		if(export_listener)
		{
			RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<br /><a class=\"label\" href=\"javascript: ExportDomain('')\">")));
			RETURN_IF_ERROR(dom_list.Append(str_export_all.CStr())); // "Export all domains"
			RETURN_IF_ERROR(dom_list.Append(UNI_L("</a>")));
		}
#ifdef SELFTEST
		// No translation, as it is only for internal builds
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<br /><br /><a class=\"label\" href=\"javascript: ListPending(); \">")));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("Pending downloads</a>")));		// List all pending URLs (URL_LOADING and of 0 bytes)
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<br /><a class=\"label\" href=\"javascript: ListUnique(); \">")));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("Unique URLs</a>")));		// List all unique URLs
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<br /><a class=\"label\" href=\"javascript: ListAllNoFiles(); \">")));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("All URLs - No files</a>")));		// List all URLs known to the cache. Many can be duplicated
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<br /><a class=\"label\" href=\"javascript: ListAll(); \">")));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("All URLs - Also files</a>")));		// List all URLs known to the cache. Many can be duplicated
#endif

		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<table><thead><th>")));
		RETURN_IF_ERROR(dom_list.Append(str_domain.CStr())); // "Domain"
		RETURN_IF_ERROR(dom_list.Append(UNI_L("</th><th>")));
		RETURN_IF_ERROR(dom_list.Append(str_urls.CStr())); // "URLs cached"
		RETURN_IF_ERROR(dom_list.Append(UNI_L("</th><th>")));
		RETURN_IF_ERROR(dom_list.Append(str_preview.CStr())); // "Preview"
		RETURN_IF_ERROR(dom_list.Append(UNI_L("</th><th>")));
		RETURN_IF_ERROR(dom_list.Append(str_list.CStr())); // "List"
		RETURN_IF_ERROR(dom_list.Append(UNI_L("</th>")));
		
		if(export_listener)
		{
			RETURN_IF_ERROR(dom_list.Append(UNI_L("<th>")));
			RETURN_IF_ERROR(dom_list.Append(str_export.CStr())); // "Export"
			RETURN_IF_ERROR(dom_list.Append(UNI_L("</th>")));
		}
		RETURN_IF_ERROR(dom_list.Append(UNI_L("</thead><tbody>")));
		
		// Put the hash table in a list
		do
		{
			summary=(DomainSummary *)iterator->GetData();
			
			OP_ASSERT(summary);
			
			if(summary)
				RETURN_IF_ERROR(domain_list.Add(summary));
		}
		while(OpStatus::IsSuccess(iterator->Next()));
		
		SortDomains(&domain_list);
		
		OpString temp_domain;
		int i=0;
		int len=0;
		
		
		// Eliminate the duplicates
		for(i=1, len=domain_list.GetCount(); i<len; i++)
		{
			DomainSummary * cur_summary=domain_list.Get(i);
			DomainSummary * prev_summary=domain_list.Get(i-1);
			
			OP_ASSERT(cur_summary && prev_summary);
			
			const uni_char *cur_stripped=(cur_summary)?cur_summary->GetStrippedDomain():(uni_char *)NULL;
			const uni_char *prev_stripped=(prev_summary)?prev_summary->GetStrippedDomain():(uni_char *)NULL;
			
			if(cur_stripped && prev_stripped && !uni_strcmp(prev_stripped, cur_stripped))
			{
				cur_summary->SetNumberOfURLs(cur_summary->GetNumberOfURLs()+prev_summary->GetNumberOfURLs());
				domain_list.Replace(i-1, NULL);
			}
		}
		
		// Create the table
		for(i=0, len=domain_list.GetCount(); i<len; i++)
		{
			summary=(DomainSummary *)domain_list.Get(i);
			
			if(summary) // merged domains are null
			{
				if(temp.CStr())
					temp[0]=0;
				
				uni_char *htmlDomain=HTMLify_string(summary->GetStrippedDomain());
				
				// temp.AppendFormat(UNI_L("<tr><td><b>%s</b></td><td>%u</td><td><i>%u</i></td><td><a href=\"javascript: PrevDomain('%s'); \">Preview</a></td><td><a href=\"javascript: ListDomain('%s'); \">List</a></td></tr>"), HTMLify_string(summary->domain.CStr()), summary->num_url, summary->num_temp, htmlDomain, htmlDomain);
				temp.AppendFormat(UNI_L("<tr><td><b>%s</b></td><td>%u</td>"), htmlDomain, summary->GetNumberOfURLs());
				
				temp.AppendFormat(UNI_L("<td><a href=\"javascript: PrevDomain('%s'); \">%s</a></td><td><a href=\"javascript: ListDomain('%s'); \">%s</a></td>"), htmlDomain, str_preview.CStr(), htmlDomain, str_list.CStr());
				if(export_listener)
					temp.AppendFormat(UNI_L("<td><a href=\"javascript: ExportDomain('%s'); \">%s</a></td></tr>"), htmlDomain, str_export.CStr());
				
				temp.Append(UNI_L("</tr>"));
				
				RETURN_IF_ERROR(dom_list.Append(temp.CStr()));
				
				OP_DELETEA(htmlDomain);
			}
		}
		
		RETURN_IF_ERROR(dom_list.Append(UNI_L("</tbody></table>")));
	}
	else
	{
#ifdef SELFTEST
		RETURN_IF_ERROR(dom_list.Append(UNI_L("</form><br /><br />")));

		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<br /><br /><a class=\"label\" href=\"javascript: ListPending(); \">")));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("Pending downloads</a>")));		// "List all pending URLs". No translation, as it is only for internal builds
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<br /><br /><a class=\"label\" href=\"javascript: ListUnique(); \">")));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("Unique URLs</a>")));		// "List all unique URLs". No translation, as it is only for internal builds
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<br /><a class=\"label\" href=\"javascript: ListAllNoFiles(); \">")));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("All URLs - No files</a>")));		// List all URLs known to the cache. Many can be duplicated
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<br /><a class=\"label\" href=\"javascript: ListAll(); \">")));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("All URLs</a>")));		// List all URLs known to the cache. Many can be duplicated
#endif
	}

	RETURN_IF_ERROR(dom_list.Append(UNI_L("</body></html>")));
		
	// Write to the browser
	if(OpStatus::IsSuccess(url.WriteDocumentData(URL::KNormal, dom_list.GetStorage())))
	{
		url.WriteDocumentDataFinished();
		
		return OpStatus::OK;
	}
	
	return OpStatus::OK;
}

// Rules that need to be applyed to decide which file show in the preview page
class FilterRules
{
private:
	friend class Context_Manager;

	// Domain enabled
	OpString domain;
	// length of the domain name
	int len_domain;
	// List of Content Types to show
	OpINT32Vector types_int;
	// List of MIME Types to show
	OpAutoVector<OpString8> types_string;
	// Minimum size allowed
	int min_size;
	// Maximum size allowed
	int max_size;
	// Types of URLs to show
	URLs_To_Show urls_to_show;
	// HTTP Response code (-1 == all of them)
	int http_response;
	/// Additional value used by SHOW_ALL_USED_URLS, SHOW_ALL_REF_URLS and SHOW_ALL_TYPE_URLS
	int show_num;
	
	// Reset the rules
	void Reset() { if(domain.CStr()) domain.CStr()[0]=0; len_domain=0; types_int.Clear(); min_size=-1; max_size=-1; types_string.DeleteAll(); types_string.Clear(); urls_to_show=SHOW_NORMAL_URLS; http_response=-1; show_num=0; }

public:
	// Parse the URL and find the rules
	OP_STATUS RetrieveRules(OpStringC *name, CacheEntriesListener *&listener, CacheEntriesListener *list_listener, CacheEntriesListener *preview_listener, CacheEntriesListener *export_listener, OpString8 &action, OpString &validation);
	// Apply the rules to see if a URL has to be shown
	BOOL ShowURL(URL_Rep *url_rep);
	// Add a MIME type to the list of approved type
	OP_STATUS AddMIME(OpString *value);
	// True if the rules filter based on the domain
	BOOL IsDomainFiltered() { return len_domain>0; }
	// Return the domain
	uni_char *GetDomain() { return domain.CStr(); }
	// Returns TRUE if pending URLs have to be shown
	URLs_To_Show GetURLsToShow() { return urls_to_show; }
};

BOOL FilterRules::ShowURL(URL_Rep *url_rep)
{
	BOOL show=TRUE;

	if(urls_to_show==SHOW_ALL_USED_URLS)
	{
		int used=url_rep->GetUsedCount();

		if( (show_num==0 && used) ||		// Accept if not used
			(show_num==1 && used!=1) ||		// Accept if used == 1
			(show_num==2 && used<2) ||      // Accept if used >  1
			(show_num==-1 && used<1) )      // Accept if used >= 1
			show=FALSE;
	}

	if(urls_to_show==SHOW_ALL_REF_URLS)
	{
		int ref=url_rep->GetRefCount();

		if( (show_num==0 && ref) ||			// Accept if not referenced
			(show_num==1 && ref!=1) ||		// Accept if ref == 1
			(show_num==2 && ref<2) ||       // Accept if ref >  1
			(show_num==-1 && ref<1) )       // Accept if ref >= 1
			show=FALSE;
	}

	if(urls_to_show==SHOW_ALL_TYPE_URLS)
	{
		URLCacheType ctype = (URLCacheType) url_rep->GetAttribute(URL::KCacheType);

		if((int)ctype != show_num)
			show=FALSE;
	}
	
	// Filter the domain
	if(len_domain)
	{
		show=FALSE;
		
		OpStringC host=url_rep->GetAttribute(URL::KUniHostName);
		const uni_char *host_str=host.CStr();
		int len=(host_str)?uni_strlen(host_str):0;
		
		if(host_str && len>=len_domain && !domain.Compare(host_str+len-len_domain))
			show=TRUE;
	}
	
	// Filter the Content type
	UINT32 num_types=types_int.GetCount();
	
	if(show && num_types>0)
	{
		show=FALSE;

		if(url_rep->GetDataStorage() && url_rep->GetDataStorage()->GetCacheStorage())
		{
			URLContentType type=url_rep->GetDataStorage()->GetCacheStorage()->GetContentType();
			
			for(UINT32 i=0; i<num_types; i++)
			{
				if(type==types_int.Get(i))
				{
					show=TRUE;
					break;
				}
			}
		}
	}
	
	if(!show || num_types==0)
	{
		// Filter the MIME type
		UINT32 mime_types=types_string.GetCount();
		
		if(mime_types>0)
		{
			OpStringC8 mime=url_rep->GetAttribute(URL::KMIME_Type);
			
			show=FALSE;
			
			for(UINT32 i=0; i<mime_types; i++)
			{
				if(!mime.Compare(types_string.Get(i)->CStr()))
				{
					show=TRUE;
					break;
				}
			}
		}
	}
	
	// Ensure that min_size<=max_size;
	if(min_size>=0 && max_size>=0 && max_size<min_size)
	{
		int t=min_size;
		
		min_size=max_size;
		max_size=t;
		
		OP_ASSERT(min_size<=max_size);
	}
	
	// Filter the size
	if(min_size>=0 || max_size>=0)
	{
		OpFileLength registered_len;
		
		url_rep->GetAttribute(URL::KContentLoaded, &registered_len);
		
		if(min_size>=0 && registered_len<(OpFileLength)(min_size*1024L))
			show=FALSE;
		if(max_size>=0 && registered_len>(OpFileLength)(max_size*1024L))
			show=FALSE;
	}

	if(http_response>=0)
	{
		UINT32 response = url_rep->GetAttribute(URL::KHTTP_Response_Code);

		if(static_cast<UINT32>(http_response)!=response)
			show=FALSE;
	}
	
	return show;
}

OP_STATUS FilterRules::AddMIME(OpString *value)
{
	if(!value || !value->CStr())
		return OpStatus::ERR_NULL_POINTER;
		
	if(value->CStr()[0]>='0' && value->CStr()[0]<='9')
		return types_int.Add(uni_atoi(value->CStr()));
		
	OpString8 *str=OP_NEW(OpString8, ());
	
	if(!str || !OpStatus::IsSuccess(str->Set(value->CStr())))
		return OpStatus::ERR_NO_MEMORY;
		
	// Problem: '/' ==> %2F... So '/' is converted to '_'... And now it is rollbacked...
	for(int i=0, len=str->Length(); i<len; i++)
		if(str->CStr()[i]=='_')
			str->CStr()[i]='/';
	
	return types_string.Add(str);
}

OP_STATUS FilterRules::RetrieveRules(OpStringC *name, CacheEntriesListener *&listener, CacheEntriesListener *list_listener, CacheEntriesListener *preview_listener, CacheEntriesListener *export_listener, OpString8 &action, OpString &validation)
{
	int start=name->FindLastOf('?')+1;
	int end=name->Length();
	const uni_char *str=name->CStr();
	OpString key;
	OpString value;
	
	OP_ASSERT(start>1);
	OP_ASSERT(preview_listener && list_listener);
	
	Reset();
	listener=NULL;
	action.Empty();
	validation.Empty();
	
	// Parse the URL
	while(start<end)
	{
		int cur_pos=start;
		
		// Look for the name of the parameter
		while(cur_pos<end && str[cur_pos]!='=' && str[cur_pos]!='&')
			cur_pos++;
			
		if(str[cur_pos]=='=' && str[cur_pos+1]!='&' && str[cur_pos+1])  // parameter with a value
		{
			int value_pos=cur_pos+1;
			int end_pos=value_pos+1;
			
			while(end_pos<end && str[end_pos]!='&')
				end_pos++;
				
			RETURN_IF_ERROR(key.Set(str+start, value_pos-1-start));
			RETURN_IF_ERROR(value.Set(str+value_pos, end_pos-value_pos));
			OP_ASSERT(str[end_pos]=='&' || !str[end_pos]);
			
			start=end_pos+1;
			
			if(!key.Compare("domain"))
			{
				RETURN_IF_ERROR(domain.Set(value));
				len_domain=domain.Length();
			}
			else if(!key.Compare("mime"))
				RETURN_IF_ERROR(AddMIME(&value));
			else if(!key.Compare("min_size"))
				min_size=uni_atoi(value.CStr());
			else if(!key.Compare("max_size"))
				max_size=uni_atoi(value.CStr());
			else if(!key.Compare("num"))
				show_num=uni_atoi(value.CStr());
			else if(!key.Compare("http_response"))
			{
				if(value.CStr() && *value.CStr())
					http_response=uni_atoi(value.CStr());
			}
			else if(!key.Compare("validation"))
				RETURN_IF_ERROR(validation.Set(value));
			else if(!key.Compare("list") || !key.Compare("listPending") || !key.Compare("listUnique") || !key.Compare("listAll") || !key.Compare("listAllNoFiles") || !key.Compare("listAllHTTPS") || !key.Compare("listUsed") || !key.Compare("listRef") || !key.Compare("listAllType"))
			{ 
				if(!value.Compare("listUnique"))
					urls_to_show=SHOW_UNIQUE_URLS;
				else if(!value.Compare("listPending"))
					urls_to_show=SHOW_PENDING_URLS;
				else if(!value.Compare("listAll"))
					urls_to_show=SHOW_ALL_URLS;
				else if(!value.Compare("listAllNoFiles"))
					urls_to_show=SHOW_ALL_URLS_NO_FILES;
				else if(!value.Compare("listAllHTTPS"))
					urls_to_show=SHOW_ALL_URLS_HTTPS;
				else if(!value.Compare("listUsed"))
					urls_to_show=SHOW_ALL_USED_URLS;
				else if(!value.Compare("listRef"))
					urls_to_show=SHOW_ALL_REF_URLS;
				else if(!value.Compare("listAllType"))
					urls_to_show=SHOW_ALL_TYPE_URLS;

				RETURN_IF_ERROR(action.Set(value.CStr()));
				if(!value.Compare("preview"))
					listener=preview_listener;
				else if(!value.Compare("export"))
				{
					OP_ASSERT(export_listener);
					
					listener=export_listener;
				}
			}
		}
		else
		{
			start=cur_pos+1;
		}
	}
	
	if(!listener)
		listener=list_listener;
		
	return OpStatus::OK;
}

OP_STATUS CacheListView::ListStart(URL document)
{
	url=document;
	
	RETURN_IF_ERROR(dom_list.Append(UNI_L("<html>")));
	RETURN_IF_ERROR(WriteScripts());
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<body><h3>")));
	
	if(filter->IsDomainFiltered())
	{
		SafePointer<uni_char> domain(HTMLify_string(filter->GetDomain()), TRUE);
		
		//RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<body><h3>Files from ")));
		RETURN_IF_ERROR(dom_list.Append(domain.GetPointer()));
	}
	
	RETURN_IF_ERROR(dom_list.Append(UNI_L("&nbsp;<span name=\"num_files\" id=\"num_files\"></span></h3>")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<table><thead>")));
	RETURN_IF_ERROR(WriteTableHeader());
	return dom_list.Append(UNI_L("</thead><tbody>"));
}

OP_STATUS CacheListView::ListEntry(URL_Rep *url_rep, int url_shown, BOOL show_link)
{
	SafePointer<uni_char> name(HTMLify_string(url_rep->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped).CStr()), TRUE);
	SafePointer<uni_char> path(HTMLify_string(url_rep->GetAttribute(URL::KUniPath_Escaped).CStr()), TRUE);
	const uni_char *desc=(const uni_char *)((filter->IsDomainFiltered())?path.GetPointer():name.GetPointer());
	OpFileLength registered_len=0;
	int len_desc=uni_strlen(desc);

	if(!loc)
		RETURN_IF_ERROR(OpLocale::Create(&loc));
	
	// TODO: look for a "/"...
	if(len_desc>50)
	{
		if(temp_desc.CStr())
			temp_desc.CStr()[0]=0;

		temp_desc.Append(desc, 20);
		temp_desc.Append(UNI_L(" ... "));
		temp_desc.Append(desc+len_desc-25, 25);
		desc=temp_desc.CStr();
	}
	
	// Name and file size
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<tr><td")));

#ifdef SELFTEST
	OpString tooltip;
	UINT32 response = url_rep->GetAttribute(URL::KHTTP_Response_Code);

	RETURN_IF_ERROR(tooltip.AppendFormat(UNI_L("Visited: %s"), (url_rep->HasBeenVisited())?UNI_L("YES"):UNI_L("NO")));
	RETURN_IF_ERROR(tooltip.AppendFormat(UNI_L(" - Is followed: %s"), (url_rep->GetIsFollowed())?UNI_L("YES"):UNI_L("NO")));
	RETURN_IF_ERROR(tooltip.AppendFormat(UNI_L(" - Used count: %d"), url_rep->GetUsedCount()));
	RETURN_IF_ERROR(tooltip.AppendFormat(UNI_L(" - Ref count: %d"), url_rep->GetRefCount()));
	RETURN_IF_ERROR(tooltip.AppendFormat(UNI_L(" - Response code: %d"), response));
	
	RETURN_IF_ERROR(dom_list.Append(UNI_L(" title=\"")));
	RETURN_IF_ERROR(dom_list.Append(tooltip.CStr()));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\"")));
#endif

	RETURN_IF_ERROR(dom_list.Append(UNI_L(">")));

	// Create a link only for not temporary URLS, as they can easily create a mess in opera:cache
	if(show_link && (URLCacheType)url_rep->GetAttribute(URL::KCacheType)!=URL_CACHE_TEMP)
	{
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<a href=\"")));
		RETURN_IF_ERROR(dom_list.Append((const uni_char *)name.GetPointer()));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\"")));
	}
	else
		RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<span")));

	RETURN_IF_ERROR(dom_list.Append(UNI_L(">")));

#ifdef SELFTEST
	URLCacheType type=(URLCacheType)url_rep->GetAttribute(URL::KCacheType);
	BOOL unique=!!url_rep->GetAttribute(URL::KUnique);

	if(unique)
		dom_list.Append("<b>(UNIQUE)</b> ");
	else if(type==URL_CACHE_TEMP)
		dom_list.Append("<b>(TEMP)</b> ");

	if(url_rep->GetRefCount()!=1)
		dom_list.AppendFormat(UNI_L("<b>(ref: %d)</b> "), url_rep->GetRefCount());

	if(url_rep->GetUsedCount())
		dom_list.AppendFormat(UNI_L("<b>[used: %d]</b> "), url_rep->GetUsedCount());

	if(url_rep->GetDataStorage()==NULL)
		dom_list.Append("<b>(NO Data Storage)</b> ");
	else if(url_rep->GetDataStorage()->GetCacheStorage()==NULL)
		dom_list.Append("<b>(NO Cache Storage)</b> ");

	if(url_rep->GetDataStorage() && !url_rep->GetDataStorage()->GetHTTPProtocolDataNoAlloc())
		dom_list.Append("<b>(NO HTTP Data)</b> ");
#endif

	RETURN_IF_ERROR(dom_list.Append(desc));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("</a></td><td>")));
	url_rep->GetAttribute(URL::KContentLoaded, &registered_len);

	if(registered_len>=4096)
	{
		OpStatus::Ignore(g_op_system_info->OpFileLengthToString((registered_len+512)/1024, &temp_len_str));

		RETURN_IF_ERROR(dom_list.Append(temp_len_str.CStr()));
		RETURN_IF_ERROR(dom_list.Append(UNI_L(" KB</td>")));
	}
	else
	{
		OpStatus::Ignore(g_op_system_info->OpFileLengthToString(registered_len, &temp_len_str));

		RETURN_IF_ERROR(dom_list.Append(temp_len_str.CStr()));
		if(registered_len==1)
			RETURN_IF_ERROR(dom_list.Append(UNI_L(" byte</td>")));
		else
			RETURN_IF_ERROR(dom_list.Append(UNI_L(" bytes</td>")));
	}
	
#ifdef SELFTEST
	// Write the type of file (embedded, container, file, or plain=used for view-source), the path and some timing information
	if(url_rep && url_rep->GetDataStorage() && url_rep->GetDataStorage()->GetCacheStorage())
	{
		Cache_Storage *cs=url_rep->GetDataStorage()->GetCacheStorage();
		const uni_char *type=UNI_L("File");
		OpString path;
		OpFileFolder fld;
		URLCacheType cache_type=cs->GetCacheType();
		
		if(cache_type==URL_CACHE_MEMORY)
			type=UNI_L("Memory");
		else if(cs->IsPlainFile())
			type=UNI_L("Plain-view source");
		else if(cs->IsEmbedded())
			type=UNI_L("Embedded");
		else if(cs->GetContainerID()>0)
			type=UNI_L("Container");
		else if(cache_type==URL_CACHE_NO)
			type=UNI_L("No Cache");
		else if(cache_type==URL_CACHE_DISK)
			type=UNI_L("Disk");
		else if(cache_type==URL_CACHE_TEMP)
			type=UNI_L("Temp");
		else if(cache_type==URL_CACHE_USERFILE)
			type=UNI_L("User File");
		else if(cache_type==URL_CACHE_STREAM)
			type=UNI_L("Stream");
		else if(cache_type==URL_CACHE_MHTML)
			type=UNI_L("MHTML");
			
		path.Set(cs->FileName(fld).CStr());
		
		// Timing information
		RETURN_IF_ERROR(dom_list.Append(UNI_L("<td><i title=\"")));

		time_t t_temp;
		tm *tm_temp;
		uni_char buf_time[100]; /* ARRAY OK 2011-04-28 lucav */

		url_rep->GetAttribute(URL::KVLocalTimeVisited, &t_temp);
		tm_temp=op_localtime(&t_temp);
		g_oplocale->op_strftime(buf_time, 99, UNI_L("Visited: %c - "), tm_temp);
		RETURN_IF_ERROR(dom_list.Append(buf_time));

		url_rep->GetAttribute(URL::KVLocalTimeLoaded, &t_temp);
		tm_temp=op_localtime(&t_temp);
		g_oplocale->op_strftime(buf_time, 99, UNI_L("Loaded: %c - "), tm_temp);
		RETURN_IF_ERROR(dom_list.Append(buf_time));

		RETURN_IF_ERROR(dom_list.AppendFormat(UNI_L("Expired: %d"), url_rep->Expired(FALSE, FALSE)));

		RETURN_IF_ERROR(dom_list.Append(UNI_L("\">")));

		// Type and path
		RETURN_IF_ERROR(dom_list.Append(type));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("</i></td><td>")));
		RETURN_IF_ERROR(dom_list.Append(path.CStr()));
		RETURN_IF_ERROR(dom_list.Append(UNI_L("</td>")));

		URLStatus st=(URLStatus) url_rep->GetAttribute(URL::KLoadStatus);
		const uni_char *status;

		if(st==URL_UNLOADED)
			status=UNI_L("UNLOADED");
		else if(st==URL_LOADED)
			status=UNI_L("LOADED");
		else if(st==URL_LOADING)
			status=UNI_L("LOADING");
		else if(st==URL_EMPTY)
			status=UNI_L("EMPTY");
		else if(st==URL_LOADING_ABORTED)
			status=UNI_L("LOADING_ABORTED");
		else if(st==URL_LOADING_FAILURE)
			status=UNI_L("LOADING_FAILURE");
		else if(st==URL_LOADING_WAITING)
			status=UNI_L("LOADING_WAITING");
		else
			status=UNI_L("LOADING_FROM_CACHE");

		RETURN_IF_ERROR(dom_list.AppendFormat(UNI_L("<td>%s</td>"), status));
		RETURN_IF_ERROR(dom_list.AppendFormat(UNI_L("<td>%s</td>"), cs->GetCacheDescription()));
	}
#endif // SELFTEST
	
	WriteAdditionalData(url_rep, url_shown, (const uni_char *)name.GetPointer(), desc);
	
	return dom_list.Append(UNI_L("</tr>"));
}

OP_STATUS CacheListView::ListEnd(CacheListStats &list_stats, int url_shown, int url_total, int num_no_ds, int num_no_cs, int num_no_http, int num_200, int num_0, int num_404, int num_301_307, int num_204)
{
	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n</tbody></table>")));
		
#ifdef SELFTEST
	OpString str;

	CacheListWriter::WriteCacheListStats(str, list_stats, FALSE);

	RETURN_IF_ERROR(dom_list.Append(UNI_L("\n<script>num_files.innerHTML='")));
	RETURN_IF_ERROR(dom_list.Append(str.CStr()));
	RETURN_IF_ERROR(dom_list.AppendUnsignedLong(url_shown));
	RETURN_IF_ERROR(dom_list.Append(UNI_L(" URLs out of ")));
	RETURN_IF_ERROR(dom_list.AppendUnsignedLong(url_total));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("<br /> No Data Storage: ")));
	RETURN_IF_ERROR(dom_list.AppendUnsignedLong(num_no_ds));
	RETURN_IF_ERROR(dom_list.Append(UNI_L(" - No Cache Storage: ")));
	RETURN_IF_ERROR(dom_list.AppendUnsignedLong(num_no_cs));
	RETURN_IF_ERROR(dom_list.Append(UNI_L(" - No HTTP Data: ")));
	RETURN_IF_ERROR(dom_list.AppendUnsignedLong(num_no_http));
	RETURN_IF_ERROR(dom_list.AppendFormat(UNI_L("<br /> HTTP Response code - 200: %d - 404: %d - 301/307: %d - 204: %d - No Reponse (0): %d - Other: %d"), num_200, num_404, num_301_307, num_204, num_0, url_shown - (num_200 + num_404 + num_301_307 + num_0)));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("';</script>")));
#endif
	
	RETURN_IF_ERROR(dom_list.Append(UNI_L("</body></html>")));
	
	RETURN_IF_ERROR(OpStatus::IsSuccess(url.WriteDocumentData(URL::KNormal, dom_list.GetStorage())));
	
	url.WriteDocumentDataFinished();
		
	return OpStatus::OK;
}

OP_STATUS CachePreviewView::Construct(const OpString &drstr_address, const OpString &drstr_size, const OpString &drstr_preview)
{
	RETURN_IF_ERROR(preview.Set(drstr_preview));
	
	return CacheListView::Construct(drstr_address, drstr_size);
}

OP_STATUS CachePreviewView::WriteAdditionalData(URL_Rep *url_rep, int url_shown, const uni_char *name, const uni_char *desc)
{
	if(url_rep->GetDataStorage())
	{
		URLContentType type=(URLContentType) url_rep->GetDataStorage()->GetAttribute(URL::KContentType);
		
		// Width x Height size and preview
		if( type==URL_GIF_CONTENT || 
			type==URL_JPG_CONTENT ||
			type==URL_BMP_CONTENT ||
			type==URL_WEBP_CONTENT ||
			type==URL_XBM_CONTENT ||
			type==URL_PNG_CONTENT ||
			type==URL_SVG_CONTENT ||
			type==URL_WBMP_CONTENT)
		{
			RETURN_IF_ERROR(dom_list.Append(UNI_L("<td id=\"size_")));
			RETURN_IF_ERROR(dom_list.AppendUnsignedLong(url_shown));
			RETURN_IF_ERROR(dom_list.Append(UNI_L("\"><br /></td><td id=\"td_")));
			RETURN_IF_ERROR(dom_list.AppendUnsignedLong(url_shown));
			RETURN_IF_ERROR(dom_list.Append(UNI_L("\" >")));
			
			RETURN_IF_ERROR(dom_list.Append(UNI_L("<img title=\"")));
			RETURN_IF_ERROR(dom_list.Append(desc));
			RETURN_IF_ERROR(dom_list.Append(UNI_L("\" src=\"")));
			RETURN_IF_ERROR(dom_list.Append(name));
			RETURN_IF_ERROR(dom_list.Append(UNI_L("\" id=\"img_")));
			RETURN_IF_ERROR(dom_list.AppendUnsignedLong(url_shown));
			RETURN_IF_ERROR(dom_list.Append(UNI_L("\" style=\"visibility:hidden\" onload=\"resizeImage(this, td_")));
			RETURN_IF_ERROR(dom_list.AppendUnsignedLong(url_shown));
			RETURN_IF_ERROR(dom_list.Append(UNI_L(", size_")));
			RETURN_IF_ERROR(dom_list.AppendUnsignedLong(url_shown));
			RETURN_IF_ERROR(dom_list.Append(UNI_L(");\" /></td>")));
		}
		else
			RETURN_IF_ERROR(dom_list.Append(UNI_L("<td></td><td></td>")));
	}
	else
		RETURN_IF_ERROR(dom_list.Append(UNI_L("<td></td><td></td>")));
		
	return OpStatus::OK;
}

OP_STATUS CachePreviewView::WriteTableHeader()
{
	OpString str;
			
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("<th>%s</th><th colspan=\"2\">%s</th><th>%s</th>"), address.CStr(), size.CStr(), preview.CStr()));

	return dom_list.Append(str.CStr());
}

OP_STATUS CachePreviewView::WriteScripts()
{
	RETURN_IF_ERROR(dom_list.Append(UNI_L("<script>\n function resizeImage(image, td, size){\n  var maxX=96, maxY=96, resize_x=image.width/maxX, ")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("  resize_y=image.height/maxY, orig_x=image.width, orig_y=image.height;\n")));
	RETURN_IF_ERROR(!dom_list.Append(UNI_L("  if(resize_x>1 || resize_y>1) {\n")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("    if(resize_x>resize_y)\n")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("      image.width=image.width/resize_x;\n")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("    else\n")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("      image.height=image.height/resize_y;\n  }\n")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("    image.style.visibility='visible';\n")));
	RETURN_IF_ERROR(dom_list.Append(UNI_L("    if(typeof(size)!='undefined')\n")));
	
	return dom_list.Append(UNI_L("      size.innerHTML=orig_x + 'x' +orig_y;\n}\n</script>"));
}

OP_STATUS CacheSaveView::ListStart(URL document)
{
	time=(UINT32)g_op_time_info->GetTimeUTC();
	
	return OpStatus::OK;
}

OP_STATUS CacheSaveView::ListEntry(URL_Rep *url_rep, int url_shown, BOOL link_url)
{
	if(path.IsEmpty())
		return OpStatus::ERR_OUT_OF_RANGE;
		
	Cache_Storage *cs=(url_rep->GetDataStorage())?url_rep->GetDataStorage()->GetCacheStorage():NULL;
	
	if(!cs)
		return OpStatus::OK;  // Empty files not written to disk
		
	OpString full_path;
	OpFile file;
	uni_char *url_path=HTMLify_string(url_rep->GetAttribute(URL::KUniPath_Escaped).CStr());
	int pos=uni_strlen(url_path)-1;
	int stop=(pos>10)?pos-10:0; // Extension cannot be more than 10 characters
	
	full_path.Set(path);
	
	// Look for the file extension
	OP_ASSERT(pos>=0);
	while(pos>=stop && url_path[pos]!='.' && url_path[pos]!='/' && url_path[pos]!='\\')
		pos--;
	
	// Construct the name with a meaningful extension
	if(pos>=stop && url_path[pos]=='.')
		full_path.AppendFormat(UNI_L("%ul_%d%s"), (unsigned long)time, url_shown, url_path+pos);
	else
	{
		OpStringC8 mime=url_rep->GetAttribute(URL::KMIME_Type);
		
		if(!mime.Compare("video/flv") || !mime.Compare("video/x-flv"))
			full_path.AppendFormat(UNI_L("%ul_%d.flv"), (unsigned long)time, url_shown);
		else if(!mime.Compare("audio/mpeg") || !mime.Compare("audio/mp3"))
			full_path.AppendFormat(UNI_L("%ul_%d.mp3"), (unsigned long)time, url_shown);
		else
			full_path.AppendFormat(UNI_L("%ul_%d"), (unsigned long)time, url_shown);
	}
		
	//RETURN_IF_ERROR(file.Construct(full_path.CStr()));
	OP_DELETEA(url_path);
	return cs->SaveToFile(full_path);
}

OP_STATUS CacheListView::Construct(const OpString &drstr_address, const OpString &drstr_size)
{
	RETURN_IF_ERROR(address.Set(drstr_address));
	
	return size.Set(drstr_size);
}

OP_STATUS CacheListView::WriteTableHeader()
{ 
	OpString str;
			
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("<th>%s</th><th>%s</th>"), address.CStr(), size.CStr()));
	
#ifdef SELFTEST
	// Not translated, as only used for debugging
	RETURN_IF_ERROR(str.Append(UNI_L("<th>Type</th><th>Path</th>")));
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("<th>Status</th><th>Storage</th>")));
#endif

	return dom_list.Append(str.CStr());
}
		  
OP_STATUS CacheListWriter::WriteAdvancedViewDetail(URL& url, BOOL &detail_mode, const OpString &drstr_address, const OpString &drstr_size, const OpString &drstr_filename, const OpString &drstr_preview)
{
	OP_ASSERT(ctx_man);

	OpStringC name_tmp=url.GetRep()->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped);
	
	// Check if we need to show the details of the URLs filtered according to the user preferences (done in this method) 
	// or not (and GenerateCacheList() will show the list of domains)
	if(name_tmp.FindLastOf('?')<=0)
	{
		detail_mode=FALSE;
		
		return OpStatus::OK;
	}

	OpString name;

	RETURN_IF_ERROR(name.Set(name_tmp));

	// Necessary to support IDN domains: CORE-33605
#ifdef DEBUG_ENABLE_OPASSERT
	int n=
#endif // DEBUG_ENABLE_OPASSERT
		UriUnescape::Unescape(name.CStr(), name_tmp.CStr(), UriUnescape::ConvertUtf8);

	OP_ASSERT(n<=name_tmp.Length());

	detail_mode=TRUE;
	FilterRules filter;
	CacheEntriesListener *listener;
	CacheListView list_listener(&filter);
	CachePreviewView preview_listener(&filter);
	OpString8 action;
	OpString validation;
	BOOL validation_ok;
	int num_no_ds=0;
	int num_no_cs=0;
	int num_no_http=0;
	
	list_listener.Construct(drstr_address, drstr_size);
	preview_listener.Construct(drstr_address, drstr_size, drstr_preview);
	
	RETURN_IF_ERROR(filter.RetrieveRules(&name, listener, &list_listener, &preview_listener, export_listener, action, validation));
	
	RETURN_IF_ERROR(CheckValidation(action, validation, validation_ok));
	
	if(!validation_ok)
		return OpStatus::ERR_NO_ACCESS;
	
	OP_ASSERT(listener);
	if(!listener)
		return OpStatus::ERR_NULL_POINTER;
	
	RETURN_IF_ERROR(listener->ListStart(url));
	
	int url_total=0;
	int url_shown=0;
	int num_200=0;
	int num_0=0;
	int num_404=0;
	int num_204=0;
	int num_301_307=0;
	CacheListStats list_stats;

	// Unique URLs are a list(Head) of URL_DataStorage, while normal URLs are a LinkObjectStore of URL_Rep,
	// so we need almost the same logic but written on a different way
	if(filter.GetURLsToShow()==SHOW_UNIQUE_URLS || filter.GetURLsToShow()==SHOW_ALL_URLS || filter.GetURLsToShow()==SHOW_ALL_URLS_NO_FILES || filter.GetURLsToShow()==SHOW_ALL_USED_URLS || filter.GetURLsToShow()==SHOW_ALL_REF_URLS || filter.GetURLsToShow()==SHOW_ALL_TYPE_URLS || filter.GetURLsToShow()==SHOW_ALL_URLS_HTTPS)
	{
		URL_DataStorage* url_ds = (URL_DataStorage*)ctx_man->LRU_list.First();

		while(url_ds)
		{
			URL_Rep *url_rep=url_ds->Rep();

			if (url_rep && IsUrlToShowInCacheList(url_rep, NULL, SHOW_UNIQUE_URLS)) // If it is a listable URL
			{
				url_total++;
				
				if(filter.ShowURL(url_rep)) // If it is included in the filters
				{
					UpdateCacheListStats(list_stats, url_rep);
					url_shown++;
					
					if(!url_rep->GetDataStorage())
						num_no_ds++;
					else
					{
						if(!url_rep->GetDataStorage()->GetCacheStorage())
							num_no_cs++;
						if(!url_rep->GetDataStorage()->GetHTTPProtocolDataNoAlloc())
							num_no_http++;
					}

					UINT32 response=url_rep->GetAttribute(URL::KHTTP_Response_Code);

					if(response==200)
						num_200++;
					else if(response==0)
						num_0++;
					else if(response==404)
						num_404++;
					else if(response==301 || response==307)
						num_301_307++;
					else if(response==204)
						num_204++;

					RETURN_IF_ERROR(listener->ListEntry(url_rep, url_shown, TRUE));
				}
			}
		
			url_ds=(URL_DataStorage *) url_ds->Suc();
		}
	}

	if(filter.GetURLsToShow()!=SHOW_UNIQUE_URLS)
	{
		URL_Rep* url_rep = ctx_man->url_store->GetFirstURL_Rep();
		
		while (url_rep)
		{
			if (IsUrlToShowInCacheList(url_rep, NULL, filter.GetURLsToShow())) // If it is a listable URL
			{
				url_total++;
				
				if(filter.ShowURL(url_rep)) // If it is included in the filters
				{
					UpdateCacheListStats(list_stats, url_rep);
					url_shown++;
					
					if(!url_rep->GetDataStorage())
						num_no_ds++;
					else
					{
						if(!url_rep->GetDataStorage()->GetCacheStorage())
							num_no_cs++;
						if(!url_rep->GetDataStorage()->GetHTTPProtocolDataNoAlloc())
							num_no_http++;
					}

					UINT32 response=url_rep->GetAttribute(URL::KHTTP_Response_Code);

					if(response==200)
						num_200++;
					else if(response==0)
						num_0++;
					else if(response==404)
						num_404++;
					else if(response==301 || response==307)
						num_301_307++;
					else if(response==204)
						num_204++;

					RETURN_IF_ERROR(listener->ListEntry(url_rep, url_shown, TRUE));
				}
			}
			
			url_rep = ctx_man->url_store->GetNextURL_Rep();
		}
	}
	
	RETURN_IF_ERROR(listener->ListEnd(list_stats, url_shown, url_total, num_no_ds, num_no_cs, num_no_http, num_200, num_0, num_404, num_301_307, num_204));
	
	return OpStatus::OK;
}

#endif // CACHE_ADVANCED_VIEW

BOOL CacheListWriter::IsUrlToShowInCacheList(URL_Rep *url_rep, BOOL *is_temp, URLs_To_Show urls_to_show)
{
	URLType type=(URLType) url_rep->GetAttribute(URL::KType);
	BOOL pw=!!url_rep->GetAttribute(URL::KHavePassword);
	BOOL auth=!!url_rep->GetAttribute(URL::KHaveAuthentication);
	BOOL unique=!!url_rep->GetAttribute(URL::KUnique);
	BOOL no_store=!!url_rep->GetAttribute(URL::KCachePolicy_NoStore);

	if(urls_to_show==SHOW_ALL_URLS)
		return TRUE;

	if(urls_to_show==SHOW_ALL_URLS_NO_FILES)
	{
		return type!=URL_FILE;
	}

	if(urls_to_show==SHOW_ALL_URLS_HTTPS)
	{
		return type==URL_HTTPS;
	}

	if(urls_to_show==SHOW_ALL_USED_URLS || urls_to_show==SHOW_ALL_REF_URLS || urls_to_show==SHOW_ALL_TYPE_URLS)
	{
		return TRUE; // Acceptable, without checking the real filter
	}

	OpFileLength registered_len=0;
	url_rep->GetAttribute(URL::KContentLoaded, &registered_len);

	URLStatus status=(URLStatus) url_rep->GetAttribute(URL::KLoadStatus);

	if(urls_to_show==SHOW_PENDING_URLS)
	{
		if((status==URL_LOADING || registered_len==0) &&  url_rep->GetDataStorage() && url_rep->GetDataStorage()->GetCacheStorage())
			return TRUE;

		return FALSE;
	}

	if (!pw && !auth && !no_store &&
		( (urls_to_show==SHOW_UNIQUE_URLS && unique) || 
		  (urls_to_show!=SHOW_UNIQUE_URLS && !unique && type == URL_HTTP) ))
		{
			const URL_HTTP_ProtocolData* http_url = (url_rep->GetDataStorage() ? url_rep->GetDataStorage()->http_data : NULL);

			if (http_url && registered_len)
			{
				URLCacheType type=(URLCacheType)url_rep->GetAttribute(URL::KCacheType);
				
				if(
	#ifndef RAMCACHE_ONLY
					(
						type == URL_CACHE_DISK ||
						(type == URL_CACHE_TEMP && !url_rep->GetAttribute(URL::KHTTPIsFormsRequest)) ||
						(type == URL_CACHE_MEMORY && !url_rep->GetAttribute(URL::KCachePolicy_NoStore))
					) &&
	#else
					(URLCacheType) url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_MEMORY &&
	#endif
					status != URL_UNLOADED)
				{
					if(is_temp)
					{
						if(type == URL_CACHE_TEMP)
							is_temp[0]=TRUE;
						else
							is_temp[0]=FALSE;
					}
						
					return TRUE;
				}
			}
		}
	
	return FALSE;
}

void CacheListWriter::UpdateCacheListStats(CacheListStats &list_stats, URL_Rep *url_rep)
{
	OP_ASSERT(url_rep);

	if(!url_rep)
		return;

	list_stats.num++;

	if(url_rep->GetUsedCount()==1)
		list_stats.used_1++;
	else if(url_rep->GetUsedCount()>=2)
		list_stats.used_2_and_more++;

	if(url_rep->GetRefCount()==1)
		list_stats.ref_1++;
	else if(url_rep->GetRefCount()>=2)
		list_stats.ref_2_and_more++;

	if(url_rep->GetAttribute(URL::KUnique))
		list_stats.unique++;

	URLStatus status=(URLStatus) url_rep->GetAttribute(URL::KLoadStatus);
	OpFileLength registered_len=0;
	url_rep->GetAttribute(URL::KContentLoaded, &registered_len);

	if((status==URL_LOADING || registered_len==0) && url_rep->GetDataStorage() && url_rep->GetDataStorage()->GetCacheStorage())
		list_stats.pending++;

	URLType type=(URLType) url_rep->GetAttribute(URL::KType);

	if(type==URL_HTTPS)
		list_stats.https++;

	URLCacheType ctype = (URLCacheType) url_rep->GetAttribute(URL::KCacheType);

	switch(ctype)
	{
		case URL_CACHE_NO:			list_stats.type_nothing++;	break;	
		case URL_CACHE_MEMORY:		list_stats.type_mem++;		break;
		case URL_CACHE_DISK:		list_stats.type_disk++;		break;
		case URL_CACHE_TEMP:		list_stats.type_temp++;		break;
		case URL_CACHE_USERFILE:	list_stats.type_user++;		break;
		case URL_CACHE_STREAM:		list_stats.type_stream++;	break;
		case URL_CACHE_MHTML:		list_stats.type_mhtml++;	break;
		default:
		{
			if (ctype == 0)
				list_stats.type_0++;
			else
				OP_ASSERT(!"Please update the above list!");
			break;
		}
	}

	if(url_rep->GetDataStorage() && url_rep->GetDataStorage()->GetCacheStorage() && url_rep->GetDataStorage()->GetCacheStorage()->IsPersistent())
	{
		list_stats.persistent++;

		if(ctype==URL_CACHE_MEMORY)
			list_stats.type_mem_persistent++; // These URL are potentially dangerous
	}
}

void CacheListWriter::WriteCacheListStats(OpString &stats, CacheListStats &list_stats, BOOL show_links)
{
	stats.Append("<table><tbody><thead><th>URL Usage Statistics</th><th></th></thead>");
	stats.AppendFormat(UNI_L("<tr><td>Total number of URLs</td><td>%d</td></tr>"), list_stats.num);
	if(show_links)
	{
		stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListUsed(-1); \">URLs in use</a></td><td>%d</td></tr>"), list_stats.used_1+list_stats.used_2_and_more);
		stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListUsed(1); \">URLs with used count of 1</a></td><td>%d</td></tr>"), list_stats.used_1);
		stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListUsed(2); \">URLs with used count of 2 or more</a></td><td>%d</td></tr>"), list_stats.used_2_and_more);
		stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListUsed(0); \">URLs NOT in use</a></td><td>%d</td></tr>"), list_stats.num-list_stats.used_1-list_stats.used_2_and_more);
		stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListRef(-1); \">URLs referenced</a></td><td>%d</td></tr>"), list_stats.ref_1+list_stats.ref_2_and_more);
		stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListRef(1); \">URLs with reference count of 1</a></td><td>%d</td></tr>"), list_stats.ref_1);
		stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListRef(2); \">URLs with reference count of 2 or more</a></td><td>%d</td></tr>"), list_stats.ref_2_and_more);
		stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListRef(0); \">URLs NOT referenced</a></td><td>%d</td></tr>"), list_stats.num-list_stats.ref_1-list_stats.ref_2_and_more);
		stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListUnique(); \">URLs Unique</a></td><td>%d</td></tr>"), list_stats.unique);
		stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListPending(); \">URLs Pending</a></td><td>%d</td></tr>"), list_stats.pending);
		stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListHTTPS(); \">URLs HTTPS</a></td><td>%d</td></tr>"), list_stats.https);

		if(list_stats.type_disk>0)
			stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListType(%d); \">Cache Type DISK</a></td><td>%d</td></tr>"), URL_CACHE_DISK, list_stats.type_disk);
		if(list_stats.type_temp>0)
			stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListType(%d); \">Cache Type TEMP</a></td><td>%d</td></tr>"), URL_CACHE_TEMP, list_stats.type_temp);
		if(list_stats.type_mem>0)
			stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListType(%d); \">Cache Type MEMORY</a></td><td>%d</td></tr>"), URL_CACHE_MEMORY, list_stats.type_mem);
		if(list_stats.type_mem_persistent>0)
			stats.AppendFormat(UNI_L("<tr><td>Cache Type MEMORY and PERSISTENT</td><td>%d</td></tr>"), list_stats.type_mem);
		if(list_stats.type_user>0)
			stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListType(%d); \">Cache Type USER FILE</a></td><td>%d</td></tr>"), URL_CACHE_USERFILE, list_stats.type_user);
		if(list_stats.type_stream>0)
			stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListType(%d); \">Cache Type STREAM</a></td><td>%d</td></tr>"), URL_CACHE_STREAM, list_stats.type_stream);
		if(list_stats.type_mhtml>0)
			stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListType(%d); \">Cache Type MHTML</a></td><td>%d</td></tr>"), URL_CACHE_MHTML, list_stats.type_mhtml);
		if(list_stats.type_nothing>0)
			stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListType(%d); \">NO Cache Storage</a></td><td>%d</td></tr>"), URL_CACHE_NO, list_stats.type_nothing);
		if(list_stats.type_0>0)
			stats.AppendFormat(UNI_L("<tr><td><a href=\"javascript: ListType(%d); \">Cache Type ZERO (...)</a></td><td>%d</td></tr>"), 0, list_stats.type_0);
		if(list_stats.persistent>0)
			stats.AppendFormat(UNI_L("<tr><td>PERSISTENT Cache Storage</td><td>%d</td></tr>"), list_stats.persistent);
	}
	else
	{
		stats.AppendFormat(UNI_L("<tr><td>URLs in use</td><td>%d</td></tr>"), list_stats.used_1+list_stats.used_2_and_more);
		stats.AppendFormat(UNI_L("<tr><td>URLs with used count of 1</td><td>%d</td></tr>"), list_stats.used_1);
		stats.AppendFormat(UNI_L("<tr><td>URLs with used count of 2 or more</td><td>%d</td></tr>"), list_stats.used_2_and_more);
		stats.AppendFormat(UNI_L("<tr><td>URLs NOT in use</td><td>%d</td></tr>"), list_stats.num-list_stats.used_1-list_stats.used_2_and_more);
		stats.AppendFormat(UNI_L("<tr><td>URLs referenced</td><td>%d</td></tr>"), list_stats.ref_1+list_stats.ref_2_and_more);
		stats.AppendFormat(UNI_L("<tr><td>URLs with reference count of 1</td><td>%d</td></tr>"), list_stats.ref_1);
		stats.AppendFormat(UNI_L("<tr><td>URLs with reference count of 2 or more</td><td>%d</td></tr>"), list_stats.ref_2_and_more);
		stats.AppendFormat(UNI_L("<tr><td>URLs NOT referenced</td><td>%d</td></tr>"), list_stats.num-list_stats.ref_1-list_stats.ref_2_and_more);
		stats.AppendFormat(UNI_L("<tr><td>URLs Unique</td><td>%d</td></tr>"), list_stats.unique);
		stats.AppendFormat(UNI_L("<tr><td>URLs Pending</td><td>%d</td></tr>"), list_stats.pending);
		stats.AppendFormat(UNI_L("<tr><td>URLs HTTPS</td><td>%d</td></tr>"), list_stats.https);

		if(list_stats.type_disk>0)
			stats.AppendFormat(UNI_L("<tr><td>Cache Type DISK</td><td>%d</td></tr>"), list_stats.type_disk);
		if(list_stats.type_temp>0)
			stats.AppendFormat(UNI_L("<tr><td>Cache Type TEMP</td><td>%d</td></tr>"), list_stats.type_temp);
		if(list_stats.type_mem>0)
			stats.AppendFormat(UNI_L("<tr><td>Cache Type MEMORY</td><td>%d</td></tr>"), list_stats.type_mem);
		if(list_stats.type_mem_persistent>0)
			stats.AppendFormat(UNI_L("<tr><td>Cache Type MEMORY and PERSISTENT</td><td>%d</td></tr>"), list_stats.type_mem);
		if(list_stats.type_user>0)
			stats.AppendFormat(UNI_L("<tr><td>Cache Type USER FILE</td><td>%d</td></tr>"), list_stats.type_user);
		if(list_stats.type_stream>0)
			stats.AppendFormat(UNI_L("<tr><td>Cache Type STREAM</td><td>%d</td></tr>"), list_stats.type_stream);
		if(list_stats.type_mhtml>0)
			stats.AppendFormat(UNI_L("<tr><td>Cache Type MHTML</td><td>%d</td></tr>"), list_stats.type_mhtml);
		if(list_stats.type_nothing>0)
			stats.AppendFormat(UNI_L("<tr><td>NO Cache storage</td><td>%d</td></tr>"), list_stats.type_nothing);
		if(list_stats.type_0>0)
			stats.AppendFormat(UNI_L("<tr><td>Cache Type ZERO (...)</td><td>%d</td></tr>"), list_stats.type_0);
		if(list_stats.persistent>0)
			stats.AppendFormat(UNI_L("<tr><td>PERSISTENT Cache Storage</td><td>%d</td></tr>"), list_stats.persistent);
	}

	stats.Append("</tbody></table><br /><br />");
}

OP_STATUS CacheListWriter::GenerateCacheList(URL& url)
{
#if CACHE_SMALL_FILES_SIZE>0 && defined(CACHE_STATS)
	RETURN_IF_LEAVE(ctx_man->AutoSaveCacheL());
#endif
	OpString drstr_address;
	OpString drstr_size;
	OpString drstr_filename;
	OpString drstr_preview;

	TRAP_AND_RETURN(err, g_languageManager->GetStringL(Str::SI_FILENAME_TEXT, drstr_filename));
	TRAP_AND_RETURN(err, g_languageManager->GetStringL(Str::SI_LOCATION_TEXT, drstr_address));
	TRAP_AND_RETURN(err, g_languageManager->GetStringL(Str::SI_SIZE_TEXT, drstr_size));
	TRAP_AND_RETURN(err, g_languageManager->GetStringL(Str::S_CACHE_PREVIEW, drstr_preview));
	
// Detail mode shows only the previews
#ifdef CACHE_ADVANCED_VIEW
	
	OpAutoStringHashTable<DomainSummary> domains;  // Domains present in the cache
	BOOL detail_mode=FALSE;
	
	RETURN_IF_ERROR(WriteAdvancedViewDetail(url, detail_mode, drstr_address, drstr_size, drstr_filename, drstr_preview));
	
	if(detail_mode)
		return OpStatus::OK;
#endif

	
#ifdef CACHE_STATS
	ctx_man->stats.container_files=0;	
#endif

	CacheListStats list_stats;

	urlsort_st sort_head;

	URL_Rep* url_rep = ctx_man->url_store->GetFirstURL_Rep();
    while (url_rep)
    {
		BOOL is_temp;
		
		// Statistics complete in the summary
		UpdateCacheListStats(list_stats, url_rep);

		if (IsUrlToShowInCacheList(url_rep, &is_temp, SHOW_NORMAL_URLS))
		{
			RETURN_IF_ERROR(sort_head.AddRecord(url_rep));

			#ifdef CACHE_STATS
				#if CACHE_CONTAINERS_ENTRIES>0
					if(url_rep->GetDataStorage()->GetCacheStorage()->GetContainerID())
						ctx_man->stats.container_files++;
				#endif
			#endif
			
			// Construct the list of domains present in the cache
			#ifdef CACHE_ADVANCED_VIEW
				OpStringC host=url_rep->GetAttribute(URL::KUniHostName);
				DomainSummary *temp_domain;
				
				if(!OpStatus::IsSuccess(domains.GetData(host.CStr(), &temp_domain)))
				{	
					OP_ASSERT(!temp_domain);
					
					temp_domain=OP_NEW(DomainSummary, ());
					
					// New domain
					if(temp_domain && 
					  (OpStatus::IsError(temp_domain->SetDomain(host.CStr(), url_rep)) || 
					   OpStatus::IsError(domains.Add(temp_domain->GetDomain(), temp_domain))))
					{
						OP_DELETE(temp_domain);
						temp_domain=NULL;
					}
				}
				
				if(temp_domain)
				{
					//if(is_temp)
					//	temp_domain->num_temp++;  // One more URL
					//else
						temp_domain->SetNumberOfURLs(temp_domain->GetNumberOfURLs()+1);  // One more URL
				}
			#endif
		}

		url_rep = ctx_man->url_store->GetNextURL_Rep();
	}

	// Update statistics for unique URLs (they are not shown in the list of domains)
	URL_DataStorage* url_ds = (URL_DataStorage*)ctx_man->LRU_list.First();

	while(url_ds)
	{
		URL_Rep *url_rep=url_ds->Rep();

		// Statistics complete in the summary
		UpdateCacheListStats(list_stats, url_rep);

		url_ds=(URL_DataStorage *) url_ds->Suc();
	}

	RETURN_IF_ERROR(WriteStatistics(list_stats, url));

#ifdef CACHE_ADVANCED_VIEW
	if(OpStatus::IsSuccess(WriteAdvancedViewSummary(url, &domains)))
	{
		return OpStatus::OK;
	}
#endif

	url.WriteDocumentData(URL::KNormal, UNI_L("<table>\n"));

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && !defined __OEM_OPERATOR_CACHE_MANAGEMENT
	OpString drstr3;
	TRAP_AND_RETURN(err, g_languageManager->GetStringL(Str::SI_IDSTR_NEVER_FLUSH_DATE, drstr3));
#endif //__OEM_EXTENDED_CACHE_MANAGEMENT
#if !defined __OEM_EXTENDED_CACHE_MANAGEMENT || defined __OEM_OPERATOR_CACHE_MANAGEMENT
		url.WriteDocumentDataUniSprintf(UNI_L("<tr>\r\n<th>%s</th>\r\n<th>%s</th>\r\n<th>%s</th>\r\n</tr>\r\n")
		, drstr_filename.CStr(), drstr_size.CStr(), drstr_address.CStr());
#else
	url.WriteDocumentDataUniSprintf(UNI_L("<tr>\r\n<th>%s</th>\r\n<th>%s</th>\r\n<th>%s</th>\r\n<th>%s</th>\r\n</tr>\r\n")
		, drstr_filename.CStr(), drstr_size.CStr(), drstr_address.CStr(), drstr3.CStr());
#endif //__OEM_EXTENDED_CACHE_MANAGEMENT

	OP_STATUS op_err = OpStatus::OK;
	//op_err = sort_head->WriteRecords(url, FALSE);
	//sort_head.reset();
	sort_head.WriteRecords(url, FALSE);
	url.WriteDocumentData(URL::KNormal, UNI_L("</table></body></html>"));

	return op_err;
}

#ifdef CACHE_GENERATE_ARRAY
OP_STATUS Context_Manager::GenerateCacheArray(uni_char** &filename, uni_char** &location, int* &size, int &num_items)
{
	BOOL cleanup = FALSE;
	URL_Rep* url_rep;
	UINT32 item = 0;

	// We can't trust url_store->URL_RepCount() because sometimes URL_Rep:s are removed from the wrong (url_store) store.
	// Let's calculate the array size to be on the safe side...

	num_items = 0;
	url_rep = url_store->GetFirstURL_Rep();
	while ( url_rep != NULL )
	{
		num_items++;
		url_rep = url_store->GetNextURL_Rep();
	}

	if ( num_items == 0 )
	{
		filename = NULL;
		location = NULL;
		size = NULL;
		return OpStatus::OK;
	}
	
	filename = OP_NEWA(uni_char*, num_items);
	location = OP_NEWA(uni_char*, num_items);
	size = OP_NEWA(int, num_items);

	if ( !filename || !location || !size )
		goto handle_oom;

	url_rep = url_store->GetFirstURL_Rep();

    while ( url_rep != NULL )
    {
		if ((URLType) url_rep->GetAttribute(URL::KType) == URL_HTTP && 
			!url_rep->GetAttribute(URL::KHavePassword) &&
			!url_rep->GetAttribute(URL::KHaveAuthentication))
		{
			const URL_HTTP_ProtocolData* http_url = (url_rep->storage ? url_rep->storage->http_data : NULL);

			OpFileLength registered_len = 0;
			url_rep->GetAttribute(URL::KContentLoaded, &registered_len);

			if (http_url && /*!url_rep->GetIsCrypt() && */
				registered_len && (URLCacheType) url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_DISK &&
				(URLStatus) url_rep->GetAttribute(URL::KLoadStatus) != URL_UNLOADED)
			{
#ifdef GENERATE_CACHE_ARRAY_WITH_FULL_PATH
				OpString val;
				TRAPD(res, url_rep->GetAttributeL(URL::KFilePathName_L, val));
				if (OpStatus::IsSuccess(res))
					filename[item] = uni_strdup(val);
				else
					filename[item] = 0;
#else
				filename[item] = uni_strdup(url_rep->GetAttribute(URL::KFileName));
#endif
				location[item] = uni_strdup(url_rep->UniName(PASSWORD_HIDDEN));
				
				OpFileLength registered_len = 0;
				url_rep->GetAttribute(URL::KContentLoaded, &registered_len);
				size[item] = (int) registered_len;
			
				item++;

				if ( !filename[item-1] || !location[item-1] )
					goto handle_oom;
			}
		}

		url_rep = url_store->GetNextURL_Rep();
	}

	num_items = item;

	if ( num_items != 0 )
		return OpStatus::OK;
	else
		cleanup = TRUE;

 handle_oom:
	for ( UINT32 i = 0; i < item; i++ )
	{
		op_free(filename[i]);
		op_free(location[i]);
	}

	OP_DELETEA(filename);
	filename = NULL;
	
	OP_DELETEA(location);
	location = NULL;

	OP_DELETEA(size);
	size = NULL;
	
	num_items = 0;
		
	return cleanup ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}
#endif // CACHE_GENERATE_ARRAY

#endif // NO_URL_OPERA
