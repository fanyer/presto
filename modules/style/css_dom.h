/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
*/

#ifndef CSS_DOM_H
#define CSS_DOM_H

#include "modules/util/tempbuf.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/style/css_types.h"

enum CSS_DOMException
{
	CSS_DOMEXCEPTION_NONE,
	CSS_DOMEXCEPTION_HIERARCHY_REQUEST_ERR,
	CSS_DOMEXCEPTION_INVALID_MODIFICATION_ERR,
	CSS_DOMEXCEPTION_INVALID_CHARACTER_ERR,
	CSS_DOMEXCEPTION_NO_MODIFICATION_ALLOWED_ERR,
	CSS_DOMEXCEPTION_NOT_FOUND_ERR,
	CSS_DOMEXCEPTION_SYNTAX_ERR,
	CSS_DOMEXCEPTION_INDEX_SIZE_ERR,
	CSS_DOMEXCEPTION_NAMESPACE_ERR
};

class DOM_Environment;
class CSS_DOMRule;
class DOM_CSSRule;
class HLDocProfile;

class CSS_DOMStyleDeclaration
{
public:
	enum Type
	{
		NORMAL,
		COMPUTED,
		CURRENT
	};

	CSS_DOMStyleDeclaration(DOM_Environment* environment, HTML_Element* element, CSS_DOMRule* rule, Type type, const uni_char* pseudo_class = NULL);
	~CSS_DOMStyleDeclaration();

	/** Used when a style object's element is moved to a new document. */
	void ChangeEnvironment(DOM_Environment* new_environment) { m_environment = new_environment; }
	DOM_Environment* GetEnvironment() { return m_environment; }

	OP_STATUS GetPropertyValue(TempBuffer* buffer, const uni_char* property_name);
	OP_STATUS GetPropertyValue(TempBuffer* buffer, int property);
	OP_STATUS GetPixelValue(double& value, int property);
	OP_STATUS GetPosValue(double& value, int property);

#ifdef CSS_TRANSFORMS
	OP_STATUS GetAffineTransform(AffineTransform& transform, const uni_char* str);
#endif

	OP_STATUS GetPropertyPriority(TempBuffer* buffer, const uni_char* property_name);

	OP_STATUS SetProperty(const uni_char* property_name, const uni_char* value, const uni_char* priority, CSS_DOMException& exception);
	OP_STATUS SetProperty(int property, const uni_char* value, CSS_DOMException& exception);
	OP_STATUS SetPixelValue(int property, const uni_char* value, CSS_DOMException& exception);
	OP_STATUS SetPosValue(int property, const uni_char* value, CSS_DOMException& exception);

	OP_STATUS RemoveProperty(TempBuffer* buffer, const uni_char* property_name, CSS_DOMException& exception);

	unsigned int GetLength();
	OP_STATUS GetItem(TempBuffer* buffer, unsigned int index);

	OP_STATUS SetText(const uni_char* text, CSS_DOMException& exception);
	OP_STATUS GetText(TempBuffer* buffer);

private:
	CSS_property_list* GetPropertyList();

	OP_STATUS SetProperty(short property, const uni_char* value, const uni_char* priority, CSS_DOMException& exception);
	OP_STATUS RemoveProperty(TempBuffer* buffer, short property, CSS_DOMException& exception);

	void GetShorthandPropertyL(TempBuffer* buffer, short property, CSS_SerializationFormat format_mode);
#ifdef CSS_TRANSITIONS
	void GetComplexShorthandPropertyL(TempBuffer* buffer,
									  CSS_decl** prop_values,
									  int n_prop_values,
									  CSS_SerializationFormat format_mode);
#endif // CSS_TRANSITIONS

	void GetBackgroundShorthandPropertyL(TempBuffer* buffer, CSS_decl** prop_values, int n_prop_values, CSS_SerializationFormat format_mode);

#ifdef CSS_TRANSFORMS
	void GetAffineTransformL(TempBuffer* buffer);
#endif

	CSS_SerializationFormat GetCSS_SerializationFormat() { return m_type == NORMAL ? CSS_FORMAT_CSSOM_STYLE : (m_type == COMPUTED ? CSS_FORMAT_COMPUTED_STYLE : CSS_FORMAT_CURRENT_STYLE); }

	CSS_decl* GetDecl(short property);
	void ReleaseDecl(CSS_decl* decl);

	OP_STATUS Update(short property);

	/** Return the base url for resolving url() values in this declaration block.
		If it belongs to a stylesheet, it's the stylesheet's url. If it is a
		style attribute, it is the current base url for the document that the
		style attribute belongs to. */
	URL GetBaseURL();

	DOM_Environment* m_environment;

	/** The rule that holds this declaration. NULL for style attributes and computed style. */
	CSS_DOMRule* m_rule;

	/** If m_rule is NULL, this is the element that holds the style attribute or computed style.
		Otherwise, it's the link/style element for the stylesheet. */
	HTML_Element* m_element;

	Type m_type;
	short m_pseudo_elm;
};

class CSS_DOMRule
{
public:

	/* These values are kept in sync with:

	   http://wiki.csswg.org/spec/cssom-constants */

	enum Type
	{
		UNKNOWN     =  0,
		STYLE       =  1,
		CHARSET     =  2,
		IMPORT      =  3,
		MEDIA       =  4,
		FONT_FACE   =  5,
		PAGE        =  6,
		KEYFRAMES   =  7,
		KEYFRAME    =  8,
		NAMESPACE   = 10,
		SUPPORTS    = 12,
		VIEWPORT    = 15
	};

	Type GetType();

	CSS_DOMRule(DOM_Environment* environment, CSS_Rule* rule, HTML_Element* sheet_elm)
		: m_environment(environment), m_rule(rule), m_element(sheet_elm), m_ref_count(0), m_dom_rule(0) {}

	OP_STATUS GetText(TempBuffer* buffer);
	OP_STATUS SetText(const uni_char* text, CSS_DOMException& exception);

	OP_STATUS GetSelectorText(TempBuffer* buffer);
	OP_STATUS SetSelectorText(const uni_char* text, CSS_DOMException& exception);
	OP_STATUS GetStyle(CSS_DOMStyleDeclaration*& styledeclaration);
	OP_STATUS GetParentRule(CSS_DOMRule*& parent);

	void SetDOMRule(DOM_CSSRule* dom_rule) { m_dom_rule = dom_rule; }
	DOM_CSSRule* GetDOMRule() { return m_dom_rule; }
	HTML_Element* GetStyleSheetElm() { return m_element; }

	// Methods that work on the rules contained within conditional rules.
	unsigned int GetRuleCount();
	OP_STATUS GetRule(CSS_DOMRule*& rule, unsigned int index, CSS_DOMException& exception);
	OP_STATUS InsertRule(const uni_char* rule, unsigned int index, CSS_DOMException& exception);
	OP_STATUS DeleteRule(unsigned int index, CSS_DOMException& exception);
	// Methods for MEDIA rules.
	CSS_MediaObject* GetMediaObject(BOOL create);

	// Methods for IMPORT rules.
	HTML_Element* GetImportLinkElm();
	const uni_char* GetHRef();
	const uni_char* GetEncoding();

#ifdef CSS_ANIMATIONS
	// Methods for KEYFRAMES rules.
	const uni_char* GetKeyframesName() const;
	OP_STATUS SetKeyframesName(const uni_char* name);
	OP_STATUS AppendKeyframeRule(const uni_char* rule, unsigned rule_length);
	OP_STATUS DeleteKeyframeRule(const uni_char* key_text, unsigned key_text_length);
	OP_STATUS FindKeyframeRule(CSS_DOMRule *&rule, const uni_char* key_text, unsigned key_text_length);

	// Methods for KEYFRAME rules.
	OP_STATUS GetKeyframeKeyText(TempBuffer* buf) const;
	OP_STATUS SetKeyframeKeyText(const uni_char* key_text, unsigned key_text_length);
#endif // CSS_ANIMATIONS

	// Methods for NAMESPACE rules.
	const uni_char* GetNamespacePrefix();
	const uni_char* GetNamespaceURI();

	void RuleDeleted() { m_rule = NULL; }

	void Ref() { m_ref_count++; }
	void Unref();

#ifdef STYLE_PROPNAMEANDVALUE_API

	/** Get the number of declarations for this css rule.

		@return If this is a style or page rule, returns the number	of declarations stored
				in the property list. Otherwise returns 0.
	*/
	unsigned int GetPropertyCount();

	/** Appends the property name of declaration idx to name, and the property value of
		declaration idx to value, and sets the important flag.

		@param idx Index of the declaration to get the name and value for. First declaration
				   has index 0.
	    @param name TempBuffer that the property name of the declaration will be appended to.
		@param value TempBuffer the the property value of the declaration will be appended to.
		@param important Set to TRUE by this method if the declaration is !important, otherwise
						 set to FALSE.
		@return OpStatus::ERR_NO_MEMORY on OOM.
	*/
	OP_STATUS GetPropertyNameAndValue(unsigned int idx, TempBuffer* name, TempBuffer* value, BOOL& important);

#endif // STYLE_PROPNAMEANDVALUE_API

#ifdef SCOPE_CSS_RULE_ORIGIN
	/** Get the original line number for this rule. */
	unsigned GetLineNumber() const;
#endif // SCOPE_CSS_RULE_ORIGIN

	/* Methods below should be used internally in style only. */
	void SetPropertyList(CSS_property_list* new_props);
	CSS_property_list* GetPropertyList();
	void PropertyListChanged() { SetPropertyList(GetPropertyList()); }
	CSS* GetCSS() { return m_element->GetCSS(); }

private:
	DOM_Environment* m_environment;
	CSS_Rule* m_rule;
	HTML_Element* m_element;
	int m_ref_count;
	DOM_CSSRule* m_dom_rule;
};

class CSS_DOMStyleSheet
{
public:
	CSS_DOMStyleSheet(DOM_Environment* environment, HTML_Element* element);

	unsigned int GetRuleCount();
	OP_STATUS GetRule(CSS_DOMRule*& rule, unsigned int index, CSS_DOMException& exception);
	OP_STATUS InsertRule(const uni_char* rule, unsigned int index, CSS_DOMException& exception);
	OP_STATUS DeleteRule(unsigned int index, CSS_DOMException& exception);

	static OP_STATUS GetImportRule(CSS_DOMRule*& import_rule, HTML_Element* sheet_elm, DOM_Environment* dom_env);

private:
	DOM_Environment* m_environment;
	HTML_Element* m_element;
};

class CSS_DOMMediaList
{
public:
	CSS_DOMMediaList(DOM_Environment* environment, HTML_Element* sheet_elm);
	CSS_DOMMediaList(DOM_Environment* environment, CSS_DOMRule* media_rule);
	~CSS_DOMMediaList();

	unsigned int GetMediaCount();
	OP_STATUS GetMedia(TempBuffer* buffer);
	OP_STATUS SetMedia(const uni_char* text, CSS_DOMException& exception);
	OP_STATUS GetMedium(TempBuffer* buffer, unsigned int index, CSS_DOMException& exception);
	OP_STATUS AppendMedium(const uni_char* medium, CSS_DOMException& exception);
	OP_STATUS DeleteMedium(const uni_char* medium, CSS_DOMException& exception);

private:
	CSS_MediaObject* GetMediaObject(BOOL create=FALSE);
	CSS* GetCSS();

	DOM_Environment* m_environment;
	HTML_Element* m_element;
	CSS_DOMRule* m_rule;
};

#ifdef STYLE_DOM_SELECTORS_API

/** Listener interface for CSS_QuerySelector.
	MatchElement will be called for each matched element. */
class CSS_QuerySelectorListener
{
public:
	virtual ~CSS_QuerySelectorListener() {}
	virtual OP_STATUS MatchElement(HTML_Element* element) = 0;
};

/** Flags to be sent as parameters to CSS_QuerySelector to say in which
	way elements are being matched. */
enum CSS_QueryFlag
{
	/** Continue after first match. */
	CSS_QUERY_ALL = 1,
	/** Match a single element. The root parameter is the element to match.
		Used for the implementation of Element.matchesSelector(). */
	CSS_QUERY_MATCHES_SELECTOR = 2
};

/** CSS implementation of DocumentSelector and ElementSelector interfaces from DOM Selectors API.

	@param hld_profile HLDocProfile for the document we're querying for.
	@param selectors The selectors string to match.
	@param root The root of the subtree to match. For DocumentSelector this is the document root,
				for ElementSelector and matchesSelector, this is the "this" element.
	@param query_flags A set of or'ed CSS_QueryFlag values. No flags set (0) means that the method
					   will return after the first match and that a match might be tried for all
					   children of root, but not root itself.
	@param listener Listener for matched elements.
	@param exception If OpStatus::ERR is returned, the exception to throw is set in this variable.

	@return OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR on selector parser errors, otherwise OpStatus::OK. */
OP_STATUS CSS_QuerySelector(HLDocProfile* hld_profile,
							const uni_char* selectors,
							HTML_Element* root,
							int query_flags,
							CSS_QuerySelectorListener* listener,
							CSS_DOMException& exception);

#endif // STYLE_DOM_SELECTORS_API

#endif // CSS_DOM_H
