/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef WAND_SUPPORT

#include "modules/wand/wandmanager.h"

#include "modules/wand/wand_internal.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/formmanager.h"
#include "modules/forms/src/formiterator.h"
#include "modules/doc/frm_doc.h"
#include "modules/util/opfile/opfile.h"
#include "modules/widgets/OpButton.h"
#include "modules/dochand/win.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/libcrypto/include/CryptoBlob.h"
#include "modules/sync/sync_util.h"
#include "modules/wand/wand_sync_item.h"


BOOL IsValidWandObject(HTML_Element* he)
{
	// No buttons..
	if (he->Type() == HE_INPUT)
	{
		InputType type = he->GetInputType();
		if (type == INPUT_RESET ||
			type == INPUT_SUBMIT ||
			type == INPUT_BUTTON ||
			type == INPUT_FILE)
		{
			return FALSE;
		}
		return TRUE;
	}

	// Not allowing textareas.. so only select left
	return he->Type() == HE_SELECT;
}

BOOL IsProtectedFormObject(HTML_Element* he)
{
	// Check if the element is protected (Should not used wand)
	// This check could be done inside IsValidWandObject but we only want to call it from Store (Optimization)
	return g_pccore->GetIntegerPref(PrefsCollectionCore::AutocompleteOffDisablesWand) &&
		he->GetAutocompleteOff();
}

WandPage::~WandPage()
{
	objects.DeleteAll();
	OP_ASSERT(objects.GetCount() == 0);
}

#ifdef SYNC_HAVE_PASSWORD_MANAGER
OP_STATUS WandPage::InitAuthTypeSpecificElements(OpSyncItem *item, BOOL modify)
{
	OP_ASSERT(g_wand_manager->HasSyncPasswordEncryptionKey() && "We should have received the encryption-key by now!");
	OP_ASSERT(HasPreparedSSLForCrypto());

	if (!item)
		return OpStatus::ERR;

	OP_ASSERT(item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_ADDED ||
	          item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_MODIFIED);

    OP_ASSERT(item->GetType() == OpSyncDataItem::DATAITEM_PM_FORM_AUTH);

	bool does_policy_apply_to_whole_server = true;
	OpString policy_scope;
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_PM_FORM_AUTH_SCOPE, policy_scope));
	if (policy_scope.HasContent())
	{
		if (policy_scope.CompareI(UNI_L("page")) == 0)
			does_policy_apply_to_whole_server = false;
		else if (policy_scope.CompareI(UNI_L("server")) == 0)
			does_policy_apply_to_whole_server = true;
		else
			return OpStatus::ERR_OUT_OF_RANGE;
	}
	if (does_policy_apply_to_whole_server)
		flags |= WAND_FLAG_ON_THIS_SERVER;

    OpAutoPtr<CryptoBlobEncryption> blob_encryptor(CryptoBlobEncryption::Create());
    RETURN_OOM_IF_NULL(blob_encryptor.get());

    RETURN_IF_ERROR(blob_encryptor->SetKey(g_wand_manager->GetSyncPasswordEncryptionKey()));

    OpString encrypted_blob;
    RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_PAGE_URL, encrypted_blob));
    if (!modify && encrypted_blob.IsEmpty()) // Required for new items
    	return OpStatus::ERR;

    if (encrypted_blob.HasContent())
    	RETURN_IF_ERROR(blob_encryptor->DecryptBase64(encrypted_blob.CStr(), url));

    RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_TOPDOC_URL, encrypted_blob));
    if (encrypted_blob.HasContent())
    	RETURN_IF_ERROR(blob_encryptor->DecryptBase64(encrypted_blob.CStr(), m_topdoc_url));

    RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_FORM_URL, encrypted_blob));

    if (encrypted_blob.HasContent())
    	RETURN_IF_ERROR(blob_encryptor->DecryptBase64(encrypted_blob.CStr(), url_action));

    OpString form_data;
    RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_FORM_DATA, form_data));
    if (!modify && form_data.IsEmpty()) // Required for new items
    	return OpStatus::ERR;

    if (form_data.HasContent())
    {
		objects.DeleteAll();
		RETURN_IF_ERROR(blob_encryptor->DecryptBase64(form_data.CStr(), form_data));
	    if (!modify && form_data.IsEmpty()) // Required for new items
	    	return OpStatus::ERR;

		XMLFragment form_data_xml;

		RETURN_IF_ERROR(form_data_xml.Parse(form_data, form_data.Length()));
		if (form_data_xml.EnterElement(UNI_L("form_data")))
		{
			const uni_char *form_number = form_data_xml.GetAttribute(UNI_L("number"));
			if (form_number && uni_isdigit(*form_number))
				formnr = uni_atoi(form_number);
			else
				return OpStatus::ERR;
			RETURN_IF_ERROR(submitname.Set(form_data_xml.GetAttribute(UNI_L("submitname"))));

			if (!form_data_xml.HasMoreElements())
				return OpStatus::ERR;

			while (form_data_xml.HasMoreElements())
			{
				// Add the form inputs
				if (form_data_xml.EnterElement(UNI_L("input")))
				{
					const uni_char *name = form_data_xml.GetAttribute(UNI_L("name"));
					const uni_char *type = form_data_xml.GetAttribute(UNI_L("type"));
					const uni_char *is_changed = form_data_xml.GetAttribute(UNI_L("is_changed"));
					const uni_char *value = form_data_xml.GetText();

					if (!name || !type || !is_changed || !value)
						return OpStatus::ERR;

					RETURN_IF_ERROR(AddObjectInfo(name, value,
								  uni_strcmp(type, "password") ? FALSE : TRUE,
								  uni_strcmp(type, "text") ? FALSE : TRUE,
								  uni_strcmp(is_changed, "yes") ? FALSE : TRUE,
								  TRUE));
				}
				else
				{
					form_data_xml.EnterAnyElement(); // We ignore unknown elements for forward compatibility
				}
				form_data_xml.LeaveElement();
			}
		}
    }

    return OpStatus::OK;
}

OP_STATUS WandPage::ConstructSyncItemAuthTypeSpecificElements(OpSyncItem *sync_item)
{
	OP_ASSERT(HasPreparedSSLForCrypto());
	OP_ASSERT(g_wand_manager->HasSyncPasswordEncryptionKey() && "This should only be called if we have the encryption-key!");

	OpStringC policy_scope(GetOnThisServer() ? UNI_L("server") : UNI_L("page"));
	sync_item->SetData(OpSyncItem::SYNC_KEY_PM_FORM_AUTH_SCOPE, policy_scope);

	// We create a XML document encrypted in a blob for all the input fields in
	// one WandPage. Note that the server will never see this xml, as it is only
	// in decrypted state on the client.

	XMLFragment form_data;
	form_data.OpenElement(UNI_L("form_data")); // <form_data>
	RETURN_IF_ERROR(form_data.SetAttributeFormat(UNI_L("number"), UNI_L("%u"), formnr));

	if (submitname.HasContent())
		RETURN_IF_ERROR(form_data.SetAttribute(UNI_L("submitname"), submitname, XMLWHITESPACEHANDLING_PRESERVE));

	OpString password;
	INT32 number_of_fields = CountObjects();

	OpString username;
	for (int idx = 0; idx < number_of_fields; idx++)
	{
		WandObjectInfo *wand_info = GetObject(idx);

		if (wand_info->is_guessed_username)
		{
			OP_ASSERT(username.IsEmpty());

			RETURN_IF_ERROR(username.Set(wand_info->value));
		}
		RETURN_IF_ERROR(form_data.OpenElement(UNI_L("input"))); /* <input> */

		if (wand_info->is_password)
		{
			OpString password;
			RETURN_IF_ERROR(wand_info->password.GetPassword(password));
			if (password.HasContent())
				RETURN_IF_ERROR(form_data.AddText(password.CStr(), ~0u, XMLWHITESPACEHANDLING_PRESERVE));
		}
		else
		{
			if (wand_info->value.IsEmpty())
				return OpStatus::ERR;

			RETURN_IF_ERROR(form_data.AddText(wand_info->value.CStr(), ~0u, XMLWHITESPACEHANDLING_PRESERVE));
		}

		RETURN_IF_ERROR(form_data.SetAttribute(UNI_L("name"), wand_info->name.CStr(), XMLWHITESPACEHANDLING_PRESERVE));
		RETURN_IF_ERROR(form_data.SetAttribute(UNI_L("is_changed"), wand_info->is_changed ? UNI_L("yes") : UNI_L("no")));

		RETURN_IF_ERROR(form_data.SetAttribute(UNI_L("type"),
											   wand_info->is_password ?
												   UNI_L("password")
												   :
												   (wand_info->is_textfield_for_sure ?
													   UNI_L("text") :
													   UNI_L("other"))));
		form_data.CloseElement(); // </input>
	}

	form_data.CloseElement(); // <form_data>

	/* We save the xml to disk */
	TempBuffer form_data_buffer;
	RETURN_IF_ERROR(form_data.GetXML(form_data_buffer, TRUE, NULL, TRUE));


	OpAutoPtr<CryptoBlobEncryption> blob_encryptor(CryptoBlobEncryption::Create());
	RETURN_OOM_IF_NULL(blob_encryptor.get());
	RETURN_IF_ERROR(blob_encryptor->SetKey(g_wand_manager->GetSyncPasswordEncryptionKey()));

	RETURN_IF_ERROR(blob_encryptor->EncryptBase64(username, username));
	RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_USERNAME, username));

	OpString blob;
	// Encrypt the xml into a blob
	RETURN_IF_ERROR(blob_encryptor->EncryptBase64(url_action.CStr(), blob));
	RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_FORM_URL, blob));


	RETURN_IF_ERROR(blob_encryptor->EncryptBase64(url.CStr(), blob));
	RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_PAGE_URL, blob));

	RETURN_IF_ERROR(blob_encryptor->EncryptBase64(m_topdoc_url.CStr(), blob));
	RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_TOPDOC_URL, blob));

	RETURN_IF_ERROR(blob_encryptor->EncryptBase64(form_data_buffer.GetStorage() , blob));
	RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_FORM_DATA, blob));

	return OpStatus::OK;
}
#endif // SYNC_HAVE_PASSWORD_MANAGER

OP_STATUS WandPage::Save(OpFile &file)
{
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	RETURN_IF_ERROR(WandSyncItem::SaveSyncData(file));
#else
	// write dummy data to keep the file format version >=6 consistent
	OpString dummy_string;
	RETURN_IF_ERROR(file.WriteBinLong(0L));
	RETURN_IF_ERROR(WriteWandString(file, dummy_string));
	RETURN_IF_ERROR(WriteWandString(file, dummy_string));
#endif // SYNC_HAVE_PASSWORD_MANAGER

	RETURN_IF_ERROR(WriteWandString(file, url));
	RETURN_IF_ERROR(WriteWandString(file, submitname));
	RETURN_IF_ERROR(WriteWandString(file, m_topdoc_url));
	RETURN_IF_ERROR(WriteWandString(file, url_action));

	RETURN_IF_ERROR(file.WriteBinLong(formnr));
	RETURN_IF_ERROR(file.WriteBinLong(offset_x));
	RETURN_IF_ERROR(file.WriteBinLong(offset_y));
	RETURN_IF_ERROR(file.WriteBinLong(document_x));
	RETURN_IF_ERROR(file.WriteBinLong(document_y));

	RETURN_IF_ERROR(file.WriteBinLong(flags));
	RETURN_IF_ERROR(file.WriteBinLong(objects.GetCount()));
	for(unsigned int i = 0; i < objects.GetCount(); i++)
	{
		RETURN_IF_ERROR(objects.Get(i)->Save(file));
	}
	return OpStatus::OK;
}

OP_STATUS WandPage::Open(OpFile &file, long version)
{
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	RETURN_IF_ERROR(WandSyncItem::OpenSyncData(file, version));
#else
	if (version >= 6)
	{
		// We are reading wand file containing wand sync data
		// in a version of opera that does not support wand sync.
		// Read dummy data to keep the file format version >=6 consistent
		long dummy;
		RETURN_IF_ERROR(file.ReadBinLong(dummy));
		OpString dummy_string;
		RETURN_IF_ERROR(ReadWandString(file, dummy_string, version));
		RETURN_IF_ERROR(ReadWandString(file, dummy_string, version));
	}
#endif // SYNC_HAVE_PASSWORD_MANAGER

	RETURN_IF_ERROR(ReadWandString(file, url, version));
	RETURN_IF_ERROR(ReadWandString(file, submitname, version));
	if (version >= 4)
	{
		RETURN_IF_ERROR(ReadWandString(file, m_topdoc_url, version));
		RETURN_IF_ERROR(ReadWandString(file, url_action, version));
	}

	long tmpformnr;
	RETURN_IF_ERROR(file.ReadBinLong(tmpformnr));
	formnr = tmpformnr;

	RETURN_IF_ERROR(file.ReadBinLong(offset_x));
	RETURN_IF_ERROR(file.ReadBinLong(offset_y));
	RETURN_IF_ERROR(file.ReadBinLong(document_x));
	RETURN_IF_ERROR(file.ReadBinLong(document_y));

	long tmpflags;
	RETURN_IF_ERROR(file.ReadBinLong(tmpflags));
	flags = tmpflags;

	long num_objects;
	RETURN_IF_ERROR(file.ReadBinLong(num_objects));

	for(int i = 0; i < num_objects; i++)
	{
		WandObjectInfo* objinfo = OP_NEW(WandObjectInfo, ());
		if (objinfo == NULL)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsError(objinfo->Open(file, version)) ||
			OpStatus::IsError(objects.Add(objinfo)))
		{
			OP_DELETE(objinfo);
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	return OpStatus::OK;
}

OP_STATUS WandPage::ReplaceWithOtherPage(WandPage* other_page)
{
	// We will not touch url, m_topdoc_url, url_action and flags
	RETURN_IF_ERROR(submitname.Set(other_page->submitname));
	formnr = other_page->formnr;
	offset_x = other_page->offset_x;
	offset_y = other_page->offset_y;
	document_x = other_page->document_x;
	document_y = other_page->document_y;

	objects.DeleteAll();
	RETURN_IF_ERROR(objects.DuplicateOf(other_page->objects));
	other_page->objects.Remove(0, other_page->objects.GetCount()); // All pointers have moved to our objects vector

	return OpStatus::OK;
}

OP_STATUS WandPage::CollectFromDocumentInternal(FramesDocument* doc, HTML_Element* he_submit, HTML_Element* he_form,
						long offset_x, long offset_y, long document_x, long document_y, BOOL encrypted)
{
	OP_ASSERT(doc);
	OP_ASSERT(he_form);

	Clear();

	this->offset_x = offset_x;
	this->offset_y = offset_y;
	this->document_x = document_x;
	this->document_y = document_y;

	if (he_submit)
		RETURN_IF_ERROR(this->submitname.Set(he_submit->GetName()));
	this->formnr = he_form->GetFormNr();

	if (GetNeverOnThisPage()) // We should NOT store any formsdata.
		return OpStatus::OK;

	if (IsProtectedFormObject(he_form))
		return OpStatus::OK;

	// Store all formobjects which has a name and a value
	// FIXME: Use a FormIterator
	for (HTML_Element* he = doc->GetDocRoot(); he; he = he->NextActual())
	{
		if (he->GetNsType() != NS_HTML)
		{
			continue;
		}

		switch (he->Type())
		{
//		case HE_OPTION:
		case HE_TEXTAREA:
		case HE_OUTPUT:
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
		case HE_KEYGEN:
#endif
		case HE_INPUT:
		case HE_SELECT:
			break;
		default:
			continue; // Not a form thing
		}

		if (IsValidWandObject(he) &&
		    !IsProtectedFormObject(he) &&
		    FormManager::BelongsToForm(doc, he_form, he))
		{
			FormValue* form_value = he->GetFormValue();
			const uni_char* name = he->GetStringAttr(ATTR_NAME);

			if (name && name[0] != 0)
			{
				BOOL is_changed = form_value->GetType() == FormValue::VALUE_LIST_SELECTION ||
					form_value->IsChangedFromOriginal(he);
				if (form_value->GetType() == FormValue::VALUE_LIST_SELECTION)
				{
					// Build a list of selected indexes as a text to be
					// compatible with the old form value code
					FormValueList* list_value = FormValueList::GetAs(form_value);
					OpINT32Vector selected_indexes;
					OpString index_text;
					RETURN_IF_ERROR(list_value->GetSelectedIndexes(he, selected_indexes));
					UINT32 sel_count = selected_indexes.GetCount();
					if (sel_count > 0)
					{
						uni_char* index_text_p = index_text.Reserve(sel_count * 10); // Not more than 10 digit number of options in the select.
						if (!index_text_p)
						{
							return OpStatus::ERR_NO_MEMORY;
						}
						for (unsigned int sel_i = 0; sel_i < sel_count; sel_i++)
						{
							if (index_text_p != index_text.CStr())
							{
								*(index_text_p++) = ',';
							}
							uni_itoa(selected_indexes.Get(sel_i), index_text_p, 10);
							index_text_p += uni_strlen(index_text_p);
						}
						RETURN_IF_ERROR(AddObjectInfo(name, index_text.CStr(), FALSE, FALSE, is_changed, FALSE));
					}
				}
				else // normal "text" value
				{
					if (form_value->GetType() == FormValue::VALUE_RADIOCHECK &&
						!FormValueRadioCheck::GetAs(form_value)->IsChecked(he))
					{
						// We only save checked checkboxes (FIXME)
						continue;
					}
					if (!form_value->HasMeaningfulTextRepresentation()) 
					{
						OP_ASSERT(FALSE); // Something strange. Maybe
										  // we should support it in
										  // wand too
						continue;
					}
					OpString value;
					OP_STATUS status = form_value->GetValueAsText(he, value);
					if (status == OpStatus::ERR_NOT_SUPPORTED)
					{
						// No text value
						continue;
					}
					RETURN_IF_ERROR(status);

					if (value.IsEmpty())
					{
						// No text value
						continue;
					}
						
					// Always store textfields with value. This is so we don't miss usernames in situations when the username
					// was prefilled on the page we are storing. Same with passwordfields.
					BOOL is_password = FALSE;
					BOOL is_textfield = FALSE;
					OP_ASSERT(he->GetNsType() == NS_HTML);
					if (he->Type() == HE_INPUT)
					{
						switch (he->GetInputType())
						{
						case INPUT_TEXT:
						case INPUT_EMAIL:
							is_textfield = TRUE;
							break;
						case INPUT_PASSWORD:
							is_password = TRUE;
						}
					}
					if (is_changed || is_password || is_textfield)
					{
						RETURN_IF_ERROR(AddObjectInfo(name, value.CStr(), is_password, is_textfield, is_changed, encrypted));
					}
				}
			}
		}
	}

	WandObjectInfo *username_info = FindAndMarkUserNameFieldCandidate(doc);
	if (username_info)
		username_info->is_guessed_username = TRUE;

	return OpStatus::OK;
}


OP_STATUS WandPage::EncryptPasswords()
{
	for (UINT32 i = 0; i < objects.GetCount(); i++)
	{
		WandObjectInfo* info = objects.Get(i);
		if (info->is_password)
		{
			RETURN_IF_ERROR(info->password.Encrypt());
		}
	}

	return OpStatus::OK;
}

OP_STATUS WandPage::Fetch(FramesDocument* doc, HTML_Element* helm, int& num_matching, BOOL only_if_matching_username)
{
	num_matching = 0;
	if (GetNeverOnThisPage()) // We should NOT fetch any formsdata.
		return OpStatus::OK;

	HTML_Element* he = helm ? helm : doc->GetDocRoot();

	BOOL need_matching_username = only_if_matching_username;

	/* pending_password and the value are used if we need a matching
	 * username, and the password field is encountered before the
	 * username field, so we have to fill it out later, if we find a
	 * matching username field.
	 * NOTE: All fields found before the username will not be filled. It's probably not very common
	 *       to have fields before the username anyway. */
	HTML_Element* pending_password = NULL;
	OpString pending_password_value;

	WandObjectInfo *username_candidate_objinfo = FindAndMarkUserNameFieldCandidate(doc);

	while (he)
	{
		if (IsValidWandObject(he))
		{
			HTML_Element* form = NULL;
			WandObjectInfo* object_info = FindMatch(doc, he, form);
			BOOL did_set_form_value = FALSE;

			if (object_info)
			{
				FormValue *form_value = he->GetFormValue();
				if (object_info->is_password)
				{
					OpString tmp;
					object_info->password.GetPassword(tmp);

					if (!need_matching_username)
					{
						did_set_form_value = WandManager::SetFormValue(doc, he, tmp.CStr());
					}
					else
					{
						pending_password = he;
						pending_password_value.Set(tmp);
					}
				}
				else if (!object_info->is_textfield_for_sure
						 || object_info == username_candidate_objinfo
						 || !form_value->IsChangedFromOriginal(he))
				{
					/* Set the form value to what wand has stored, unless the element is a textfield
					 * (but not a username field) and the user explicitely changed it, in that
					 * case use the user supplied value. Useful for login boxes with captcha 
					 * fields (CORE-16185 is about this). */

					if (need_matching_username && object_info == username_candidate_objinfo)
					{
						OpString value;
						if (OpStatus::IsSuccess(form_value->GetValueAsText(he, value)) &&
							(value.IsEmpty() || value == username_candidate_objinfo->value))
						{
							// We found the username field and its value matches the one stored
							need_matching_username = FALSE;
						}
					}

					if (!need_matching_username)
					{
						did_set_form_value = WandManager::SetFormValue(doc, he, object_info->value.CStr());

						if (did_set_form_value && pending_password)
						{
							if (WandManager::SetFormValue(doc, pending_password, pending_password_value.CStr()))
								num_matching++;
							pending_password = NULL;
						}
					}
				}
			}
#ifdef WAND_ECOMMERCE_SUPPORT
			else if (url.IsEmpty()) ///< Only fetch personal info if using the ecommerce WandPage. Not with regular stored wandpages.
			{
				const uni_char* field_name = he->GetStringAttr(ATTR_NAME);

				if (field_name && WandManager::CanImportValueFromPrefs(he, field_name))
				{
					OpString value;
					RETURN_IF_ERROR(WandManager::ImportValueFromPrefs(he, field_name, value));
					did_set_form_value = WandManager::SetFormValue(doc, he, value.CStr());
				}
			}
#endif // WAND_ECOMMERCE_SUPPORT

			if (did_set_form_value)
				num_matching++;
		}

		he = helm ? NULL : he->Next();
	}

	pending_password_value.Wipe();

	return OpStatus::OK;
}

INT32 WandPage::CountMatches(FramesDocument* doc)
{
	if (GetNeverOnThisPage()) // We should NOT fetch any formsdata.
		return 0;

	INT32 match_count = 0;
	HTML_Element* he = doc->GetDocRoot();
	while (he)
	{
		HTML_Element* form = NULL;
		if (IsValidWandObject(he) && FindMatch(doc, he, form))
			match_count++;
		he = he->NextActualStyle();
	}
	return match_count;
}

void WandPage::Clear()
{
	objects.DeleteAll();
	submitname.Empty();
	formnr = 0;
}

WandObjectInfo* WandPage::FindNameMatch(const uni_char* name)
{
	if (name == NULL)
		return NULL;

	for(unsigned int i = 0; i < objects.GetCount(); i++)
	{
		WandObjectInfo* objinfo = objects.Get(i);
		if (objinfo && !objinfo->name.IsEmpty() && uni_stri_eq(name, objinfo->name.CStr()))
		{
			// Don't match empty stored values (F.ex. Personal info
			// stores some data empty, until user has specified them.)
			if (!objinfo->is_password && objinfo->value.IsEmpty())
				return NULL;

			return objinfo;
		}
	}

	return NULL;
}


WandObjectInfo* WandPage::FindMatch(FramesDocument* doc, HTML_Element* he, HTML_Element*& he_form)
{
	const uni_char* name = he->GetStringAttr(ATTR_NAME);
	const uni_char* value = he->GetStringAttr(ATTR_VALUE);
	if (name == NULL || !IsValidWandObject(he) || (formnr != 0 && formnr != (UINT32)he->GetFormNr()))
		return NULL;

	// If we have a absolute action url saved, we should check if the elements formaction points to the same server as the stored one.
	if (!url_action.IsEmpty() && url_action.Find(UNI_L("://")) != KNotFound && doc)
	{
		if (!he_form)
			he_form = FormManager::FindFormElm(doc, he);

		if (he_form)
		{
			OP_ASSERT(FormManager::BelongsToForm(doc, he_form, he));
			URL* action_url = he_form->GetUrlAttr(ATTR_ACTION, NS_IDX_HTML, doc->GetLogicalDocument());
			OpString form_action;
			if (action_url)
			{
				unsigned short dummy_charset_id = 0; // not used since we convert to a *_Escaped string.
				OP_STATUS status = action_url->GetAttribute(URL::KUniName_Escaped, dummy_charset_id, form_action);
				if (OpStatus::IsError(status))
				{
					return NULL;
				}
				if (!form_action.IsEmpty())
				{
					MakeWandUrl(form_action);
					MakeWandServerUrl(form_action);
					if (form_action.Compare(url_action))
					{
						// Possible phishing! return no match! (Bug 239868)
						return NULL;
					}
				}
			}
		}
	}

	// If the top document's url is different from the document url,
	// we must not match for perceived security reasons (Bug 197657).
	if (doc)
	{
		OpString topdoc_url, this_url;
		unsigned short dummy_charset_id = 0; // not used since we convert to a *_Escaped string.
		// Get top doc (which is the referrer if this is a frameset)
		if (OpStatus::IsError(doc->GetWindow()->GetCurrentURL().GetAttribute(URL::KUniName_With_Fragment_Escaped, dummy_charset_id, topdoc_url)))
		{
			return NULL;
		}
		if (!topdoc_url.IsEmpty())
		{
			MakeWandUrl(topdoc_url);
			MakeWandServerUrl(topdoc_url);
		}
		// Only if it is a different domain. Assume url is matching
		// doc's url (Otherwise WandPage::FindMatch should not have
		// been called)
		if (OpStatus::IsError(this_url.Set(url)))
			return NULL;
		MakeWandUrl(this_url);
		MakeWandServerUrl(this_url);
		if (this_url.CStr() && uni_stri_eq(this_url.CStr(), topdoc_url.CStr()))
			topdoc_url.Empty();

		if (!uni_stri_eq(m_topdoc_url.CStr() ? m_topdoc_url.CStr() : UNI_L(""),
						topdoc_url.CStr() ? topdoc_url.CStr() : UNI_L("")))
			return NULL;
	}

	for(unsigned int i = 0; i < objects.GetCount(); i++)
	{
		WandObjectInfo* objinfo = objects.Get(i);
		if (objinfo && !objinfo->name.IsEmpty() && uni_stri_eq(name, objinfo->name.CStr()))
		{
			// Don't match empty stored values (F.ex. Personal info
			// stores some data empty, until user has specified them.)
			if (objinfo->value.IsEmpty() && !objinfo->is_password)
				return NULL;
			// If it's a radiobutton, we need to see if it is the
			// checked one since we only match checked radio buttons
			if (he->Type() == HE_INPUT && he->GetInputType() == INPUT_RADIO)
			{
				if (!value ||
					objinfo->value.IsEmpty() ||
					!uni_stri_eq(value, objinfo->value.CStr()))
				{
					return NULL;
				}
			}

			return objinfo;
		}
	}

	return NULL;
}

extern BOOL HasPreparedSSLForCrypto();

OP_STATUS WandPage::AddObjectInfo(const uni_char* name, const uni_char* value, BOOL is_password, BOOL is_textfield, BOOL is_changed, BOOL add_encrypted)
{
	OP_ASSERT(!is_password || !add_encrypted || HasPreparedSSLForCrypto());
	WandObjectInfo* objinfo = OP_NEW(WandObjectInfo, ());
	if (objinfo == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(objinfo->Init(name, value, is_password, is_textfield, is_changed, add_encrypted)) ||
		OpStatus::IsError(objects.Add(objinfo)))
	{
		OP_DELETE(objinfo);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

#ifdef SELFTEST
OP_STATUS WandPage::SelftestConstruct(const uni_char* in_url,
                                      const uni_char* topdoc_url,
                                      const uni_char *in_url_action,
                                      const uni_char* in_submitname,
                                      UINT32 in_formnr,
                                      UINT32 in_flags)
{
	formnr = in_formnr;
	flags = in_flags;

	RETURN_IF_ERROR(url.Set(in_url));
	RETURN_IF_ERROR(m_topdoc_url.Set(topdoc_url));
	RETURN_IF_ERROR(url_action.Set(in_url_action));
	return submitname.Set(in_submitname);
}
#endif // SELFTEST

OP_STATUS WandPage::SetUrl(FramesDocument* doc, HTML_Element* he_form)
{
	RETURN_IF_ERROR(GetWandUrl(doc, url));

	// Set action url
	if (he_form)
	{
		URL* action_url = he_form->GetUrlAttr(ATTR_ACTION, NS_IDX_HTML, doc->GetLogicalDocument());
		if (action_url)
		{
			unsigned short dummy_charset_id = 0; // not used since we convert to a *_Escaped string.
			RETURN_IF_ERROR(action_url->GetAttribute(URL::KUniName_Escaped, dummy_charset_id, url_action));
		}
		if (!url_action.IsEmpty())
		{
			MakeWandUrl(url_action);
			MakeWandServerUrl(url_action);
		}
	}

	// Set referrer url (top document's url)
	unsigned short dummy_charset_id = 0; // not used since we convert to a *_Escaped string.
	RETURN_IF_ERROR(doc->GetWindow()->GetCurrentURL().GetAttribute(URL::KUniName_With_Fragment_Escaped, dummy_charset_id, m_topdoc_url));

	if (!m_topdoc_url.IsEmpty())
	{
		MakeWandUrl(m_topdoc_url);
		MakeWandServerUrl(m_topdoc_url);

		// Only if it is a different domain from doc's url.
		OpString tmp;
		RETURN_IF_ERROR(tmp.Set(url));
		MakeWandUrl(tmp);
		MakeWandServerUrl(tmp);
		if (uni_stri_eq(tmp.CStr(), m_topdoc_url.CStr()))
			m_topdoc_url.Empty();
	}
	else
		m_topdoc_url.Empty();

	return OpStatus::OK;
}

void MakeWandServerUrl(OpString& str)
{
	// Will return "http://www.google.com" if str is "http://www.google.com/yadayada/yada/"
	INT32 firstpos = str.FindFirstOf('/');
	while(TRUE)
	{
		INT32 lastpos = str.FindLastOf('/');
		if (lastpos <= firstpos + 1)
			return;
		else
			str.Delete(lastpos);
	}
}

BOOL WandPage::IsMatching(FramesDocument* doc)
{
	// This is called _a lot_ so it has to be as fast as possible
	OpString doc_url;

	RETURN_VALUE_IF_ERROR(GetWandUrl(doc, doc_url), FALSE);
	if (GetUrl().CompareI(doc_url) != 0)
	{
		// Not the same, but maybe we should just match the server part
		if ((flags & WAND_FLAG_ON_THIS_SERVER) == 0)
		{
			// Nope, just match the ordinary url and that wasn't a match
			return FALSE;
		}

		// Compare server parts
		OpString server_url_this;
		OpString server_url_doc;
		if (OpStatus::IsError(server_url_this.Set(GetUrl())) ||
			OpStatus::IsError(server_url_doc.Set(doc_url)))
		{
			return FALSE;
		}
			
		MakeWandServerUrl(server_url_this);
		MakeWandServerUrl(server_url_doc);
		if (server_url_doc.CompareI(server_url_this) != 0)
		{
			return FALSE;
		}
	}

	// If we got here it was the same url but we need to check the
	// top most document as well to see if it's a phishing attempt
	// and in that case don't let the page match.
	FramesDocument* top_doc = doc->GetTopDocument();
	if (doc != top_doc && !doc->GetURL().SameServer(top_doc->GetURL()))
	{
		OpString topdoc_url;
		unsigned short dummy_charset_id = 0; // not used since we convert to a *_Escaped string.
		RETURN_VALUE_IF_ERROR(doc->GetWindow()->GetCurrentURL().GetAttribute(URL::KUniName_With_Fragment_Escaped, dummy_charset_id, topdoc_url), FALSE);

		MakeWandServerUrl(topdoc_url);
		if (topdoc_url.CompareI(m_topdoc_url) != 0)
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOL WandPage::IsMatching(WandPage* new_doc_page)
{
	// This will normally (in case of no frames) be two empty strings
	if (new_doc_page->m_topdoc_url.CompareI(m_topdoc_url))
		return FALSE;
	
	if (flags & WAND_FLAG_ON_THIS_SERVER)
	{
		// Only compare the server part
		OpString server_url_doc, server_url_this;
		RETURN_VALUE_IF_ERROR(server_url_doc.Set(new_doc_page->GetUrl()), FALSE);
		RETURN_VALUE_IF_ERROR(server_url_this.Set(GetUrl()), FALSE);

		MakeWandServerUrl(server_url_doc);
		MakeWandServerUrl(server_url_this);
		return server_url_doc.CompareI(server_url_this) == 0;
	}

	return GetUrl().CompareI(new_doc_page->GetUrl()) == 0;
}

BOOL WandPage::GetNeverOnThisPage()
{
	return (flags & WAND_FLAG_NEVER_ON_THIS_PAGE) != 0;
}

BOOL WandPage::GetOnThisServer()
{
	// FIXME: Rename "IsPolicyApplyingToWholeServer"
	return (flags & WAND_FLAG_ON_THIS_SERVER) != 0;
}

INT32 WandPage::CountPasswords()
{
	INT32 num_passwords = 0;
	INT32 num_items = objects.GetCount();
	for(int i = 0; i < num_items; i++)
		if (objects.Get(i)->is_password)
			num_passwords++;
	return num_passwords;
}

BOOL WandPage::IsSameFormAndUsername(WandPage* page, FramesDocument* doc)
{
	if (!uni_stri_eq(m_topdoc_url.CStr() ? m_topdoc_url.CStr() : UNI_L(""),
					page->m_topdoc_url.CStr() ? page->m_topdoc_url.CStr() : UNI_L("")))
		return FALSE;

	if (formnr != page->formnr)
		return FALSE;

	const uni_char* this_username;
	const uni_char* other_username;
	FindUserNameResult this_username_result = FindUserName(this_username, doc);
	FindUserNameResult other_username_result = page->FindUserName(other_username, doc);
	if (this_username_result != FIND_USER_OK ||
		other_username_result != FIND_USER_OK)
	{
		// Same error?
		return this_username_result == other_username_result;
	}

	// Compare usernames
	return this_username ? other_username && uni_str_eq(this_username, other_username) : !other_username;
}

BOOL WandPage::MightBeUserName(FramesDocument* doc, WandObjectInfo* object_info)
{
	OP_ASSERT(objects.Find(object_info) >= 0);

	if (object_info->is_password)
	{
		return FALSE;
	}

	if (!doc)
	{	
		// Can't really do any good checks without a document
		return TRUE; // Who knows
	}

	// This code is very slow worst case. Candidate for rewrite/redesign.
	// GetFormNr can be slow, FindMatch can be even slower, and potentially we
	// do them for every element in the tree.

	HTML_Element* he = doc->GetDocRoot();
	HTML_Element* he_form = NULL;

	while (he)
	{
		// Check that the names match before calling FindMatch to
		// avoid searching through all the objects in vain.
		const uni_char* name = he->GetStringAttr(ATTR_NAME);
		if (IsValidWandObject(he) && object_info->name.CompareI(name) == 0 &&
			formnr == (UINT32)he->GetFormNr())
			// Check by using the same method as the "fill in" code.
			// We can reuse he_form between calls since we check formnr before using it.
		{
			if (object_info == FindMatch(doc, he, he_form))
				// User names are written in text fields or email fields.
				return he->IsMatchingType(HE_INPUT, NS_HTML) && (
						he->GetInputType() == INPUT_EMAIL ||
						he->GetInputType() == INPUT_TEXT);
		}

		he = he->NextActual();
	}
	// Didn't even find the element on the page so it's not a username field.
	return FALSE;
}

WandObjectInfo* WandPage::FindAndMarkUserNameFieldCandidate(FramesDocument* doc /*= NULL*/)
{
	INT32 num_items = objects.GetCount();
	WandObjectInfo* candidate = NULL;
	int candidate_points = 0;

	// Find the field most likely to be the username.
	// Give each object points and the one with the highest
	// point wins. If two elements have the same point,
	// the first of the two wins.

	// 0x4 point for textfield
	// 0x2 points for is_changed
	// 0x1 for first element
	for (INT32 i = 0; i < num_items; i++)
	{
		WandObjectInfo* object = objects.Get(i);
		object->is_guessed_username = FALSE;
		int points = 0;
		if (object->is_textfield_for_sure)
			points += 4;
		if (object->is_changed)
			points += 2;
		if (candidate == NULL)
			points += 1;
		if (points > candidate_points && MightBeUserName(doc, object)) // MightBeUserName filters out things but is very slow.
		{
			if (candidate)
				candidate->is_guessed_username = FALSE;
			object->is_guessed_username = TRUE;

			candidate = object;
			candidate_points = points;

			// When a candidate has 6 or 7 points it can't be beaten so end the search.
			if (points > 5)
				break;
		}
	}

	return candidate;
}

WandPage::FindUserNameResult WandPage::FindUserName(const uni_char*& user_name, FramesDocument* doc /*= NULL*/)
{
	user_name = NULL;
	if (GetNeverOnThisPage())
	{
		if (GetOnThisServer())
			return FIND_USER_ERR_NOTHING_STORED_ON_SERVER;
		return FIND_USER_ERR_NOTHING_STORED_ON_PAGE;
	}

	WandObjectInfo* candidate = FindAndMarkUserNameFieldCandidate(doc);
	
	if (!candidate)
		return FIND_USER_ERR_NO_USERNAME;

	user_name = candidate->value.CStr();
	return FIND_USER_OK;
}

// Deprecated. Contains hardcoded strings that should be in the UI, if anywhere at all
const uni_char* WandPage::FindUserName()
{
	const uni_char* user_name;
	FindUserNameResult res = FindUserName(user_name);
	switch (res)
	{
	case FIND_USER_ERR_NOTHING_STORED_ON_SERVER:
		return UNI_L("<never on entire server>");
	case FIND_USER_ERR_NOTHING_STORED_ON_PAGE:
		return UNI_L("<never on this page>");
	case FIND_USER_ERR_NO_USERNAME:
		return UNI_L("<no username found>");
	default:
		OP_ASSERT(!"Not reachable");
		/* fall through */
	case FIND_USER_OK:
		return user_name;
	}
}

WandPassword* WandPage::FindPassword()
{
	INT32 num_items = objects.GetCount();
	for(int i = 0; i < num_items; i++)
		if (objects.Get(i)->is_password)
			return &objects.Get(i)->password;
	return NULL;
}

void WandPage::SetUserChoiceFlags(UINT32 new_flags)
{
	flags |= new_flags;
}

OP_STATUS WandPage::Encrypt()
{
	INT32 num_items = objects.GetCount();
	for(int i = 0; i < num_items; i++)
	{
		if (objects.Get(i)->is_password)
		{
			RETURN_IF_ERROR(objects.Get(i)->password.Encrypt());
		}
	}
	return OpStatus::OK;
}

#ifdef WAND_ECOMMERCE_SUPPORT

BOOL WandPage::HasECommerceMatch(HTML_Element* he)
{
	const uni_char* name = he->GetStringAttr(ATTR_NAME);
	if (name == NULL || !IsValidWandObject(he))
		return FALSE;

	// First check if we have previously stored ecommerce-data
	HTML_Element* he_form = NULL;
	if (FindMatch(NULL, he, he_form))
		return TRUE;

	// Then check if it can be imported from prefs personal information.
	if (WandManager::CanImportValueFromPrefs(he, name))
		return TRUE;

	return FALSE;
}

BOOL WandPage::HasChangedECommerceData()
{
/*	INT32 num_infos = objects.GetCount();
	for (int i = 0; i < num_infos; i++)
	{
		WandObjectInfo* info = objects.Get(i);
		if (WandManager::IsECommerceName(info->name))
		{
			OpString current_val;
			OP_STATUS status = GetStoredECommerceValue(info->name, current_val);
			if ((OpStatus::IsSuccess(status) && info->value.Compare(current_val) != 0) ||
				(status == OpStatus::ERR && !info->value.IsEmpty())) // No value yet
			{
				return TRUE;
			}
		}
	}*/
	return FALSE;
}

// Slow
OP_STATUS WandPage::GetStoredECommerceValue(const OpString& name, OpString& out_val)
{
	WandPage* personal_profile = g_wand_manager->GetProfile(0)->GetWandPage(0);
	int stored_obj_count = personal_profile->CountObjects();
	for (int i = 0; i < stored_obj_count; i++)
	{
		WandObjectInfo* stored = personal_profile->GetObject(i);
		if (name.Compare(stored->name) == 0)
		{
			return out_val.Set(stored->value);
		}
	}

	return OpStatus::ERR;
}

#endif // WAND_ECOMMERCE_SUPPORT

HTML_Element* WandPage::FindTargetSubmit(FramesDocument* doc)
{
	HTML_Element* form = FindTargetForm(doc);
	if (form)
	{
		FormIterator form_iterator(doc, form);
		while (HTML_Element* he = form_iterator.GetNext())
		{
			if (he->GetNsType() == NS_HTML &&
				((he->Type() == HE_INPUT && he->GetInputType() == INPUT_SUBMIT) || 
				 (he->Type() == HE_INPUT && he->GetInputType() == INPUT_IMAGE) ||
				 (he->Type() == HE_BUTTON && he->GetInputType() == INPUT_SUBMIT)) &&
				submitname.Compare(he->GetName()) == 0)
				return he;
		}
	}
	return NULL;
}

HTML_Element* WandPage::FindTargetForm(FramesDocument* doc)
{
	HTML_Element *he = NULL;
	
	if (doc->GetLogicalDocument())
		he = doc->GetLogicalDocument()->GetRoot();
		
	while (he)
	{
		if (he->Type() == HE_FORM && formnr == (UINT32) he->GetFormNr())
			return he;
		he = (HTML_Element *) he->Next();
	}
	return NULL;
}

#endif // WAND_SUPPORT
