/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef CSS_H
#define CSS_H

#include "modules/style/css_media.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/layout/layout_coord.h"
#include "modules/logdoc/html.h"
#include "modules/logdoc/namespace.h"
#include "modules/logdoc/class_attribute.h"
#include "modules/logdoc/logdocenum.h"
#include "modules/hardcore/base/opstatus.h"
#include "modules/display/color.h"
#include "modules/util/opfile/unistream.h"
#include "modules/util/OpHashTable.h"
#include "modules/style/css_types.h"
#include "modules/style/css_collection.h"
#include "modules/style/css_webfont.h"
#include "modules/style/src/css_rule.h"
#include "modules/style/css_viewport.h"

class URL;
class HLDocProfile;
class HTML_Element;
class CSS_ConditionalRule;
class CSS_BlockRule;
class CSS_ImportRule;
class CSS_Selector;
class LogicalDocument;
class CSS_property_list;
class CSS_decl;
class CSS_PageRule;
class DataSrc;
class CSS_MatchContextElm;
class TempBuffer;
class CSS_DOMRule;
class CSS_KeyframesRule;
class CSS_KeyframeRule;

#ifdef STYLE_GETMATCHINGRULES_API
class CSS_MatchRuleListener;
#endif

#ifndef M_PI
#define M_PI			3.14159265358979323846	/* pi */
#endif

#include "modules/style/css_all_properties.h"
#include "modules/style/css_all_values.h"

class CSS_Properties
{
private:
	CSS_decl* css_decl[CSS_PROPERTIES_LENGTH];
	UINT32 rule_num[CSS_PROPERTIES_LENGTH];
	unsigned short specificity[CSS_PROPERTIES_LENGTH];

public:
	CSS_Properties() { Reset(); }
	void Reset() { for (int i = 0; i < CSS_PROPERTIES_LENGTH; i++) { css_decl[i] = NULL; specificity[i] = 0; } }
	void ResetProperty(int property) { css_decl[property] = NULL; specificity[property] = 0; }
	CSS_decl* GetDecl(int property) const { return css_decl[property]; }
	void SetDecl(int property, CSS_decl* decl) { css_decl[property] = decl; }
	void SelectProperty(CSS_decl* css_decl, unsigned short specificity, UINT32 rule_number, BOOL apply_partially);
	void SelectSelectionProperty(CSS_decl* css_decl, unsigned short specificity, UINT32 rule_number);
#ifdef STYLE_GETMATCHINGRULES_API
	void AddDeclsFrom(CSS_Properties& props, BOOL include_inherit_no);
#endif // STYLE_GETMATCHINGRULES_API
};

#ifdef STYLE_GETMATCHINGRULES_API
/** Return TRUE if the CSS property (CSS_PROPERTY_*) has "Inherited: yes" in the relevant spec. */
BOOL PropertyIsInherited(short property);
#endif // STYLE_GETMATCHINGRULES_API


/** A class for representing CSS stylesheets. */
class CSS : public CSSCollectionElement
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

public:

	CSS(HTML_Element* he, const URL& base_url, BOOL user_defined);
	virtual ~CSS();

	/** Return the HTML_Element which represents this stylesheet. Either a
		link or style element. Http, META http, or @import stylesheets are
		link elements with HE_INSERTED_BY_CSS_IMPORT. */
	virtual HTML_Element* GetHtmlElement() { return m_src_html_element; }
	virtual void Added(CSSCollection* collection, FramesDocument* doc);
	virtual void Removed(CSSCollection* collection, FramesDocument* doc);
	virtual Type GetType() { return STYLESHEET; }

	BOOL GetUserDefined() { return m_user_def; }
	BOOL IsLocal() { return m_is_local; }
	void SetIsLocal() { m_is_local = TRUE; }

	void AddNameSpaceL(HLDocProfile* hld_prof, const uni_char* name, const uni_char* uri);
	int GetNameSpaceIdx(const uni_char* prefix);
	int GetDefaultNameSpaceIdx() const { return m_default_ns_idx; }

	enum SheetType { STYLE_PERSISTENT, STYLE_PREFERRED, STYLE_ALTERNATE };

	SheetType GetKind() const {	return m_sheet_type; }

	const uni_char* GetTitle() const;
	const URL* GetHRef(LogicalDocument* logdoc = NULL) const;

	BOOL IsEnabled() const { return m_enabled; }
	void SetEnabled(BOOL enabled) { m_enabled = enabled; }

	void SetXml() { m_xml = TRUE; }
	BOOL IsXml() { return m_xml; }

	int GetSuccessiveAdjacent() { return m_succ_adj; }

#ifdef DOM_FULLSCREEN_MODE
	/** @return TRUE if there is a selector in this stylesheet which has
		:fullscreen or :fullscreen-ancestor in other places than
		the rightmost simple selector. */
	BOOL HasComplexFullscreenStyles() const { return m_complex_fullscreen; }
#endif // DOM_FULLSCREEN_MODE

	/** Set a flag to tell the matching code if this stylesheet may be skipped during matching
		if it hasn't been modified. */
	void SetSkip(BOOL skip) { m_skip = skip; }

	/** Returns TRUE if this stylesheet may be skipped because the previous stylesheet is the same. */
	BOOL Skip() { return m_skip && !IsModified(); }

	/** This stylesheet points to the same source as the css argument.
	    It basically means they have the same url and neither of the
		two have been modified by dom. */
	BOOL IsSame(LogicalDocument* logdoc, CSS* css);

	/** Parse the source of a stylesheet pointed to by src_head into
		an internal structure. Starts loading of imported stylesheets
		as they are encountered.
		@param start_line_number  The line number where this source is relative to
		                          the start of the file.  Will usually be 1, unless
		                          it's from an inline style element.
		@param start_line_character  The character position in the line where the source
		                             starts.  Will usually 0, unless it's from an inline
		                             style element.
		*/
	CSS_PARSE_STATUS Load(HLDocProfile* hld_profile, DataSrc* src_head, unsigned start_line_number, unsigned start_line_character);

#ifdef PAGED_MEDIA_SUPPORT
	/** Load the css @page declarations from this stylesheet, which apply to the given pseudo page(s), into css_properties. */
	void GetPageProperties(FramesDocument* doc,
						   CSS_Properties* css_properties,
						   unsigned int stylesheet_number,
						   BOOL first_page,
						   BOOL left_page,
						   CSS_MediaType media_type = CSS_MEDIA_TYPE_ALL);
#endif // PAGED_MEDIA_SUPPORT

#ifdef CSS_VIEWPORT_SUPPORT
	/** Get viewport properties from @viewport rules in this stylesheet and add them to the CSS_Viewport object.

		@param hld_profile HLDocProfile for the document.
		@param viewport The CSS_Viewport object to add properties to.
		@param media_type The media type of the canvas used when matching media queries. */
	void GetViewportProperties(FramesDocument* doc, CSS_Viewport* viewport, CSS_MediaType media_type);
#endif // CSS_VIEWPORT_SUPPORT

	/** Load the css declarations from this stylesheet, which apply to the
		HTML_Element he, into css_properties. css_properties may be NULL.
		In that case, the method will just return TRUE or FALSE based on
		whether we have a matching selector or not. Members of CSS_MatchContext
		affect the behavior of this method and side-effects. See for instance
		match_all, mark_pseudo_bits, and prefetch_mode.

		@param match_content CSS_MatchContext used for info used during
							 matching.
		@param css_properties Where the matching declarations are stored.
							  One declaration per property. Declarations
							  will be overridden or not based on source
							  and specificity. Can be NULL when used for
							  MarkAffectedElementsPropsDirty.
		@return TRUE if at least one selector matched. */
	BOOL GetProperties(CSS_MatchContext* match_context,
					   CSS_Properties* css_properties,
					   unsigned int stylesheet_number) const;

	/** Is an @import stylesheet. */
	BOOL IsImport();

	/** Base url used to resolve relative urls within the stylesheet. */
	const URL& GetBaseURL() const { return m_base_url; };

	CSS* GetNextImport(BOOL skip_children);

	/** Returns TRUE if this stylesheet applies to the given canvas medium. */
	BOOL CheckMedia(FramesDocument* doc, CSS_MediaType media)
	{
		return (media == CSS_MEDIA_TYPE_ALL || !m_media_object || (m_media_object->EvaluateMediaTypes(doc) & (media | CSS_MEDIA_TYPE_ALL)) != 0);
	}

	/** Add a rule to the end of the stylesheet. */
	OP_STATUS AddRule(HLDocProfile* hld_prof, CSS_Rule* rule, CSS_ConditionalRule* current_conditional = NULL);

	/** Delete a given rule. Returns TRUE if deleted, FALSE if not in stylesheet. */
	BOOL DeleteRule(HLDocProfile* hld_prof, CSS_Rule* rule);

	/** Update rule lists and other internal caching as a result of removing the provided rule. */
	void RuleRemoved(HLDocProfile* hld_prof, CSS_Rule* rule);

	/** Returns TRUE if this stylesheet has one or more rules */
	BOOL HasRules() const { return !m_rules.Empty(); }

	/** Replace a rule because it has been reparsed through cssText in DOM. */
	OP_STATUS ReplaceRule(HLDocProfile* hld_prof, CSS_Rule* old_rule, CSS_Rule* new_rule, CSS_ConditionalRule* current_conditional = NULL);

	/** Insert a new rule before an existing rule. */
	OP_STATUS InsertRule(HLDocProfile* hld_prof, CSS_Rule* before_rule, CSS_Rule* new_rule, CSS_ConditionalRule* current_conditional = NULL);

	/** Update rule lists and other internal caching as a result of adding the provided rule. */
	OP_STATUS RuleInserted(HLDocProfile* hld_prof, CSS_Rule* rule);

	/** Return number of rules in stylesheet. */
	unsigned int GetRuleCount() { return m_rules.Cardinal(); }

	/** Return rule number idx. Returns NULL if out of range. */
	CSS_Rule* GetRule(unsigned int idx) { Link* l = m_rules.First(); while (l && idx-- > 0) l = l->Suc(); return (CSS_Rule*)l; }

	/** Return the import rule for the stylesheet, given by sheet_elm, imported from this stylesheet. */
	CSS_ImportRule* FindImportRule(HTML_Element* sheet_elm);

	/** Parse a partial stylesheet to set cssText, selectorText, or insert a new rule. */
	CSS_PARSE_STATUS ParseAndInsertRule(HLDocProfile* hld_prof, CSS_Rule* before_rule, CSS_ConditionalRule* conditional_rule, CSS_KeyframesRule* keyframes_rule, BOOL replace, int start_token, const uni_char* buffer, int length);

	/** Delete rule at index idx. Returns TRUE if deleted, FALSE for index out of range. */
	BOOL DeleteRule(HLDocProfile* hld_prof, unsigned int idx);

	/** Mark the stylesheet as modified (by dom). */
	void SetModified() { m_modified = TRUE; }

	/** Returns TRUE if the stylesheet has been modified (by dom). */
	BOOL IsModified() const { return m_modified; }

	/** Parse an html style attribute and return a list of CSS properties. */
	static CSS_property_list* LoadHtmlStyleAttr(const uni_char* buf_start,
												int buf_len,
												const URL& base_url,
												HLDocProfile* hld_profile,
												CSS_PARSE_STATUS& stat);

	/** Parse a css value string for a given property. Used to assign a value
		to members of CSSStyleDeclaration in DOM.

		@param hld_profile The document's HLDocProfile.
		@param rule If the CSSStyleDeclaration is a style property of a
					CSSRule, this is the pointer to that rule. If it is
					Element.style, this parameter is NULL.
		@param base_url The document base url for style attributes and
						the stylesheet base url for style properties in
						stylesheets.
		@param prop The CSS property to set.
		@param buf_start Pointer to the first character of the string to
						 be parsed.
		@param buf_len The length of the input string buffer.
		@param stat The returned parse status.

		@return A CSS_property_list representation of the parsed value. */
	static CSS_property_list* LoadDOMStyleValue(HLDocProfile* hld_profile,
												CSS_DOMRule* rule,
												const URL& base_url,
												int prop,
												const uni_char* buf_start,
												int buf_len,
												CSS_PARSE_STATUS& stat);

    enum CssValue_t {
		CSS_VALUE_TYPE_hash,
        CSS_VALUE_TYPE_long,
        CSS_VALUE_TYPE_color,
        CSS_VALUE_TYPE_number,
        CSS_VALUE_TYPE_string,
        CSS_VALUE_TYPE_keyword,
		CSS_VALUE_TYPE_property,
		CSS_VALUE_TYPE_func_url,
		CSS_VALUE_TYPE_func_attr,
		CSS_VALUE_TYPE_func_skin,
		CSS_VALUE_TYPE_func_local,
		CSS_VALUE_TYPE_func_format,
		CSS_VALUE_TYPE_func_counter,
		CSS_VALUE_TYPE_func_counters,
		CSS_VALUE_TYPE_unquoted_string,
		CSS_VALUE_TYPE_func_linear_gradient,
		CSS_VALUE_TYPE_func_radial_gradient
    };

	static BOOL IsColorProperty(short property);
	static const uni_char* GetDimKeyword(float number);

	/** Various methods for converting css values to strings. Used for currentStyle and getComputedStyle. */
	static void FormatQuotedStringL(TempBuffer* tmp_buf, const uni_char* str_val);
	static void FormatCssStringL(TempBuffer* tmp_buf, const uni_char* str_val, const uni_char* prefix, const uni_char* suffix, BOOL force_quote);
	static void FormatCssPropertyL(short property, TempBuffer* tmp_buf) { FormatCssPropertyNameL(property, tmp_buf); tmp_buf->AppendL(UNI_L(": ")); }

	/** Non-leaving variant of FormatQuotedStringL for use in a non
		leaving environment. */
	static OP_STATUS FormatQuotedString(TempBuffer* tmp_buf, const uni_char* str_val);

	/** Turns a CSS_decl into a string. This is mostly used for reading property values by scripts but also by the
	    debugger and similar.

	    @param[out] tmp_buf The resulting string is added to the current contents of tmp_buf.
	    @param[in] decl The declaration to turn into a string.
	    @param[in] space_before If TRUE a SPACE is added before adding any other content to the TempBuffer.
	    @param[in] format_mode Some declarations can be output in different formats. The classic
	    example is colors where the input "red" can be output as either "red" or "rgb(255, 0, 0)".
	    For compatibility reasons some serializations (typically computed style) should prefer the generic
	    color format while pure reading of a style attribute should prefer the original string. Setting
	    format_mode = CSS_FORMAT_CSSOM_STYLE will give "red" in the example above.
	    Setting it to CSS_FORMAT_COMPUTED_STYLE will give "rgb(255, 0, 0)". This should be
	    specified in http://dev.w3.org/csswg/cssom/#serializing-css-values
	    but currently the problem area isn't fully understood by the specification authors. For colors
	    the spec only has a note "Issue: preserve system colors, maybe color keywords...".
	    Finally there is a format CSS_FORMAT_CURRENT_STYLE that tries to mimic MSIE's
	    unspecified behaviour as much as possible. For colors that means the hex format: "#ff0000". */
	static void FormatDeclarationL(TempBuffer* tmp_buf, const CSS_decl* decl, BOOL space_before, CSS_SerializationFormat format_mode);
	static void FormatCssColorL(long color_value, TempBuffer* tmp_buf, BOOL space_before, CSS_SerializationFormat format_mode);
	static void FormatCssValueL(void* value, CssValue_t type, TempBuffer* tmp_buf, BOOL space_before);
	static void FormatCssNumberL(float num, short type, TempBuffer* tmp_buf, BOOL space_before);
	static void FormatCssAspectRatioL(short w, short h, TempBuffer* tmp_buf);
	static void FormatCssPropertyNameL(short property, TempBuffer* tmp_buf);
	static void FormatCssGenericValueL(CSSProperty property, const CSS_generic_value& gen_val, TempBuffer *tmp_buf, BOOL space_before, CSS_SerializationFormat format_mode);
	static void FormatCssTimingValueL(const CSS_generic_value *gen_arr, TempBuffer* tmp_buf, BOOL space_before);

	typedef enum
	{
		QUERY_EQUALS = 1,
		QUERY_MIN = 2,
		QUERY_MAX = 4
	} MediaQueryType;

	/** Stylesheets need to be re-evaluated when [min/max-]widths are
		crossed when resizing windows. This method adds a width to an
		internal array of widths that needs to be checked. Added values
	    are typically min/max or exact widths from media queries. */
	OP_STATUS AddMediaQueryWidth(LayoutCoord width, MediaQueryType type) { return AddMediaQueryLength(m_query_widths, width, type); }

	/** Stylesheets need to be re-evaluated when [min/max-]heights are
		crossed when resizing windows. This method adds a height to an
		internal array of heights that needs to be checked. Added values
	    are typically min/max or exact heights from media queries. */
	OP_STATUS AddMediaQueryHeight(LayoutCoord height, MediaQueryType type) { return AddMediaQueryLength(m_query_heights, height, type); }

	/** Stylesheets need to be re-evaluated when
		device-[min/max-]widths are crossed when resizing screen. This
		method adds a width to an internal array of widths that needs
		to be checked. Added values are typically min/max or exact
		widths from media queries. */
	OP_STATUS AddDeviceMediaQueryWidth(LayoutCoord width, MediaQueryType type) { return AddMediaQueryLength(m_query_device_widths, width, type); }

	/** Stylesheets need to be re-evaluated when
		device-[min/max-]heights are crossed when resizing
		screen. This method adds a height to an internal array of
		heights that needs to be checked. Added values are typically
		min/max or exact heights from media queries. */
	OP_STATUS AddDeviceMediaQueryHeight(LayoutCoord height, MediaQueryType type) { return AddMediaQueryLength(m_query_device_heights, height, type); }

	/** Stylesheets need to be re-evaluated when [min/max-]aspect-ratios are
		crossed when resizing windows. This method adds a ratio to an
		internal array of ratios that needs to be checked. Added values
	    are typically min/max or exact ratios from media queries. The
		ratio is width/height. */
	OP_STATUS AddMediaQueryRatio(LayoutCoord width, LayoutCoord height, MediaQueryType type) { return AddMediaQueryRatio(m_query_ratios, width, height, type); }

	/** Stylesheets need to be re-evaluated when
		[min/max-]device-aspect-ratios are crossed when resizing
		screen. This method adds a ratio to an internal array of
		ratios that needs to be checked. Added values are typically
		min/max or exact ratios from media queries. The ratio is
		width/height. */
	OP_STATUS AddDeviceMediaQueryRatio(LayoutCoord width, LayoutCoord height, MediaQueryType type) { return AddMediaQueryRatio(m_query_device_ratios, width, height, type); }

	/** Check if a resize of the window will change the result of a media query in this stylesheet. */
	BOOL HasMediaQueryChanged(LayoutCoord old_width, LayoutCoord old_height, LayoutCoord new_width, LayoutCoord new_height);
	/** Check if a resize of the screen will change the result of a media query in this stylesheet. */
	BOOL HasDeviceMediaQueryChanged(LayoutCoord old_device_width, LayoutCoord old_device_height, LayoutCoord new_device_width, LayoutCoord new_device_height);

	/** Tell the style sheet that the media attribute of the link/style element has changed. */
	OP_STATUS MediaAttrChanged();

	/** @return TRUE if this stylesheet contains at least one @font-face rule. */
	BOOL HasFontFaceRules() { return m_webfonts.GetCount() > 0; }

	/** Return a CSS_WebFont object for the given family-name if there is an @font-face rule
		with a font-family descriptor matching that family_name. */
	CSS_WebFont* GetWebFont(const uni_char* family_name)
	{
		Head* ret_val = NULL;
		return (OpStatus::OK == m_webfonts.GetData(family_name, &ret_val)) ? static_cast<CSS_WebFont*>(ret_val->First()) : NULL;
	}

#ifdef CSS_ANIMATIONS
	/** Look up a @keyframes rule with a matching name descriptor */
	CSS_KeyframesRule* GetKeyframesRule(FramesDocument* doc, const uni_char* name, CSS_MediaType media_type);
#endif // CSS_ANIMATIONS

	/** Do this style sheet have a "valid css header". This flag can
		be used to check if a suspicious file really starts with valid
		css declaration. */
	BOOL HasSyntacticallyValidCSSHeader() { return m_has_syntactically_valid_css_header; }

	/** Set whether this style sheet have a "valid css header" */
	void SetHasSyntacticallyValidCSSHeader(BOOL v) { m_has_syntactically_valid_css_header = v; }

#ifdef CSS_VIEWPORT_SUPPORT
	/** @returns TRUE if the stylesheet contains any @viewport rules. */
	BOOL HasViewportRules() { return m_viewport_rules.First() != NULL; }
#endif // CSS_VIEWPORT_SUPPORT

private:

	/** Is this a user stylesheet? */
	BOOL m_user_def;

	/** Is this a local stylesheet (user def or browser stylesheet)? */
	BOOL m_is_local;

	/** Does this style sheet start with a syntactically valid
		cssheader. Can be used to detect alleged css loaded
		cross-domain that are really something else than css. */
	BOOL m_has_syntactically_valid_css_header;

	/** The namespace index for the default @namespace rule. */
	int m_default_ns_idx;

	/** Namspaces declared in @namespace rules. */
	NS_ListElm* m_ns_elm_list;

	/** A linked list of the stylesheet rules. */
	List<CSS_Rule> m_rules;

	/** The style or link html element which referred this stylesheet. */
	HTML_Element* m_src_html_element;

	/** The media object representing the media queries in the media attribute. */
	CSS_MediaObject* m_media_object;

	/** The base url used to resolve relative urls in the stylesheet. */
	URL m_base_url;

	/** The maximum number of successive adjacent selectors in this stylesheet. */
	int m_succ_adj;

#ifdef DOM_FULLSCREEN_MODE
	/** Set to TRUE if there is a selector in this collection which has
		:fullscreen or :fullscreen-ancestor in other places than
		the rightmost simple selector. */
	BOOL m_complex_fullscreen;
#endif // DOM_FULLSCREEN_MODE

	BOOL m_xml;

	/** Is this stylesheet enabled? */
	BOOL m_enabled;

	/** TRUE if loading properties from this stylesheet may be skipped
		because the previous stylesheet in the list is the same. */
	BOOL m_skip;

	/** Alternate, preferred, or persistent. */
	SheetType m_sheet_type;

	/** Update the rule numbers for style and page rule to keep the ordering correct in the CSS_RuleElmLists.
		@param inserted_rule The inserted rule to start updating from. This rule will have a number assigned
							 to it based on the previous rule. */
	void UpdateRuleNumbers(CSS_BlockRule* inserted_rule);

	/** Set to TRUE if stylesheet has been modified (by dom). */
	BOOL m_modified;

	OP_STATUS AddMediaQueryLength(Head& list, LayoutCoord length, MediaQueryType type);
	OP_STATUS AddMediaQueryRatio(Head& list, LayoutCoord width, LayoutCoord height, MediaQueryType type);
	BOOL HasMediaQueryChanged(const Head& list, LayoutCoord old_length, LayoutCoord new_length);
	BOOL HasMediaQueryChanged(const Head& list, LayoutCoord old_width, LayoutCoord old_height, LayoutCoord new_width, LayoutCoord new_height);

	Head m_query_widths;
	Head m_query_heights;
	Head m_query_ratios;
	Head m_query_device_widths;
	Head m_query_device_heights;
	Head m_query_device_ratios;

	OP_STATUS InsertFontFaceRule(class CSS_FontfaceRule* font_rule);
	OpStringHashTable<Head> m_webfonts;

// This public keyword is a workaround for VS6.
public:

	class CSS_RuleElm
	{
	public:
		CSS_RuleElm(CSS_BlockRule* rule, CSS_Selector* selector) : m_rule(rule), m_sel(selector), m_next(NULL) {}
		void Precede(CSS_RuleElm* elm) { m_next = elm; }
		CSS_RuleElm* Next() { return m_next; }
		CSS_BlockRule* GetRule() { return m_rule; }
		CSS_Selector* GetSelector() { return m_sel; }
	private:
		CSS_BlockRule* m_rule;
		CSS_Selector* m_sel;
		CSS_RuleElm* m_next;
	};

	class CSS_RuleElmList
	{
	public:
		CSS_RuleElmList() : m_first(NULL) {}
		~CSS_RuleElmList();
		CSS_RuleElm* First() { return m_first; }
		OP_STATUS AddRule(CSS_BlockRule* rule, CSS_Selector* selector);
		OP_STATUS InsertRule(CSS_BlockRule* rule, CSS_Selector* selector);
		void DeleteRule(CSS_Rule* rule);

	private:
		void Prepend(CSS_RuleElm* elm) { if (m_first) elm->Precede(m_first); m_first = elm; }
		CSS_RuleElm* m_first;
	};

	class RuleElmIterator
	{
	public:
		RuleElmIterator(FramesDocument* doc, CSS_RuleElm** lists, CSS_MediaType media) : m_doc(doc), m_lists(lists), m_cur_conditional(NULL), m_cur_elm(NULL), m_media(media) {}
		CSS_RuleElm* Next();

	private:
		FramesDocument* m_doc;
		CSS_RuleElm** m_lists;
		CSS_ConditionalRule* m_cur_conditional;
		CSS_RuleElm* m_cur_elm;
		CSS_MediaType m_media;
	};

private:

	/** Helper method for CSS::GetProperties. */
	CSS_RuleElm** MakeDynamicRuleElmList(CSS_RuleElm** elm_list, unsigned int i, const ClassAttribute* class_attr, unsigned int& list_count) const;

	unsigned int m_next_rule_number;

	/** Hash table for rules with type selector in rightmost simple selector. */
	OpINT32HashTable<CSS_RuleElmList> m_type_rules;

	/** Hash table for rules with universal id selector in rightmost simple selector. */
	OpStringHashTable<CSS_RuleElmList> m_id_rules;

	/** Hash table for rules with universal class selector in rightmost simple selector. */
	OpStringHashTable<CSS_RuleElmList> m_class_rules;

	/** The list of @page rules in this stylesheet */
	CSS_RuleElmList m_page_rules;

#ifdef CSS_VIEWPORT_SUPPORT
	/** The list of @viewport rules in this stylesheet */
	CSS_RuleElmList m_viewport_rules;
#endif // CSS_VIEWPORT_SUPPORT

#ifdef CSS_ANIMATIONS
	CSS_RuleElmList m_keyframes_rules;
#endif // CSS_ANIMATIONS
};

int GetCSS_Property(const uni_char* str, int len);
const char* GetCSS_PropertyName(int property);
CSSValue CSS_GetKeyword(const uni_char* str, int len);
const uni_char* CSS_GetKeywordString(short val);
unsigned short CSS_GetPseudoClass(const uni_char* str, int len);
const char* CSS_GetPseudoClassString(unsigned short type);
short CSS_GetPseudoPage(const uni_char* str, int len);
const char* CSS_GetPseudoPageString(short pseudo);
#ifdef CSS_ANIMATIONS
double CSS_GetKeyframePosition(const uni_char* str, int len);
#endif // CSS_ANIMATIONS

/** Parses a color string according to the css spec and returns an rgba value.

	@param string The string to be parsed.
	@param len The length of the string.
	@param color If TRUE is returned, the color is returned in this variable.
	@return FALSE if parse error, otherwise TRUE.
*/
BOOL ParseColor(const uni_char* string, int len, COLORREF& color);

/** Parses a font shorthand string according to the CSS spec., and
	return a property list.
	If parsing is successful, the caller is responsible for
	Unref()'ing the property list when no longer using it (i.e, the
	caller owns one reference).

	@param string The string to be parsed.
	@param len The length of the string.
	@param hld_profile
	@param out_props The property list
	@return OpStatus::ERR if parsing failed,
	        OpStatus::OK if successful,
	        OpStatus::ERR_NO_MEMORY on OOM.
 */
OP_STATUS ParseFontShorthand(const uni_char* string, unsigned len,
							 HLDocProfile* hld_profile, CSS_property_list*& out_props);

/** Converts an hsl value to rgb. This method will use modulo to get h between
	0 and 360, and clamp s and l in the range of 0..100.

	@param h Hue Angle in degrees.
	@param s Saturation in percentage.
	@param l Lightness in percentage.
*/
COLORREF HSLA_TO_RGBA(double h, double s, double l, double a = 1.0);

#ifdef _DEBUG
Debug& operator<<(Debug& d, const CSSProperty& property);
#endif // _DEBUG

#endif /* CSS_H */
