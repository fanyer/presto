/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef GADGET_SUPPORT
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/gadgets/OpGadgetUpdate.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/gadget_utils.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/util/datefun.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"

/* static */ OP_GADGET_STATUS
OpGadgetUpdateObject::Make(OpGadgetUpdateObject*& new_obj,
						   MessageHandler* mh,
						   OpGadgetClassProxy& klass,
						   const uni_char* additional_params)
{
	new_obj = OP_NEW(OpGadgetUpdateObject, (mh, klass));
	if (!new_obj)
		return OpStatus::ERR_NO_MEMORY;

	const uni_char* update_url_str = klass.GetUpdateDescriptionDocumentUrl();

	RETURN_IF_ERROR(new_obj->Construct(update_url_str, additional_params));

	return OpStatus::OK;
}

OpGadgetUpdateObject::OpGadgetUpdateObject(MessageHandler* mh, OpGadgetClassProxy& klass)
: m_mh(mh)
, m_class_proxy(klass)
, m_finished(FALSE)
{
	OP_ASSERT(mh);
}

OpGadgetUpdateObject::~OpGadgetUpdateObject()
{
	if (!m_update_url.IsEmpty())
	{
		m_update_url.StopLoading(m_mh);
		m_update_url = URL();
	}

	Out();
	UnsetCallbacks();
}

OP_GADGET_STATUS
OpGadgetUpdateObject::Construct(const uni_char* update_url_str, const uni_char* additional_params)
{
	m_update_url = g_url_api->GetURL(update_url_str);

	if (additional_params && *additional_params)
	{
		OpString   url;
		OpStringC8 query =    m_update_url.GetAttribute(URL::KQuery);
		OpStringC8 fragment = m_update_url.GetAttribute(URL::KFragment_Name);

		char sep = '?';
		if (query.HasContent())
			sep = '&';

		if (fragment.HasContent())
		{
			RETURN_IF_ERROR(url.Append(update_url_str, uni_strlen(update_url_str) - fragment.Length() - 1)); // plus one for hash
		}
		else
		{
			RETURN_IF_ERROR(url.Append(update_url_str));
		}

		RETURN_IF_ERROR(url.AppendFormat("%c%s", sep, additional_params));

		// Even if there was a fragment originally, we can safely ignore it here,
		// as it would neither be transmitted, nor used later.
		m_update_url = g_url_api->GetURL(url.CStr());
	}

	URL_LoadPolicy load_policy(FALSE, URL_Reload_Conditional, FALSE, TRUE, FALSE);
	CommState state = m_update_url.LoadDocument(m_mh, URL(), load_policy);
	if (state != COMM_LOADING)
		return OpStatus::OK;

	if (m_class_proxy.IsGadgetClassInstalled())
		m_class_proxy.GetGadgetClass()->SetIsUpdating(TRUE);

	RETURN_IF_ERROR(SetCallbacks());

	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetUpdateObject::SetCallbacks()
{
	URL_ID id = m_update_url.Id();

	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_URL_DATA_LOADED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_NOT_MODIFIED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_URL_LOADING_FAILED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_URL_MOVED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_MULTIPART_RELOAD, id));

	return OpStatus::OK;
}

void
OpGadgetUpdateObject::UnsetCallbacks()
{
	if (m_mh)
		m_mh->UnsetCallBacks(this);
}

void
OpGadgetUpdateObject::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(static_cast<URL_ID>(par1) == m_update_url.Id());
	switch (msg)
	{
	case MSG_URL_MOVED:
		UnsetCallbacks();
		m_update_url = m_update_url.GetAttribute(URL::KMovedToURL);
		SetCallbacks();
		break;
	case MSG_URL_LOADING_FAILED:
		HandleError();
		delete this; // Finished processing, get rid of update object
		break;
	case MSG_URL_DATA_LOADED:
	case MSG_NOT_MODIFIED:
	case MSG_MULTIPART_RELOAD:
		if (m_update_url.Status(TRUE) == URL_LOADED)
		{
			if (!m_finished)
				HandleFinished(msg != MSG_NOT_MODIFIED);
			delete this; // Finished processing, get rid of update object
		}
		break;
	default:
		OP_ASSERT(!"You shouldn't be here, are we sending messages to ourselves that we do not handle?");
	}
}

void
OpGadgetUpdateObject::HandleError()
{
	m_finished = TRUE;

	UnsetCallbacks();

	if (!m_update_url.IsEmpty())
	{
		GADGETS_REPORT_ERROR_TO_CONSOLE((GetClass()->GetGadgetPath(), UNI_L("Widget update: unable to load URL %s"), m_update_url.GetAttribute(URL::KUniName).CStr()));

		m_update_url.StopLoading(m_mh);
		m_update_url = URL();

		OpGadgetListener* listener = g_windowCommanderManager->GetGadgetListener();
		if (listener)
		{
			OpGadgetListener::OpGadgetErrorData data;
			data.error = OpGadgetListener::GADGET_ERROR_LOADING_FAILED;
			data.gadget_class = m_class_proxy.GetGadgetClass();
#ifdef GOGI_GADGET_INSTALL_FROM_UDD
			data.update_description_document_url = m_class_proxy.GetUpdateDescriptionDocumentUrl();
			data.user_data = m_class_proxy.GetUserData();
#endif // GOGI_GADGET_INSTALL_FROM_UDD
			listener->OnGadgetUpdateError(data);
		}
	}
}

OP_GADGET_STATUS
OpGadgetUpdateObject::HandleFinished(BOOL modified)
{
	m_finished = TRUE;

	UnsetCallbacks();

	if (m_class_proxy.IsGadgetClassInstalled())
		m_class_proxy.GetGadgetClass()->SetIsUpdating(FALSE);

	if (!m_update_url.IsEmpty())
	{
		m_update_url.StopLoading(m_mh);

		OP_GADGET_STATUS status = ProcessUpdateXML(modified);
		m_update_url = URL();
		if (OpStatus::IsError(status))
		{
			OpGadgetListener* listener = g_windowCommanderManager->GetGadgetListener();
			if (listener)
			{
				OpGadgetListener::OpGadgetErrorData data;
				data.error = OpGadgetListener::GADGET_ERROR_PARSING_FAILED;
				data.gadget_class = m_class_proxy.GetGadgetClass();
#ifdef GOGI_GADGET_INSTALL_FROM_UDD
				data.update_description_document_url = m_class_proxy.GetUpdateDescriptionDocumentUrl();
				data.user_data = m_class_proxy.GetUserData();
#endif // GOGI_GADGET_INSTALL_FROM_UDD
				listener->OnGadgetUpdateError(data);
			}
			return status;
		}
	}

	return OpStatus::OK;
}

BOOL
OpGadgetUpdateObject::IsUpdateNewer(const OpGadgetUpdateInfo* update_info, const OpGadgetClass* gadget_class)
{
	if (!gadget_class) return TRUE; // Update is always newer when there's no gadget class at all
	const uni_char* gadget_version = gadget_class->GetAttribute(WIDGET_ATTR_VERSION);
	return gadget_version == NULL || update_info->GetVersion() != NULL && uni_strcmp(update_info->GetVersion(), gadget_version);
}

OP_GADGET_STATUS
OpGadgetUpdateObject::ProcessUpdateXML(BOOL modified)
{
	if (!m_class_proxy.GetUpdateInfo() || modified)
	{
		OpGadgetUpdateInfo* update_info = OP_NEW(OpGadgetUpdateInfo, (m_class_proxy));
		if (!update_info)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<OpGadgetUpdateInfo> update_info_anchor(update_info);
		XMLFragment f;
		OP_STATUS parse_status = f.Parse(m_update_url);
		if (OpStatus::IsError(parse_status))
		{
#ifdef XML_ERRORS
			if (parse_status == OpStatus::ERR)
				GADGETS_REPORT_ERROR_TO_CONSOLE((m_class_proxy.GetUpdateDescriptionDocumentUrl(), UNI_L("Widget update: error parsing the update XML\nLine: %d at %s"), f.GetErrorLocation().start.line, f.GetErrorDescription()));
			else
#endif // XML_ERRORS
				GADGETS_REPORT_ERROR_TO_CONSOLE((m_class_proxy.GetUpdateDescriptionDocumentUrl(), UNI_L("Widget update: error processing the update XML")));
			return parse_status;
		}

		if (f.EnterElement(XMLExpandedName(UNI_L("http://www.w3.org/ns/widgets"), UNI_L("widgetupdate"))))			// Can we remove the old one? Might break old platform implementations!
			update_info->SetType(OpGadgetUpdateInfo::GADGET_UPDATE_UPDATE);
		else if (f.EnterElement(XMLExpandedName(UNI_L("http://www.w3.org/ns/widgets"), UNI_L("update-info"))))
			update_info->SetType(OpGadgetUpdateInfo::GADGET_UPDATE_UPDATE);
		else if (f.EnterElement(XMLExpandedName(UNI_L("http://widgets.opera.com/ns/update"), UNI_L("disable"))))
			update_info->SetType(OpGadgetUpdateInfo::GADGET_UPDATE_DISABLE);
		else if (f.EnterElement(XMLExpandedName(UNI_L("http://widgets.opera.com/ns/update"), UNI_L("activate"))))
			update_info->SetType(OpGadgetUpdateInfo::GADGET_UPDATE_ACTIVATE);
		else if (f.EnterElement(XMLExpandedName(UNI_L("http://widgets.opera.com/ns/update"), UNI_L("delete"))))
			update_info->SetType(OpGadgetUpdateInfo::GADGET_UPDATE_DELETE);
		else
			update_info->SetType(OpGadgetUpdateInfo::GADGET_UPDATE_UNKNOWN);

		RETURN_IF_ERROR(update_info->SetID(f.GetAttribute(UNI_L("id"))));
		RETURN_IF_ERROR(update_info->SetSrc(f.GetAttribute(UNI_L("src"))));
		RETURN_IF_ERROR(update_info->SetVersion(f.GetAttribute(UNI_L("version"))));
		update_info->SetByteCount(OpGadgetUtils::AttrStringToI(f.GetAttribute(UNI_L("bytes"))));
		update_info->SetIsMandatory(OpGadgetUtils::IsAttrStringTrue(f.GetAttribute(UNI_L("mandatory"))));

		if (f.EnterElement(XMLExpandedName(UNI_L("http://www.w3.org/ns/widgets"), UNI_L("details"))))
		{
			RETURN_IF_ERROR(update_info->SetHref(f.GetAttribute(UNI_L("href"))));
			RETURN_IF_ERROR(update_info->SetDetails(f.GetText()));

			f.LeaveElement(); // details
		}

		f.LeaveElement(); // widgetupdate

		if (!update_info->IsValid())
			return OpStatus::ERR;

		update_info_anchor.release();
		// SetUpdateInfo deletes older UpdateInfo so there should be no memory leak
		m_class_proxy.SetUpdateInfo(update_info);
	}

	// Get either the old or the current update info.
	OpGadgetUpdateInfo* update_info = m_class_proxy.GetUpdateInfo();

	// Getting the time of last modification.
	OpString8 date_str;
	RETURN_IF_ERROR(m_update_url.GetAttribute(URL::KHTTP_LastModified, date_str));
	if (date_str.HasContent())
		update_info->SetLastModified(GetDate(date_str.CStr()));
	update_info->SetExpires(0); //  what about expiry date?

	if (IsUpdateNewer(update_info, m_class_proxy.GetGadgetClass()))
	{
		// Update is mandatory, disable this widget until the update is in place.
		if (update_info->IsMandatory())
			g_gadget_manager->Disable(m_class_proxy.GetGadgetClass());

		update_info->NotifyUpdate();
	}
	else
		update_info->NotifyNoUpdate();

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////

/* static */ OP_GADGET_STATUS
OpGadgetDownloadObject::Make(OpGadgetDownloadObject*& new_obj, MessageHandler* mh, const uni_char* url, const uni_char* caption, void* userdata)
{
	new_obj = OP_NEW(OpGadgetDownloadObject, (mh, userdata));
	if (!new_obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(new_obj->Construct(url, caption)))
	{
		OP_DELETE(new_obj);
		new_obj = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OpGadgetDownloadObject::OpGadgetDownloadObject(MessageHandler* mh, void* userdata)
	: m_mh(mh)
	, m_userdata(userdata)
	, m_finished(FALSE)
{
}

OpGadgetDownloadObject::~OpGadgetDownloadObject()
{
	Out();
	UnsetCallbacks();
}

OP_GADGET_STATUS
OpGadgetDownloadObject::Construct(const uni_char* url, const uni_char* caption)
{
	RETURN_IF_ERROR(m_url_str.Set(url));
	RETURN_IF_ERROR(m_caption.Set(caption));

	m_download_url = g_url_api->GetURL(url);
	if (m_download_url.IsEmpty())
		return OpStatus::ERR;

	CommState state = m_download_url.Load(m_mh, URL(), TRUE, TRUE, FALSE);

	if (state != COMM_LOADING)
		return OpStatus::OK;

	RETURN_IF_ERROR(SetCallbacks());

	return OpStatus::OK;
}

void
OpGadgetDownloadObject::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_URL_MOVED:
		UnsetCallbacks();
		m_download_url = m_download_url.GetAttribute(URL::KMovedToURL); // TODO? Update m_url_str with new target?
		SetCallbacks();
		break;
	case MSG_URL_LOADING_FAILED:
		HandleError();
		OP_DELETE(this);
		return;
	case MSG_URL_DATA_LOADED:
	case MSG_NOT_MODIFIED:
	case MSG_MULTIPART_RELOAD:
		if (m_download_url.Status(TRUE) == URL_LOADED)
		{
			HandleFinished();
			OP_DELETE(this);
		}
		return;
	default:
		OP_ASSERT(!"You shouldn't be here, are we sending messages to ourselves that we do not handle?");
	}
}

OP_GADGET_STATUS
OpGadgetDownloadObject::SetCallbacks()
{
	URL_ID id = m_download_url.Id();

	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_URL_DATA_LOADED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_NOT_MODIFIED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_URL_LOADING_FAILED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_URL_MOVED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_MULTIPART_RELOAD, id));

	return OpStatus::OK;
}

void
OpGadgetDownloadObject::UnsetCallbacks()
{
	if (m_mh)
		m_mh->UnsetCallBacks(this);
}

void
OpGadgetDownloadObject::HandleFinished()
{
	m_finished = TRUE;
	UnsetCallbacks();

	OP_GADGET_STATUS err = g_gadget_manager->DownloadSuccessful(this);
	if (OpStatus::IsRaisable(err))
		g_memory_manager->RaiseCondition(err);
}

void
OpGadgetDownloadObject::HandleError()
{
	m_finished = TRUE;
	UnsetCallbacks();

	OP_GADGET_STATUS err = g_gadget_manager->DownloadFailed(this);
	if (OpStatus::IsRaisable(err))
		g_memory_manager->RaiseCondition(err);
}

/* static */ OP_GADGET_STATUS
OpGadgetDownloadCallbackToken::Make(OpGadgetDownloadCallbackToken*& new_token, const uni_char* url, const uni_char* caption, void* userdata)
{
	new_token = OP_NEW(OpGadgetDownloadCallbackToken, ());
	if (!new_token)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_token->url.Set(url));
	RETURN_IF_ERROR(new_token->caption.Set(caption));
	new_token->userdata = userdata;

	return OpStatus::OK;
}

/* virtual */
OpGadgetDownloadCallbackToken::~OpGadgetDownloadCallbackToken()
{
	Out();
}

void
OpGadgetDownloadCallbackToken::OnGadgetDownloadPermission(BOOL allow)
{
	OP_GADGET_STATUS err = g_gadget_manager->DownloadAndInstall_stage2(allow, url.CStr(), caption.CStr(), userdata);
	if (OpStatus::IsRaisable(err))
		g_memory_manager->RaiseCondition(err);

	OP_DELETE(this);
}

//////////////////////////////////////////////////////////////////////////
// OpGadgetUpdateInfo
//////////////////////////////////////////////////////////////////////////

OpGadgetUpdateInfo::OpGadgetUpdateInfo(OpGadgetClassProxy& klass)
	: m_last_modified(0)
	, m_expires(0)
	, m_type(GADGET_UPDATE_UNKNOWN)
	, m_bytes(0)
	, m_mandatory(FALSE)
	, m_class(klass)
{
}

OpGadgetUpdateInfo::OpGadgetUpdateInfo(OpGadgetClass* klass)
	: m_last_modified(0)
	, m_expires(0)
	, m_type(GADGET_UPDATE_UNKNOWN)
	, m_bytes(0)
	, m_mandatory(FALSE)
	, m_class(klass)
{
}

OpGadgetUpdateInfo::~OpGadgetUpdateInfo()
{
}

OP_GADGET_STATUS
OpGadgetUpdateInfo::SaveToFile(PrefsFile* prefsfile, const OpStringC& section, const uni_char* prefix)
{
	RETURN_IF_LEAVE(SaveToFileL(prefsfile, section, prefix));
	return OpStatus::OK;
}

void
OpGadgetUpdateInfo::SaveToFileL(PrefsFile* prefsfile, const OpStringC& section, const uni_char* prefix)
{
	OpString tmp;
	prefsfile->WriteIntL(section, tmp.SetL(prefix).AppendL(UNI_L("last modified")), (int) m_last_modified); // Safe?
	prefsfile->WriteIntL(section, tmp.SetL(prefix).AppendL(UNI_L("expires")), (int) m_expires); // Safe?

	if (m_type != GADGET_UPDATE_UNKNOWN)
	{
		OpString type;
		if (m_type == GADGET_UPDATE_UPDATE)
			type.SetL(UNI_L("update"));
		else if (m_type == GADGET_UPDATE_DISABLE)
			type.SetL(UNI_L("disable"));
		else if (m_type == GADGET_UPDATE_ACTIVATE)
			type.SetL(UNI_L("activate"));
		else if (m_type == GADGET_UPDATE_DELETE)
			type.SetL(UNI_L("delete"));
		prefsfile->WriteStringL(section, tmp.SetL(prefix).AppendL(UNI_L("type")), type);
		prefsfile->WriteStringL(section, tmp.SetL(prefix).AppendL(UNI_L("id")), m_id);
		prefsfile->WriteStringL(section, tmp.SetL(prefix).AppendL(UNI_L("src")), m_src);
		prefsfile->WriteStringL(section, tmp.SetL(prefix).AppendL(UNI_L("version")), m_version);
		prefsfile->WriteIntL(section, tmp.SetL(prefix).AppendL(UNI_L("bytes")), m_bytes);
		prefsfile->WriteIntL(section, tmp.SetL(prefix).AppendL(UNI_L("mandatory")), m_mandatory);
		prefsfile->WriteStringL(section, tmp.SetL(prefix).AppendL(UNI_L("details")), m_details);
		prefsfile->WriteStringL(section, tmp.SetL(prefix).AppendL(UNI_L("href")), m_href);
	}
}

OP_GADGET_STATUS
OpGadgetUpdateInfo::LoadFromFile(PrefsFile* prefsfile, const OpStringC& section, const uni_char* prefix)
{
	RETURN_IF_LEAVE(LoadFromFileL(prefsfile, section, prefix));
	return OpStatus::OK;
}

void
OpGadgetUpdateInfo::LoadFromFileL(PrefsFile* prefsfile, const OpStringC& section, const uni_char* prefix)
{
	OpString tmp; ANCHOR(OpString, tmp);
	m_last_modified = prefsfile->ReadIntL(section, tmp.SetL(prefix).AppendL(UNI_L("last modified")), 0);
	m_expires = prefsfile->ReadIntL(section, tmp.SetL(prefix).AppendL(UNI_L("expires")), 0);

	OpString type_str; ANCHOR(OpString, type_str);
	prefsfile->ReadStringL(section, tmp.SetL(prefix).AppendL(UNI_L("type")), type_str, UNI_L(""));
	UpdateType type = GADGET_UPDATE_UNKNOWN;
	if (type_str.Length() > 0)
	{
		if (type_str.CompareI(UNI_L("update")) == 0)
			type = GADGET_UPDATE_UPDATE;
		else if (type_str.CompareI(UNI_L("disable")) == 0)
			type = GADGET_UPDATE_DISABLE;
		else if (type_str.CompareI(UNI_L("activate")) == 0)
			type = GADGET_UPDATE_ACTIVATE;
		else if (type_str.CompareI(UNI_L("delete")) == 0)
			type = GADGET_UPDATE_DELETE;
		else if (type_str.CompareI(UNI_L("unknown")) == 0)
			LEAVE(OpStatus::ERR);
		else
			LEAVE(OpStatus::ERR);

		m_type = type;
		prefsfile->ReadStringL(section, tmp.SetL(prefix).AppendL(UNI_L("id")), m_id, UNI_L(""));
		if (m_id.Length() == 0)
			LEAVE(OpStatus::ERR);
		prefsfile->ReadStringL(section, tmp.SetL(prefix).AppendL(UNI_L("src")), m_src, UNI_L(""));
		if (m_src.Length() == 0)
			LEAVE(OpStatus::ERR);
		prefsfile->ReadStringL(section, tmp.SetL(prefix).AppendL(UNI_L("version")), m_version, UNI_L(""));
		if (m_version.Length() == 0)
			LEAVE(OpStatus::ERR);
		INT32 bytes = prefsfile->ReadIntL(section, tmp.SetL(prefix).AppendL(UNI_L("bytes")), -1);
		if (bytes == -1)
			LEAVE(OpStatus::ERR);
		m_bytes = static_cast<UINT32>(bytes);
		m_mandatory = prefsfile->ReadBoolL(section, tmp.SetL(prefix).AppendL(UNI_L("mandatory")));

		prefsfile->ReadStringL(section, tmp.SetL(prefix).AppendL(UNI_L("details")), m_details, UNI_L(""));
		prefsfile->ReadStringL(section, tmp.SetL(prefix).AppendL(UNI_L("href")), m_href, UNI_L(""));
	}
	else
	{
		m_type = GADGET_UPDATE_UNKNOWN;
		m_id.Empty();
		m_src.Empty();
		m_version.Empty();
		m_bytes = 0;
		m_mandatory = FALSE;
		m_details.Empty();
		m_href.Empty();
	}
}

OpGadgetListener::GadgetUpdateType
OpGadgetUpdateInfo::ToGadgetListenerUpdateType(OpGadgetUpdateInfo::UpdateType update_type)
{
	switch (update_type)
	{
	case GADGET_UPDATE_UPDATE:
		return OpGadgetListener::GADGET_UPDATE;
	case GADGET_UPDATE_DISABLE:
		return OpGadgetListener::GADGET_DISABLE;
	case GADGET_UPDATE_ACTIVATE:
		return OpGadgetListener::GADGET_ACTIVATE;
	case GADGET_UPDATE_DELETE:
		return OpGadgetListener::GADGET_DELETE;
	default:
		return OpGadgetListener::GADGET_UNKNOWN;
	};
}

void
OpGadgetUpdateInfo::NotifyUpdate(OpGadgetListener::GadgetUpdateType gadget_listener_update_type)
{
	OpGadgetListener::OpGadgetUpdateData data;
	op_memset(&data, 0, sizeof(data));

	data.id = m_id.CStr();
	data.type = gadget_listener_update_type;
	data.src = m_src.CStr();
	data.version = m_version.CStr();
	data.details = m_details.CStr();
	data.href = m_href.CStr();

	data.bytes = m_bytes;
	data.mandatory = m_mandatory;

	data.gadget_class = m_class.GetGadgetClass();
#ifdef GOGI_GADGET_INSTALL_FROM_UDD
	data.update_description_document_url = m_class.GetUpdateDescriptionDocumentUrl();
	data.user_data = m_class.GetUserData();
#endif // GOGI_GADGET_INSTALL_FROM_UDD

	g_gadget_manager->NotifyGadgetUpdateReady(data);
}

#endif // GADGET_SUPPORT
