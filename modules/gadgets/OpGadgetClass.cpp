/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef GADGET_SUPPORT

#include "modules/gadgets/OpGadgetClass.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/gadgets/OpGadgetUpdate.h"
#include "modules/gadgets/OpGadgetParsers.h"
#include "modules/gadgets/gadget_utils.h"
#include "modules/gadgets/gadgets_module.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/util/opstring.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opstrlst.h"
#include "modules/util/zipload.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpPainter.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/security_manager/src/security_gadget_representation.h"
#include "modules/viewers/viewers.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/url/tools/content_detector.h"

#ifdef DOM_JIL_API_SUPPORT
#include "modules/device_api/jil/JILFeatures.h"
#endif // DOM_JIL_API_SUPPORT

#ifdef SIGNED_GADGET_SUPPORT
#include "modules/libcrypto/include/GadgetSignatureStorage.h"
#include "modules/libcrypto/include/GadgetSignatureVerifier.h"
#endif // SIGNED_GADGET_SUPPORT

#define GADGETS_BIDI_PDE	0x202c

/* Default start files, as per Widgets 1.0: Packaging and Configuration */
CONST_ARRAY(g_gadget_start_file, char*)
	CONST_ENTRY("index.htm"),
	CONST_ENTRY("index.html"),
	CONST_ENTRY("index.svg"),
	CONST_ENTRY("index.xhtml"),
	CONST_ENTRY("index.xht")
CONST_END(g_gadget_start_file)

/* MIME type for the start files. */
CONST_ARRAY(g_gadget_start_file_type, char*)
	CONST_ENTRY("text/html"),
	CONST_ENTRY("text/html"),
	CONST_ENTRY("image/svg+xml"),
	CONST_ENTRY("application/xhtml+xml"),
	CONST_ENTRY("application/xhtml+xml")
CONST_END(g_gadget_start_file_type)

/* Default icons, as per Widgets 1.0: Packaging and Configuration */
CONST_ARRAY(g_gadget_default_icon, char*)
	CONST_ENTRY("icon.svg"),
	CONST_ENTRY("icon.ico"),
	CONST_ENTRY("icon.png"),
	CONST_ENTRY("icon.gif"),
	CONST_ENTRY("icon.jpg")
CONST_END(g_gadget_default_icon)

/* MIME type for the default icons. */
/*
CONST_ARRAY(g_gadget_default_icon_type, char*)
	CONST_ENTRY("image/svg+xml"),
	CONST_ENTRY("image/vnd.microsoft.icon"),
	CONST_ENTRY("image/png"),
	CONST_ENTRY("image/gif"),
	CONST_ENTRY("image/jpeg"),
CONST_END(g_gadget_default_icon_type)
*/

/* static */ OP_STATUS
OpGadgetFeatureParam::Make(OpGadgetFeatureParam **new_param, const uni_char *name, const uni_char *value)
{
	OP_ASSERT(new_param);
	OP_ASSERT(name && *name);

	OpAutoPtr<OpGadgetFeatureParam> param(OP_NEW(OpGadgetFeatureParam, ()));
	RETURN_OOM_IF_NULL(param.get());
	RETURN_IF_ERROR(param->m_name.Set(name));
	RETURN_IF_ERROR(param->m_value.Set(value));
	*new_param = param.release();
	return OpStatus::OK;
}

/* static */ OP_STATUS
OpGadgetFeature::Make(OpGadgetFeature **new_feature, const uni_char *uri, BOOL required)
{
	OP_ASSERT(new_feature);
	OP_ASSERT(uri && *uri);

	OpAutoPtr<OpGadgetFeature> feature(OP_NEW(OpGadgetFeature, (required)));
	RETURN_OOM_IF_NULL(feature.get());
	RETURN_IF_ERROR(feature->m_uri.Set(uri));
	*new_feature = feature.release();
	return OpStatus::OK;
}

void
OpGadgetFeature::AddParam(OpGadgetFeatureParam *param)
{
	OP_ASSERT(param);

	param->Into(&m_params);
}

/* static */ OP_STATUS
OpGadgetIcon::Make(OpGadgetIcon **new_icon, const uni_char *src, INT32 width, INT32 height)
{
	OP_ASSERT(new_icon);
	OP_ASSERT(src && *src);

	OpAutoPtr<OpGadgetIcon> icon(OP_NEW(OpGadgetIcon, (width, height)));
	RETURN_OOM_IF_NULL(icon.get());
	RETURN_IF_ERROR(icon->m_src.Set(src));
	*new_icon = icon.release();
	return OpStatus::OK;
}

/* static */ OP_STATUS
OpGadgetPreference::Make(OpGadgetPreference **new_pref, const uni_char *name, const uni_char *value, BOOL readonly)
{
	OP_ASSERT(new_pref);
	OP_ASSERT(name && *name);

	OpAutoPtr<OpGadgetPreference> pref(OP_NEW(OpGadgetPreference, ()));
	RETURN_OOM_IF_NULL(pref.get());
	pref->m_readonly = readonly;
	RETURN_IF_ERROR(pref->m_name.Set(name));
	RETURN_IF_ERROR(pref->m_value.Set(value));
	*new_pref = pref.release();
	return OpStatus::OK;
}

/* static */ OP_STATUS
OpGadgetAccess::Make(OpGadgetAccess **new_access, const uni_char *origin, BOOL subdomains)
{
	OP_ASSERT(new_access);
	OP_ASSERT(origin && *origin);

	OpAutoPtr<OpGadgetAccess> access(OP_NEW(OpGadgetAccess, (subdomains)));
	RETURN_OOM_IF_NULL(access.get());
	RETURN_IF_ERROR(access->m_origin.Set(origin));
	*new_access = access.release();
	return OpStatus::OK;
}


OpGadgetClass::OpGadgetClass()
	: m_is_updating(FALSE)
	, m_persistent(FALSE)
	, m_is_enabled(TRUE)
	, m_has_locales_folder(NO) // BOOL3; see header
	, m_icons_checked(FALSE)
	, m_sign_state(GADGET_SIGNATURE_UNSIGNED)
#ifdef GADGET_PRIVILEGED_SIGNING_SUPPORT
	, m_is_signed_with_privileged_cert(FALSE)
#endif // GADGET_PRIVILEGED_SIGNING_SUPPORT
	, m_update_info(NULL)
	, m_security_policy(NULL)
#ifdef SELFTEST
	, m_global_sec_policy(NULL)
#endif // SELFTEST
	, m_namespace(GADGETNS_UNKNOWN)
	, m_default_bidi_marker(0)
#ifdef SIGNED_GADGET_SUPPORT
	, m_signature_storage(NULL)
	, m_signature_verifier(NULL)
#endif // SIGNED_GADGET_SUPPORT
{
	ClearSetAttributes();

	for (int i = 0; i < LAST_INTEGER_ATTRIBUTE; i++)
		m_integer_attributes[i] = 0;
}

OP_GADGET_STATUS
OpGadgetClass::Construct(const OpStringC& gadget_path, URLContentType type)
{
	// Make sure the Content-Type is valid
	switch (type)
	{
	case URL_GADGET_INSTALL_CONTENT:
#ifdef WEBSERVER_SUPPORT
	case URL_UNITESERVICE_INSTALL_CONTENT:
#endif
#ifdef EXTENSION_SUPPORT
	case URL_EXTENSION_INSTALL_CONTENT:
#endif
		m_content_type = type;
		break;

	default:
		return OpGadgetStatus::ERR_GADGET_TYPE_NOT_SUPPORTED;
	}

	return m_gadget_path.Set(gadget_path);
}

OpGadgetClass::~OpGadgetClass()
{
	Out();

	g_gadget_manager->CancelUpdate(this);
#ifdef SIGNED_GADGET_SUPPORT
	g_main_message_handler->UnsetCallBack(this, MSG_GADGET_SIGNATURE_VERIFICATION_FINISHED);
	OP_DELETE(m_signature_verifier);
	OP_DELETE(m_signature_storage);
#endif // SIGNED_GADGET_SUPPORT
	OP_DELETE(m_update_info);

	Clear();	// Clear will be called by Reload as well.
}

void
OpGadgetClass::Clear()
{
	m_icons.Clear();
	m_features.Clear();
	m_access.Clear();
	m_default_preferences.Clear();

	OP_DELETE(m_security_policy); m_security_policy = NULL;
#ifdef SELFTEST
	OP_DELETE(m_global_sec_policy); m_global_sec_policy = NULL;
#endif // SELFTEST

	ClearSetAttributes();

	UINT32 i;
	for (i = 0; i < LAST_INTEGER_ATTRIBUTE; i++)
		m_integer_attributes[i] = 0;
	for (i = 0; i < LAST_STRING_ATTRIBUTE; i++)
		m_string_attributes[i].Empty();

	m_default_bidi_marker = 0;
	m_icons_checked = FALSE;
}

#define WIDGET_DEFAULT_START() for (UINT32 i = 0; i < LAST_INTEGER_ATTRIBUTE; i++) { switch (i) {
#define WIDGET_DEFAULT(i, v) case i: if (!(m_integer_attributes_set & (1 << (UINT32)i))) { m_integer_attributes[i] = v; } break;
#define WIDGET_DEFAULT_END() default: if (!(m_integer_attributes_set & (1 << (UINT32)i))) { m_integer_attributes[i] = 0; } } }

OP_GADGET_STATUS
OpGadgetClass::ApplyDefaults()
{
	// Will set default values to attribues which are unset
	// Unset values which aren't set here, will be set to 0
	// Note that the 'set' flag will still be unset.

	WIDGET_DEFAULT_START()
		WIDGET_DEFAULT(WIDGET_ATTR_WIDTH, GADGETS_DEFAULT_WIDTH)
		WIDGET_DEFAULT(WIDGET_ATTR_HEIGHT, GADGETS_DEFAULT_HEIGHT)
		WIDGET_DEFAULT(WIDGET_ATTR_MODE, WINDOW_VIEW_MODE_UNKNOWN)
		WIDGET_DEFAULT(WIDGET_ATTR_DEFAULT_MODE, WINDOW_VIEW_MODE_FLOATING)
		WIDGET_DEFAULT(WIDGET_ATTR_TRANSPARENT, TRUE)
		WIDGET_DEFAULT(WIDGET_ATTR_DOCKABLE, TRUE)
		WIDGET_DEFAULT(WIDGET_ACCESS_ATTR_NETWORK, GADGET_NETWORK_NONE)
		WIDGET_DEFAULT(WIDGET_ACCESS_ATTR_FILES, FALSE)
		WIDGET_DEFAULT(WIDGET_ACCESS_ATTR_PLUGINS, FALSE)
		WIDGET_DEFAULT(WIDGET_ACCESS_ATTR_JSPLUGINS, FALSE)
		WIDGET_DEFAULT(WIDGET_ACCESS_ATTR_WEBSERVER, FALSE)
		WIDGET_DEFAULT(WIDGET_ACCESS_ATTR_URLFILTER, FALSE)
		WIDGET_DEFAULT(WIDGET_ACCESS_ATTR_GEOLOCATION, FALSE)
		WIDGET_DEFAULT(WIDGET_WIDGETTYPE, GADGET_TYPE_CHROMELESS)
		WIDGET_DEFAULT(WIDGET_WEBSERVER_ATTR_TYPE, GADGET_WEBSERVER_TYPE_APPLICATION)
		WIDGET_DEFAULT(WIDGET_EXTENSION_SPEEDDIAL_CAPABLE, FALSE)
		WIDGET_DEFAULT(WIDGET_EXTENSION_SHARE_URL_CONTEXT, FALSE);
		WIDGET_DEFAULT(WIDGET_EXTENSION_SCREENSHOT_SUPPORT, FALSE);
		WIDGET_DEFAULT(WIDGET_EXTENSION_ALLOW_CONTEXTMENUS, FALSE);
	WIDGET_DEFAULT_END()

	RETURN_IF_ERROR(SetAttribute(WIDGET_CONTENT_ATTR_CHARSET, UNI_L("UTF-8")));
	RETURN_IF_ERROR(SetAttribute(WIDGET_EXTENSION_SPEEDDIAL_TITLE, GetAttribute(WIDGET_NAME_TEXT)));

	return OpStatus::OK;
}

UINT32
OpGadgetClass::GetAttribute(GadgetIntegerAttribute attr, BOOL* set)
{
	OP_ASSERT(attr >= 0 && attr < LAST_INTEGER_ATTRIBUTE);

	if (set)
		*set = (m_integer_attributes_set & (1 << (UINT32)attr)) != 0;

	return m_integer_attributes[attr];
}

OP_STATUS
OpGadgetClass::SetAttribute(GadgetIntegerAttribute attr, UINT32 value, BOOL override)
{
	OP_ASSERT(attr >= 0 && attr < LAST_INTEGER_ATTRIBUTE);

	if (!override && (m_integer_attributes_set & (1 << (UINT32)attr)))
		return OpStatus::OK;
	m_integer_attributes_set |=  1 << (UINT32)attr;

	m_integer_attributes[attr] = value;

	return OpStatus::OK;	// Return OP_STATUS to allow future expansion
}

const uni_char *
OpGadgetClass::GetAttribute(GadgetStringAttribute attr, BOOL* set) const
{
	OP_ASSERT(attr >= 0 && attr < LAST_STRING_ATTRIBUTE);

	if (set)
		*set = (m_string_attributes_set & (1 << (UINT32)attr)) != 0;

	return m_string_attributes[attr].CStr();
}

OP_STATUS
OpGadgetClass::SetAttribute(GadgetStringAttribute attr, const uni_char* value, BOOL override)
{
	OP_ASSERT(attr >= 0 && attr < LAST_STRING_ATTRIBUTE);

	if (!override && (m_string_attributes_set & (1 << (UINT32)attr)))
		return OpStatus::OK;
	m_string_attributes_set |=  1 << (UINT32)attr;

	return m_string_attributes[attr].Set(value);
}

const uni_char *
OpGadgetClass::GetLocaleForAttribute(GadgetLocalizedAttribute attr, BOOL* set)
{
	OP_ASSERT(attr >= 0 && attr < LAST_LOCALIZED_ATTRIBUTE);

	BOOL is_set = (m_localized_attributes_set & (1 << (UINT32)attr)) != 0;
	if (set)
		*set = is_set;
	if (is_set)
		return m_localized_attributes[attr].CStr();
	else
		return GetAttribute(WIDGET_DEFAULT_LOCALE);
}

OP_STATUS
OpGadgetClass::SetLocaleForAttribute(GadgetLocalizedAttribute attr, const uni_char* value)
{
	OP_ASSERT(attr >= 0 && attr < LAST_LOCALIZED_ATTRIBUTE);

	m_localized_attributes_set |=  1 << (UINT32)attr;

	return m_localized_attributes[attr].Set(value);
}

//////////////////////////////////////////////////////////////////////////
// Attribute functions that operate on XMLFragment data
//////////////////////////////////////////////////////////////////////////

OP_STATUS
OpGadgetClass::SetAttribute(GadgetStringAttribute attr, XMLFragment *f, BOOL override)
{
	OP_ASSERT(attr >= 0 && attr < LAST_STRING_ATTRIBUTE);

	if (!override && (m_string_attributes_set & (1 << (UINT32)attr)))
		return OpStatus::OK;
	m_string_attributes_set |=  1 << (UINT32)attr;

	TempBuffer result;
	RETURN_IF_ERROR(OpGadgetUtils::GetTextNormalized(f, &result, m_default_bidi_marker));

	return m_string_attributes[attr].Set(result.GetStorage());
}

OP_STATUS
OpGadgetClass::SetAttribute(GadgetStringAttribute attr, XMLFragment *f, const uni_char *attr_str_arg, BOOL override, uni_char bidi_marker)
{
	OP_ASSERT(attr >= 0 && attr < LAST_STRING_ATTRIBUTE);

	if (!override && (m_string_attributes_set & (1 << (UINT32)attr)))
		return OpStatus::OK;

	const uni_char *attr_str = f->GetAttribute(attr_str_arg);
	if (!attr_str)
		return OpStatus::OK;

	TempBuffer result;
	RETURN_IF_ERROR(OpGadgetUtils::NormalizeText(attr_str, &result, bidi_marker ? bidi_marker : m_default_bidi_marker));

	m_string_attributes_set |=  1 << (UINT32)attr;
	return m_string_attributes[attr].Set(result.GetStorage());
}

OP_STATUS
OpGadgetClass::SetAttribute(GadgetIntegerAttribute attr, XMLFragment *f, const uni_char *attr_str_arg, BOOL override)
{
	OP_ASSERT(attr >= 0 && attr < LAST_INTEGER_ATTRIBUTE);

	if (!override && (m_integer_attributes_set & (1 << (UINT32)attr)))
		return OpStatus::OK;

	const uni_char *attr_str = f->GetAttribute(attr_str_arg);
	if (!attr_str)
		return OpStatus::OK;

	UINT32 result;
	OP_STATUS success;
	OpGadgetUtils::AttrStringToI(attr_str, result, success);
	if (OpStatus::IsSuccess(success))
	{
		m_integer_attributes_set |=  1 << (UINT32)attr;
		m_integer_attributes[attr] = result;
	}

	return OpStatus::OK;
}

OP_STATUS
OpGadgetClass::SetAttributeBoolean(GadgetIntegerAttribute attr, XMLFragment* f, const uni_char *attr_str_arg, BOOL dval, BOOL override)
{
	OP_ASSERT(attr >= 0 && attr < LAST_INTEGER_ATTRIBUTE);

	if (!override && (m_integer_attributes_set & (1 << (UINT32)attr)))
		return OpStatus::OK;

	const uni_char *attr_str = f->GetAttribute(attr_str_arg);
	if (!attr_str)
		return OpStatus::OK;

	TempBuffer result;
	RETURN_IF_ERROR(OpGadgetUtils::NormalizeText(attr_str, &result));

	m_integer_attributes_set |=  1 << (UINT32)attr;
	m_integer_attributes[attr] = OpGadgetUtils::IsAttrStringTrue(result.GetStorage(), dval);

	return OpStatus::OK;
}

OP_STATUS
OpGadgetClass::SetAttributeElementString(GadgetStringAttribute attr, XMLFragment *f, BOOL override)
{
	OP_ASSERT(attr >= 0 && attr < LAST_STRING_ATTRIBUTE);

	if (!override && (m_string_attributes_set & (1 << (UINT32)attr)))
		return OpStatus::OK;

	TempBuffer result;
	RETURN_IF_ERROR(f->GetAllText(result));

	m_string_attributes_set |=  1 << (UINT32)attr;
	return m_string_attributes[attr].Set(result.GetStorage());
}

uni_char
OpGadgetClass::GetBIDIMarker(const uni_char *dir)
{
	if (dir && uni_strcmp(dir, UNI_L("rtl")) == 0)
		return 0x202b; // LRE
	else if (dir && uni_strcmp(dir, UNI_L("ltr")) == 0)
		return 0x202a; // RLE
	else if (dir && uni_strcmp(dir, UNI_L("lro")) == 0)
		return 0x202d; // LRO
	else if (dir && uni_strcmp(dir, UNI_L("rlo")) == 0)
		return 0x202e; // RLO
	else
		return 0;
}

OP_STATUS
OpGadgetClass::CollectBIDIString(TempBuffer &buffer, XMLFragment *f, uni_char bidi_marker)
{
	BOOL pushed = FALSE;
	const uni_char *dir = f->GetAttribute(UNI_L("dir"));
	uni_char marker = GetBIDIMarker(dir);
	if (!marker)
		marker = bidi_marker;
	if (marker)
	{
		RETURN_IF_ERROR(buffer.Append(marker));
		pushed = TRUE;
	}

	while (f->HasMoreContent())
	{
		if (f->GetNextContentType() == XMLFragment::CONTENT_TEXT)
		{
			RETURN_IF_ERROR(buffer.Append(f->GetText()));
			f->SkipContent();
		}
		else
		{
			if (f->EnterAnyElement())
			{
				// process this element
				RETURN_IF_ERROR(CollectBIDIString(buffer, f));
				f->LeaveElement();
			}
		}
	}

	if (pushed)
		RETURN_IF_ERROR(buffer.Append(GADGETS_BIDI_PDE));		// PDF, pop

	return OpStatus::OK;
}

OP_STATUS
OpGadgetClass::SetAttributeElementStringBIDI(GadgetStringAttribute attr, XMLFragment *f, BOOL normalize, BOOL override)
{
	OP_ASSERT(attr >= 0 && attr < LAST_STRING_ATTRIBUTE);

	if (!override && (m_string_attributes_set & (1 << (UINT32)attr)))
		return OpStatus::OK;

	TempBuffer result;
	TempBuffer bidi;
	if (normalize)
	{
		RETURN_IF_ERROR(CollectBIDIString(bidi, f, m_default_bidi_marker));
		RETURN_IF_ERROR(OpGadgetUtils::NormalizeText(bidi.GetStorage(), &result));
	}
	else
		RETURN_IF_ERROR(CollectBIDIString(result, f, m_default_bidi_marker));

	m_string_attributes_set |=  1 << (UINT32)attr;
	return m_string_attributes[attr].Set(result.GetStorage());
}

OP_STATUS
OpGadgetClass::GetAttributeString(XMLFragment *f, const uni_char *attr_str_arg, OpString &string)
{
	const uni_char *attr_str = f->GetAttribute(attr_str_arg);
	if (!attr_str)
		return OpStatus::OK;

	TempBuffer result;
	RETURN_IF_ERROR(OpGadgetUtils::NormalizeText(attr_str, &result));

	return string.Set(result.GetStorage());
}

OP_STATUS
OpGadgetClass::GetAttributeBoolean(XMLFragment *f, const uni_char *attr_str_arg, BOOL &result, BOOL dval)
{
	result = dval;

	const uni_char *attr_str = f->GetAttribute(attr_str_arg);
	if (!attr_str)
		return OpStatus::OK;

	TempBuffer str;
	RETURN_IF_ERROR(OpGadgetUtils::NormalizeText(attr_str, &str));

	result = OpGadgetUtils::IsAttrStringTrue(str.GetStorage(), dval);

	return OpStatus::OK;
}

BOOL3
OpGadgetClass::OpenAllowed()
{
	switch(SignatureState())
	{
	case GADGET_SIGNATURE_PENDING:
	case GADGET_SIGNATURE_UNKNOWN:
		return MAYBE;
	case GADGET_SIGNATURE_UNSIGNED:
#ifdef DOM_JIL_API_SUPPORT
		return GetAttribute(WIDGET_JIL_ATTR_BILLING) ? NO : YES;
#else
		/* fallthrough */
#endif // DOM_JIL_API_SUPPORT
	case GADGET_SIGNATURE_SIGNED:
		return YES;
	default:
		OP_ASSERT(!"Unknown signature state");
		/* fallthrough */
	case GADGET_SIGNATURE_VERIFICATION_FAILED:
		return NO;
	}
}

BOOL OpGadgetClass::HasFileAccess()
{
	return GetAttribute(WIDGET_ACCESS_ATTR_FILES)
		&& (m_namespace != GADGETNS_JIL_1_0 || GetAttribute(WIDGET_JIL_ACCESS_ATTR_FILESYSTEM));
}

OP_STATUS
OpGadgetClass::GetAttributeIRI(XMLFragment *f, const uni_char *attr_str_arg, OpString &string, BOOL allow_wildcard)
{
	const uni_char *attr_str = f->GetAttribute(attr_str_arg);
	if (!attr_str)
		return OpStatus::OK;

	TempBuffer result;
	RETURN_IF_ERROR(OpGadgetUtils::NormalizeText(attr_str, &result));

	if (!OpGadgetUtils::IsValidIRI(result.GetStorage(), allow_wildcard))
		return OpStatus::OK;

	return string.Set(result.GetStorage());
}

OP_STATUS
OpGadgetClass::SetAttributeIRI(GadgetStringAttribute attr, XMLFragment *f, const uni_char *attr_str_arg, BOOL override, BOOL allow_wildcard)
{
	OP_ASSERT(attr >= 0 && attr < LAST_STRING_ATTRIBUTE);

	if (!override && (m_string_attributes_set & (1 << (UINT32)attr)))
		return OpStatus::OK;

	const uni_char *attr_str = f->GetAttribute(attr_str_arg);
	if (!attr_str)
		return OpStatus::OK;

	TempBuffer result;
	RETURN_IF_ERROR(OpGadgetUtils::NormalizeText(attr_str, &result));

	const uni_char *str = result.GetStorage();

	if (OpGadgetUtils::IsValidIRI(str, allow_wildcard))
	{
		m_string_attributes_set |=  1 << (UINT32)attr;
		return m_string_attributes[attr].Set(str);
	}

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////

OP_STATUS
OpGadgetClass::ValidateIcon(const uni_char* src, BOOL& valid, OpString& resolved_path)
{
	valid = FALSE;
	URL url = g_url_api->GetNewOperaURL();
	BOOL found = FALSE;
#if PATHSEPCHAR != '/'
	OpString local_path;
	RETURN_IF_ERROR(local_path.Set(src));
	OpGadgetUtils::ChangeToLocalPathSeparators(local_path.DataPtr());
#else
	OpStringC local_path(src);
#endif

	RETURN_IF_ERROR(g_gadget_manager->Findi18nPath(this, local_path, HasLocalesFolder(), resolved_path, found, NULL));
	BOOL is_image = FALSE;
	if (found)
	{
		RETURN_IF_ERROR(url.SetAttribute(URL::KFileName, src));
		const uni_char* last_pathsep = uni_strrchr(resolved_path.CStr(), PATHSEPCHAR);
		const uni_char* extension = last_pathsep ? uni_strrchr(last_pathsep, '.') : NULL;
		if (extension && (extension > last_pathsep + 1) && uni_stri_eq(extension + 1, "svg"))
			is_image = TRUE;
		else
		{
			OpFile icon_file;
			RETURN_IF_ERROR(icon_file.Construct(resolved_path.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP));
			RETURN_IF_ERROR(icon_file.Open(OPFILE_READ));
			char data[ContentDetector::DETERMINISTIC_HEADER_LENGTH];
			OpFileLength bytes_read;
			RETURN_IF_ERROR(icon_file.Read(data, ContentDetector::DETERMINISTIC_HEADER_LENGTH, &bytes_read));

			RETURN_IF_ERROR(url.WriteDocumentData(URL::KNormal, data, static_cast<unsigned int>(bytes_read)));
			url.WriteDocumentDataFinished();

			ContentDetector detector(url.GetRep(), TRUE, TRUE, ContentDetector::IETF);
			RETURN_IF_ERROR(detector.DetectContentType());
			is_image = !!url.GetAttribute(URL::KIsImage);
		}
	}

	valid = is_image;
	return OpStatus::OK;
}

OP_STATUS
OpGadgetClass::ValidateIcons()
{
	if (m_icons_checked)
		return OpStatus::OK;
	OpGadgetIcon* current_icon = static_cast<OpGadgetIcon*>(m_icons.First());
	while (current_icon)
	{
		BOOL valid;
		OpString resolved_path;
		RETURN_IF_ERROR(ValidateIcon(current_icon->Src(), valid, resolved_path));
		if (!valid)
		{
			GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: Could not load icon file '%s'."), current_icon->Src()));
			Link* old = current_icon;
			current_icon = static_cast<OpGadgetIcon*>(current_icon->Suc());
			old->Out();
			OP_DELETE(old);
		}
		else
		{
#ifdef GADGET_DUMP_TO_FILE
			RETURN_IF_ERROR(current_icon->StoreResolvedPath(resolved_path.CStr()));
#endif
			current_icon = static_cast<OpGadgetIcon*>(current_icon->Suc());
		}
	}


	// Resolve any default icons that might exist.
	OpString file_name;
	OpString resolved_path;
	for (int i = 0; i < NUMBER_OF_DEFAULT_ICONS; ++ i)
	{
		BOOL valid;
		RETURN_IF_ERROR(file_name.Set(g_gadget_default_icon[i]));
		RETURN_IF_ERROR(ValidateIcon(file_name.CStr(), valid, resolved_path));
		if (!valid)
			continue;

		OpGadgetIcon* gadget_icon;
		RETURN_IF_ERROR(OpGadgetIcon::Make(&gadget_icon, file_name.CStr()));
		OP_ASSERT(gadget_icon);
		OpAutoPtr<OpGadgetIcon> icon_anchor(gadget_icon);
#ifdef GADGET_DUMP_TO_FILE
		gadget_icon->StoreResolvedPath(resolved_path);
#endif //GADGET_DUMP_TO_FILE
		if (OpStatus::IsSuccess(ProcessIcon(gadget_icon)))
			icon_anchor.release();
	}

	m_icons_checked = TRUE;
	return OpStatus::OK;
}

OP_STATUS
OpGadgetClass::GetGadgetIconPath(OpGadgetIcon *gadget_icon, BOOL full_path, OpString &icon_path)
{
	RETURN_IF_ERROR(ValidateIcons());
	if (!gadget_icon)
		return OpStatus::ERR;

	const uni_char *icon_src = gadget_icon->Src();
	OP_ASSERT(icon_src && *icon_src);

	if (*icon_src == '/')
		icon_src++;

	// Convert path string to use local path separator. To catch uses of
	// reserved characters, we move away any actual references to the the
	// local path separator.
#if PATHSEPCHAR != '/'
	OpString local_path;
	RETURN_IF_ERROR(local_path.Set(icon_src));
	OpGadgetUtils::ChangeToLocalPathSeparators(local_path.CStr());
#else
	OpStringC local_path(icon_src);
#endif

	// The icon might be localized, see if we can find it
	BOOL found;
	OpString absolute_path;
	OpStatus::Ignore(g_gadget_manager->Findi18nPath(this, local_path, HasLocalesFolder(), absolute_path, found));

	icon_path.Empty();
	if (full_path)
	{
		RETURN_IF_ERROR(icon_path.Set(absolute_path));
	}
	else
	{
		RETURN_IF_ERROR(icon_path.Set(absolute_path.CStr() + m_gadget_path.Length()));
	}

	return OpStatus::OK;
}

OP_STATUS
OpGadgetClass::GetGadgetIcon(UINT32 index, OpString& icon_path, INT32& width, INT32& height, BOOL full_path)
{
	RETURN_IF_ERROR(ValidateIcons());
	OpGadgetIcon* gadget_icon = (OpGadgetIcon*) m_icons.First();
	while (gadget_icon && index--) { gadget_icon =  (OpGadgetIcon*) gadget_icon->Suc(); }

	RETURN_IF_ERROR(GetGadgetIconPath(gadget_icon, full_path, icon_path));

	width = gadget_icon->Width();
	height = gadget_icon->Height();

	return OpStatus::OK;
}

OP_STATUS
OpGadgetClass::GetGadgetIcon(OpBitmap** iconBmp, UINT32 index)
{
	RETURN_IF_ERROR(ValidateIcons());
	OpString iconPath;
	INT32 specified_width, specified_height;
	RETURN_IF_ERROR(GetGadgetIcon(index, iconPath, specified_width, specified_height));
	return GetGadgetIconInternal(iconBmp, iconPath, FALSE, 0, 0);
}

OP_STATUS
OpGadgetClass::GetGadgetIcon(OpBitmap** iconBmp, INT32 requested_width, INT32 requested_height, BOOL resize, BOOL allow_size_check)
{
	RETURN_IF_ERROR(ValidateIcons());
	if (requested_width <= 0 || requested_height <= 0)
	{
		requested_width = requested_height = 0;
	}
	long requested_area = static_cast<long>(requested_width) * static_cast<long>(requested_height);

	OpGadgetIcon* best_smaller = NULL;
	long best_smaller_area = 0;
	OpGadgetIcon* best_bigger = NULL; // actually best bigger or equal
	long best_bigger_area = 0;

	OpGadgetIcon* icon = static_cast<OpGadgetIcon*>(m_icons.First());

	while (icon)
	{
		if (!icon->IsSVG()) // currently not supported
		{
			INT32 width = icon->Width();
			INT32 height = icon->Height();
			if (allow_size_check && width == 0 && height == 0)
			{
				width = icon->RealWidth();
				height = icon->RealHeight();
				if (width == -1 && height == -1 &&
				    OpStatus::IsSuccess(PeekGadgetIconDimension(icon)))
				{
					width = icon->RealWidth();
					height = icon->RealHeight();
				}
			}

			if (width > 0 && height > 0)
			{
				if (width == requested_width && height == requested_height)
				{
					best_bigger = icon;
					break;
				}
				long icon_area = static_cast<long>(width) * static_cast<long>(height);
				if (icon_area >= requested_area)
				{
					if (!best_bigger || best_bigger_area > icon_area)
					{
						best_bigger = icon;
						best_bigger_area = icon_area;
					}
				}
				else
				{
					if (!best_smaller || best_smaller_area < icon_area)
					{
						best_smaller = icon;
						best_smaller_area = icon_area;
					}
				}
			}
		}
		icon = static_cast<OpGadgetIcon*>(icon->Suc());
	}

	if (best_bigger)
	{
		icon = best_bigger;
	}
	else if (best_smaller)
	{
		icon = best_smaller;
	}
	else
	{
		icon = static_cast<OpGadgetIcon*>(m_icons.First());
	}
	if (!icon)
	{
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	}
	OpString iconPath;
	RETURN_IF_ERROR(GetGadgetIconPath(icon, TRUE, iconPath));
	return GetGadgetIconInternal(iconBmp, iconPath, resize, requested_width, requested_height);
}

OP_STATUS
OpGadgetClass::GetGadgetIconInternal(OpBitmap** iconBmp, OpString& iconPath, BOOL resize, INT32 requested_width, INT32 requested_height)
{
	*iconBmp = NULL;

	if (iconPath.IsEmpty())
		return OpStatus::ERR;

	OpFile iconFile;
	OpFileLength length = 0;

	RETURN_IF_ERROR(iconFile.Construct(iconPath.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP));
	RETURN_IF_ERROR(iconFile.Open(OPFILE_READ));
	RETURN_IF_ERROR(iconFile.GetFileLength(length));

	char* buffer = OP_NEWA(char, static_cast<int>(length));		/* ARRAY OK 2010-08-24 peter */
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;
	OpHeapArrayAnchor<char> buffer_ptr(buffer);

	OpFileLength read;
	RETURN_IF_ERROR(iconFile.Read((void*)buffer, length, &read));
	if (length != read)
		return OpStatus::ERR;

	BufferContentProvider* contentprovider;
	RETURN_IF_ERROR(BufferContentProvider::Make(contentprovider, buffer_ptr.release(), (UINT32)length));
	OpStackAutoPtr<ImageContentProvider> contentprovider_ptr(contentprovider);

	Image icon = imgManager->GetImage(contentprovider);

	RETURN_IF_ERROR(icon.IncVisible(null_image_listener));
	OP_STATUS status = icon.OnLoadAll(contentprovider);
	if (OpStatus::IsError(status))
	{
		icon.DecVisible(null_image_listener);
		return status;
	}

	OpBitmap* bmp = icon.ImageDecoded() ? icon.GetBitmap(NULL) : 0;
	if (!bmp)
	{
		icon.DecVisible(null_image_listener);
		return OpStatus::ERR;
	}

	BOOL do_resize;
	INT32 rendering_width;
	INT32 rendering_height;

	if (resize && requested_width > 0 && requested_height > 0 && (static_cast<UINT32>(requested_width) != bmp->Width() || static_cast<UINT32>(requested_height) != bmp->Height()))
	{
		do_resize = TRUE;
		rendering_width = requested_width;
		rendering_height = requested_height;
	}
	else
	{
		do_resize = FALSE;
		rendering_width = bmp->Width();
		rendering_height = bmp->Height();
	}

	status = OpBitmap::Create(iconBmp, rendering_width, rendering_height, bmp->IsTransparent(), bmp->HasAlpha(), 0, FALSE, do_resize);
	if (OpStatus::IsSuccess(status))
	{
		if (do_resize)
		{
			OP_ASSERT((*iconBmp)->Supports(OpBitmap::SUPPORTS_PAINTER));
			OpPainter *painter = (*iconBmp)->GetPainter();
			if (!painter)
			{
				status = OpStatus::ERR_NOT_SUPPORTED;
			}
			else
			{
				OpRect source_rect = OpRect(0, 0, bmp->Width(), bmp->Height());
				OpRect dest_rect = OpRect(0, 0, rendering_width, rendering_height);

				painter->DrawBitmapScaled(bmp, source_rect, dest_rect);
				(*iconBmp)->ReleasePainter();
			}
		}
		else
		{
			UINT32 lineCount = bmp->Height();
			UINT32* data = OP_NEWA(UINT32, bmp->Width());	/* ARRAY OK 2010-08-24 peter */
			if (!data)
			{
				status = OpStatus::ERR_NO_MEMORY;
			}
			else
			{
				ANCHOR_ARRAY(UINT32, data);

				for (UINT32 i = 0; i < lineCount; i++)
				{
					if (bmp->GetLineData(data, i) == TRUE)
						(*iconBmp)->AddLine(data, i);
				}
			}
		}
	}

	if (OpStatus::IsError(status))
	{
		OP_DELETE(*iconBmp);
		*iconBmp = NULL;
	}

	icon.ReleaseBitmap();
	icon.DecVisible(null_image_listener);

	return status;
}

OP_STATUS
OpGadgetClass::PeekGadgetIconDimension(OpGadgetIcon* gadget_icon)
{
	if (gadget_icon->RealWidth() >= 0 && gadget_icon->RealHeight()  >= 0)
		return OpStatus::OK;

	OpString iconPath;
	RETURN_IF_ERROR(GetGadgetIconPath(gadget_icon, TRUE, iconPath));
	if (iconPath.IsEmpty())
		return OpStatus::ERR;

	OpFile iconFile;
	OpFileLength length = 0;

	RETURN_IF_ERROR(iconFile.Construct(iconPath.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP));
	RETURN_IF_ERROR(iconFile.Open(OPFILE_READ));
	RETURN_IF_ERROR(iconFile.GetFileLength(length));

	char* buffer = OP_NEWA(char, static_cast<int>(length));
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;
	OpHeapArrayAnchor<char> buffer_ptr(buffer);

	OpFileLength read;
	RETURN_IF_ERROR(iconFile.Read((void*)buffer, length, &read));
	if (length != read)
		return OpStatus::ERR;

	BufferContentProvider* contentprovider;
	RETURN_IF_ERROR(BufferContentProvider::Make(contentprovider, buffer_ptr.release(), (UINT32)length));
	OpStackAutoPtr<ImageContentProvider> contentprovider_ptr(contentprovider);

	Image icon = imgManager->GetImage(contentprovider);

	RETURN_IF_ERROR(icon.IncVisible(null_image_listener));
	icon.PeekImageDimension();
	OP_STATUS status = icon.IsFailed() ? OpStatus::ERR : OpStatus::OK;
	if (OpStatus::IsSuccess(status))
	{
		gadget_icon->m_real_height = icon.Height();
		gadget_icon->m_real_width = icon.Width();
	}
	icon.DecVisible(null_image_listener);
	return status;
}


#ifdef EXTENSION_SUPPORT
OP_STATUS
OpGadgetClass::GetGadgetIncludesPath(OpString& path)
{
	if (IsExtension())
		return path.SetConcat(GetGadgetRootPath(), UNI_L(PATHSEP), UNI_L("includes"));
	else
		return OpStatus::ERR;
}
#endif

BOOL
OpGadgetClass::HasLocalesFolder()
{
	if (MAYBE == m_has_locales_folder)
	{
		// Support for folder-based localization is enabled for this
		// widget, but we haven't checked if we have a "locales" folder
		// yet. Do it now. Softly ignore any errors, and re-do the
		// check the next time we are called.
		OpString locales_path;
		OpFile locales_file;
		if (OpStatus::IsSuccess(locales_path.SetConcat(GetGadgetRootPath(),
								UNI_L(PATHSEP), UNI_L("locales"))) &&
			OpStatus::IsSuccess(locales_file.Construct(locales_path.CStr(),
								OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP)))
		{
			BOOL found = FALSE;
			if (OpStatus::IsSuccess(locales_file.Exists(found)))
				m_has_locales_folder = found ? YES : NO;
		}
	}

	return m_has_locales_folder == YES;
}

OP_STATUS
OpGadgetClass::GetGadgetNamespaceUrl(OpString& url)
{
	if (SupportsNamespace(GADGETNS_OPERA_2006))
		return g_gadget_manager->GetNamespaceUrl(GADGETNS_OPERA_2006, url);
	else if (SupportsNamespace(GADGETNS_W3C_1_0))
		return g_gadget_manager->GetNamespaceUrl(GADGETNS_W3C_1_0, url);
	else if (SupportsNamespace(GADGETNS_OPERA))
		return g_gadget_manager->GetNamespaceUrl(GADGETNS_OPERA, url);
	else if (SupportsNamespace(GADGETNS_JIL_1_0))
		return g_gadget_manager->GetNamespaceUrl(GADGETNS_JIL_1_0, url);

	return url.Set("");
}

BOOL
OpGadgetClass::HasIconWithSrc(const uni_char* src)
{
	OpGadgetIcon *i = static_cast<OpGadgetIcon*>(m_icons.First());
	while (i)
	{
		if (uni_str_eq(i->Src(), src))
			return TRUE;

		i = static_cast<OpGadgetIcon*>(i->Suc());
	}

	return FALSE;
}

OP_STATUS
OpGadgetClass::SetIsEnabled(BOOL is_enabled)
{
	m_is_enabled = is_enabled;
	m_disabled_details.Empty();
	return OpStatus::OK;
}

OP_STATUS
OpGadgetClass::SetIsEnabled(BOOL is_enabled, const OpStringC &details)
{
	m_is_enabled = is_enabled;
	return m_disabled_details.Set(details);
}

GadgetClass
OpGadgetClass::GetClass()
{
#ifdef WEBSERVER_SUPPORT
	if (IsSubserver())
		return GADGET_CLASS_UNITE;
	else
#endif // WEBSERVER_SUPPORT
#ifdef EXTENSION_SUPPORT
	if (IsExtension())
		return GADGET_CLASS_EXTENSION;
	else
#endif // EXTENSION_SUPPORT
		return GADGET_CLASS_WIDGET;
}

OP_STATUS
OpGadgetClass::Reload()
{
	Clear();
	OpString error_reason;
	OP_GADGET_STATUS error = LoadInfoFromInfoFile(error_reason);
	if (OpStatus::IsError(error))
	{
		OpString message;
		GetGadgetErrorMessage(error, error_reason.CStr(), message);
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("%s"), message.CStr()));
	}
	return error;
}


/* static */ void
OpGadgetClass::GetGadgetErrorMessage(OP_GADGET_STATUS error, const uni_char* error_reason, OpString& message)
{
	OpStatus::Ignore(message.Set("Widget loading error: WIDGET_ERROR_"));
	switch (error)
	{
	case OpStatus::ERR_NO_MEMORY:
		OpStatus::Ignore(message.Append("OUT_OF_MEMORY"));
		break;
	case OpGadgetStatus::ERR_INVALID_ZIP:
		OP_ASSERT(error_reason && *error_reason);
		OpStatus::Ignore(message.Append("INVALID_ZIP"));
		break;
	case OpGadgetStatus::ERR_WRONG_NAMESPACE:
		OpStatus::Ignore(message.Append("CONFIG_FILE_WRONG_NAMESPACE"));
		break;
	case OpGadgetStatus::ERR_BAD_PARAM:
		OpStatus::Ignore(message.Append("CONFIG_FILE_BAD_PARAM_IN_FEATURE"));
		break;
	case OpStatus::ERR_PARSING_FAILED:
		OpStatus::Ignore(message.Append("CONFIG_FILE_INVALID_XML"));
		break;
	case OpGadgetStatus::ERR_WRONG_WIDGET_FORMAT:
		OP_ASSERT(error_reason && *error_reason);
		OpStatus::Ignore(message.Append("CONFIG_FILE_INVALID_FORMAT"));
		break;
	case OpGadgetStatus::ERR_CONFIGURATION_FILE_NOT_FOUND:
		OpStatus::Ignore(message.Append("CONFIG_FILE_NOT_FOUND"));
		break;
	case OpGadgetStatus::ERR_START_FILE_NOT_FOUND:
		OpStatus::Ignore(message.Append("START_FILE_NOT_FOUND"));
		break;
	case OpGadgetStatus::ERR_CONTENT_TYPE_NOT_SUPPORTED:
		OpStatus::Ignore(message.Append("INVALID_START_FILE_TYPE"));
		break;
	case OpGadgetStatus::ERR_FEATURE_NOT_SUPPORTED:
		OpStatus::Ignore(message.Append("FEATURE_NOT_SUPPORTED"));
		break;
	case OpGadgetStatus::ERR_GADGET_TYPE_NOT_SUPPORTED:
		OpStatus::Ignore(message.Append("WIDGET_TYPE_NOT_SUPPORTED"));
		break;
	default:
		OP_ASSERT(!"Unhandled error code!");
	case OpStatus::ERR: // some internal calls may just return err on failure - no meaningful way to express what went wrong to developer
		OpStatus::Ignore(message.Set("INTERNAL"));
		break;
	}
	if (error_reason && *error_reason)
		OpStatus::Ignore(message.AppendFormat(UNI_L("\n%s"), error_reason));
}

/** Load widget configuration file. Can be called several times
  * (e.g on reload).
  * @param error_reason Set to reason of failure in case of an error(not modified otherwise). 
  * @return Status of the operation.
  */
OP_STATUS
OpGadgetClass::LoadInfoFromInfoFile(OpString& error_reason)
{
	// A few asserts that tries to figure out if we've been called before.
	OP_ASSERT(m_icons.Empty());
	OP_ASSERT(m_features.Empty());
	OP_ASSERT(m_default_preferences.Empty());
	OP_ASSERT(!m_integer_attributes_set);
	OP_ASSERT(!m_string_attributes_set);

	error_reason.Empty();
	// The configuration file is always in the root of the widget archive.
	RETURN_IF_ERROR(m_config_file.SetConcat(m_gadget_path.CStr(), UNI_L(PATHSEP), GADGET_CONFIGURATION_FILE));

	BOOL legacy_config_file = FALSE;
	BOOL config_file_found = FALSE;
	OpFile file;
	RETURN_IF_ERROR(file.Construct(m_config_file.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP));
	RETURN_IF_ERROR(file.Exists(config_file_found));

	if (!config_file_found)
	{
		// ...except for legacy widgets, were we do allow bad archives with
		// a sub-directory named after the widget base-name.
		// Find the root folder of the legacy widget and remember it.
		RETURN_IF_ERROR(g_gadget_manager->FindLegacyRootPath(m_gadget_path, m_gadget_root_path));
		if (!m_gadget_root_path.IsEmpty())
		{
			RETURN_IF_ERROR(m_config_file.SetConcat(m_gadget_root_path.CStr(), UNI_L(PATHSEP), GADGET_CONFIGURATION_FILE));
			RETURN_IF_ERROR(file.Construct(m_config_file.CStr()));
			RETURN_IF_ERROR(file.Exists(config_file_found));
			legacy_config_file = TRUE;
		}
	}

	if (!config_file_found)
		return OpGadgetStatus::ERR_CONFIGURATION_FILE_NOT_FOUND;

	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	XMLFragment f;

	f.SetDefaultWhitespaceHandling(XMLWHITESPACEHANDLING_PRESERVE);

	OP_STATUS err = f.Parse(static_cast<OpFileDescriptor*>(&file));
	if (OpStatus::IsError(err))
	{

#if defined(XML_ERRORS)
		OpString desc;
		XMLRange loc;
		OpStatus::Ignore(desc.Set(f.GetErrorDescription()));
		loc = f.GetErrorLocation();
		if (desc.HasContent())
			OpStatus::Ignore(error_reason.AppendFormat(UNI_L("Line: %d\n%s."), loc.start.line, desc.CStr()));
#endif // XML_ERRORS
		return OpStatus::ERR_PARSING_FAILED;
	}
	RETURN_IF_ERROR(ProcessXMLContent(&f, error_reason));

	OpStatus::Ignore(file.Close());

	// Check for specific namespaces.
	if (SupportsNamespace(GADGETNS_W3C_1_0))
	{
		// Return error if the config file is not in the correct place for W3C widgets.
		if (legacy_config_file)
			return OpGadgetStatus::ERR_CONFIGURATION_FILE_NOT_FOUND;

		// Enable support for folder-based localization for W3C widgets.
		m_has_locales_folder = MAYBE;
	}
#ifdef EXTENSION_SUPPORT
	else if (m_content_type == URL_EXTENSION_INSTALL_CONTENT)
	{
		// Only allow w3c widgets as extensions.
		return OpGadgetStatus::ERR_WRONG_NAMESPACE;
	}
#endif // EXTENSION_SUPPORT

#ifdef SIGNED_GADGET_SUPPORT
	err = VerifySignature();
#endif // SIGNED_GADGET_SUPPORT

	RETURN_IF_ERROR(PostProcess(error_reason));
	return err;
}

/** Process a configuration file.
  * Remeber that this function might be called later as a response to a widget reload.
  * @param[in]  f A parsed config.xml file.
  * @param[out] error_reason An error message, if one is called for.
  * @return OpStatus::OK if parsing was fine, OpStatus::ERR if the content
  *  was bogus, and OpStatus::ERR_NO_MEMORY on OOM.
  */
OP_GADGET_STATUS
OpGadgetClass::ProcessXMLContent(XMLFragment* f, OpString& error_reason)
{
	return g_gadget_parsers->ProcessGadgetConfigurationFile(this, f, error_reason);
}

OP_GADGET_STATUS
OpGadgetClass::PostProcess(OpString& error_reason)
{
	RETURN_IF_ERROR(ApplyDefaults());
	RETURN_IF_ERROR(OpSecurityManager::PostProcess(this, error_reason));

	// Default to widget name if no servicepath has been provided.
	if (IsSubserver())
		RETURN_IF_ERROR(SetAttribute(WIDGET_SERVICEPATH_TEXT, GetAttribute(WIDGET_NAME_TEXT)));

	// TODO : check for specific errors
	if (SupportsNamespace(GADGETNS_W3C_1_0))
	{
		// Check if Opera supports the given content-type and that it's handled by Opera (W3C only)
		const uni_char *content_type = GetAttribute(WIDGET_CONTENT_ATTR_TYPE);
		if (content_type && *content_type)
		{
			Viewer *viewer = g_viewers->FindViewerByMimeType(content_type);
			if (!viewer || viewer && viewer->GetAction() != VIEWER_OPERA)
				return OpGadgetStatus::ERR_CONTENT_TYPE_NOT_SUPPORTED;
		}

		const char *canon = NULL;
		const uni_char *encoding = GetAttribute(WIDGET_CONTENT_ATTR_CHARSET);
		if (encoding && *encoding)
		{
			OpString8 encoding8;
			RETURN_IF_ERROR(encoding8.Set(encoding));
			canon = g_charsetManager->GetCanonicalCharsetName(encoding8.CStr());
		}
		if (!canon) // Unrecognized encoding name
			RETURN_IF_ERROR(SetAttribute(WIDGET_CONTENT_ATTR_CHARSET, UNI_L("UTF-8"), TRUE));

		// Resolve any default icons that might exist.
		OpString file_name;
		for (int i = 0; i < NUMBER_OF_DEFAULT_ICONS; ++ i)
		{
			OpString localepath;

			BOOL found = FALSE;
			RETURN_IF_ERROR(file_name.Set(g_gadget_default_icon[i]));
			OpStatus::Ignore(g_gadget_manager->Findi18nPath(this, file_name, HasLocalesFolder(), localepath, found, NULL));
			if (found)
			{
				// Add this icon to the list if it exists.
				OpGadgetIcon* gadget_icon;
				RETURN_IF_ERROR(OpGadgetIcon::Make(&gadget_icon, file_name));
				OpAutoPtr<OpGadgetIcon> icon_anchor(gadget_icon);
				if (OpStatus::IsSuccess(ProcessIcon(gadget_icon)))
					icon_anchor.release();
			}
		}
	}

	BOOL3 start_file_exists = MAYBE;
	// Find path to start file. Localization is handled by the
	// hooks in the loadhandler.
	const uni_char *src = GetAttribute(WIDGET_CONTENT_ATTR_SRC);
	if (src)
	{
		if (src && *src == '/')
			RETURN_IF_ERROR(m_gadget_file.Set(src + 1));	// Absolute path
		else
			RETURN_IF_ERROR(m_gadget_file.Set(src));		// Relative path

#if PATHSEPCHAR != '/'
		// Convert path string to use local path separator. To catch uses of
		// reserved characters, we move away any actual references to the the
		// local path separator.
		OpGadgetUtils::ChangeToLocalPathSeparators(m_gadget_file.CStr());
#endif
	}
	else
	{
		// No start file was defined.

		if (SupportsNamespace(GADGETNS_W3C_1_0))
		{
			// No start file was defined. We need to loop over the
			// default start file names to see if any of them are
			// available.
			OpString file_name;
			for (int i = 0; i < NUMBER_OF_DEFAULT_START_FILES; ++ i)
			{
				OpString localepath;
				BOOL found = FALSE;
				RETURN_IF_ERROR(file_name.Set(g_gadget_start_file[i]));
				OpStatus::Ignore(g_gadget_manager->Findi18nPath(this, file_name, HasLocalesFolder(), localepath, found, NULL));
				if (found)
				{
					// Set start element to the bare file name, as this
					// is what is seen outside, even if we only found it
					// in a locales sub-folder.
					RETURN_IF_ERROR(m_gadget_file.TakeOver(file_name));

					// Remember the start element and its MIME type.
					SetAttribute(WIDGET_CONTENT_ATTR_SRC, m_gadget_file.CStr(), TRUE);
					OpString mime_type;
					RETURN_IF_ERROR(mime_type.Set(g_gadget_start_file_type[i]));
					SetAttribute(WIDGET_CONTENT_ATTR_TYPE, mime_type.CStr(), TRUE);
					start_file_exists = YES;
					break;
				}
			}

			if (m_gadget_file.IsEmpty())
			{
				// No start file found, flag this as an error.
				start_file_exists = NO;

				// Just to make sure nothing crashes, set a m_gadget_file anyway.
				RETURN_IF_ERROR(m_gadget_file.Set(g_gadget_start_file[0]));
			}
		}
		else
		{
			// No start file was defined. The legacy widget specification
			// states that we always fall back to index.html
			RETURN_IF_ERROR(m_gadget_file.Set(GADGET_INDEX_FILE));
		}
	}

	// Make sure the selected start file exists.
	if (MAYBE == start_file_exists && !m_gadget_file.IsEmpty())
	{
		OpString localepath;
		BOOL found = FALSE;
		OpStatus::Ignore(g_gadget_manager->Findi18nPath(this, m_gadget_file, HasLocalesFolder(), localepath, found, NULL));
		start_file_exists = found ? YES : NO;
	}

	// Disallow loading of widget if the start file does not exist (in any
	// allowed locale).
	if (NO == start_file_exists)
		return OpGadgetStatus::ERR_START_FILE_NOT_FOUND;
	return OpStatus::OK;
}

#ifdef SIGNED_GADGET_SUPPORT
OP_GADGET_STATUS
OpGadgetClass::VerifySignature()
{
	m_sign_state = GADGET_SIGNATURE_UNKNOWN;

	if (!m_signature_storage)
		m_signature_storage = OP_NEW(GadgetSignatureStorage, ());
	RETURN_OOM_IF_NULL(m_signature_storage);

	if (!m_signature_verifier)
		m_signature_verifier = OP_NEW(GadgetSignatureVerifier, ());
	if (!m_signature_verifier)
	{
		OP_DELETE(m_signature_storage);
		m_signature_storage = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	TRAPD(err,
		g_main_message_handler->SetCallBack(this,
			MSG_GADGET_SIGNATURE_VERIFICATION_FINISHED, m_signature_verifier->Id());
		m_signature_verifier->SetGadgetFilenameL(m_gadget_path);
		m_signature_verifier->SetCAStorage(g_libcrypto_cert_storage);
		m_signature_verifier->SetGadgetSignatureStorageContainer(m_signature_storage);
		m_signature_verifier->ProcessL();
		m_sign_state = GADGET_SIGNATURE_PENDING;
	);

	if (OpStatus::IsError(err))
	{
		GADGETS_REPORT_ERROR_TO_CONSOLE((m_gadget_path.CStr(), UNI_L("widget signature could not be verified!\n")));
		g_main_message_handler->UnsetCallBack(this, MSG_GADGET_SIGNATURE_VERIFICATION_FINISHED);
		m_sign_state = GADGET_SIGNATURE_VERIFICATION_FAILED;
	}
	return err;
}
#endif // SIGNED_GADGET_SUPPORT

OP_GADGET_STATUS
OpGadgetClass::ProcessFeature(OpGadgetFeature *feature, OpString& error_reason)
{
	OP_ASSERT(feature->URI());
	OP_ASSERT(*feature->URI());
	if (uni_strcmp(feature->URI(), "http://xmlns.opera.com/fileio") == 0
#ifdef EXTENSION_SUPPORT
		&& m_content_type != URL_EXTENSION_INSTALL_CONTENT
#endif // EXTENSION_SUPPORT
		)
	{
		SetAttribute(WIDGET_ACCESS_ATTR_FILES, TRUE);
	}
#ifdef WEBSERVER_SUPPORT
	else if (uni_strcmp(feature->URI(), "http://xmlns.opera.com/webserver") == 0 &&
	         m_content_type == URL_UNITESERVICE_INSTALL_CONTENT)
	{
		const uni_char *servicepath = feature->GetParamValue(UNI_L("servicepath"));
		if (servicepath)
			RETURN_IF_ERROR(SetAttribute(WIDGET_SERVICEPATH_TEXT, servicepath));
		else
		{
			OpStatus::Ignore(error_reason.Set(UNI_L("Invalid <feature> element - A 'servicepath' param is required for feature 'http://xmlns.opera.com/webserver'.")));
			return OpGadgetStatus::ERR_WRONG_WIDGET_FORMAT;
		}
		const uni_char *type = feature->GetParamValue(UNI_L("type"));
		if (type)
		{
			if (uni_stricmp(type, "service") == 0)
				SetAttribute(WIDGET_WEBSERVER_ATTR_TYPE, GADGET_WEBSERVER_TYPE_SERVICE);
		}
		SetAttribute(WIDGET_WEBSERVER_ATTR_TYPE, GADGET_WEBSERVER_TYPE_APPLICATION);	// default

		SetAttribute(WIDGET_ACCESS_ATTR_WEBSERVER, TRUE);
	}
#endif

# ifdef URL_FILTER
	else if (uni_strcmp(feature->URI(), "opera:urlfilter") == 0)
		SetAttribute(WIDGET_ACCESS_ATTR_URLFILTER, TRUE);
# endif
	else if (uni_strcmp(feature->URI(), "feature:a9bb79c1") == 0)
	{
		// Do nothing, other than "supported"
	}
#ifdef WIDGET_RUNTIME_SUPPORT
	else if (uni_strcmp(feature->URI(), UNI_L("http://xmlns.opera.com/wcb")) == 0)
	{
		//  Widget runtime feature
	}
#endif // WIDGET_RUNTIME_SUPPORT
#ifdef DOM_JIL_API_SUPPORT
	else if (JILFeatures::IsJILFeatureSupported(feature->URI(), this))
	{
		// JIL features

	}
#endif // DOM_JIL_API_SUPPORT
#ifdef DOM_GEOLOCATION_SUPPORT
	else if (uni_strcmp(feature->URI(), "http://www.w3.org/TR/geolocation-API") == 0
			&& !IsExtension() && !IsSubserver())
	{
		SetAttribute(WIDGET_ACCESS_ATTR_GEOLOCATION, TRUE);
	}
#endif // DOM_GEOLOCATION_SUPPORT
	else if (uni_strcmp(feature->URI(), "http://opera.com/widgets/features/api") == 0
			 && !IsExtension())
	{
		// Extended opera widget api.
		// Do nothing, other than "supported".
	}
#ifdef SELFTEST
	else if (uni_strncmp(feature->URI(), "opera:selftest", 14) == 0)
	{
		// Feature used for testing.
		// Do nothing, other than "supported".
	}
#endif // SELFTEST
#ifdef EXTENSION_SUPPORT
	else if (m_content_type == URL_EXTENSION_INSTALL_CONTENT &&
			 uni_strcmp(feature->URI(), "opera:speeddial") == 0)
	{
		if (GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_CAPABLE))
			return OpStatus::OK; // Ignore subsequent declarations of this feature.

		const uni_char *url_string = feature->GetParamValue(UNI_L("url"));
		if (!url_string)
		{
			RETURN_IF_ERROR(error_reason.Set(UNI_L("Invalid <feature> element - An 'url' param is required for feature 'opera:speeddial'")));
			return OpGadgetStatus::ERR_BAD_PARAM; // url is required.
		}

		URL url = g_url_api->GetURL(url_string);
		OpString absolute_url_string;
		if (!url.GetServerName())
		{
			GADGETS_REPORT_TO_CONSOLE((NULL, OpConsoleEngine::Information, UNI_L("Widget loading warning: an 'url' param for the 'opera:speeddial' <feature> must be an absolute URL: %s"), url_string));
			RETURN_IF_ERROR(absolute_url_string.AppendFormat("http://%s", url_string));
			url_string = absolute_url_string.CStr();
		}

		RETURN_IF_ERROR(SetAttribute(WIDGET_EXTENSION_SPEEDDIAL_FALLBACK_URL, url_string));
		RETURN_IF_ERROR(SetAttribute(WIDGET_EXTENSION_SPEEDDIAL_URL, url_string));

		SetAttribute(WIDGET_EXTENSION_SPEEDDIAL_CAPABLE, TRUE);
	}
	else if (m_content_type == URL_EXTENSION_INSTALL_CONTENT &&
			 uni_strcmp(feature->URI(), "opera:contextmenus") == 0)
	{
		SetAttribute(WIDGET_EXTENSION_ALLOW_CONTEXTMENUS, TRUE);
	}
	else if (m_content_type == URL_EXTENSION_INSTALL_CONTENT &&
			 uni_strcmp(feature->URI(), "opera:share-cookies") == 0)
	{
		SetAttribute(WIDGET_EXTENSION_SHARE_URL_CONTEXT, TRUE);
	}
	else if (m_content_type == URL_EXTENSION_INSTALL_CONTENT &&
			 uni_strcmp(feature->URI(), "opera:screenshot") == 0)
	{
		SetAttribute(WIDGET_EXTENSION_SCREENSHOT_SUPPORT, TRUE);
	}
#endif // EXTENSION_SUPPORT
	else if (feature->IsRequired())
	{
		OpStatus::Ignore(error_reason.AppendFormat(UNI_L("Invalid <feature> element - unsupported feature %s"), feature->URI()));
		return OpGadgetStatus::ERR_FEATURE_NOT_SUPPORTED;	// Feature not supported
	}
	else
		return OpGadgetStatus::OK_UNKNOWN_FEATURE; // Signal to the callee that we don't support this feature, but it's not required.

	return OpStatus::OK;
}

OpGadgetFeature *
OpGadgetClass::GetFeature(const uni_char *feature)
{
	OpGadgetFeature *f = (OpGadgetFeature *) m_features.First();
	while (f)
	{
		if (uni_strcmp(feature, f->URI()) == 0)
			return f;

		f = (OpGadgetFeature *) f->Suc();
	}
	return NULL;
}

OpGadgetFeature *
OpGadgetClass::GetFeature(const char *feature)
{
	OpGadgetFeature *f = (OpGadgetFeature *) m_features.First();
	while (f)
	{
		if (uni_stri_eq(feature, f->URI()))
			return f;

		f = (OpGadgetFeature *) f->Suc();
	}
	return NULL;
}

BOOL
OpGadgetClass::IsFeatureRequired(const uni_char *feature)
{
	OpGadgetFeature *f = GetFeature(feature);
	return f && f->IsRequired();
}

BOOL
OpGadgetClass::IsFeatureRequired(const char *feature)
{
	OpGadgetFeature *f = GetFeature(feature);
	return f && f->IsRequired();
}

OP_GADGET_STATUS
OpGadgetClass::ProcessIcon(OpGadgetIcon *gadget_icon)
{
	// Don't add the icon if it's already in the list.
	if (!gadget_icon->Src() || !*gadget_icon->Src()  || HasIconWithSrc(gadget_icon->Src()))
		return OpStatus::ERR;

	// All the other validation is performed in ValidateIcons at the time the icon is first required.
	gadget_icon->Into(&m_icons);
	return OpStatus::OK;
}

OP_STATUS
OpGadgetClass::SaveToFile(PrefsFile* prefsfile, const OpStringC& section, const uni_char *prefix)
{
	OpString state_name;
	RETURN_IF_ERROR(state_name.Set(prefix));
	RETURN_IF_ERROR(state_name.Append(UNI_L("state")));
	OpStringC enabled_string = m_is_enabled ? UNI_L("enabled") : UNI_L("disabled");
	RETURN_IF_LEAVE(prefsfile->WriteStringL(section, state_name, enabled_string));

	if (m_disabled_details.HasContent())
	{
		OpString details_name;
		RETURN_IF_ERROR(details_name.Set(prefix));
		RETURN_IF_ERROR(details_name.Append(UNI_L("state details")));
		RETURN_IF_LEAVE(prefsfile->WriteStringL(section, details_name, m_disabled_details));
	}

	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetClass::LoadFromFile(PrefsFile* prefsfile, const OpStringC& section, const uni_char *prefix)
{
	OpString state_name;
	RETURN_IF_ERROR(state_name.Set(prefix));
	RETURN_IF_ERROR(state_name.Append(UNI_L("state")));
	OpStringC state_string;
	RETURN_IF_LEAVE(state_string = prefsfile->ReadStringL(section, state_name));

	OpString disabled_details;
	OpString details_name;
	RETURN_IF_ERROR(details_name.Set(prefix));
	RETURN_IF_ERROR(details_name.Append(UNI_L("state details")));
	RETURN_IF_LEAVE(prefsfile->ReadStringL(section, details_name, disabled_details));

	BOOL is_enabled = TRUE;
	if (state_string.CompareI("disabled") == 0)
		is_enabled = FALSE;

	RETURN_IF_ERROR(SetIsEnabled(is_enabled, disabled_details));

	return OpStatus::OK;
}

BOOL
OpGadgetClass::HasPreference(const uni_char *pref)
{
	OpGadgetPreference *p = (OpGadgetPreference *) m_default_preferences.First();
	while (p)
	{
		if (uni_strcmp(pref, p->Name()) == 0)
			return TRUE;

		p = (OpGadgetPreference *) p->Suc();
	}

	return FALSE;
}

OpGadgetFeature*
OpGadgetClass::GetFeature(UINT index)
{
	Link *feature = m_features.First();
	while (feature && index--) { feature = feature->Suc(); }
	return (OpGadgetFeature *)feature;
}

OpGadgetFeatureParam*
OpGadgetFeature::GetParam(UINT index)
{
	Link *param = m_params.First();
	while (param && index--) { param = param->Suc(); }
	return (OpGadgetFeatureParam *)param;
}

const uni_char*
OpGadgetFeature::GetParamValue(const uni_char *name)
{
	OpGadgetFeatureParam *param = (OpGadgetFeatureParam *) m_params.First();
	while (param)
	{
		if (uni_strcmp(param->Name(), name) == 0)
			return param->Value();

		param = (OpGadgetFeatureParam *) param->Suc();
	}

	return NULL;
}

#ifdef GADGET_DUMP_TO_FILE
void
OpGadgetFeature::DumpFeatureL(TempBuffer *buf)
{
	// format: [name="feature", required="BOOL", [[name="param", value="value"], [...]]]

	buf->AppendL(UNI_L("[name=\""));
	OpGadgetClass::DumpStringL(buf, URI());
	buf->AppendL(UNI_L("\", required=\""));
	buf->AppendL(IsRequired() ? UNI_L("true") : UNI_L("false"));
	buf->AppendL(UNI_L("\", params=["));

	OpGadgetFeatureParam *param = static_cast<OpGadgetFeatureParam*>(m_params.First());
	while (param)
	{
		buf->AppendL(UNI_L("[name=\""));
		OpGadgetClass::DumpStringL(buf, param->Name());
		buf->AppendL(UNI_L("\", value=\""));
		OpGadgetClass::DumpStringL(buf, param->Value());
		buf->AppendL(UNI_L("\"]"));

		param = static_cast<OpGadgetFeatureParam*>(param->Suc());

		if (param)
			buf->AppendL(UNI_L(", "));
	}

	buf->AppendL(UNI_L("]]"));
}
#endif // GADGET_DUMP_TO_FILE

OP_STATUS
OpGadgetClass::RequestUpdateInfo()
{
	return g_gadget_manager->Update(this);
}


void
OpGadgetClass::SetUpdateInfo(OpGadgetUpdateInfo *update_info)
{
	OP_DELETE(m_update_info);
	m_update_info = update_info;
}

#ifdef GADGET_DUMP_TO_FILE
void
OpGadgetClass::DumpAttributeL(TempBuffer *buf, const uni_char *name, GadgetStringAttribute attr)
{
	BOOL set;
	const uni_char *string = GetAttribute(attr, &set);

	buf->AppendL(name);
	if (set)
	{
		buf->AppendL(UNI_L("=\""));
		DumpStringL(buf, string);
		buf->AppendL(UNI_L("\"\n"));
	}
	else
		buf->AppendL(UNI_L("=null\n"));
}

void
OpGadgetClass::DumpAttributeL(TempBuffer *buf, const uni_char *name, GadgetIntegerAttribute attr)
{
	BOOL set;
	INT32 i = GetAttribute(attr, &set);

	buf->AppendL(name);
	if (set)
	{
		buf->AppendL(UNI_L("="));
		buf->AppendUnsignedLongL(i);
		buf->AppendL(UNI_L("\n"));
	}
	else
		buf->AppendL(UNI_L("=null\n"));
}

/* static */ void
OpGadgetClass::DumpStringL(TempBuffer *buf, const uni_char *name, const uni_char *str)
{
	buf->AppendL(name);

	if (str)
	{
		buf->AppendL(UNI_L("=\""));
		DumpStringL(buf, str);
		buf->AppendL(UNI_L("\"\n"));
	}
	else
		buf->AppendL(UNI_L("=null\n"));
}

/* static */ void
OpGadgetClass::DumpStringL(TempBuffer *buf, const uni_char *str)
{
	const uni_char *p = str;

	while (p && *p)
	{
		if (*p == '\\' || *p == '\"' || *p == '\'')
		{
			buf->AppendL(UNI_L("\\"));
			buf->AppendL(p, 1);
		}
		else if (*p == '\r')
			buf->AppendL(UNI_L("\\r"));
		else if (*p == '\n')
			buf->AppendL(UNI_L("\\n"));
		else
			buf->AppendL(p, 1);

		p++;
	}
}

void
OpGadgetClass::DumpFeaturesL(TempBuffer *buf)
{
	buf->AppendL(UNI_L("features=["));

	OpGadgetFeature *feature = static_cast<OpGadgetFeature*>(m_features.First());
	while (feature)
	{
		feature->DumpFeatureL(buf);

		feature = static_cast<OpGadgetFeature*>(feature->Suc());

		if (feature)
			buf->AppendL(UNI_L(", "));
	}

	buf->AppendL(UNI_L("]\n"));
}

void
OpGadgetIcon::DumpIconL(TempBuffer *buf)
{
	buf->AppendL(UNI_L("[src=\""));
	OpGadgetClass::DumpStringL(buf, m_resolved_path.CStr());
	if (m_alt.HasContent())
	{
		buf->AppendL(UNI_L("\", alt=\""));
		OpGadgetClass::DumpStringL(buf, Alt());
		buf->AppendL(UNI_L("\""));
	}
	else
		buf->AppendL(UNI_L("\", alt=null"));
	buf->AppendL(UNI_L(", width="));
	Width() > 0 ? buf->AppendUnsignedLongL(Width()) : buf->AppendL(UNI_L("null"));
	buf->AppendL(UNI_L(", height="));
	Height() > 0 ? buf->AppendUnsignedLongL(Height()) : buf->AppendL(UNI_L("null"));
	buf->AppendL(UNI_L("]"));
}

void
OpGadgetClass::DumpIconsL(TempBuffer *buf)
{
	buf->AppendL(UNI_L("icons=["));
	ValidateIcons();
	OpGadgetIcon *icon = static_cast<OpGadgetIcon*>(m_icons.First());
	while (icon)
	{
		buf->AppendL(UNI_L("[src=\""));
		icon->DumpIconL(buf);
		icon = static_cast<OpGadgetIcon*>(icon->Suc());

		if (icon)
			buf->AppendL(UNI_L(", "));
	}

	buf->AppendL(UNI_L("]\n"));
}

void
OpGadgetClass::DumpPreferencesL(TempBuffer *buf)
{
	buf->AppendL(UNI_L("preferences=["));

	OpGadgetPreference *pref = static_cast<OpGadgetPreference*>(m_default_preferences.First());
	while (pref)
	{
		buf->AppendL(UNI_L("[name=\""));
		DumpStringL(buf, pref->Name());
		buf->AppendL(UNI_L("\", value=\""));
		DumpStringL(buf, pref->Value());
		buf->AppendL(UNI_L("\", readonly=\""));
		buf->AppendL(pref->IsReadOnly() ? UNI_L("true") : UNI_L("false"));
		buf->AppendL(UNI_L("\"]"));

		pref = static_cast<OpGadgetPreference*>(pref->Suc());

		if (pref)
			buf->AppendL(UNI_L(", "));
	}

	buf->AppendL(UNI_L("]\n"));
}

void
OpGadgetClass::DumpAccessL(TempBuffer *buf)
{
	buf->AppendL(UNI_L("access=["));

	OpGadgetAccess *access = static_cast<OpGadgetAccess*>(m_access.First());
	while (access)
	{
		buf->AppendL(UNI_L("[origin=\""));
		DumpStringL(buf, access->Name());
		buf->AppendL(UNI_L("\", subdomains=\""));
		buf->AppendL(access->Subdomains() ? UNI_L("true") : UNI_L("false"));
		buf->AppendL(UNI_L("\"]"));

		access = static_cast<OpGadgetAccess*>(access->Suc());

		if (access)
			buf->AppendL(UNI_L(", "));
	}

	buf->AppendL(UNI_L("]\n"));
}

void
OpGadgetClass::DumpLocalesL(TempBuffer *buf)
{
	buf->AppendL(UNI_L("user agent locales=["));

	OpString languages;
	OpGadgetManager::GadgetLocaleIterator locale_iterator = g_gadget_manager->GetBrowserLocaleIterator();
	const uni_char* tok;
	while ((tok = locale_iterator.GetNextLocale()) != NULL)
	{
		buf->AppendL(UNI_L("\""));
		buf->AppendL(tok);
		buf->AppendL(UNI_L("\", "));
	}
	BOOL set;
	const uni_char* default_locale = GetAttribute(WIDGET_DEFAULT_LOCALE, &set);
	if (set && default_locale)
	{
		buf->AppendL(UNI_L("\""));
		buf->AppendL(default_locale);
		buf->AppendL(UNI_L("\", "));
	}
	buf->AppendL(UNI_L("\"*\"]\n"));
}

OP_STATUS
OpGadgetClass::DumpConfiguration(TempBuffer *buf)
{
	TRAPD(err, DumpConfigurationL(buf));
	return err;
}

void
OpGadgetClass::DumpConfigurationL(TempBuffer *buf)
{
	// missing
	//
	// preferences
	// user agent locales

	// broken
	//
	// widget config doc
	// window modes
	// widget start file
	// start file

	const uni_char *ns;
	if (SupportsNamespace(GADGETNS_W3C_1_0)) ns = UNI_L("http://www.w3.org/ns/widgets");
	else if (SupportsNamespace(GADGETNS_OPERA)) ns = UNI_L("");
	else if (SupportsNamespace(GADGETNS_OPERA_2006)) ns = UNI_L("http://xmlns.opera.com/2006/widget");
	else ns = UNI_L("UNKNOWN");
	DumpStringL(buf, UNI_L("namespace"), ns);

	const uni_char *type;
	switch (GetContentType())
	{
	case URL_GADGET_INSTALL_CONTENT: type = UNI_L("widget"); break;
#ifdef WEBSERVER_SUPPORT
	case URL_UNITESERVICE_INSTALL_CONTENT: type = UNI_L("webserver"); break;
#endif
#ifdef EXTENSION_SUPPORT
	case URL_EXTENSION_INSTALL_CONTENT: type = UNI_L("extension"); break;
#endif
	default: type = UNI_L("UNKNOWN");
	}
	DumpStringL(buf, UNI_L("widget type"), type);

	INT32 network = GetAttribute(WIDGET_ACCESS_ATTR_NETWORK);
	OpString network_type; ANCHOR(OpString, network_type);
	if (network & GADGET_NETWORK_PRIVATE) LEAVE_IF_ERROR(OpGadgetUtils::AppendToList(network_type, "private"));
	if (network & GADGET_NETWORK_PUBLIC) LEAVE_IF_ERROR(OpGadgetUtils::AppendToList(network_type, "public"));
	DumpStringL(buf, UNI_L("network type"), network_type.CStr());

	INT32 viewmodes  = GetAttribute(WIDGET_ATTR_MODE);
	OpString viewmodes_str; ANCHOR(OpString, viewmodes_str);
	if (viewmodes & WINDOW_VIEW_MODE_UNKNOWN) LEAVE_IF_ERROR(OpGadgetUtils::AppendToList(viewmodes_str, "unknown"));
	if (viewmodes & WINDOW_VIEW_MODE_WIDGET) LEAVE_IF_ERROR(OpGadgetUtils::AppendToList(viewmodes_str, "widget"));
	if (viewmodes & WINDOW_VIEW_MODE_FLOATING) LEAVE_IF_ERROR(OpGadgetUtils::AppendToList(viewmodes_str, "floating"));
	if (viewmodes & WINDOW_VIEW_MODE_DOCKED) LEAVE_IF_ERROR(OpGadgetUtils::AppendToList(viewmodes_str, "docked"));
	if (viewmodes & WINDOW_VIEW_MODE_APPLICATION) LEAVE_IF_ERROR(OpGadgetUtils::AppendToList(viewmodes_str, "application"));
	if (viewmodes & WINDOW_VIEW_MODE_FULLSCREEN) LEAVE_IF_ERROR(OpGadgetUtils::AppendToList(viewmodes_str, "fullscreen"));
	if (viewmodes & WINDOW_VIEW_MODE_WINDOWED) LEAVE_IF_ERROR(OpGadgetUtils::AppendToList(viewmodes_str, "windowed"));
	if (viewmodes & WINDOW_VIEW_MODE_MAXIMIZED) LEAVE_IF_ERROR(OpGadgetUtils::AppendToList(viewmodes_str, "maximized"));
	if (viewmodes & WINDOW_VIEW_MODE_MINIMIZED) LEAVE_IF_ERROR(OpGadgetUtils::AppendToList(viewmodes_str, "minimized"));
	DumpStringL(buf, UNI_L("window modes"), viewmodes_str.CStr());

	DumpAttributeL(buf, UNI_L("author email"), WIDGET_AUTHOR_ATTR_EMAIL);
	DumpAttributeL(buf, UNI_L("author href"), WIDGET_AUTHOR_ATTR_HREF);
	DumpAttributeL(buf, UNI_L("author name"), WIDGET_AUTHOR_TEXT);
	DumpAttributeL(buf, UNI_L("start file encoding"), WIDGET_CONTENT_ATTR_CHARSET);
	DumpAttributeL(buf, UNI_L("start file content-type"), WIDGET_CONTENT_ATTR_TYPE);
	DumpStringL(buf, UNI_L("config doc"), m_config_file.CStr());
	DumpAttributeL(buf, UNI_L("description"), WIDGET_DESCRIPTION_TEXT);
	DumpAttributeL(buf, UNI_L("height"), WIDGET_ATTR_HEIGHT);
	DumpAttributeL(buf, UNI_L("id"), WIDGET_ATTR_ID);
	DumpAttributeL(buf, UNI_L("license"), WIDGET_LICENSE_TEXT);
	//DumpAttributeL(buf, UNI_L("license file"), WIDGET_LICENSE_FILE);
	DumpAttributeL(buf, UNI_L("license href"), WIDGET_LICENSE_ATTR_HREF);
	DumpAttributeL(buf, UNI_L("name"), WIDGET_NAME_TEXT);
	DumpPreferencesL(buf);
	DumpAttributeL(buf, UNI_L("short name"), WIDGET_NAME_SHORT);
	DumpAttributeL(buf, UNI_L("version"), WIDGET_ATTR_VERSION);
	DumpAttributeL(buf, UNI_L("width"), WIDGET_ATTR_WIDTH);
	DumpStringL(buf, UNI_L("start file"), m_gadget_file.CStr());

	OpString resolved_path, locale;
	ANCHOR(OpString, resolved_path);
	ANCHOR(OpString, locale);
	BOOL found = FALSE;
	OP_STATUS rc =
		g_gadget_manager->Findi18nPath(this, m_gadget_file.CStr(), HasLocalesFolder(), resolved_path, found, &locale);
	if (OpStatus::IsSuccess(rc))
	{
		DumpStringL(buf, UNI_L("resolved start file"), resolved_path);
		DumpStringL(buf, UNI_L("start file locale"), locale);
		DumpStringL(buf, UNI_L("start file found"), found ? UNI_L("true") : UNI_L("false"));
	}
	else if (OpStatus::IsMemoryError(rc))
	{
		LEAVE(rc);
	}

	DumpAttributeL(buf, UNI_L("update-description href"), WIDGET_UPDATE_ATTR_URI);

	DumpLocalesL(buf);
	DumpFeaturesL(buf);
	DumpIconsL(buf);
	DumpAccessL(buf);
}
#endif // GADGET_DUMP_TO_FILE

void
OpGadgetClass::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
#ifdef SIGNED_GADGET_SUPPORT
	if (msg == MSG_GADGET_SIGNATURE_VERIFICATION_FINISHED && par1 == m_signature_verifier->Id())
	{
		// m_signature_verifier has done its job and is not needed any more.
		OP_ASSERT(m_signature_verifier);
		OP_DELETE(m_signature_verifier);
		m_signature_verifier = NULL;

		OP_ASSERT(m_signature_storage);
		CryptoXmlSignature::VerifyError error = m_signature_storage->GetCommonVerifyError();

		switch (error)
		{
			case CryptoXmlSignature::OK_CHECKED_WITH_OCSP:
			case CryptoXmlSignature::OK_CHECKED_LOCALLY:
			{
				m_sign_state = GADGET_SIGNATURE_SIGNED;

				const GadgetSignature* signature = m_signature_storage->GetBestSignature();
				OP_ASSERT(signature);
				OP_ASSERT(signature->GetVerifyError() == error);

				const CryptoCertificateChain* chain = signature->GetCertificateChain();
				OP_ASSERT(chain);

				const CryptoCertificate* cert = chain->GetCertificate(0);
				OP_ASSERT(cert);

				RAISE_AND_RETURN_VOID_IF_ERROR(m_signature_id.Set(cert->GetShortName()));

#if defined(GADGET_PRIVILEGED_SIGNING_SUPPORT) && defined(SECMAN_PRIVILEGED_GADGET_CERTIFICATE)
				int chain_length = chain->GetNumberOfCertificates();
				OP_ASSERT(chain_length > 0);

				const CryptoCertificate* ca_certificate = chain->GetCertificate(chain_length - 1);
				OP_ASSERT(ca_certificate);

				int root_cert_hash_length = 0;
				const byte *root_cert_hash = ca_certificate->GetCertificateHash(root_cert_hash_length);
				OP_ASSERT(root_cert_hash != NULL);

				int privileged_cert_hash_length = 0;
				CryptoCertificate *privileged_certificate = g_secman_instance->GetGadgetPrivilegedCA();
				if (privileged_certificate)
				{
					const byte *privileged_cert_hash = privileged_certificate->GetCertificateHash(privileged_cert_hash_length);

					if (privileged_cert_hash_length != 0
						&& root_cert_hash_length == privileged_cert_hash_length
						&& op_memcmp(privileged_cert_hash, root_cert_hash, root_cert_hash_length) == 0)
					{
						m_is_signed_with_privileged_cert = TRUE;
					}
				}
#endif // defined(GADGET_PRIVILEGED_SIGNING_SUPPORT) && defined(SECMAN_PRIVILEGED_GADGET_CERTIFICATE)

				break;
			}

			case CryptoXmlSignature::SIGNATURE_FILE_MISSING:
			case CryptoXmlSignature::WIDGET_ERROR_NOT_A_ZIPFILE:
				m_sign_state = GADGET_SIGNATURE_UNSIGNED;
				break;

			default:
				m_sign_state = GADGET_SIGNATURE_VERIFICATION_FAILED;
		} // switch (error)

		g_gadget_manager->SignatureVerified(this);
	}
#endif // SIGNED_GADGET_SUPPORT
}

#undef GADGETS_BIDI_PDE

BOOL OpGadgetClass::SupportsOperaExtendedAPI()
{
	return SupportsNamespace(GADGETNS_OPERA) || SupportsNamespace(GADGETNS_OPERA_2006) || HasFeature("http://opera.com/widgets/features/api");
}

#endif // GADGET_SUPPORT
