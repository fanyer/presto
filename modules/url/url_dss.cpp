/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url2.h"
#include "modules/url/url_rel.h"

#include "modules/hardcore/mh/messages.h"

#include "modules/pi/OpSystemInfo.h"

#include "modules/viewers/viewers.h"

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/util/datefun.h"
#include "modules/url/uamanager/ua.h"

#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_man.h"
#include "modules/cache/url_cs.h"
#include "modules/url/url_pd.h"
#include "modules/formats/url_dfr.h"
#include "modules/url/loadhandler/url_lh.h"

#include "modules/formats/argsplit.h"

#include "modules/encodings/detector/charsetdetector.h"
#include "modules/encodings/utility/charsetnames.h"

#ifdef CACHE_FAST_INDEX
	#include "modules/cache/simple_stream.h"
#endif

#ifdef APPLICATION_CACHE_SUPPORT
#include "modules/applicationcache/application_cache_manager.h"
#include "modules/applicationcache/application_cache.h"

#endif // APPLICATION_CACHE_SUPPORT

#include "modules/url/url_dynattr_int.h"

/** Edits the strings content, reducing its size */
void GeneralValidateFilenameSyntax(OpString &suggested)
{
	if(suggested.IsEmpty())
		return;

	const uni_char *scan_pos = suggested.CStr();
	uni_char *target_pos = suggested.DataPtr();
	uni_char temp_char;
	BOOL was_dot = FALSE;

	while((temp_char = *(scan_pos++)) != '\0')
	{
		if(uni_isspace(temp_char))
		{
			// Collapse multiple whitespace into a single whitespace
			while(*scan_pos && uni_isspace(*scan_pos))
				scan_pos++;
			if(*scan_pos == '\0')
			{
				temp_char='\0';
			}
		}
		else if(temp_char == '{')
		{
			// censorship: remove the entire sequence of "{something}"
			temp_char = '\0';
			while(*scan_pos && *scan_pos != '}')
				scan_pos ++;

			if(*scan_pos != '\0')
				scan_pos ++;
			if(was_dot && *scan_pos == '.')
			{
				scan_pos ++;
			}
		}
		else if((temp_char >= 0x202A && temp_char <= 0x202E) || temp_char == 0x200E || temp_char == 0x200F)
			temp_char = 0;

		if(temp_char)
		{
			*(target_pos++) = temp_char;
			was_dot = (temp_char == '.');
		}
	}

	*target_pos = '\0';
}

OP_STATUS GeneralValidateSuggestedFileName(OpString &suggested, const OpStringC &suggested_extension)
{
	GeneralValidateFilenameSyntax(suggested);

	if(suggested.IsEmpty())
		return OpStatus::OK;

	int ext1 = suggested.FindLastOf('.');

	uni_char *ext = (ext1 != KNotFound ? suggested.DataPtr() + ext1 : NULL);

	const uni_char *copy_ext = suggested_extension.CStr();
	const uni_char *comp_ext = suggested_extension.CStr();

	OpStringC compare_extension(comp_ext);

	if((ext && ext[1] && compare_extension.CompareI(ext+1) != 0) || comp_ext != copy_ext || (!ext && copy_ext))
	{
		const uni_char *temp_dot = NULL;

		if(ext)
			ext[1] = '\0';
		else if(copy_ext)
			temp_dot = UNI_L(".");

		RETURN_IF_ERROR(suggested.Append(temp_dot));
		RETURN_IF_ERROR(suggested.Append(copy_ext));
	}

	return OpStatus::OK;
}

OP_STATUS GeneralValidateSuggestedFileExtension(OpString &suggested, const OpStringC &suggested_mime_type)
{
	GeneralValidateFilenameSyntax(suggested);

	if(suggested_mime_type.HasContent() && suggested_mime_type.CompareI("application/octet-stream") != 0)
	{
		Viewer* suggested_mime_type_viewer = g_viewers->FindViewerByMimeType(suggested_mime_type);
		if (suggested_mime_type_viewer!=NULL &&
		    (suggested.IsEmpty() ||  // if a known MIME-type, and either no suggested extension, OR
		     !suggested_mime_type_viewer->GetAllowAnyExtension() && // the MIME-type does neither allow any extension to be used, NOR
		     !suggested_mime_type_viewer->HasExtension(suggested))) // The suggested extension is NOT a known extension for the type
		{
			Viewer* suggested_extension_viewer = g_viewers->FindViewerByExtension(suggested);

			if (suggested.HasContent() && suggested_extension_viewer &&
				suggested_mime_type.CompareI(suggested_extension_viewer->AllowedContainer()) == 0)
			{
				// CORE-39801: Allow the extension if it is a container format that matches the mime type
				return OpStatus::OK;
			}

			BOOL collision = ((suggested_extension_viewer && suggested_extension_viewer!=suggested_mime_type_viewer) || // if the extension IS known to Opera for another type
							  g_viewers->GetDefaultContentTypeStringFromExt(suggested) != NULL); // OR if the extension IS known to Opera for another type (from default viewerlist)

			BOOL critical = collision; // we absolutely need to have a correct extension (security concerns)

			switch (suggested_mime_type_viewer->GetAction())
			{
			case VIEWER_PLUGIN:
			case VIEWER_OPERA:
			case VIEWER_ASK_USER:
				if (suggested.HasContent() &&
					(suggested_mime_type.CompareI("text/plain") == 0 || // avoid renaming .lng -> .txt for text/plain
					 (!suggested_extension_viewer && suggested_mime_type.CompareI("application/zip") == 0))) // CORE-39801: Allow any unregistered extension on zip content
				{
					return OpStatus::OK;
				}
				critical = FALSE;
				break;
			case VIEWER_REG_APPLICATION:
				critical = TRUE;
				break;
			default:
				critical = collision;
			}

			if (!critical && collision && suggested.HasContent())
			{
				if (suggested_extension_viewer &&
					suggested_extension_viewer->GetContentType() == suggested_mime_type_viewer->GetContentType() &&
					suggested_extension_viewer->GetContentType() != URL_UNKNOWN_CONTENT &&
					suggested_extension_viewer->HasExtension(suggested))
					return OpStatus::OK; // The content type and file extensions matches a viewer, so use origiginal extension.
			}

			// Then replace the extension with the first known extension
			const uni_char *vext = suggested_mime_type_viewer->GetExtensions();
			if (vext && *vext)			// only if the viewer have a extension set
			{
				RETURN_IF_ERROR(suggested.Set(suggested_mime_type_viewer->GetExtension(0)));

				if (suggested.Length() > 10) //Limit to 10 characters
				{
					*(suggested.CStr()+10) = 0;
				}
				else if (suggested.IsEmpty() && suggested_mime_type_viewer->GetContentTypeString()!=NULL && critical)
				{
					const uni_char *subtype = uni_strchr(suggested_mime_type_viewer->GetContentTypeString(), '/');

					if(subtype)
					{
						subtype ++;
						if(uni_strnicmp(subtype, UNI_L("x-"), 2) == 0)
							subtype += 2;

						const uni_char* ext = uni_strrchr(subtype, '.');
						if(ext && ext[1])
							subtype = ext+1;

						if(*subtype)
							RETURN_IF_ERROR(suggested.Set(subtype));
					}
				}
			}
		}
	}

	return OpStatus::OK;
}

void GeneralValidateSuggestedFileNameFromMIMEL(OpString &suggested, const OpStringC &mime_type)
{
	OpString ext;
	ANCHOR(OpString, ext);

	int ext1 = suggested.FindLastOf('.');

	if(ext1 != KNotFound)
		ext.SetL(&suggested[ext1+1]);

	LEAVE_IF_ERROR(GeneralValidateSuggestedFileExtension(ext, mime_type));
	LEAVE_IF_ERROR(GeneralValidateSuggestedFileName(suggested, ext));
}

OP_STATUS URL_Rep::ExtractExtension(OpString &source, OpString &target) const
{
	target.Empty();

	int ext1 = source.FindLastOf('.');

	if(ext1 != KNotFound)
		RETURN_IF_ERROR(target.Set(&source[ext1+1]));

	return OpStatus::OK;
}

OP_STATUS URL_Rep::GetSuggestedFileName(OpString &target) const
{
	OpString suggested;

	if(storage)
		RETURN_IF_ERROR(storage->GetAttribute(URL::KSuggestedFileName_L, suggested));

	if(suggested.IsEmpty())
	{
		RETURN_IF_ERROR(name.GetAttribute(URL::KSuggestedFileName_L, suggested));
	}

	OpString suggested_extension;
	if(suggested.HasContent())
	{
		RETURN_IF_ERROR(ExtractExtension(suggested, suggested_extension));
	}
	else
	{
		RETURN_IF_ERROR(suggested.Set(UNI_L("default")));
	}

	OpString suggested_mime_type;
	RETURN_IF_ERROR(suggested_mime_type.Set(g_viewers->GetContentTypeString((URLContentType) GetAttribute(URL::KContentType))));
	if(suggested_mime_type.IsEmpty() && (URLContentType) GetAttribute(URL::KContentType) != URL_UNDETERMINED_CONTENT)
		RETURN_IF_ERROR(GetAttribute(URL::KMIME_Type,suggested_mime_type));

	RETURN_IF_ERROR(GeneralValidateSuggestedFileExtension(suggested_extension, suggested_mime_type));

	if(suggested_extension.HasContent())
	{
		RETURN_IF_ERROR(GeneralValidateSuggestedFileName(suggested, suggested_extension));
	}

	suggested.Strip();
	if(suggested.HasContent()){
		int pos = 0;
		uni_char c;
		while((c = suggested[pos]) != 0)
		{
			if(uni_iscntrl(c))
			{
				suggested.Delete(pos,1);
				continue; // Dont' increment
			}
			if(uni_isspace(c))
			{
				int pos1 = pos+1;
				while((c= suggested[pos1]) != 0)
				{
					if(!uni_isspace(c))
						break;
					pos1++;
				}
				if(pos1 > pos+1)
					suggested.Delete(pos+1,pos1-(pos+1));
			}
			pos ++;
		}
	}

	target.TakeOver(suggested);
	return OpStatus::OK;
}

OP_STATUS URL_Rep::GetSuggestedFileNameExtension(OpString &target) const
{
	OpString suggestedname;

	target.Empty();

	RETURN_IF_ERROR(GetSuggestedFileName(suggestedname));
	return ExtractExtension(suggestedname, target);
}

URL_DataDescriptor* URL_Rep::GetDescriptor(MessageHandler *mh, URL::URL_Redirect follow_ref, BOOL get_raw_data, BOOL get_decoded_data, Window *window, URLContentType override_content_type, unsigned short override_charset_id, BOOL get_original_content, unsigned short parent_charset_id)
{
	if(follow_ref != URL::KNoRedirect)
	{
		URL url = GetAttribute(URL::KMovedToURL,URL::KFollowRedirect);

		if(url.IsValid())
			return url.GetDescriptor(mh, URL::KNoRedirect, get_raw_data, get_decoded_data, window, override_content_type, override_charset_id, get_original_content, parent_charset_id);
	}

	return storage ? storage->GetDescriptor(mh, get_raw_data, get_decoded_data, window, override_content_type, override_charset_id, get_original_content, parent_charset_id) : NULL;
}

void URL_Rep::Access(BOOL set_visited)
{
	urlManager->SetLRU_Item(this);
	if (set_visited
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
		|| (storage && storage->GetNeverFlush())
#endif
		)
		SetIsFollowed(TRUE);
}

void URL_Rep::SetLocalTimeVisited(time_t lta)
{
#ifdef NEED_URL_VISIBLE_LINKS
	if (urlManager && urlManager->GetVisLinks())
#endif // NEED_URL_VISIBLE_LINKS
		last_visited = lta;
}

unsigned long URL_Rep::PrepareForViewing(URL::URL_Redirect follow_ref, BOOL get_raw_data, BOOL get_decoded_data, BOOL force_to_file)
{
	if(follow_ref  != URL::KNoRedirect)
	{
		URL url = GetAttribute(URL::KMovedToURL,URL::KFollowRedirect);

		if(url.IsValid())
			return url.PrepareForViewing(URL::KNoRedirect, get_raw_data, get_decoded_data, force_to_file);
	}

	if(storage)
		return storage->PrepareForViewing(get_raw_data, get_decoded_data, force_to_file);
	return 0;
}

BOOL URL_Rep::IsFollowed()
{ // No LogFunc
	if (GetIsFollowed())
	{
		time_t time_now = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);
		if (time_now - last_visited > g_pcnet->GetFollowedLinksExpireTime())
		{
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
			if(!storage || !storage->GetNeverFlush())
#endif
				SetIsFollowed(FALSE);
		}
	}

	return GetIsFollowed();
}

void URL_DataStorage::UnsetCacheFinished()
{
	if(storage)
	{
		storage->UnsetFinished();
	}
}

#ifdef _MIME_SUPPORT_
URL_MIME_ProtocolData *URL_DataStorage::CheckMIMEProtocolData(OP_STATUS &op_err)
{
	op_err = OpStatus::OK;
	if(!protocol_data.mime_data)
	{
		protocol_data.mime_data = OP_NEW(URL_MIME_ProtocolData, ());
		if(!protocol_data.mime_data)
			op_err = OpStatus::ERR_NO_MEMORY;
	}
	return protocol_data.mime_data ;
}
#endif // _MIME_SUPPORT_

OpStringS8 *URL_Rep::GetHTTPEncoding()
{
	return (storage ? storage->GetHTTPEncoding() : NULL);
}

OpStringS8 *URL_DataStorage::GetHTTPEncoding()
{
	if(storage && storage->GetContentEncoding().HasContent())
		return &storage->GetContentEncoding();
	return (http_data ? &http_data->keepinfo.encoding : NULL);
}

void URL_DataStorage::MIME_ForceContentTypeL(OpStringC8	mtype)
{
	ParameterList type(KeywordIndex_HTTP_General_Parameters);
	ANCHOR(ParameterList, type);

	type.SetValueL(mtype.CStr(),PARAM_SEP_SEMICOLON|PARAM_STRIP_ARG_QUOTES);

	Parameters * OP_MEMORY_VAR element = type.First();
	if(!element)
		return;

	element->SetNameID(HTTP_General_Tag_Unknown);

	OpStringS8 stripped_name;
	ANCHOR(OpStringS8, stripped_name);
	BOOL illegal_name=FALSE;

	if(element->Name())
	{
		stripped_name.SetL(element->Name());
		int i;
		for(i=0;((unsigned char) stripped_name[i])>=32 && ((unsigned char) stripped_name[i])<=127;i++)
		;
		if (stripped_name[i]!=0)
			illegal_name=TRUE;
	}

	content_type_string.SetL(illegal_name? "application/octet-stream" : element->Name());

	URLContentType tmp_url_ct = (illegal_name? URL_UNKNOWN_CONTENT : URL_UNDETERMINED_CONTENT);

	if(!illegal_name && content_type_string.HasContent())
	{
		OpString16 name;
		ANCHOR(OpString16, name);
		OP_STATUS op_err = OpStatus::OK;

		op_err = url->GetAttribute(URL::KUniPath, 0, name);
		RAISE_IF_ERROR(op_err); // Not critical if this fails

		if( content_type_string.Length() )
		{
			LEAVE_IF_ERROR(FindContentType(tmp_url_ct, content_type_string.CStr(), 0, name.CStr(), TRUE));
		}
	}

	SetAttributeL(URL::KContentType, tmp_url_ct);

	if(content_type_string.HasContent() && strni_eq(content_type_string.CStr(), "TEXT/", 5))
		element = type.GetParameterByID(HTTP_General_Tag_Charset, PARAMETER_ASSIGNED_ONLY);
	else
		element = NULL;

	if(element)
	{
		SetAttributeL(URL::KMIME_CharSet, element->Value());
	}
}

uint32 URL_DataStorage::GetAttribute(URL::URL_Uint32Attribute attr) const
{
	if(attr > URL::KUInt_Start_HTTPdata &&  attr < URL::KUInt_End_HTTPdata)
	{
		if(!http_data)
		{
			switch(attr)
			{
			case URL::KHTTP_Refresh_Interval:
				return (uint32) (-1);
			case URL::KStillSameUserAgent:
				return TRUE;
			case URL::KUsedUserAgentId:
				return (uint32) g_uaManager->GetUABaseId();
#ifdef UNUSED_CODE
			case URL::KUsedUserAgentVersion:
				return g_uaManager->GetUAVersionStringId();
#endif
			}
			return 0;
		}

		return http_data->GetAttribute(attr);
	}
#ifdef _MIME_SUPPORT_
#ifdef NEED_URL_MIME_DECODE_LISTENERS
	if(attr > URL::KUInt_Start_MIMEdata &&  attr < URL::KUInt_End_MIMEdata)
	{
		return  (protocol_data.prot_data ? protocol_data.prot_data->GetAttribute(attr) : FALSE);
	}
#endif
#endif
	if(attr>URL::KStartUintDynamicAttributeList)
	{
		return GetDynAttribute(attr);
	}

	switch(attr)
	{
	case URL::KCacheInUse:
		return storage && !storage->Empty();
	case URL::KCachePersistent:
		return storage && storage->IsPersistent();
	case URL::KCachePolicy_AlwaysVerify:
		return info.cache_always_verify;
	case URL::KCachePolicy_NoStore:
		return info.cache_no_store;
	case URL::KCacheType:
		return (storage ? storage->GetCacheType() : URL_CACHE_NO);
	case URL::KDataPresent:
		return storage && storage->DataPresent();
	case URL::KDisableProcessCookies:
		return info.disable_cookie;
	case URL::KFermented:
		{
			time_t current_time = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);

#if defined _LOCALHOST_SUPPORT_ || !defined RAMCACHE_ONLY
			if ((URLType) url->GetAttribute(URL::KType) == URL_FILE)
			{
				if (local_time_loaded + 10 < current_time) // Don't bother to check for file changes more then once every 10 seconds
				{
					OpFile local_file;
					time_t last_modified;

					local_file.Construct(url->GetAttribute(URL::KFileName));
					if (OpStatus::IsError(local_file.GetLastModified(last_modified))) return true; // Unable to read time stamp
					if (last_modified >= local_time_loaded) return true; // File is fermented

					// local_time_loaded for files means last time we had an updated copy (according to its time stamp).
					// We currently have an updated copy:
					url->SetAttribute(URL::KVLocalTimeLoaded, &current_time);
				}
				return false;
			}
#endif
			return (current_time > (local_time_loaded  + (1 * 60))); // More than 1 minute old
		}
	case URL::KHeaderLoaded:
		return info.header_loaded;
	case URL::KIsResuming:
		return info.is_resuming;
	case URL::KIsThirdParty:
		return info.is_third_party;
	case URL::KIsThirdPartyReach:
		return info.is_third_party_reach;
	case URL::KUseGenericAuthenticationHandling:
		return info.use_generic_authentication_handling;
		break;
	case URL::KReloadSameTarget:
		return info.use_same_target;
	case URL::KResumeSupported:
		return GET_RANGED_ENUM(URL_Resumable_Status, info.resume_supported);
	case URL::KSecurityStatus:
		return info.security_status;
	case URL::KUntrustedContent:
		return info.untrusted_content;
	case URL::KForceCacheKeepOpenFile:
		return info.forceCacheKeepOpenFile;
	case URL::KUseProxy:
		return info.use_proxy;
	case URL::K_INTERNAL_ContentType:
		return (storage ? storage->GetContentType() : URL_UNDETERMINED_CONTENT);
	case URL::KIsImage:
	case URL::KContentType:
	case URL::KLoadStatus:
	case URL::KReloading:
		return url->GetAttribute(attr);
	case URL::KTimeoutPollIdle:
		return timeout_poll_idle;
	case URL::KTimeoutMaxTCPConnectionEstablished:
		return timeout_max_tcp_connection_established;
	case URL::KMIME_CharSetId:
		return mime_charset;
	case URL::KAutodetectCharSetID:
		return info.autodetect_charset;
#ifdef TRUST_RATING
	case URL::KTrustRating:
		return info.trust_rating;
#endif
	case URL::KLimitNetType:
		return info.use_nettype;
	case URL::KLoadedFromNetType:
		return info.loaded_from_nettype;
#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
	case URL::KUsedForFileDownload:
		return info.is_file_download;
#endif
#ifdef WEB_TURBO_MODE
	case URL::KTurboBypassed:
		return GetLoadDirect();
	case URL::KTurboCompressed:
		return (!GetLoadDirect() && turbo_transferred_bytes < turbo_orig_transferred_bytes);
#endif // WEB_TURBO_MODE
#ifdef USE_SPDY
	case URL::KLoadedWithSpdy:
		return loaded_with_spdy;
#endif // USE_SPDY
	case URL::KIsUserInitiated:
		return info.user_initiated;
	case URL::KMultimedia:
		return info.multimedia;
	case URL::KMIME_Type_Was_NULL:
		return info.content_type_was_null;
	case URL::KStorageId:
		return storage ? storage->GetStorageId() : 0;
	}

	return 0;
}

void URL_DataStorage::SetAttributeL(URL::URL_Uint32Attribute attr, uint32 value)
{
	LEAVE_IF_ERROR(SetAttribute(attr,value));
}

OP_STATUS URL_DataStorage::SetAttribute(URL::URL_Uint32Attribute attr, uint32 value)
{
	if(attr > URL::KUInt_Start_HTTPdata &&  attr < URL::KUInt_End_HTTPdata)
	{
		if( (attr == URL::KSendAcceptEncoding || attr == URL::KSpecialRedirectRestriction || attr == URL::KHTTP_Priority) &&
			((URLType) url->GetAttribute(URL::KType) != URL_HTTP && (URLType) url->GetAttribute(URL::KType) != URL_HTTPS))
		{
			return OpStatus::OK; // HTTP(S) only
		}
		RETURN_IF_ERROR(CheckHTTPProtocolData());

		if (attr == URL::KHTTP_Method && (value == HTTP_METHOD_POST || value == HTTP_METHOD_PUT ) && !url->GetAttribute(URL::KUnique))
			urlManager->MakeUnique(url);

		http_data->SetAttribute(attr, value);
		return OpStatus::OK;
	}
#if 0 //def _MIME_SUPPORT_ // Disabled
	if(attr > URL::KUInt_Start_MIMEdata &&  attr < URL::KUInt_End_MIMEdata)
	{
		RETURN _IF_ERROR(CheckMIMEProtocolData())
		protocol_data.prot_data->SetAttribute(attr, value);
		return OpStatus::OK;
	}
#endif
	if(attr>URL::KStartUintDynamicAttributeList)
	{
		return SetDynAttribute(attr, value);
	}

	BOOL flag = (value ? TRUE : FALSE);

	switch(attr)
	{
	case URL::KCachePolicy_AlwaysVerify:
		info.cache_always_verify = flag;
		break;
	case URL::KCachePolicy_NoStore:
		info.cache_no_store = flag;
		break;
	case URL::KDisableProcessCookies:
		info.disable_cookie = flag;
		break;
	case URL::KForceCacheKeepOpenFile:
		info.forceCacheKeepOpenFile= TRUE;
		if(storage)
			storage->ForceKeepOpen();
		break;
	case URL::KHeaderLoaded:
		info.header_loaded = flag;
		break;
	case URL::K_INTERNAL_ContentType:
		if(storage)
			storage->SetContentType((URLContentType) value);
		break;
	case URL::K_INTERNAL_Override_Reload_Protection:
		info.overide_load_protection = flag;
		break;
	case URL::KIsResuming:
		info.is_resuming = flag;
		break;
	case URL::KCacheType:
		if(storage)
			storage->SetCacheType((URLCacheType) value);
		break;
	case URL::KCacheTypeStreamCache:
		RETURN_IF_ERROR(CreateStreamCache());
		OP_ASSERT(url->GetRefCount() > 0);
		break;
	case URL::KIsThirdParty:
		info.is_third_party = flag;
		break;
	case URL::KIsThirdPartyReach:
		info.is_third_party_reach = flag;
		break;
	case URL::KUseGenericAuthenticationHandling:
		info.use_generic_authentication_handling = flag;
		break;
	case URL::KReloadSameTarget:
		info.use_same_target = flag;
		break;
	case URL::KResumeSupported:
		CHECK_RANGED_ENUM(URL_Resumable_Status, value);
		info.resume_supported = FROM_RANGED_ENUM(URL_Resumable_Status, value);
		break;
	case URL::KSecurityStatus:
		info.security_status = value;
		break;
	case URL::KResetCache:
		((URL_DataStorage *) this)->ResetCache();
		break;
	case URL::KUntrustedContent:
		info.untrusted_content = flag;
		break;
	case URL::KLoadStatus:
	case URL::KContentType:
	case URL::KReloading:
		return url->SetAttribute(attr, value);
	case URL::KTimeoutPollIdle:
		timeout_poll_idle = value;
		if(timeout_poll_idle)
		{
			if ((!GetAttribute(URL::KTimeoutStartsAtRequestSend) && !GetAttribute(URL::KTimeoutStartsAtConnectionStart)) || (URLStatus) url->GetAttribute(URL::KLoadStatus, TRUE) == URL_LOADING)
			{
				StartIdlePoll();
			}
		}
	case URL::KTimeoutMaxTCPConnectionEstablished:
		timeout_max_tcp_connection_established = value;
		break;
#ifdef _MIME_SUPPORT_
	case URL::KBigAttachmentIcons:
		RETURN_IF_ERROR(CreateCache());
		storage->SetBigAttachmentIcons(flag);
		break;
	case URL::KPreferPlaintext:
		RETURN_IF_ERROR(CreateCache());
		storage->SetPreferPlaintext(flag);
		break;
	case URL::KIgnoreWarnings:
		RETURN_IF_ERROR(CreateCache());
		storage->SetIgnoreWarnings(flag);
		break;
#endif
#ifdef URL_ENABLE_ACTIVE_COMPRESS_CACHE
	case URL::KCompressCacheContent:
		info.compress_cache = flag;
		break;
#endif
	case URL::KMIME_CharSetId:
		if(storage)
			storage->SetCharsetID((unsigned short) value);
		g_charsetManager->IncrementCharsetIDReference(value);
		g_charsetManager->DecrementCharsetIDReference(mime_charset);
		mime_charset = value;
		break;
	case URL::KAutodetectCharSetID:
		info.autodetect_charset= value;
		break;
#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
	case URL::KPauseDownload:
		if(loading)
		{
			if(flag)
				loading->PauseLoading();
			else
				return loading->ContinueLoading();
		}
		break;
#endif
#ifdef TRUST_RATING
	case URL::KTrustRating:
		info.trust_rating = flag;
		break;
#endif
	case URL::KMultimedia:
		info.multimedia = flag;
		break;
	case URL::KMIME_Type_Was_NULL:
		info.content_type_was_null = flag;
		break;

	case URL::KLimitNetType:
		info.use_nettype = value;
		break;
	case URL::KLoadedFromNetType:
		info.loaded_from_nettype = value;
		break;
#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
	case URL::KUsedForFileDownload:
		info.is_file_download = flag;
		if( loading )
			loading->SetIsFileDownload(flag);
		break;
#endif

	case URL::KIsUserInitiated:
		info.user_initiated = flag;
		break;

#ifdef WEB_TURBO_MODE
	case URL::KTurboBypassed:
		SetLoadDirect(flag);
		break;
#endif // WEB_TURBO_MODE
#ifdef USE_SPDY
	case URL::KLoadedWithSpdy:
		loaded_with_spdy = flag;
		break;
#endif // USE_SPDY
	}
	return OpStatus::OK;
}

#ifndef RETURN_CLASS_COPY
#if defined BREW || defined ADS12
#define RETURN_CLASS_COPY(class_name, operation) \
	do{\
		class_name ret_copy( operation );\
		return ret_copy;\
	}while(0)
#define RETURN_DEFAULT_COPY(class_name) \
	do{\
		class_name ret_copy;\
		return ret_copy;\
	}while(0)
#else
#define RETURN_CLASS_COPY(class_name, operation) return operation
#define RETURN_DEFAULT_COPY(class_name) return class_name ();
#endif
#endif

const OpStringC8 URL_DataStorage::GetAttribute(URL::URL_StringAttribute attr) const
{
	if(attr > URL::KStr_Start_HTTP &&  attr < URL::KStr_End_HTTP)
	{
		if(attr == URL::KHTTPEncoding &&
			storage && storage->GetContentEncoding().HasContent())
				RETURN_CLASS_COPY(OpStringC8, storage->GetContentEncoding());
		if(http_data)
			RETURN_CLASS_COPY(OpStringC8, http_data->GetAttribute(attr));
	}
	else if(attr > URL::KStr_Start_Protocols &&  attr < URL::KStr_End_Protocols)
	{
		if(protocol_data.prot_data)
			RETURN_CLASS_COPY(OpStringC8, protocol_data.prot_data->GetAttribute(attr));
	}
	else
	{

		switch(attr)
		{
		case URL::KAutodetectCharSet:
			{
				const char *retString = CharsetDetector::AutoDetectStringFromId((CharsetDetector::AutoDetectMode)info.autodetect_charset);
				RETURN_CLASS_COPY(OpStringC8, retString);
			}
		case URL::KMIME_CharSet:
			{
				unsigned short char_set_id = (storage ? storage->GetCharsetID() : 0);

				RETURN_CLASS_COPY(OpStringC8, (g_charsetManager ? g_charsetManager->GetCharsetFromID(char_set_id ? char_set_id : mime_charset) : NULL));
			}
			break;
		case URL::KMIME_ForceContentType:
		case URL::KMIME_Type:
			if(storage)
			{
				OpStringC8 cand(storage->GetMIME_Type());

				if(cand.HasContent())
					return cand;
			}
			// Fall through to original content type
		case URL::KOriginalMIME_Type:
			return content_type_string;
		case URL::KServerMIME_Type:
			return server_content_type_string;
		}
	}

	RETURN_DEFAULT_COPY(OpStringC8);
}

#include "modules/olddebug/tstdump.h"

void URL_DataStorage::SetAttributeL(URL::URL_StringAttribute attr, const OpStringC8 &string)
{
	LEAVE_IF_ERROR(SetAttribute(attr,string));
}

OP_STATUS URL_DataStorage::SetAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string)
{
	OpStringS8 *target = NULL;

	if(attr > URL::KStr_Start_HTTP &&  attr < URL::KStr_End_HTTP)
	{
		RETURN_IF_ERROR(CheckHTTPProtocolData());
		return http_data->SetAttribute(attr,string);
	}
	if(attr > URL::KStr_Start_FTP &&  attr < URL::KStr_End_FTP)
	{
		RETURN_IF_ERROR(CheckFTPProtocolData());
		return protocol_data.prot_data->SetAttribute(attr,string);
	}
#if 0
	if(attr > URL::KStr_Start_Mail &&  attr < URL::KStr_End_Mail)
	{
		RETURN_IF_ERROR(CheckMailtoProtocolData());
		return protocol_data.prot_data->SetAttributeL(attr,string);
	}
#endif
	if(attr>URL::KStartStrDynamicAttributeList)
	{
		return SetDynAttribute(attr, string);
	}


	switch(attr)
	{
	case URL::KHTTPAgeHeader:
		{
			if(string.HasContent())
			{
				time_t age = (time_t) op_strtoul(string.CStr(), NULL, 10);
				OpStringC8 date(GetAttribute(URL::KHTTP_Date));

				time_t load_date = (date.HasContent() ? GetDate(date.CStr()) : 0);
				time_t current_time = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);

				time_t t_age = load_date <= current_time ? current_time-load_date : 0;

				return SetAttribute(URL::KHTTP_Age, (uint32) (MAX(t_age, age) + (local_time_loaded ? current_time - local_time_loaded: 0)));

				/*
				PrintfTofile("headers.txt", "%s: Setting Age %s (%lu : %lu %lu : %lu %lu : %lu)\n",url->GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI).CStr(), string .CStr(),
					GetAttribute(URL::KHTTP_Age), t_age, age, local_time_loaded , current_time, load_date);
					*/
			}
			break;
		}
	case URL::KHTTPCacheControl:
	case URL::KHTTPCacheControl_HTTP:
		{
			if(!http_data || string.IsEmpty())
				return OpStatus::OK;

#ifdef APPLICATION_CACHE_SUPPORT
			/* HTTP caching rules, such as Cache-Control: no-store, are ignored for the purposes of the application cache download process.*/			
			ApplicationCache *application_cache = g_application_cache_manager->GetCacheFromContextId(url->GetID());			
			const uni_char* url_name = url->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, URL::KNoRedirect).CStr();
			
			if (application_cache && application_cache->IsCached(url_name))
			{
				break;
			}
			
#endif // APPLICATION_CACHE_SUPPORT
			
			ParameterList cache_parameters(KeywordIndex_HTTP_General_Parameters);

			OP_STATUS op_err = cache_parameters.SetValue(string.CStr(), PARAM_SEP_COMMA | PARAM_STRIP_ARG_QUOTES);
			if(op_err == OpStatus::ERR_PARSING_FAILED)
				return OpStatus::OK;
			else
				RETURN_IF_ERROR(op_err);

			if(cache_parameters.GetParameterByID(HTTP_General_Tag_No_Cache, PARAMETER_NO_ASSIGNED) != NULL /*|| // "Must-revalidate" is not relevant for clients, except for warnings
				cache_parameters.GetParameter("must-revalidate", PARAMETER_NO_ASSIGNED) != NULL*/)
			{
				RETURN_IF_ERROR(SetAttribute(URL::KCachePolicy_AlwaysVerify, TRUE));
				//if(cache_parameters.GetParameterByID(HTTP_General_Tag_Private, PARAMETER_NO_ASSIGNED) != NULL)
				//	urlManager->MakeUnique(url);
			}

			if(attr == URL::KHTTPCacheControl_HTTP &&
				cache_parameters.GetParameterByID(HTTP_General_Tag_Must_Revalidate, PARAMETER_NO_ASSIGNED) != NULL &&
				(URLType) url->GetAttribute(URL::KType) == URL_HTTPS &&
				url->GetAttribute(URL::KHTTP_Method) == HTTP_METHOD_GET)
			{
				RETURN_IF_ERROR(SetAttribute(URL::KCachePolicy_MustRevalidate, TRUE));
			}

			// Useless in HTML, but we will start using this for HTTP as well
			if(cache_parameters.GetParameterByID(HTTP_General_Tag_No_Store, PARAMETER_NO_ASSIGNED))
				RETURN_IF_ERROR(SetAttribute(URL::KCachePolicy_NoStore, TRUE));

			Parameters *item = cache_parameters.GetParameterByID(HTTP_General_Tag_Max_Age, PARAMETER_ASSIGNED_ONLY);
			if(item)
			{
				time_t maxage= item->GetUnsignedValue();
				if(maxage == 0)
					RETURN_IF_ERROR(SetAttribute(URL::KCachePolicy_AlwaysVerify, TRUE));
				else
				{
					info.max_age_set = TRUE;
					time_t expired =0;
					GetAttribute(URL::KVHTTP_ExpirationDate, &expired);

					if(expired == 0)
					{
						expired = local_time_loaded + maxage - GetAttribute(URL::KHTTP_Age);
						RETURN_IF_ERROR(SetAttribute(URL::KVHTTP_ExpirationDate, &expired));
						/*
						PrintfTofile("headers.txt", "%s: Setting expiration %lu (%lu : %lu : %lu : %d : %lu)\n",url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr(),
							(time_t) GetAttribute(URL::KHTTP_ExpirationDate), GetAttribute(URL::KHTTP_Age),
							(time_t) (g_op_time_info->GetTimeUTC()/1000.0), g_timecache->CurrentTime() ,
							g_op_time_info->GetTimezone() , g_timecache->CurrentTime() + g_op_time_info->GetTimezone() );
							*/
					}
				}
			}

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && !defined __OEM_OPERATOR_CACHE_MANAGEMENT
			item = cache_parameters.GetParameterByID(HTTP_General_Tag_Never_Flush, PARAMETER_NO_ASSIGNED);
			if(item && item->Value())
			{
				if(url->GetAttribute(URL::KServerName, NULL) &&
					((ServerName *) url->GetAttribute(URL::KServerName, NULL))->TestAndSetNeverFlushTrust() == NeverFlush_Trusted)
					RETURN_IF_ERROR(SetAttribute(URL::KNeverFlush, TRUE));
			}
#endif
		}
		break;
	case URL::KHTTPPragma:
		{
			if(!http_data || string.IsEmpty())
				return OpStatus::OK;

			ParameterList cache_parameters(KeywordIndex_HTTP_General_Parameters);

			OP_STATUS op_err = cache_parameters.SetValue(string.CStr(), PARAM_SEP_COMMA | PARAM_STRIP_ARG_QUOTES);
			if(op_err == OpStatus::ERR_PARSING_FAILED)
				return OpStatus::OK;
			else
				RETURN_IF_ERROR(op_err);

			if(cache_parameters.GetParameterByID(HTTP_General_Tag_No_Cache, PARAMETER_NO_ASSIGNED) != NULL)
				RETURN_IF_ERROR(SetAttribute(URL::KCachePolicy_AlwaysVerify, TRUE));
		}
		break;
	case URL::KHTTPExpires:
		{
			if (!http_data || string.IsEmpty())
				return OpStatus::OK;

			time_t expiration_time = 0;

			http_data->GetAttribute(URL::KVHTTP_ExpirationDate, &expiration_time);

			// Don't set if already there. This is to avoid writing over expiry set by
			// max-age.
			if(expiration_time != 0)
				return OpStatus::OK;

			/* If Date is set for the response, set expiry relative to this */
			OpStringC8 http_date = GetAttribute(URL::KHTTP_Date);
			time_t current_time = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);
			time_t expiry_time = (time_t) GetDate(string.CStr());

			if (http_date.HasContent())
			{
				time_t server_time = (time_t) GetDate(http_date.CStr());
				if (expiry_time > server_time)
				{
					expiration_time = current_time + (expiry_time - server_time);
				}
				else
					expiration_time = current_time;
			}
			else
				expiration_time = MAX(expiry_time, current_time);

			RETURN_IF_ERROR(http_data->SetAttribute(URL::KVHTTP_ExpirationDate, &expiration_time));

			break;
		}
	case URL::KMIME_ForceContentType:
		TRAP_AND_RETURN(op_err, MIME_ForceContentTypeL(string));
		break;
	case URL::KMIME_Type:
		target = &content_type_string;
		break;
	case URL::KServerMIME_Type:
		target = &server_content_type_string;
		break;
	case URL::KAutodetectCharSet:
		info.autodetect_charset = (string.HasContent() ? CharsetDetector::AutoDetectIdFromString(string.CStr()) : 0 );
		break;
	case URL::KMIME_CharSet:
		if(g_charsetManager)
		{
			unsigned short old_id =	mime_charset;
			unsigned short new_id = (string.HasContent() ? g_charsetManager->GetCharsetIDL(string.CStr()) : 0);
			g_charsetManager->IncrementCharsetIDReference(new_id);
			g_charsetManager->DecrementCharsetIDReference(old_id);
			if(storage)
				storage->SetCharsetID(new_id);
			mime_charset = new_id;
		}
		break;
	}

	if(target)
		return target->Set(string);

	return OpStatus::OK;
}

void URL_DataStorage::GetAttributeL(URL::URL_StringAttribute attr, OpString8 &val) const
{
	LEAVE_IF_ERROR(GetAttribute(attr, val));
}

OP_STATUS URL_DataStorage::GetAttribute(URL::URL_StringAttribute attr, OpString8 &val) const
{
	if(attr > URL::KStr_Start_HTTP &&  attr < URL::KStr_End_HTTP)
	{
		if(attr == URL::KHTTPEncoding &&
			storage && storage->GetContentEncoding().HasContent())
		{
			val.SetL(storage->GetContentEncoding());
			return OpStatus::OK;;
		}

		if(http_data)
			return http_data->GetAttribute(attr,val);

		val.Empty();
		return OpStatus::OK;
	}
	if(attr>URL::KStartStrDynamicAttributeList)
	{
		return GetDynAttribute(attr,val);
	}
	return val.Set(GetAttribute(attr));
}

const OpStringC URL_DataStorage::GetAttribute(URL::URL_UniStringAttribute attr) const
{
	if(attr > URL::KUStr_Start_HTTP &&  attr < URL::KUStr_End_HTTP)
	{
		if(http_data)
		{
			OpStringC ret(http_data->GetAttribute(attr));
			return ret; // ADS 1.2 can't cope with tail-recursive return ...
		}
	}
	else switch(attr)
	{
#if defined _LOCALHOST_SUPPORT_ || !defined RAMCACHE_ONLY
	case URL::KFileName:
		// Ugly override
		((URL_DataStorage *) this)->DumpSourceToDisk();
		if(storage)
		{
			OpFileFolder folder1 = OPFILE_ABSOLUTE_FOLDER;
			RETURN_CLASS_COPY(OpStringC, storage->FileName(folder1));
		}
		break;
	case URL::KFilePathName:
		OP_ASSERT(0); // NO LONGER USED, use URL::KFilePathName_L instead !!!
		break;
#endif // _LOCALHOST_SUPPORT_ || !RAMCACHE_ONLY
	case URL::KInternalErrorString:
		return internal_error_message;
	case URL::KSecurityText:
		return security_text;
#ifdef _MIME_SUPPORT_
	case URL::KBodyCommentString:
		if(protocol_data.prot_data)
			protocol_data.prot_data->GetAttribute(attr);
#endif // _MIME_SUPPORT_
	}

	RETURN_DEFAULT_COPY(OpStringC);
}

void URL_DataStorage::GetAttributeL(URL::URL_UniStringAttribute attr, OpString &val) const
{
	LEAVE_IF_ERROR(GetAttribute(attr,val));
}

OP_STATUS URL_DataStorage::GetAttribute(URL::URL_UniStringAttribute attr, OpString &val) const
{
	if(attr > URL::KUStr_Start_HTTP &&  attr < URL::KUStr_End_HTTP)
	{
		if(http_data)
			return http_data->GetAttribute(attr,val);
		else
			val.Empty();
		return OpStatus::OK;
	}
	else switch(attr)
	{
#if defined _LOCALHOST_SUPPORT_ || !defined RAMCACHE_ONLY
	case URL::KFilePathName:
	case URL::KFilePathName_L:
		// Ugly override
		((URL_DataStorage *) this)->DumpSourceToDisk();
		if(storage)
			return storage->FilePathName(val);
		else
			val.Empty();
		break;
#endif
	case URL::KSuggestedFileName_L:
		if(http_data)
			return http_data->GetAttribute(attr, val);
		else
			val.Empty();
		break;
	case URL::KInternalErrorString:
	case URL::KSecurityText:
#ifdef _MIME_SUPPORT_
	case URL::KBodyCommentString:
#endif // _MIME_SUPPORT_
	default:
		return val.Set(GetAttribute(attr));
	}

	return OpStatus::OK;
}

void URL_DataStorage::SetAttributeL(URL::URL_UniStringAttribute attr, const OpStringC &string)
{
	LEAVE_IF_ERROR(SetAttribute(attr,string));
}

OP_STATUS URL_DataStorage::SetAttribute(URL::URL_UniStringAttribute attr, const OpStringC &string)
{
	OpStringS *target = NULL;

	switch(attr)
	{
	case URL::KInternalErrorString:
		target = &internal_error_message;
		break;
#if defined _LOCALHOST_SUPPORT_ || !defined RAMCACHE_ONLY
	case URL::KFileName:
	case URL::KFilePathName:
#endif
	case URL::KSuggestedFileName_L:
		RETURN_IF_ERROR(CheckHTTPProtocolData());
		return http_data->SetAttribute(attr,string);
	case URL::KSecurityText:
		target = &security_text;
		break;
#ifdef _MIME_SUPPORT_
	case URL::KBodyCommentString:
		RETURN_IF_ERROR(CheckMailtoProtocolData());
		return protocol_data.mime_data->SetAttribute(attr,string);
#endif // _MIME_SUPPORT_
	}

	if(target)
		return target->Set(string);

	return OpStatus::OK;
}



const void *URL_DataStorage::GetAttribute(URL::URL_VoidPAttribute  attr, const void *param) const
{
	if(attr > URL::KVoip_Start_HTTP && attr < URL::KVoip_End_HTTP)
		return (http_data ? http_data->GetAttribute(attr, param) : NULL);
#ifdef HAS_SET_HTTP_DATA
	if(attr > URL::KVoip_Start_Mailto && attr < URL::KVoip_End_Mailto)
		return (protocol_data.prot_data ? protocol_data.prot_data->GetAttribute(attr, param) : NULL);
#endif

	switch(attr)
	{
	case URL::KContentLoaded:
	case URL::KContentLoaded_Update:
		OP_ASSERT(param); // Parameter MUST be provided
		if(param)
			*((OpFileLength *) param) = (storage ? storage->ContentLoaded(attr == URL::KContentLoaded_Update ? TRUE : FALSE) : 0);
		break;
	case URL::KContentSize:
		if(param)
				*((OpFileLength *) param) = content_size;
		break;
#ifdef WEB_TURBO_MODE
	case URL::KTurboTransferredBytes:
		if(param)
			*((UINT32 *) param) = turbo_transferred_bytes;
		break;
	case URL::KTurboOriginalTransferredBytes:
		if(param)
			*((UINT32 *) param) = turbo_orig_transferred_bytes;
		break;
#endif // WEB_TURBO_MODE
	case URL::KVLastModified:
		if(!param)
			return NULL;

		*((time_t *) param) = 0;

		if(((URLType) url->GetAttribute(URL::KType) == URL_HTTP ||
			(URLType) url->GetAttribute(URL::KType) == URL_HTTPS) &&
			http_data)
			return http_data->GetAttribute(URL::KVLastModified, param);

#ifdef _LOCALHOST_SUPPORT_
		if ((URLType) url->GetAttribute(URL::KType) == URL_FILE && storage)
		{
			OpString src_name;
			RETURN_VALUE_IF_ERROR(storage->FilePathName(src_name), 0);
			OpFile temp_file;

			if(src_name.HasContent() && OpStatus::IsSuccess(temp_file.Construct(src_name)))
			{
				temp_file.GetLastModified(*((time_t *) param));
				return param;
			}
		}
#endif // _LOCALHOST_SUPPORT_
		return param;
	case URL::KMultimediaCacheSize:
	{
		OP_ASSERT(info.multimedia);

		if(!param)
			return NULL;

	#ifdef MULTIMEDIA_CACHE_SUPPORT
		if(!info.multimedia || !storage)
			*((OpFileLength *)param)=FILE_LENGTH_NONE;
		else
			*((OpFileLength *)param)=((Multimedia_Storage *)storage)->GetCacheSize();
	#else
		*((OpFileLength *)param)=FILE_LENGTH_NONE;
	#endif // MULTIMEDIA_CACHE_SUPPORT

		return param;
	}
	case URL::KVLocalTimeLoaded:
		if(!param)
			return NULL;

		*((time_t *) param) = local_time_loaded;
		return param;
	}

	return NULL;
}

void URL_DataStorage::SetAttributeL(URL::URL_VoidPAttribute  attr, const void *param)
{
	LEAVE_IF_ERROR(SetAttribute(attr,param));
}

OP_STATUS URL_DataStorage::SetAttribute(URL::URL_VoidPAttribute  attr, const void *param)
{
	if(attr > URL::KVoip_Start_HTTP && attr < URL::KVoip_End_HTTP)
	{
		RETURN_IF_ERROR(CheckHTTPProtocolData());
		RETURN_IF_ERROR(http_data->SetAttribute(attr, param));
		if(attr == URL::KAddHTTPHeader)
			urlManager->MakeUnique(url);
		return OpStatus::OK;
	}
#if defined(NEED_URL_MIME_DECODE_LISTENERS)
	if(attr > URL::KVoip_Start_MIME &&  attr < URL::KVoip_End_MIME)
	{
		RETURN_IF_ERROR(CheckMailtoProtocolData());
		return protocol_data.prot_data->SetAttribute(attr, param);
	}
#endif

	switch(attr)
	{
	case URL::KContentSize:
		content_size =  (param ? *((OpFileLength *) param) : 0);
		break;
	case URL::KVLocalTimeLoaded:
		local_time_loaded = (param ? *((time_t *) param) : 0);
		break;
#ifdef WEB_TURBO_MODE
	case URL::KTurboTransferredBytes:
		turbo_transferred_bytes = (param ? *((UINT32 *) param) : 0);
		break;
	case URL::KTurboOriginalTransferredBytes:
		turbo_orig_transferred_bytes = (param ? *((UINT32 *) param) : 0);
		break;
#endif // WEB_TURBO_MODE
	}

	return OpStatus::OK;
}

OP_STATUS URL_DataStorage::SetAttribute(const URL_DataStorage::URL_UintAttributeEntry *list)
{
	if(!list)
		return OpStatus::OK;

	while(list->attribute != URL::KNoInt)
	{
		RETURN_IF_ERROR(SetAttribute(list->attribute, list->value));
		list++;
	}
	return OpStatus::OK;
}

OP_STATUS URL_DataStorage::SetAttribute(const URL_DataStorage::URL_StringAttributeEntry *list)
{
	if(!list)
		return OpStatus::OK;

	while(list->attribute != URL::KNoStr)
	{
		RETURN_IF_ERROR(SetAttribute(list->attribute, list->value));
		list++;
	}
	return OpStatus::OK;
}

#ifdef DISK_CACHE_SUPPORT
void URL_DataStorage::SetAttributeL(const URL_DataStorage::URL_UintAttributeEntry *list, DataFile_Record *rec)
{
	if(!list)
		return;

	while(list->attribute != URL::KNoInt)
	{
		uint32 read_value = 0;

		if((list->value & MSB_VALUE) != 0)
			read_value = rec->GetIndexedRecordBOOL(list->value);
		else
			read_value = rec->GetIndexedRecordUIntL(list->value);

		SetAttributeL(list->attribute, read_value);
		list++;
	}
}

#ifdef CACHE_FAST_INDEX
void URL_DataStorage::SetAttributeL(const URL_DataStorage::URL_UintAttributeEntry *list, CacheEntry *entry)
{
	if(!list)
		return;

	UINT32 read_value;

	while(list->attribute != URL::KNoInt)
	{
		if(entry->GetIntValueByTag(list->value, read_value))
			SetAttributeL(list->attribute, read_value);
		list++;
	}
}
#endif // CACHE_FAST_INDEX
#endif // DISK_CACHE_SUPPORT

#ifdef DISK_CACHE_SUPPORT
void URL_DataStorage::SetAttributeL(const URL_DataStorage::URL_StringAttributeRecEntry *list, DataFile_Record *rec)
{
	if(!list)
		return;

	OpString8 temp_val;
	ANCHOR(OpString8, temp_val);

	while(list->attribute != URL::KNoStr)
	{
		rec->GetIndexedRecordStringL(list->value, temp_val);

		if(temp_val.HasContent())
			SetAttributeL(list->attribute, temp_val); // A little inefficient, should we create a takeover version?
		temp_val.Empty();
		list++;
	}
}

#ifdef CACHE_FAST_INDEX
void URL_DataStorage::SetAttributeL(const URL_DataStorage::URL_StringAttributeRecEntry *list, CacheEntry *entry)
{
	if(!list)
		return;

	OpString8 *temp_val;

	while(list->attribute != URL::KNoStr)
	{
		if(entry->GetStringPointerByTag(list->value, &temp_val))
		{
			if(temp_val && temp_val->HasContent())
				SetAttributeL(list->attribute, *temp_val);
		}
		list++;
	}
}
#endif // CACHE_FAST_INDEX
#endif // DISK_CACHE_SUPPORT

#ifdef DISK_CACHE_SUPPORT
void URL_DataStorage::GetAttributeL(const URL_DataStorage::URL_UintAttributeEntry *list, DataFile_Record *rec)
{
	if(!list)
		return;

	while(list->attribute != URL::KNoInt)
	{
		uint32 write_value = GetAttribute(list->attribute);

		if((list->value & MSB_VALUE) != 0)
		{
			if(write_value)
				rec->AddRecordL(list->value);
		}
		else
			rec->AddRecordL(list->value, write_value);

		list++;
	}
}

#ifdef CACHE_FAST_INDEX
void URL_DataStorage::GetAttributeL(const URL_DataStorage::URL_UintAttributeEntry *list, CacheEntry *entry)
{
	if(!list)
		return;

	while(list->attribute != URL::KNoInt)
	{
		uint32 write_value = GetAttribute(list->attribute);

		if((list->value & MSB_VALUE) != 0)
		{
			if(write_value)
				entry->SetIntValueByTag(list->value, TRUE);
		}
		else
			entry->SetIntValueByTag(list->value, write_value);

		list++;
	}
}
#endif // CACHE_FAST_INDEX
#endif // DISK_CACHE_SUPPORT

#ifdef DISK_CACHE_SUPPORT
void URL_DataStorage::GetAttributeL(const URL_DataStorage::URL_StringAttributeRecEntry *list, DataFile_Record *rec)
{
	if(!list)
		return;

	while(list->attribute != URL::KNoStr)
	{
		{
			OpStringC8 temp_val(GetAttribute(list->attribute));

			if(temp_val.HasContent())
				rec->AddRecordL(list->value, temp_val);
		}

		list++;
	}
}

#ifdef CACHE_FAST_INDEX
OP_STATUS URL_DataStorage::GetAttributeL(const URL_DataStorage::URL_StringAttributeRecEntry *list, CacheEntry *entry)
{
	if(!list)
		return OpStatus::OK;

	while(list->attribute != URL::KNoStr)
	{
		OpStringC8 temp = GetAttribute(list->attribute);
		RETURN_IF_ERROR(entry->SetStringValueByTag(list->value, &temp));

		list++;
	}

	return OpStatus::OK;
}
#endif // CACHE_FAST_INDEX
#endif // DISK_CACHE_SUPPORT

URL URL_DataStorage::GetAttribute(URL::URL_URLAttribute  attr)
{
#ifdef _MIME_SUPPORT_
	if(attr > URL::KURL_Start_MIME && attr < URL::KURL_End_MIME)
	{
		if((attr != URL::KAliasURL || url->GetAttribute(URL::KIsAlias)) &&
			protocol_data.mime_data)
		{
			return protocol_data.mime_data->GetAttribute(attr);
		}
	}
	else
#endif // _MIME_SUPPORT_
	if(attr > URL::KURL_Start_HTTP && attr < URL::KURL_End_HTTP)
	{
		if(http_data != NULL)
			return http_data->GetAttribute(attr, url);
	}
	if(attr>URL::KStartURLDynamicAttributeList)
	{
		return GetDynAttribute(attr);
	}

	return URL();
}

void URL_DataStorage::SetAttributeL(URL::URL_URLAttribute  attr, const URL &param)
{
	LEAVE_IF_ERROR(SetAttribute(attr,param));
}

OP_STATUS URL_DataStorage::SetAttribute(URL::URL_URLAttribute  attr, const URL &param)
{
#ifdef _MIME_SUPPORT_
	if(attr > URL::KURL_Start_MIME && attr < URL::KURL_End_MIME)
	{
		if(attr == URL::KAliasURL)
		{
			if(param.IsEmpty())
				return OpStatus::OK;

			if((URLStatus) GetAttribute(URL::KLoadStatus) != URL_UNLOADED)
				return OpStatus::ERR;
		}

		OP_STATUS op_err = OpStatus::OK;
		if(CheckMIMEProtocolData(op_err))
			op_err = protocol_data.prot_data->SetAttribute(attr, param);
		RETURN_IF_ERROR(op_err);
		if(attr == URL::KAliasURL && protocol_data.prot_data->GetAttribute(URL::KAliasURL) == param)
			RETURN_IF_ERROR(url->SetAttribute(URL::KIsAlias, TRUE));
		return OpStatus::OK;
	}
#endif // _MIME_SUPPORT_
	if(attr > URL::KURL_Start_HTTP && attr < URL::KURL_End_HTTP)
	{
		RETURN_IF_ERROR(CheckHTTPProtocolData());
		return http_data->SetAttribute(attr, param);
	}
	
	if(attr>URL::KStartURLDynamicAttributeList)
	{
		return SetDynAttribute(attr, param);
	}
	return OpStatus::OK;
}


	OP_STATUS		URL_DataStorage::SetSecurityStatus(uint32 sec_stat, const uni_char *stat_text){
		URL_SET_ATTRIBUTE_RETCOND(URL::KSecurityLowStatusReason, (sec_stat & (~SECURITY_STATE_MASK)) >> SECURITY_STATE_MASK_BITS);
		URL_SET_ATTRIBUTE_RETCOND(URL::KSecurityStatus, sec_stat & SECURITY_STATE_MASK);
		URL_SET_ATTRIBUTE_RETCOND(URL::KSecurityText, stat_text);
		if((GetAttribute(URL::KSecurityLowStatusReason) & (
			SECURITY_LOW_REASON_UNABLE_TO_CRL_VALIDATE | SECURITY_LOW_REASON_UNABLE_TO_OCSP_VALIDATE |
			SECURITY_LOW_REASON_OCSP_FAILED | SECURITY_LOW_REASON_CRL_FAILED)) != 0)
		{
			// If we were unable to check revocation, do not reuse this response.
			// The next request will be done from scratch, hopefully over a fully secure connection
			urlManager->MakeUnique(url);
		}
		return OpStatus::OK;
	}

#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	void URL_DataStorage::SetIsOutOfCoverageFile(BOOL flag){ URL_SET_ATTRIBUTE(URL::KIsOutOfCoverageFile, flag); }
	BOOL URL_DataStorage::GetIsOutOfCoverageFile() { return !!GetAttribute(URL::KIsOutOfCoverageFile); }
	void URL_DataStorage::SetNeverFlush(BOOL flag){ URL_SET_ATTRIBUTE(URL::KNeverFlush, flag); }
	BOOL URL_DataStorage::GetNeverFlush() { return !!GetAttribute(URL::KNeverFlush); }
#endif

void URL_Rep::IterateMultipart()
{
	if(storage)
		storage->IterateMultipart();
}

void URL_DataStorage::IterateMultipart()
{
	if(storage && storage->GetIsMultipart())
		storage->SetMultipartStatus(Multipart_NextBodypart);
}
