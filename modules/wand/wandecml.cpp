/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/wand/wandmanager.h"

#include "modules/forms/formvalue.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#ifdef WAND_SUPPORT
#ifdef WAND_ECOMMERCE_SUPPORT

#define PREFS_NAMESPACE PrefsCollectionCore

const WandModule::ECOM_ITEM* GetECommerceItem(HTML_Element* he, const uni_char* name)
{
	const uni_char* common_prefix = UNI_L("Ecom_");
	const int common_prefix_len = 5;

	// Can be smarter and more efficient
	if (name && uni_strncmp(name, common_prefix, common_prefix_len) == 0)
	{
		const int item_count = ECOM_ITEMS_LEN;
		const WandModule::ECOM_ITEM* eCom_items = g_opera->wand_module.m_eCom_items;

		for (int i = 0; i < item_count; i++)
			if (uni_str_eq(name+common_prefix_len, eCom_items[i].field_name))
				return &eCom_items[i];
	}

#ifdef WAND_GUESS_COMMON_FORMFIELD_MEANING
	const WandModule::ECOM_ITEM* common_items = g_opera->wand_module.m_common_items;
	if (name && he && he->Type() == HE_INPUT && he->GetInputType() == INPUT_TEXT)
	{
		const int item_count = ECOM_COMMON_ITEMS_LEN;

		for (int i = 0; i < item_count; i++)
			if (uni_stristr(name, common_items[i].field_name))
				return &common_items[i];
	}
	if (he && he->Type() == HE_INPUT && he->GetInputType() == INPUT_EMAIL)
	{
		const int index_of_email_entry = 0; // See table above
		OP_ASSERT(common_items[index_of_email_entry].pref_id == PREFS_NAMESPACE::EMail);
		return common_items + index_of_email_entry;
	}
#endif // WAND_GUESS_COMMON_FORMFIELD_MEANING

	return NULL;
}

/* static */
BOOL WandManager::IsECommerceName(const uni_char* field_name)
{
	return GetECommerceItem(NULL, field_name) ? TRUE : FALSE;
}

BOOL WandManager::CanImportValueFromPrefs(HTML_Element* he, const uni_char* field_name)
{
	FormValue* form_value = he->GetFormValue();
	if (form_value && form_value->IsChangedFromOriginal(he))
		// Ignore changed formfields.
		return FALSE;

	const WandModule::ECOM_ITEM* item = GetECommerceItem(he, field_name);
	if (item && item->pref_id != 0)
		if (g_pccore->GetStringPref((PREFS_NAMESPACE::stringpref)item->pref_id).Length())
			return TRUE;
	return FALSE;
}

/* static */
OP_STATUS WandManager::ImportValueFromPrefs(HTML_Element* he, const uni_char* field_name, OpString& str)
{
	const WandModule::ECOM_ITEM* item = GetECommerceItem(he, field_name);
	OP_ASSERT(item && item->pref_id != 0);
	return str.Set(g_pccore->GetStringPref((PREFS_NAMESPACE::stringpref)item->pref_id));
}

#endif // WAND_ECOMMERCE_SUPPORT
#endif // WAND_SUPPORT
