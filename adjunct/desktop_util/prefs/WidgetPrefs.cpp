/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_util/prefs/WidgetPrefs.h"

#include "modules/widgets/OpWidget.h"

namespace WidgetPrefs
{
	// template function implementations
	template<class Collection, class PrefType>
	OP_STATUS GetIntegerPref(OpWidget* widget, Collection* collection, PrefType preference)
	{
		if (widget)
			widget->SetValue(collection->GetIntegerPref(preference));
		return OpStatus::OK;
	}

	template<class Collection, class PrefType>
	OP_STATUS GetIntegerPref(OpWidget* widget, Collection* collection, PrefType preference, const uni_char* host)
	{
		if (!host || !*host)
			return GetIntegerPref(widget, collection, preference);
		else if (widget)
			widget->SetValue(collection->GetIntegerPref(preference, host));
		return OpStatus::OK;
	}

	template<class Collection, class PrefType>
	OP_STATUS SetIntegerPref(OpWidget* widget, Collection* collection, PrefType preference)
	{
		if (widget)
		{
			int result = widget->GetValue();
			int original = collection->GetIntegerPref(preference);
			if (result != original)
				RETURN_IF_LEAVE(collection->WriteIntegerL(preference, result));
		}
		return OpStatus::OK;
	}

	template<class Collection, class PrefType>
	OP_STATUS SetIntegerPref(OpWidget* widget, Collection* collection, PrefType preference, const uni_char* host)
	{
		if (!host || !*host)
			SetIntegerPref(widget, collection, preference);
		else if (widget)
		{
			int result = widget->GetValue();
			int original = collection->GetIntegerPref(preference, host);
			if (result != original)
				RETURN_IF_LEAVE(collection->OverridePrefL(host, preference, result, TRUE));
		}
		return OpStatus::OK;
	}

	template<class Collection, class PrefType>
	OP_STATUS GetStringPref(OpWidget* widget, Collection* collection, PrefType preference)
	{
		if (widget)
			widget->SetText(collection->GetStringPref(preference).CStr());
		return OpStatus::OK;
	}

	template<class Collection, class PrefType>
	OP_STATUS GetStringPref(OpWidget* widget, Collection* collection, PrefType preference, const uni_char* host)
	{
		if (!host || !*host)
			return GetStringPref(widget, collection, preference);
		else if (widget)
			widget->SetText(collection->GetStringPref(preference, host).CStr());
		return OpStatus::OK;
	}

	template<class Collection, class PrefType>
	OP_STATUS SetStringPref(OpWidget* widget, Collection* collection, PrefType preference)
	{
		if (widget)
		{
			const OpStringC original = collection->GetStringPref(preference);
			OpString result;
			RETURN_IF_ERROR(widget->GetText(result));
			if (result.Compare(original) != 0)
				RETURN_IF_LEAVE(collection->WriteStringL(preference, result));
		}
		return OpStatus::OK;
	}

	template<class Collection, class PrefType>
	OP_STATUS SetStringPref(OpWidget* widget, Collection* collection, PrefType preference, const uni_char* host)
	{
		if (!host || !*host)
			return SetStringPref(widget, collection, preference);
		else if (widget)
		{
			const OpStringC original = collection->GetStringPref(preference, host);
			OpString result;
			RETURN_IF_ERROR(widget->GetText(result));
			if (result.Compare(original) != 0)
				RETURN_IF_LEAVE(collection->OverridePrefL(host, preference, result, TRUE));
		}
		return OpStatus::OK;
	}

	// Overloaded functions for all preference types
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionApp::integerpref preference) { return GetIntegerPref(widget, g_pcapp, preference); }
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionCore::integerpref preference, const uni_char *host) { return GetIntegerPref(widget, g_pccore, preference, host); }
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionDisplay::integerpref preference, const uni_char *host) { return GetIntegerPref(widget, g_pcdisplay, preference, host); }
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionDoc::integerpref preference, const uni_char *host) { return GetIntegerPref(widget, g_pcdoc, preference, host); }
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionJS::integerpref preference, const uni_char *host) { return GetIntegerPref(widget, g_pcjs, preference, host); }
#ifdef M2_SUPPORT
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionM2::integerpref preference)  { return GetIntegerPref(widget, g_pcm2, preference); }
#endif //M2_SUPPORT
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionNetwork::integerpref preference, const uni_char *host) { return GetIntegerPref(widget, g_pcnet, preference, host); }
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionUI::integerpref preference, const uni_char *host) { return GetIntegerPref(widget, g_pcui, preference, host); }
#ifdef _PRINT_SUPPORT_
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionPrint::integerpref preference, const uni_char *host) { return GetIntegerPref(widget, g_pcprint, preference, host); }
#endif // _PRINT_SUPPORT_

	OP_STATUS GetStringPref(OpWidget* widget, PrefsCollectionApp::stringpref preference) { return GetStringPref(widget, g_pcapp, preference); }
	OP_STATUS GetStringPref(OpWidget* widget, PrefsCollectionCore::stringpref preference, const uni_char *host) { return GetStringPref(widget, g_pccore, preference, host); }
	OP_STATUS GetStringPref(OpWidget* widget, PrefsCollectionJS::stringpref preference, const uni_char *host) { return GetStringPref(widget, g_pcjs, preference, host); }
	OP_STATUS GetStringPref(OpWidget* widget, PrefsCollectionUI::stringpref preference, const uni_char *host) { return GetStringPref(widget, g_pcui, preference, host); }
#ifdef _PRINT_SUPPORT_
	OP_STATUS GetStringPref(OpWidget* widget, PrefsCollectionPrint::stringpref preference, const uni_char *host) { return GetStringPref(widget, g_pcprint, preference, host); }
#endif // _PRINT_SUPPORT_

	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionApp::integerpref preference) { return SetIntegerPref(widget, g_pcapp, preference); }
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionCore::integerpref preference, const uni_char *host) { return SetIntegerPref(widget, g_pccore, preference, host); }
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionDisplay::integerpref preference, const uni_char *host) { return SetIntegerPref(widget, g_pcdisplay, preference, host); }
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionDoc::integerpref preference, const uni_char *host) { return SetIntegerPref(widget, g_pcdoc, preference, host); }
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionJS::integerpref preference, const uni_char *host) { return SetIntegerPref(widget, g_pcjs, preference, host); }
#ifdef M2_SUPPORT
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionM2::integerpref preference) { return SetIntegerPref(widget, g_pcm2, preference); }
#endif //M2_SUPPORT
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionNetwork::integerpref preference, const uni_char *host) { return SetIntegerPref(widget, g_pcnet, preference, host); }
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionUI::integerpref preference, const uni_char *host )  { return SetIntegerPref(widget, g_pcui, preference, host); }
#ifdef _PRINT_SUPPORT_
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionPrint::integerpref preference, const uni_char *host) { return SetIntegerPref(widget, g_pcprint, preference, host); }
#endif // _PRINT_SUPPORT_

	OP_STATUS SetStringPref(OpWidget* widget, PrefsCollectionApp::stringpref preference) { return SetStringPref(widget, g_pcapp, preference); }
	OP_STATUS SetStringPref(OpWidget* widget, PrefsCollectionCore::stringpref preference, const uni_char *host) { return SetStringPref(widget, g_pccore, preference, host); }
	OP_STATUS SetStringPref(OpWidget* widget, PrefsCollectionJS::stringpref preference, const uni_char *host) { return SetStringPref(widget, g_pcjs, preference, host); }
	OP_STATUS SetStringPref(OpWidget* widget, PrefsCollectionUI::stringpref preference, const uni_char *host) { return SetStringPref(widget, g_pcui, preference, host); }
	OP_STATUS SetStringPref(OpWidget* widget, PrefsCollectionNetwork::stringpref preference, const uni_char *host) { return SetStringPref(widget, g_pcnet, preference, host); }
#ifdef _PRINT_SUPPORT_
	OP_STATUS SetStringPref(OpWidget* widget, PrefsCollectionPrint::stringpref preference, const uni_char *host) { return SetStringPref(widget, g_pcprint, preference, host); }
#endif // _PRINT_SUPPORT_
#ifdef DOM_GEOLOCATION_SUPPORT
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionGeolocation::integerpref preference) { return GetIntegerPref(widget, g_pcgeolocation, preference); }
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionGeolocation::integerpref preference) { return SetIntegerPref(widget, g_pcgeolocation, preference); }
#endif // DOM_GEOLOCATION_SUPPORT
}
