/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file OpGadgetParsers.h
 *
 * Declarations for OpGadgetParsers class.
 *
 * @author wmaslowski@opera.com
*/

#ifndef GADGETS_OP_GADGET_PARSERS_H
#define GADGETS_OP_GADGET_PARSERS_H

#ifdef GADGET_SUPPORT

#include "modules/gadgets/gadget_utils.h"
#include "modules/xmlutils/xmlnames.h"

class OpGadgetClass;
class OpGadgetParsers;

/** This class stores information about parsing one element of xml file.
 */
class GadgetConfigurationParserElement
{
public:
	/** Type alias of element handler function.
	 * @param klass - gadget klass object which configuration is being parsed.
	 * @param f - currently parsed element.
	 * @param[out] error_reason - Set to reason of parsing failure.
	 * @return OK if parsing succeeded or any apropriate OpGadgetStatus in case of failure.
	 */
	typedef OP_GADGET_STATUS (*ConfigurationElementHandler)(OpGadgetClass* klass, XMLFragment* f, OpString& error_reason);

	GadgetConfigurationParserElement(const uni_char* element_name, const uni_char* xml_namespace, ConfigurationElementHandler handler, BOOL multiple, BOOL translatable);
private:
	friend class OpGadgetParsers;
	/** Full name of an element handled */
	XMLExpandedName m_element_name;

	/** Pointer to element handler function. */
	ConfigurationElementHandler m_handler;
	/** If TRUE then this element can occurr multiple times. */
	BOOL m_multiple;
	/** If TRUE then this element can be localizable via its:lang attribute. */
	BOOL m_translatable;
	/** Language priority of last encountered occurrence of this element in the document.
	 *
	 * @see OpGadgetManager::GetLanguagePriority
	 */
	int m_last_language_prio;
	/** Number of times this element occurred in the document. */
	unsigned int m_occurrences;

	/** Resets document parsing state. */
	void Clean();
};

class OpGadgetFeature;
class XMLFragment;

/** This class encapsulates parsing of widget configuration document. */
class OpGadgetParsers
{
public:
	static OP_STATUS Make(OpGadgetParsers*& parsers);

	/** Processes widget configuration document.
	 *
	 * @param klass - gadget class object which will be configured according to configuration document.
	 * @param config_file - parsed xml representation of configuration document.
	 * @param error_reason - string describing the cause of failure in case of error.
	 * @returen OK or any appropriate OpGadgetStatus.
	 */
	OP_GADGET_STATUS ProcessGadgetConfigurationFile(OpGadgetClass* klass, XMLFragment* config_file, OpString& error_reason);

	OP_STATUS AddElement(OpAutoVector<GadgetConfigurationParserElement>& elements, const uni_char* element_name_, const uni_char* xml_namespace_, GadgetConfigurationParserElement::ConfigurationElementHandler handler_, BOOL multiple_, BOOL translatable_);
private:
	OP_STATUS Construct();

	/** Prepares element for processing and checks if the element should be processed at all taking localization and multiplicity into account.
	 *
	 * @param element - parsing halndler descriptor for the element to be processed.
	 * @param f - element to be processed.
	 * @param default_locale - default widget locale (specified by 'defaultlocale' attribute).
	 * @return TRUE if the element should be processed.
	 */
	BOOL PrepareElementForProcessing(GadgetConfigurationParserElement& element, XMLFragment* f, const uni_char* default_locale);

	/** Processes top level element of widget configuration document(and calls ProcessSecondLevelElements to parse its children).
	 *
	 * @param klass - gadget class object which will be configured according to configuration document.
	 * @param config_file - parsed xml representation of configuration document.
	 * @param error_reason - string describing the cause of failure in case of error.
	 * @returen OK or any appropriate OpGadgetStatus.
	 */
	OP_GADGET_STATUS ProcessTopLevelElement(OpGadgetClass* klass, XMLFragment* config_file, OpString& error_reason);

	/** Processes direct children of top level element of widget configuration document.
	 *
	 * @param klass - gadget class object which will be configured according to configuration document.
	 * @param config_file - parsed xml representation of configuration document.
	 * @param error_reason - string describing the cause of failure in case of error.
	 * @returen OK or any appropriate OpGadgetStatus.
	 */
	OP_GADGET_STATUS ProcessSecondLevelElements(OpGadgetClass* klass, XMLFragment* config_file, OpString& error_reason);

	/** Final validation if all the data read from configuration document is correct.
	 *
	 * For now it only checks if widget name was present for opera widgets.
	 * @param klass - gadget class object which will be configured according to configuration document.
	 * @param error_reason - string describing the cause of failure in case of error.
	 * @returen OK or any appropriate OpGadgetStatus.
	 */
	OP_GADGET_STATUS ValidateProcessedGadget(OpGadgetClass* this_class, OpString& error_reason);

	/** Helper for obtaining lang attribute */
	static const uni_char* GetElementLang(XMLFragment* f);

	/** Resets parsing state data for parser elements descriptors */
	void CleanParserElements();

	/// Element handler functions.
	/// @see GadgetConfigurationParserElement::ConfigurationElementHandler

	static OP_GADGET_STATUS ProcessW3CWidgetTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessIconTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason) { return ProcessIconTagGeneric(this_class, f, error_reason, TRUE); }
	static OP_GADGET_STATUS ProcessAccessTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessContentTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessLicenseTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessDescriptionTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessAuthorTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessNameTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessFeatureTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessParamTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason, OpGadgetFeature* feature);
	static OP_GADGET_STATUS ProcessPreferenceTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessUpdateDescriptionTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);

#ifdef DOM_JIL_API_SUPPORT
	static OP_GADGET_STATUS ProcessJIL1_1WidgetTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessJILAccessTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessJILMaximumDisplayModeTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessJILBillingTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessJILUpdateTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessJILContentTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
#endif // DOM_JIL_API_SUPPORT

	static OP_GADGET_STATUS ProcessOpera2006WidgetTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessOperaWidgetTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);

	static OP_GADGET_STATUS ProcessOperaIconTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason) { return ProcessIconTagGeneric(this_class, f, error_reason, FALSE); }
	static OP_GADGET_STATUS ProcessOperaWidgetFileTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessOperaWidgetTypeTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessOperaAuthorTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessOperaWidgetNameTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessOperaWidgetWidthTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessOperaWidgetHeightTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessOperaWidgetSecurityTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessOperaWidgetServicepathTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessOperaFileTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);
	static OP_GADGET_STATUS ProcessOperaIdTag(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason);

	static OP_GADGET_STATUS ProcessIconTagGeneric(OpGadgetClass* this_class, XMLFragment* f, OpString& error_reason, BOOL src_is_attr);

	/** Element handler descriptors for top level elements (<widget>). */
	OpAutoVector<GadgetConfigurationParserElement> m_top_level_config_parser_elements;
	/** Element handler descriptors for children of top level elements */
	OpAutoVector<GadgetConfigurationParserElement> m_config_parser_elements;

	// Needed for synthetic selftests.
	friend class ST_gadgetsgadgetparsers;
};

#endif // GADGET_SUPPORT

#endif // !GADGETS_OP_GADGET_PARSERS_H
