/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file OpGadgetParsers.cpp
 *
 * Implementation of OpGadgetParsers class.
 *
 * @author wmaslowski@opera.com
*/
#include "core/pch.h"

#ifdef GADGET_SUPPORT
#include "modules/gadgets/OpGadgetParsers.h"

#include "modules/gadgets/OpGadgetManager.h"
#include "modules/gadgets/OpGadgetClass.h"

#include "modules/xmlutils/xmlfragment.h"

GadgetConfigurationParserElement::GadgetConfigurationParserElement(const uni_char* element_name, const uni_char* xml_namespace, ConfigurationElementHandler handler, BOOL multiple, BOOL translatable)
		: m_element_name(xml_namespace, element_name)
		, m_handler(handler)
		, m_multiple(multiple)
		, m_translatable(translatable)
		, m_last_language_prio(INT_MAX)
		, m_occurrences(0)
{
}

void GadgetConfigurationParserElement::Clean()
{
	m_last_language_prio = INT_MAX;
	m_occurrences = 0;
}

OP_STATUS
OpGadgetParsers::AddElement(OpAutoVector<GadgetConfigurationParserElement>& elements, const uni_char* element_name, const uni_char* xml_namespace, GadgetConfigurationParserElement::ConfigurationElementHandler handler, BOOL multiple, BOOL translatable)
{
	OpAutoPtr<GadgetConfigurationParserElement> element(OP_NEW(GadgetConfigurationParserElement, (element_name, xml_namespace, handler, multiple, translatable)));
	RETURN_OOM_IF_NULL(element.get());
	RETURN_IF_ERROR(elements.Add(element.get()));
	element.release();
	return OpStatus::OK;
}

/* static */ OP_STATUS
OpGadgetParsers::Make(OpGadgetParsers*& parsers)
{
	parsers = OP_NEW(OpGadgetParsers, ());
	RETURN_OOM_IF_NULL(parsers);
	return parsers->Construct();
}

OP_STATUS OpGadgetParsers::Construct()
{
	const uni_char *widget_namespace = UNI_L("http://www.w3.org/ns/widgets");
#ifdef DOM_JIL_API_SUPPORT
	const uni_char *jil_1_2_2_namespace = UNI_L("http://www.jil.org/ns/widgets1.2");
	const uni_char *jil_1_1_namespace = UNI_L("http://www.jil.org/ns/widgets");
#endif // DOM_JIL_API_SUPPORT
	const uni_char *opera_2006_namespace = UNI_L("http://xmlns.opera.com/2006/widget");

	RETURN_IF_ERROR(AddElement(m_top_level_config_parser_elements, UNI_L("widget"), widget_namespace, &OpGadgetParsers::ProcessW3CWidgetTag , FALSE, FALSE));
#ifdef DOM_JIL_API_SUPPORT
	RETURN_IF_ERROR(AddElement(m_top_level_config_parser_elements, UNI_L("widget"), jil_1_1_namespace, &OpGadgetParsers::ProcessJIL1_1WidgetTag , FALSE, FALSE));
#endif // DOM_JIL_API_SUPPORT
	RETURN_IF_ERROR(AddElement(m_top_level_config_parser_elements, UNI_L("widget"), opera_2006_namespace, &OpGadgetParsers::ProcessOpera2006WidgetTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_top_level_config_parser_elements, UNI_L("widget"), NULL, &OpGadgetParsers::ProcessOperaWidgetTag , FALSE, FALSE));


	//                                                     Tag Name,      Namespace,            Handler function,       multiple? translatable?
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("icon"), widget_namespace, &OpGadgetParsers::ProcessIconTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("access"), widget_namespace, &OpGadgetParsers::ProcessAccessTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("content"), widget_namespace, &OpGadgetParsers::ProcessContentTag , FALSE, TRUE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("license"), widget_namespace, &OpGadgetParsers::ProcessLicenseTag , FALSE, TRUE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("description"), widget_namespace, &OpGadgetParsers::ProcessDescriptionTag , FALSE, TRUE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("author"), widget_namespace, &OpGadgetParsers::ProcessAuthorTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("name"), widget_namespace, &OpGadgetParsers::ProcessNameTag , FALSE, TRUE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("feature"), widget_namespace, &OpGadgetParsers::ProcessFeatureTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("preference"), widget_namespace, &OpGadgetParsers::ProcessPreferenceTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("update-description"), widget_namespace, &OpGadgetParsers::ProcessUpdateDescriptionTag , FALSE, FALSE));
#ifdef DOM_JIL_API_SUPPORT
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("access"), jil_1_2_2_namespace, &OpGadgetParsers::ProcessJILAccessTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("maximum_display_mode"), jil_1_2_2_namespace, &OpGadgetParsers::ProcessJILMaximumDisplayModeTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("billing"), jil_1_2_2_namespace, &OpGadgetParsers::ProcessJILBillingTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("update"), jil_1_2_2_namespace, &OpGadgetParsers::ProcessJILUpdateTag , FALSE, FALSE));

	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("icon"), jil_1_1_namespace, &OpGadgetParsers::ProcessIconTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("access"), jil_1_1_namespace, &OpGadgetParsers::ProcessJILAccessTag , FALSE, FALSE)); // Handle it as JIL access not W3C!
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("content"), jil_1_1_namespace, &OpGadgetParsers::ProcessJILContentTag , FALSE, TRUE)); // TODO : check if we can't just use W3C one
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("license"), jil_1_1_namespace, &OpGadgetParsers::ProcessLicenseTag , FALSE, TRUE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("description"), jil_1_1_namespace, &OpGadgetParsers::ProcessDescriptionTag , FALSE, TRUE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("author"), jil_1_1_namespace, &OpGadgetParsers::ProcessAuthorTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("name"), jil_1_1_namespace, &OpGadgetParsers::ProcessNameTag , FALSE, TRUE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("feature"), jil_1_1_namespace, &OpGadgetParsers::ProcessFeatureTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("preference"), jil_1_1_namespace, &OpGadgetParsers::ProcessPreferenceTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("maximum_display_mode"), jil_1_1_namespace, &OpGadgetParsers::ProcessJILMaximumDisplayModeTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("billing"), jil_1_1_namespace, &OpGadgetParsers::ProcessJILBillingTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("update"), jil_1_1_namespace, &OpGadgetParsers::ProcessJILUpdateTag , FALSE, FALSE));
#endif // DOM_JIL_API_SUPPORT

	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("icon"), NULL, &OpGadgetParsers::ProcessOperaIconTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("widgetfile"), NULL, &OpGadgetParsers::ProcessOperaWidgetFileTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("widgettype"), NULL, &OpGadgetParsers::ProcessOperaWidgetTypeTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("description"), NULL, &OpGadgetParsers::ProcessDescriptionTag , FALSE, TRUE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("author"), NULL, &OpGadgetParsers::ProcessOperaAuthorTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("widgetname"), NULL, &OpGadgetParsers::ProcessOperaWidgetNameTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("width"), NULL, &OpGadgetParsers::ProcessOperaWidgetWidthTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("height"), NULL, &OpGadgetParsers::ProcessOperaWidgetHeightTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("security"), NULL, &OpGadgetParsers::ProcessOperaWidgetSecurityTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("servicepath"), NULL, &OpGadgetParsers::ProcessOperaWidgetServicepathTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("file"), NULL, &OpGadgetParsers::ProcessOperaFileTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("feature"), NULL, &OpGadgetParsers::ProcessFeatureTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("id"), NULL, &OpGadgetParsers::ProcessOperaIdTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("update-description"), NULL, &OpGadgetParsers::ProcessUpdateDescriptionTag , FALSE, FALSE));

	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("icon"), opera_2006_namespace, &OpGadgetParsers::ProcessOperaIconTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("widgetfile"), opera_2006_namespace, &OpGadgetParsers::ProcessOperaWidgetFileTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("widgettype"), opera_2006_namespace, &OpGadgetParsers::ProcessOperaWidgetTypeTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("description"), opera_2006_namespace, &OpGadgetParsers::ProcessDescriptionTag , FALSE, TRUE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("author"), opera_2006_namespace, &OpGadgetParsers::ProcessOperaAuthorTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("width"), opera_2006_namespace, &OpGadgetParsers::ProcessOperaWidgetWidthTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("height"), opera_2006_namespace, &OpGadgetParsers::ProcessOperaWidgetHeightTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("security"), opera_2006_namespace, &OpGadgetParsers::ProcessOperaWidgetSecurityTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("servicepath"), opera_2006_namespace, &OpGadgetParsers::ProcessOperaWidgetServicepathTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("file"), opera_2006_namespace, &OpGadgetParsers::ProcessOperaFileTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("feature"), opera_2006_namespace, &OpGadgetParsers::ProcessFeatureTag , TRUE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("id"), opera_2006_namespace, &OpGadgetParsers::ProcessOperaIdTag , FALSE, FALSE));
	RETURN_IF_ERROR(AddElement(m_config_parser_elements, UNI_L("update-description"), opera_2006_namespace, &OpGadgetParsers::ProcessUpdateDescriptionTag , FALSE, FALSE));
	return OpStatus::OK;
}

void OpGadgetParsers::CleanParserElements()
{
	for (unsigned int i = 0; i < m_config_parser_elements.GetCount(); ++i)
	{
		GadgetConfigurationParserElement* element = m_config_parser_elements.Get(i);
		OP_ASSERT(element);
		element->Clean();
	}

	for (unsigned int j = 0; j < m_top_level_config_parser_elements.GetCount(); ++j)
	{
		GadgetConfigurationParserElement* element = m_top_level_config_parser_elements.Get(j);
		OP_ASSERT(element);
		element->Clean();
	}
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessTopLevelElement(OpGadgetClass* klass, XMLFragment* f, OpString& error_reason)
{
	BOOL element_handled = FALSE;
	for (unsigned int i = 0; i < m_top_level_config_parser_elements.GetCount(); ++i)
	{
		GadgetConfigurationParserElement* element = m_top_level_config_parser_elements.Get(i);
		OP_ASSERT(element);
		if (f->EnterElement(element->m_element_name))
		{
			element_handled = TRUE;
			OP_GADGET_STATUS error = element->m_handler(klass, f, error_reason);
			if (OpStatus::IsSuccess(error))
			{
				error = ProcessSecondLevelElements(klass, f, error_reason);
			}
			f->LeaveElement();
			RETURN_IF_ERROR(error);
			break;
		}
	}
	if (!element_handled)
	{
		OpStatus::Ignore(error_reason.Set("No valid top level <widget> element found."));
		return OpGadgetStatus::ERR_WRONG_WIDGET_FORMAT;
	}
	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessSecondLevelElements(OpGadgetClass* klass, XMLFragment* f, OpString& error_reason)
{
	BOOL default_locale_set;
	const uni_char *default_locale = klass->GetAttribute(WIDGET_DEFAULT_LOCALE, &default_locale_set);
	while (f->HasMoreElements())
	{
		BOOL element_handled = FALSE;
		// TODO: this can be made faster using sorted arrey + binary search or using hash map
		for (unsigned int i = 0; i < m_config_parser_elements.GetCount(); ++i)
		{
			GadgetConfigurationParserElement* element = m_config_parser_elements.Get(i);
			OP_ASSERT(element);
			if (f->EnterElement(element->m_element_name))
			{
				element_handled = TRUE;
				OP_GADGET_STATUS error = OpStatus::OK;
				if (PrepareElementForProcessing(*element, f, default_locale))
				{
					error = element->m_handler(klass, f, error_reason);
				}
				f->LeaveElement();
				RETURN_IF_ERROR(error);
				break;
			}
		}
		if (!element_handled)
		{
			f->EnterAnyElement();
			const XMLCompleteName& element_name = f->GetElementName();
			GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: unsupported element <%s> ignored."), element_name.GetLocalPart()));
			f->LeaveElement();
		}
	}
	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessGadgetConfigurationFile(OpGadgetClass* klass, XMLFragment* config_file, OpString& error_reason)//(GadgetConfigurationParserElement* elements, unsigned int count, XMLFragment* f, OpString& error_reason)
{
	CleanParserElements();
	RETURN_IF_ERROR(ProcessTopLevelElement(klass, config_file, error_reason));
	return ValidateProcessedGadget(klass, error_reason);
}

BOOL
OpGadgetParsers::PrepareElementForProcessing(GadgetConfigurationParserElement& element, XMLFragment* f, const uni_char* default_locale)
{
	if (element.m_translatable)
	{
		const uni_char *xmllang = GetElementLang(f);
		int language_priority = xmllang ? g_gadget_manager->GetLanguagePriority(xmllang, default_locale) : INT_MAX;
		if (language_priority == 0) // if language_priority == 0 then the language is incompatible -> don't process it.
			return FALSE;
		if (language_priority < element.m_last_language_prio) // higher language priority then last element we saw -> process it.
		{
			++element.m_occurrences;
			element.m_last_language_prio = language_priority;
			return TRUE;
		}
	}
	if (element.m_multiple || element.m_occurrences == 0) // first ocurrence of the element or element can occur multiple tasks -> process it.
	{
		++element.m_occurrences;
		return TRUE;
	}
	return FALSE; // otherwise ignore it.
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessIconTagGeneric(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason, BOOL src_is_attr)
{
	OpString src;
	if (src_is_attr)
		RETURN_IF_ERROR(this_class->GetAttributeString(f, UNI_L("src"), src));
	else // src is content.
		RETURN_IF_ERROR(src.Set(f->GetText()));

	if (src.IsEmpty())
	{
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: Empty icon path - element ignored.")));
		return OpStatus::OK;
	}
	int width = OpGadgetUtils::AttrStringToI(f->GetAttribute(UNI_L("width")), 0);
	int height = OpGadgetUtils::AttrStringToI(f->GetAttribute(UNI_L("height")), 0);

	OpGadgetIcon* gadget_icon;
	RETURN_IF_ERROR(OpGadgetIcon::Make(&gadget_icon, src.CStr(), width, height));
	OpAutoPtr<OpGadgetIcon> icon_anchor(gadget_icon);

	if (OpStatus::IsSuccess(this_class->ProcessIcon(gadget_icon))) //TODO: fix this fun
		icon_anchor.release();
	else
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: Could not load icon file '%s'."), gadget_icon->Src()));
	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessAccessTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	OpString origin;
	RETURN_IF_ERROR(this_class->GetAttributeIRI(f, UNI_L("origin"), origin, TRUE));

	BOOL subdomains;
	RETURN_IF_ERROR(this_class->GetAttributeBoolean(f, UNI_L("subdomains"), subdomains, FALSE));

	if (origin.IsEmpty())
	{
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: Empty <access> origin - element ignored.")));
		return OpStatus::OK;
	}

	OpGadgetAccess *access;
	RETURN_IF_ERROR(OpGadgetAccess::Make(&access, origin.CStr(), subdomains));
	OpAutoPtr<OpGadgetAccess> access_anchor(access);

	OP_GADGET_STATUS error = OpSecurityManager::AddGadgetPolicy(this_class, access, error_reason);
	if (OpStatus::IsSuccess(error))
		access_anchor.release()->Into(&this_class->m_access);
	else if (error == OpStatus::ERR_PARSING_FAILED)
	{
		// Access tag is invalid - ignore it
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: %s."), error_reason.CStr()));
		return OpStatus::OK;
	}
	return error;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessContentTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	OpString src;
	RETURN_IF_ERROR(this_class->GetAttributeString(f, UNI_L("src"), src));

	if (!src.IsEmpty())
	{
		// Convert path string to use local path separator. To catch uses of
		// reserved characters, we move away any actual references to the the
		// local path separator.
#if PATHSEPCHAR != '/'
		OpString local_path;
		RETURN_IF_ERROR(local_path.Set(src));
		OpGadgetUtils::ChangeToLocalPathSeparators(local_path.CStr());
#else
		OpStringC local_path(src);
#endif

		OpString localepath;
		BOOL found = FALSE;
		OpStatus::Ignore(g_gadget_manager->Findi18nPath(this_class, local_path, this_class->HasLocalesFolder(), localepath, found, NULL));
		if (found)
		{
			RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_CONTENT_ATTR_SRC, src.CStr()));
			RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_CONTENT_ATTR_CHARSET, f, UNI_L("encoding")));

			// Check for type="mimetype;charset=charset"
			OpString type;
			RETURN_IF_ERROR(this_class->GetAttributeString(f, UNI_L("type"), type));

			uni_char *charset = NULL;
			if (type.HasContent() && (charset = uni_strchr(type.CStr(), ';')))
			{
				*charset = '\0';
				RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_CONTENT_ATTR_TYPE, type.CStr()));

				if  (uni_strncmp(++charset, UNI_L("charset="), 8) == 0)
					RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_CONTENT_ATTR_CHARSET, charset + 8));
			}

			// Default behavior
			RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_CONTENT_ATTR_TYPE, f, UNI_L("type")));
		}
		else
			GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: Start file '%s' specified but not found in widget."), local_path.CStr()));
	}
	else
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL,UNI_L("Widget loading warning: Empty <content> src.")));

	return OpStatus::OK;
}

const uni_char* OpGadgetParsers::GetElementLang(XMLFragment* f)
{
	XMLExpandedName xml_lang_attr(UNI_L("http://www.w3.org/XML/1998/namespace"), UNI_L("lang"));
	return f->GetAttribute(xml_lang_attr);
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessLicenseTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	RETURN_IF_ERROR(this_class->SetAttributeElementStringBIDI(WIDGET_LICENSE_TEXT, f, FALSE, TRUE));

	const uni_char *xmllang = GetElementLang(f);
	if (xmllang)
		RETURN_IF_ERROR(this_class->SetLocaleForAttribute(WIDGET_LOCALIZED_LICENSE, xmllang));

	const uni_char* license_iri = f->GetAttribute(UNI_L("href"));
	if (OpGadgetUtils::IsValidIRI(license_iri))
		RETURN_IF_ERROR(this_class->SetAttributeIRI(WIDGET_LICENSE_ATTR_HREF, f, UNI_L("href")));
	else
	{
		// Check if it is a valid path within the archive.
		BOOL found = FALSE;
#if PATHSEPCHAR != '/'
		OpString path;
		RETURN_IF_ERROR(path.Set(license_iri));
		OpGadgetUtils::ChangeToLocalPathSeparators(path.DataPtr());
#else
		OpStringC path(license_iri);
#endif
		OpString unused;
		g_gadget_manager->Findi18nPath(this_class, path, this_class->HasLocalesFolder(), unused, found, NULL);
		if (!found)
			GADGETS_REPORT_ERROR_TO_CONSOLE((NULL,UNI_L("Widget loading warning: License href='%s' is neither a valid IRI, nor a path within a widget - ignoring this attribute."), license_iri));
		else
			return this_class->SetAttribute(WIDGET_LICENSE_ATTR_HREF, path.CStr());
	}
	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessDescriptionTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	RETURN_IF_ERROR(this_class->SetAttributeElementStringBIDI(WIDGET_DESCRIPTION_TEXT, f, FALSE, TRUE));

	const uni_char *xmllang = GetElementLang(f);
	if (xmllang)
		RETURN_IF_ERROR(this_class->SetLocaleForAttribute(WIDGET_LOCALIZED_DESCRIPTION, xmllang));
	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessAuthorTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_AUTHOR_ATTR_EMAIL, f, UNI_L("email")));
	RETURN_IF_ERROR(this_class->SetAttributeIRI(WIDGET_AUTHOR_ATTR_HREF, f, UNI_L("href")));
	RETURN_IF_ERROR(this_class->SetAttributeElementStringBIDI(WIDGET_AUTHOR_TEXT, f));
	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessNameTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	RETURN_IF_ERROR(this_class->SetAttributeElementStringBIDI(WIDGET_NAME_TEXT, f, TRUE, TRUE));

	const uni_char *dir = f->GetAttribute(UNI_L("dir"));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_NAME_SHORT, f, UNI_L("short"), TRUE, this_class->GetBIDIMarker(dir)));

	const uni_char *xmllang = GetElementLang(f);
	if (xmllang)
		RETURN_IF_ERROR(this_class->SetLocaleForAttribute(WIDGET_LOCALIZED_NAME, xmllang));
	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessFeatureTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	OpString name;
	RETURN_IF_ERROR(this_class->GetAttributeIRI(f, UNI_L("name"), name));

	BOOL required;
	RETURN_IF_ERROR(this_class->GetAttributeBoolean(f, UNI_L("required"), required, TRUE));

	if (name.IsEmpty())
	{
		const uni_char* name_attr = f->GetAttribute(UNI_L("name"));
		if (!name_attr || !*name_attr)
		{
			GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: ignored <feature> element - empty 'name' attribute.")));
			return OpStatus::OK;
		}
		else if (required)
		{
			error_reason.AppendFormat(UNI_L("Invalid <feature> element - 'name' attribute('%s') is not a valid IRI and required=true"), name_attr);
			return OpGadgetStatus::ERR_WRONG_WIDGET_FORMAT;
		}
		else
		{
			GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: ignored <feature> element: ('%s') - 'name' attribute is not a valid IRI."), name.CStr()));
			return OpStatus::OK;
		}
	}

	OpGadgetFeature *feature;
	RETURN_IF_ERROR(OpGadgetFeature::Make(&feature, name.CStr(), required));
	OpAutoPtr<OpGadgetFeature> feature_anchor(feature);

	while (f->HasMoreElements())
	{
		if (f->EnterAnyElement())
		{
			const XMLCompleteName& element_name = f->GetElementName();
			OP_GADGET_STATUS error = OpStatus::OK;
			if (uni_str_eq(element_name.GetLocalPart(), UNI_L("param")))
				error = ProcessParamTag(this_class, f, error_reason, feature);
			else
			{
				const XMLCompleteName& element_name = f->GetElementName();
				GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: unsupported element %s inside <feature> element ignored."), element_name.GetLocalPart()));
			}
			f->LeaveElement(); // param
			RETURN_IF_ERROR(error);
		}
	}

	// Check feature and enable said feature if possible
	OP_GADGET_STATUS err = this_class->ProcessFeature(feature, error_reason);
	if (OpStatus::IsSuccess(err))
	{
		// if supported, add it
		if (err != OpGadgetStatus::OK_UNKNOWN_FEATURE)
			feature_anchor.release()->Into(&this_class->m_features);
		else
			GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Unsupported optional feature '%s' element ignored."), name.CStr()));
		return OpStatus::OK;
	}
	return err;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessParamTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason, OpGadgetFeature* feature)
{
	OpString name, value;
	RETURN_IF_ERROR(this_class->GetAttributeString(f, UNI_L("name"), name));
	RETURN_IF_ERROR(this_class->GetAttributeString(f, UNI_L("value"), value));
	if (!name.HasContent())
	{
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: Feature <param> ignored because it has no 'name' attribute.")));
		return OpStatus::OK;
	}
	else if(!value.HasContent())
	{
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: Feature <param> ignored because it has no 'value' attribute.")));
		return OpStatus::OK;
	}

	OpGadgetFeatureParam *param;
	RETURN_IF_ERROR(OpGadgetFeatureParam::Make(&param, name.CStr(), value.CStr()));
	feature->AddParam(param);
	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessPreferenceTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	BOOL readonly;
	RETURN_IF_ERROR(this_class->GetAttributeBoolean(f, UNI_L("readonly"), readonly));
	OpString name, value;
	RETURN_IF_ERROR(this_class->GetAttributeString(f, UNI_L("name"), name));
	RETURN_IF_ERROR(this_class->GetAttributeString(f, UNI_L("value"), value));
	if (!name.HasContent())
	{
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: <preference> element ignored because it has no 'name' attribute.")));
		return OpStatus::OK;
	}
	else if (this_class->HasPreference(name.CStr()))
	{
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: <preference> element ignored because preference with such name was already defined.")));
		return OpStatus::OK;
	}

	OpGadgetPreference *pref;
	RETURN_IF_ERROR(OpGadgetPreference::Make(&pref, name.CStr(), value.CStr(), readonly));
	pref->Into(&this_class->m_default_preferences);
	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessUpdateDescriptionTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	RETURN_IF_ERROR(this_class->SetAttributeIRI(WIDGET_UPDATE_ATTR_URI, f, UNI_L("href")));
	return OpStatus::OK;
}

#ifdef DOM_JIL_API_SUPPORT
OP_GADGET_STATUS
OpGadgetParsers::ProcessJIL1_1WidgetTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	this_class->m_namespace = GADGETNS_JIL_1_0;

	const uni_char *default_direction = f->GetAttribute(UNI_L("dir"));
	this_class->m_default_bidi_marker = this_class->GetBIDIMarker(default_direction);

	// Parse widget attributes.
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_ATTR_ID, f, UNI_L("id")));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_ATTR_VERSION, f, UNI_L("version")));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_ATTR_WIDTH, f, UNI_L("width")));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_ATTR_HEIGHT, f, UNI_L("height")));
	return OpStatus::OK;
}

OP_GADGET_STATUS OpGadgetParsers::ProcessJILAccessTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	BOOL jil_network_access_allowed = OpGadgetUtils::IsAttrStringTrue(f->GetAttribute(UNI_L("network")));
	this_class->SetAttribute(WIDGET_JIL_ATTR_ACCESS_NETWORK, jil_network_access_allowed);
	this_class->SetAttribute(WIDGET_ACCESS_ATTR_NETWORK, jil_network_access_allowed ? GADGET_NETWORK_PRIVATE | GADGET_NETWORK_PUBLIC : GADGET_NETWORK_NONE);
	RETURN_IF_ERROR(this_class->SetAttributeBoolean(WIDGET_JIL_ACCESS_ATTR_FILESYSTEM, f, UNI_L("localfs"), FALSE));
	RETURN_IF_ERROR(this_class->SetAttributeBoolean(WIDGET_JIL_ACCESS_ATTR_REMOTE_SCRIPTS, f, UNI_L("remote_scripts"), FALSE));
	return OpStatus::OK;
}

OP_GADGET_STATUS OpGadgetParsers::ProcessJILMaximumDisplayModeTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_JIL_ATTR_MAXIMUM_WIDTH, f, UNI_L("width")));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_JIL_ATTR_MAXIMUM_HEIGHT, f, UNI_L("height")));
	return OpStatus::OK;
}
OP_GADGET_STATUS OpGadgetParsers::ProcessJILBillingTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	this_class->SetAttributeBoolean(WIDGET_JIL_ATTR_BILLING, f, UNI_L("required"), TRUE);
	return OpStatus::OK;
}
OP_GADGET_STATUS OpGadgetParsers::ProcessJILUpdateTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_UPDATE_ATTR_URI, f, UNI_L("href")));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_UPDATE_ATTR_JIL_PERIOD, f, UNI_L("period")));
	return OpStatus::OK;
}

OP_GADGET_STATUS OpGadgetParsers::ProcessJILContentTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_CONTENT_ATTR_SRC, f, UNI_L("src")));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_CONTENT_ATTR_TYPE, f, UNI_L("type")));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_CONTENT_ATTR_CHARSET, f, UNI_L("charset")));
	return OpStatus::OK;
}
#endif // DOM_JIL_API_SUPPORT
OP_GADGET_STATUS OpGadgetParsers::ProcessOperaWidgetFileTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_CONTENT_ATTR_SRC, f));
	return OpStatus::OK;
}
OP_GADGET_STATUS OpGadgetParsers::ProcessOperaWidgetTypeTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	if (uni_strcmp(f->GetText(), UNI_L("windowed")) == 0)
	{
		this_class->SetAttribute(WIDGET_WIDGETTYPE, GADGET_TYPE_WINDOWED);
	}
	else if (uni_strcmp(f->GetText(), UNI_L("toolbar")) == 0)
	{
		this_class->SetAttribute(WIDGET_WIDGETTYPE, GADGET_TYPE_TOOLBAR);
	}

	this_class->SetAttribute(WIDGET_WIDGETTYPE, GADGET_TYPE_CHROMELESS);	// Set default value
	return OpStatus::OK;
}
OP_GADGET_STATUS OpGadgetParsers::ProcessOperaAuthorTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	while (f->HasMoreElements())
	{
		if (f->EnterAnyElement())
		{
			const XMLCompleteName& element_name = f->GetElementName();
			OP_GADGET_STATUS error = OpStatus::OK;
			if (uni_str_eq(element_name.GetLocalPart(), UNI_L("name")))
				error = this_class->SetAttribute(WIDGET_AUTHOR_TEXT, f->GetText());
			else if (uni_str_eq(element_name.GetLocalPart(), UNI_L("link")))
				error = this_class->SetAttribute(WIDGET_AUTHOR_ATTR_HREF, f->GetText());
			else if (uni_str_eq(element_name.GetLocalPart(), UNI_L("email")))
				error = this_class->SetAttribute(WIDGET_AUTHOR_ATTR_EMAIL, f->GetText());
			else if (uni_str_eq(element_name.GetLocalPart(), UNI_L("organization")))
				error = this_class->SetAttribute(WIDGET_AUTHOR_ATTR_ORGANIZATION, f->GetText());
			else
			{
				const XMLCompleteName& element_name = f->GetElementName();
				GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: unsupported element <%s> inside <author> element ignored."), element_name.GetLocalPart()));
			}
			f->LeaveElement();
			RETURN_IF_ERROR(error);
		}
	}
	return OpStatus::OK;
}
OP_GADGET_STATUS OpGadgetParsers::ProcessOperaWidgetNameTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_NAME_TEXT, f->GetText()));
	return OpStatus::OK;
}

OP_GADGET_STATUS OpGadgetParsers::ProcessOperaWidgetWidthTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	const uni_char *width = f->GetText();
	if (width)
		this_class->SetAttribute(WIDGET_ATTR_WIDTH, OpGadgetUtils::AttrStringToI(width, 150));
	return OpStatus::OK;
}

OP_GADGET_STATUS OpGadgetParsers::ProcessOperaWidgetHeightTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	const uni_char *height = f->GetText();
	if (height)
		this_class->SetAttribute(WIDGET_ATTR_HEIGHT, OpGadgetUtils::AttrStringToI(height, 300));
	return OpStatus::OK;
}
OP_GADGET_STATUS OpGadgetParsers::ProcessOperaWidgetSecurityTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	return OpSecurityManager::SetGadgetPolicy(this_class, f, error_reason);
}
OP_GADGET_STATUS OpGadgetParsers::ProcessOperaWidgetServicepathTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	return this_class->SetAttribute(WIDGET_SERVICEPATH_TEXT, f->GetText());
}
OP_GADGET_STATUS OpGadgetParsers::ProcessOperaFileTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	return this_class->SetAttribute(WIDGET_ACCESS_ATTR_FILES, TRUE);
}

OP_GADGET_STATUS OpGadgetParsers::ProcessOperaIdTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	OpString id;
	const uni_char *host = NULL;
	const uni_char *name = NULL;
	const uni_char *revised = NULL;

	while (f->HasMoreElements() && 	f->EnterAnyElement())
	{
		const XMLCompleteName& element_name = f->GetElementName();
		if (uni_str_eq(element_name.GetLocalPart(), UNI_L("host")))
			host = f->GetText();
		else if (uni_str_eq(element_name.GetLocalPart(), UNI_L("name")))
			name = f->GetText();
		else if (uni_str_eq(element_name.GetLocalPart(), UNI_L("revised")))
			revised = f->GetText();
		else
		{
			const XMLCompleteName& element_name = f->GetElementName();
			GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: unsupported element <%s> inside <id> element ignored."), element_name.GetLocalPart()));
		}
		f->LeaveElement();
	}

	if (host && name && revised)
	{
		RETURN_IF_ERROR(id.AppendFormat(UNI_L("http://%s/%s/%s"), host, name, revised));
		RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_ATTR_ID, id.CStr()));
	}
	else
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget loading warning: <id> element not fully defined.")));

	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessW3CWidgetTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	this_class->m_namespace = GADGETNS_W3C_1_0;
	const uni_char *default_direction = f->GetAttribute(UNI_L("dir"));
	this_class->m_default_bidi_marker = this_class->GetBIDIMarker(default_direction);

	// Network access for W3C Widgets using W3C Widget access defaults to private|public
	this_class->SetAttribute(WIDGET_ACCESS_ATTR_NETWORK, GADGET_NETWORK_PRIVATE | GADGET_NETWORK_PUBLIC);

	// Parse widget attributes.
	RETURN_IF_ERROR(this_class->SetAttributeIRI(WIDGET_ATTR_ID, f, UNI_L("id")));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_ATTR_VERSION, f, UNI_L("version")));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_ATTR_WIDTH, f, UNI_L("width")));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_ATTR_HEIGHT, f, UNI_L("height")));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_DEFAULT_LOCALE, f, UNI_L("defaultlocale")));
	RETURN_IF_ERROR(this_class->SetAttributeIRI(WIDGET_HOME_PAGE, f, UNI_L("href")));

	const uni_char *viewmodes = f->GetAttribute(UNI_L("viewmodes"));
	if (viewmodes)
	{
		OpString viewmodes_string;
		RETURN_IF_ERROR(viewmodes_string.Set(viewmodes));

		uni_char *tok;
		uni_char *token_string = viewmodes_string.CStr();

		UINT32 default_mode = 0;
		UINT32 mode = 0;
		while ((tok = uni_strtok(token_string, UNI_L(" \t\r\n\f"))) != NULL)
		{
			if (uni_strcmp(tok, "windowed") == 0)
				mode |= WINDOW_VIEW_MODE_WINDOWED;
			else if (uni_strcmp(tok, "floating") == 0)
				mode |= WINDOW_VIEW_MODE_FLOATING;
			else if (uni_strcmp(tok, "fullscreen") == 0)
				mode |= WINDOW_VIEW_MODE_FULLSCREEN;
			else if (uni_strcmp(tok, "maximized") == 0)
				mode |= WINDOW_VIEW_MODE_MAXIMIZED;
			else if (uni_strcmp(tok, "minimized") == 0)
				mode |= WINDOW_VIEW_MODE_MINIMIZED;

			token_string = NULL;

			// The first mode we find, is the default mode
			if (!default_mode)
				default_mode = mode;
		}

		// if no modes where found, default to floating
		if (!default_mode)
			default_mode = WINDOW_VIEW_MODE_FLOATING;

		this_class->SetAttribute(WIDGET_ATTR_MODE, mode);					// Set the supported modes
		this_class->SetAttribute(WIDGET_ATTR_DEFAULT_MODE, default_mode);	// Set the default viewmode (first viewmode found)
	}
	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessOpera2006WidgetTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	this_class->m_namespace = GADGETNS_OPERA_2006;
	// Some defaults for 2006 widgets
	this_class->SetAttribute(WIDGET_ACCESS_ATTR_NETWORK, GADGET_NETWORK_PRIVATE | GADGET_NETWORK_PUBLIC);
	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetParsers::ProcessOperaWidgetTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason)
{
	this_class->m_namespace = GADGETNS_OPERA;
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_ATTR_ID, f, UNI_L("id")));
	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_ATTR_VERSION, f, UNI_L("version")));

	this_class->SetAttribute(WIDGET_ATTR_DEFAULT_MODE, WINDOW_VIEW_MODE_WIDGET); // widget is the deafult mode for opera widgets
	const uni_char *defaultmode = f->GetAttribute(UNI_L("defaultmode"));
	if (defaultmode)
	{
		if (uni_strcmp(defaultmode, "docked") == 0)
			this_class->SetAttribute(WIDGET_ATTR_DEFAULT_MODE, WINDOW_VIEW_MODE_DOCKED, TRUE);
		else if (uni_strcmp(defaultmode, "application") == 0)
			this_class->SetAttribute(WIDGET_ATTR_DEFAULT_MODE, WINDOW_VIEW_MODE_APPLICATION, TRUE);
		else if (uni_strcmp(defaultmode, "fullscreen") == 0)
			this_class->SetAttribute(WIDGET_ATTR_DEFAULT_MODE, WINDOW_VIEW_MODE_FULLSCREEN, TRUE);
	}

	const uni_char *network = f->GetAttribute(UNI_L("network"));
	if (network)
	{
		OpString network_string;
		RETURN_IF_ERROR(network_string.Set(network));

		UINT32 access = this_class->GetAttribute(WIDGET_ACCESS_ATTR_NETWORK);

		uni_char *tok;
		uni_char *token_string = network_string.CStr();
		while((tok = uni_strtok(token_string, UNI_L(" \t\r\n\f"))) != NULL)
		{
			if (uni_strcmp(tok, "public") == 0)
				access |= GADGET_NETWORK_PUBLIC;
			else if (uni_strcmp(tok, "private") == 0)
				access |= GADGET_NETWORK_PRIVATE;

			token_string = NULL;
		}

		this_class->SetAttribute(WIDGET_ACCESS_ATTR_NETWORK, access);
	}

	RETURN_IF_ERROR(this_class->SetAttribute(WIDGET_ATTR_DOCKABLE, FALSE));
	RETURN_IF_ERROR(this_class->SetAttributeBoolean(WIDGET_ATTR_DOCKABLE, f, UNI_L("dockable"), FALSE, TRUE));
	RETURN_IF_ERROR(this_class->SetAttributeBoolean(WIDGET_ATTR_TRANSPARENT, f, UNI_L("transparent"), FALSE));
	RETURN_IF_ERROR(this_class->SetAttributeBoolean(WIDGET_ACCESS_ATTR_FILES, f, UNI_L("file"), FALSE));
	RETURN_IF_ERROR(this_class->SetAttributeBoolean(WIDGET_ACCESS_ATTR_JSPLUGINS, f, UNI_L("jsplugins"), FALSE));
	RETURN_IF_ERROR(this_class->SetAttributeBoolean(WIDGET_ACCESS_ATTR_WEBSERVER, f, UNI_L("webserver"), FALSE));
	return OpStatus::OK;
}

OP_GADGET_STATUS OpGadgetParsers::ValidateProcessedGadget(OpGadgetClass* this_class, OpString& error_reason)
{
	if (this_class->SupportsNamespace(GADGETNS_OPERA))
		if (!this_class->GetAttribute(WIDGET_NAME_TEXT) || !*this_class->GetAttribute(WIDGET_NAME_TEXT))
		{
			OpStatus::Ignore(error_reason.Set(UNI_L("required <widgetname> element missing or empty\n")));
			return OpStatus::ERR_PARSING_FAILED;
		}
	return OpStatus::OK;
}


#endif // GADGET_SUPPORT
