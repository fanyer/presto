/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_PARSER_H
#define CSS_PARSER_H

#include "modules/style/css.h"
#include "modules/style/css_dom.h"
#include "modules/style/css_gradient.h"
#include "modules/style/src/css_keyframes.h"
#include "modules/style/src/css_lexer.h"
#include "modules/style/src/css_buffer.h"
#include "modules/style/src/css_selector.h"
#include "modules/style/src/css_ruleset.h"
#include "modules/style/src/css_supports_rule.h"
#include "modules/style/src/css_media_rule.h"
#include "modules/style/src/css_fontface_rule.h"
#include "modules/style/src/css_viewport_rule.h"
#include "modules/style/css_condition.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/display/vis_dev.h"

#ifdef CSS_ERROR_SUPPORT
# include "modules/console/opconsoleengine.h"
#endif

class CSS_Parser
{
private:
	enum { CSS_VAL_ARRAY_SIZE = 64 };

public:

	/** A helper class to parse the background shorthand. */
	friend class CSS_BackgroundShorthandInfo;

	class ValueArray
	{
	private:

		/** The array of values currently being parsed. */
		PropertyValue* m_val_array;

		/** If using a preallocated array that should not be owned/deleted by ValueArray */
		PropertyValue* m_prealloc_array;

		/** The intial reallocation step size */
		int m_initial_step_size;

		/** The size of the currently allocated array of values */
		int m_val_array_size;

		/** The number of values currently in the PropertyValue array. */
		int m_val_count;

		void CheckArraySizeL()
		{
			if (!m_val_array || m_val_count + 1 >= m_val_array_size)
			{
				int new_size = (!m_val_array ? m_initial_step_size : m_val_array_size*2);
				PropertyValue* new_array = OP_NEWA_L(PropertyValue, new_size);

				for (int i=0; i<m_val_count; i++)
					new_array[i] = m_val_array[i];

				if (m_val_array != m_prealloc_array)
					OP_DELETEA(m_val_array);

				m_val_array_size = new_size;
				m_val_array = new_array;
			}

			OP_ASSERT(m_val_count + 1 < m_val_array_size);
		}

	public:
		ValueArray(int default_step_size = 8, PropertyValue* stack_prealloc_block = NULL) :
			m_val_array(stack_prealloc_block),
			m_prealloc_array(stack_prealloc_block),
			m_initial_step_size(default_step_size),
			m_val_array_size(default_step_size),
			m_val_count(0) {}

		~ValueArray()
		{
			if (m_val_array != m_prealloc_array)
				OP_DELETEA(m_val_array);
		}

		/** Add an expression to the value array. Used for declarations, but also for media queries. */
		void AddValueL(const PropertyValue& value)
		{
			CheckArraySizeL();
			m_val_array[m_val_count++] = value;
		}

		void ResetValCount()
		{
			m_val_count = 0;

			if (m_val_array != m_prealloc_array)
			{
				OP_DELETEA(m_val_array);
				m_val_array = m_prealloc_array;
				m_val_array_size = m_initial_step_size;
			}
		}

		void CommitToL(ValueArray& dest);

		int GetCount() const { return m_val_count; }
		PropertyValue& operator[](int i) { OP_ASSERT(i < m_val_count); return m_val_array[i]; }

		const PropertyValue& operator[](int i) const { OP_ASSERT(i < m_val_count); return m_val_array[i]; }

	};

	CSS_Parser(CSS* css, CSS_Buffer* buf, const URL& base_url, HLDocProfile* hld_profile, unsigned start_line_number);

	~CSS_Parser();

	/** Parse and construct an internal representation of the stylesheet. */
	void ParseL();

	/** Return the CSS_Buffer. Used in the grammar actions to retrieve strings. */
	CSS_Buffer* InputBuffer() { return m_input_buffer; }

	/** When setting cssText, replace this rule. insertRule will insert new rule before. */
	void SetDOMRule(CSS_Rule* rule, BOOL replace) { m_replace_rule = replace; m_dom_rule = rule; }

	/** Return the HLDocProfile for the document.
		Returns NULL if the stylesheet is not connected to a document. */
	HLDocProfile* HLDProfile() { return m_hld_prof; }

	BOOL IsXml() { return m_stylesheet ? m_stylesheet->IsXml() : m_hld_prof->IsXml(); }

	/** Return the url which should be used as a base url for resolving
		relative urls within the stylesheet. */
	const URL& GetBaseURL() { return m_base_url; }

	/** Return the next token from the lexical analyzer. */
	int Lex(YYSTYPE* value);

	void SetCurrentSimpleSelector(CSS_SimpleSelector* new_selector) { new_selector->Into(&m_simple_selector_stack); }
	CSS_SimpleSelector* GetCurrentSimpleSelector() { return (CSS_SimpleSelector*)m_simple_selector_stack.Last(); }
	void DeleteCurrentSimpleSelector() { CSS_SimpleSelector* last = (CSS_SimpleSelector*)m_simple_selector_stack.Last(); last->Out(); OP_DELETE(last); }
	bool AddSelectorAttributeL(CSS_SelectorAttribute* sel_attr);
	bool AddClassSelectorL(int start_pos, int length);
	void SetCurrentSelector(CSS_Selector* new_sel) { OP_DELETE(m_current_selector); m_current_selector = new_sel; }
	void DeleteCurrentSelector() { OP_DELETE(m_current_selector); m_current_selector = NULL; }
	void AddCurrentSelectorL();
	bool AddCurrentSimpleSelector(short combinator = CSS_COMBINATOR_DESCENDANT);
	void DiscardRuleset();
	BOOL HasSelectors()
	{
#ifdef MEDIA_HTML_SUPPORT
		if (m_in_cue)
			return FALSE;
#endif // MEDIA_HTML_SUPPORT

		return !m_selector_list.Empty();
	}
	/** Finished parsing selectorText in dom. Update the rule accordingly. */
	void EndSelectorL(uni_char* page_name=NULL);
#ifdef MEDIA_HTML_SUPPORT
	void BeginCueSelector();
	void EndCueSelector();
	OP_STATUS ApplyCueSelectors();
	void AddCueRootSelectorL();
#endif // MEDIA_HTML_SUPPORT

	/** State values for which type of rules are allowed at
		the current position of the stylesheet being parsed. */
	enum AllowLevel
	{
		ALLOW_CHARSET,
		ALLOW_IMPORT,
		ALLOW_NAMESPACE,
		ALLOW_STYLE
	};

	enum DeclarationBlockType
	{
		DECL_RULESET,
		DECL_PAGE,
		DECL_FONTFACE,
		DECL_VIEWPORT,
		DECL_KEYFRAMES
	};

	/** Return values for SetColor */
	typedef enum
	{
		/** transparent, currentColor, inherit, invert */
		COLOR_KEYWORD,
		COLOR_SKIN,
		COLOR_RGBA,
		/** a named color */
		COLOR_NAMED,
		COLOR_INVALID
	} ColorValueType;

	/** Parses a color token using member variables. May find
	    a color, a named color, a non-color keyword valid in <color>, a skin color,
	    a keyword that is found in place of a <color>, or nothing that is valid in or instead of <color>.

	    The following non-color keywords are valid in <color>:
	    - transparent
	    - currentColor

	    The following are not <color> values, but are sometimes found as alternative property values:
	    - inherit
	    - invert

	    All of the above are treated as COLOR_KEYWORD.

	    If SKIN_SUPPORT is disabled, -o-skin() will be treated as invalid and skin_color is not used.

	    @param[out] color           if the return value is COLOR_RGBA or COLOR_NAMED, contains the color.
	    @param[out] special_keyword if the return value is COLOR_KEYWORD, contains a keyword.
	                                @note When COLOR_NAMED is returned, special_keyword MAY contain a keyword.
	                                      However, do not rely on this behavior.
	    @param[out] skin_color      if the return value is COLOR_SKIN, contains a string naming the skin color.
	    @param[in] value            color token to be parsed.
	    @return                     a ColorValueType indicating whether the method found
	                                - a color value (COLOR_RGBA)
	                                - a named color (COLOR_NAMED)
	                                - a non-color keyword (COLOR_KEYWORD)
	                                - a skin color (COLOR_SKIN)
	                                - nothing that is valid in or instead of <color> (COLOR_INVALID) */
	ColorValueType SetColorL(COLORREF& color, CSSValue& special_keyword, uni_char*& skin_color, const PropertyValue& value) const;

#ifdef CSS_GRADIENT_SUPPORT
	/** Convenience function that calls either SetLinearGradientL(), SetDoubleRainbowL or SetRadialGradientL()
		depending on type.

		@param[in,out] start_index  the current position in m_val_array. If parsing is successful
		                            the parser position is at the end of the function; otherwise
		                            it is undefined.
		@param[in,out] type         one of \c CSS_FUNCTION_LINEAR_GRADIENT,
		                                    \c CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT,
		                                    \c CSS_FUNCTION_O_LINEAR_GRADIENT,
		                                    \c CSS_FUNCTION_RADIAL_GRADIENT,
		                                    \c CSS_FUNCTION_REPEATING_LINEAR_GRADIENT or
		                                    \c CSS_FUNCTION_REPEATING_RADIAL_GRADIENT.
									May also be a CSS_FUNCTION_DOUBLE_RAINBOW, but this will
									be changed to CSS_FUNCTION_RADIAL_GRADIENT.
		@return a gradient if parsing is successful, otherwise NULL.
	*/
	CSS_Gradient* SetGradientL(int& start_index, CSSValueType& type) const;

	/** Parses the current ValueArray and returns a linear gradient, if found.
		@return a gradient if parsing is successful, otherwise NULL.
		@param[in,out] start_index @see SetGradientL()
		@param[in] legacy_syntax TRUE if the syntax without "to" should be accepted, FALSE otherwise
		@return a gradient if parsing is successful, otherwise NULL.
	*/
	CSS_LinearGradient* SetLinearGradientL(int& start_index, BOOL legacy_syntax) const;

	/** Parses the current ValueArray and returns a radial gradient, if found.
		@param[in,out] start_index @see SetGradientL()
		@return a gradient if parsing is successful, otherwise NULL.
	*/
	CSS_RadialGradient* SetRadialGradientL(int& start_index) const;

	/** Parses the current ValueArray and returns a double rainbow, if found.
		Syntax: -o-double-rainbow( <position> [, [ <size> | <length> | <percentage>]]? )

		@param[in,out] start_index @see SetGradientL()
		@return a gradient if parsing is successful, otherwise NULL.
	*/
	CSS_RadialGradient* SetDoubleRainbowL(int& start_index) const;

private:

	/** Used by SetRadialGradientL() to parse the position.
	    Consumes either a valid position prefixed by "at", or nothing. Only "at"
	    followed by an invalid position will result in an error.

		@param[in,out] pos the current position in m_val_array. If parsing is successful
	                       the parser position is on the next token to consume; otherwise
	                       it is undefined.
	    @param  gradient   the gradient to which to add the parsed position
	    @return FALSE if there is a parsing error, otherwise TRUE.
	*/
	BOOL SetRadialGradientPos(int& pos, CSS_RadialGradient& gradient) const;

	/** Used by SetLinearGradientL() and SetRadialGradientL() to parse color stops.
		@param[in,out] start_index the current position in m_val_array. If parsing is successful
		                           the parser position is at the end of the function; otherwise
		                           it is undefined.
		@param  gradient           the gradient to which to add the parsed color stops.
		@return FALSE if there is a parsing error, otherwise TRUE.
	*/
	BOOL SetColorStops(int& start_index, CSS_Gradient& gradient) const;

public:
#endif // CSS_GRADIENT_SUPPORT

	/** Charset rules are only allowed before all other rules, then imports, then
		namespaces, and finally all other rules. As rule types are added, this
		method is called to limit which rules are allowed to be added to the
		end of the stylesheet. For inserting new rules into the stylesheet
		through DOM, there is also a limit on which rules can be inserted both
		based on preceding and succeeding rules (hence the allow_max parameter).
		See \ref m_allow_min and \ref m_allow_max for further documentation.

		@param allow_min Limits which rules can be added to the stylesheet
						 rules after this call.
		@param allow_max Limits which rules can be inserted into the stylesheet
						 for the current DOM operation.	Calls passing this
						 parameter (not using default) should only be done
						 when parsing a single rule from DOM, and should only
						 be called once per parser object. */
	void SetAllowLevel(AllowLevel allow_min, AllowLevel allow_max = ALLOW_STYLE)
	{
		/* Doesn't make sense. If the values passed are correct, it would mean
		   that the rule list is already in an inconsistent state (style rule
		   before import for instance). */
		OP_ASSERT(allow_min <= allow_max);

		if (m_allow_min < allow_min)
			m_allow_min = allow_min;
		if (m_allow_max > allow_max)
			m_allow_max = allow_max;

		/* Should not happen. Same reason as previous assert. */
		OP_ASSERT(allow_min <= allow_max);
	}

	/** Returns TRUE if @charset rules are allowed at this point. */
	BOOL AllowCharset() { return m_allow_min == ALLOW_CHARSET && m_allow_max > ALLOW_CHARSET; }

	/** Returns TRUE if @import rules are allowed at this point. */
	BOOL AllowImport() { return m_allow_min <= ALLOW_IMPORT && m_allow_max >= ALLOW_IMPORT; }

	/** Returns TRUE if @namespace rules are allowed at this point. */
	BOOL AllowNamespace() { return m_allow_min <= ALLOW_NAMESPACE && m_allow_max >= ALLOW_NAMESPACE; }

	/** Returns TRUE if other rules than charset/import/namespace are allowed.
		Will only return FALSE when inserting rules before other rules.
		For instance, if you try to insert a StyleRule before an ImportRule. */
	BOOL AllowRuleset() { return m_allow_max == ALLOW_STYLE; }

	/** Returns TRUE if the document is in strict mode. User and browser stylesheets are always in strict mode. */
	BOOL StrictMode() { return (!m_hld_prof || m_hld_prof->IsInStrictMode()); }

	/** Set the current property list to add declarations to during parsing. */
	void SetCurrentPropList(CSS_property_list* props)
	{
		if (m_current_props)
			m_current_props->Unref();
		m_current_props = props;
		if (props)
			props->Ref();
	}

	/** Return the property list currently used for parsing a declaration block or style attribute. */
	CSS_property_list* GetCurrentPropList() { return m_current_props; }

	/** Add a rule to the stylesheet. */
	void AddRuleL(CSS_Rule* rule);

	/** Add an import rule to the stylesheet. */
	void AddImportL(const uni_char* url_str);

	/** Add a style rule to the stylesheet. */
	void EndRulesetL();

	/** Add a page rule to the stylesheet. */
	void EndPageRulesetL(uni_char* name);

	/** Add a supports rule to the stylesheet and set as current conditional rule. */
	void AddSupportsRuleL();

	/** Add a media rule to the stylesheet and set as current conditional rule. */
	void AddMediaRuleL();

	/** Conditional rule is closed. Set current conditional rule to the parent, which will be NULL if there is none. */
	void EndConditionalRule() { m_current_conditional_rule = m_current_conditional_rule->GetConditionalRule(); }

	/** When adding rules to a conditional rule in dom, set that conditional rule as the current one. */
	void SetCurrentConditionalRule(CSS_ConditionalRule* rule) { m_current_conditional_rule = rule; }

#ifdef CSS_ANIMATIONS
	/** When adding rules to a keyframes rule in dom, set
		that keyframes rule as the current keyframes rule. */
	void SetCurrentKeyframesRule(CSS_KeyframesRule* rule) { m_current_keyframes_rule = rule; }
#endif // CSS_ANIMATIONS

	/** Set the media object (list of media queries) currently being parsed. */
	void SetCurrentMediaObject(CSS_MediaObject* media_obj)
	{
		if (m_current_media_object)
			OP_DELETE(m_current_media_object);
		m_current_media_object = media_obj;
	}

	/** Set the media query currently being parsed. */
	void SetCurrentMediaQuery(CSS_MediaQuery* query)
	{
		if (m_current_media_query)
			OP_DELETE(m_current_media_query);
		m_current_media_query = query;
	}

	/** Matched a media query. Add it to the media object. */
	void EndMediaQueryL()
	{
		if (m_current_media_object)
		{
			if (!m_current_media_query)
				m_current_media_query = OP_NEW_L(CSS_MediaQuery, (TRUE, CSS_MEDIA_TYPE_ALL));
			m_current_media_object->AddMediaQuery(m_current_media_query);
			m_current_media_query = NULL;
		}
	}

	/** End of media-query-list. Commit media queries to the media object. */
	void EndMediaQueryListL() { if (m_current_media_object) m_current_media_object->EndMediaQueryListL(); }

	/** Add a media feature expression to the current media query. If the
		feature is unknown or has an invalid value, false is returned,
		otherwise true. */
	bool AddMediaFeatureExprL(CSS_MediaFeature feature);

	/** Return and reset the current media query to NULL. The caller takes over
		the responsibility for deleting the returned CSS_MediaQuery. */
	CSS_MediaQuery* GetMediaQuery() { CSS_MediaQuery* ret = m_current_media_query; m_current_media_query = NULL; return ret; }

	/** Return and reset the current media object to NULL. The caller takes over
		the responsibility for deleting the returned CSS_MediaObject. */
	CSS_MediaObject* GetMediaObject() { CSS_MediaObject* ret = m_current_media_object; m_current_media_object = NULL; return ret; }

	CSS_Condition* PopCondition()
	{
		OP_ASSERT(m_condition_list);
		CSS_Condition* c = m_condition_list;
		m_condition_list = c->next;
		c->next = NULL;
		return c;
	}

	void QueueCondition(CSS_Condition* c)
	{
		c->next = m_condition_list;
		m_condition_list = c;
	}

	void NegateCurrentCondition()
	{
		OP_ASSERT(m_condition_list);
		m_condition_list->Negate();
	}

	void FlushConditionQueue()
	{
		OP_DELETE(m_condition_list);
		m_condition_list = NULL;
	}


	/** Add an @font-face rule to the stylesheet setting the current property
		list as the font-face descriptors. */
	void AddFontfaceRuleL()
	{
		if (m_current_props)
			m_current_props->PostProcess(TRUE, FALSE);

		CSS_FontfaceRule* rule = OP_NEW_L(CSS_FontfaceRule, (m_current_props));
		AddRuleL(rule);

		SetAllowLevel(ALLOW_STYLE);
	}

	/** Add an @viewport rule to the stylesheet setting the current property
		list as the viewport properties. */
	void AddViewportRuleL()
	{
		if (m_current_props)
			m_current_props->PostProcess(TRUE, FALSE);

		CSS_ViewportRule* rule = OP_NEW_L(CSS_ViewportRule, (m_current_props));
		AddRuleL(rule);

		SetAllowLevel(ALLOW_STYLE);
	}

	/** Set/reset the declaration parsing in anonymous @keyframes
		mode. By itself useful for when parsing single properties in a
		keyframe setting. */
	void BeginKeyframes()
	{
#ifdef CSS_ANIMATIONS
		m_decl_type = DECL_KEYFRAMES;
#endif
	}

	void EndKeyframes()
	{
#ifdef CSS_ANIMATIONS
		m_decl_type = DECL_RULESET;
#endif // CSS_ANIMATIONS
	}

#ifdef CSS_ANIMATIONS
	/** Set/reset the declaration parsing in @keyframes mode. */
	void BeginKeyframesL(uni_char* name);

	/** Append keyframe selector */
	void AppendKeyframeSelector(CSS_KeyframeSelector* new_sel)
	{
		new_sel->Into(&m_current_keyframe_selectors);
	}

	/** Add a keyframe ruleset from @keyframes to the stylesheet. */
	void AddKeyframeRuleL();
#endif // CSS_ANIMATIONS

	/** Add an expression to the value array. Used for declarations, but also for media queries. */
	void AddValueL(const PropertyValue& value)
	{
		m_val_array.AddValueL(value);
	}

	void AddFunctionValueL(const PropertyValue& value)
	{
		m_function_val_array.AddValueL(value);
	}

	/** Commit the PropertyValues from the function value list to the current declaration value list. */
	void CommitFunctionValuesL()
	{
		m_function_val_array.CommitToL(m_val_array);
	}

	/** Discard the current declaration value list. */
	void ResetValCount()
	{
		m_val_array.ResetValCount();
		m_function_val_array.ResetValCount();
	}

	/** AddDeclaration and related functionality */
	typedef enum
	{
		OK = 0,
		INVALID = 1,
		OUT_OF_MEM = 2
	} DeclStatus;

	/** Add a declaration to the declaration block represented by prop_list. */
	DeclStatus AddDeclarationL(short property, BOOL important)
	{
		if (property < 0)
			return INVALID;

		m_last_decl = m_current_props->GetLastDecl();

		switch (m_decl_type)
		{
		case DECL_RULESET:
			return AddRulesetDeclL(property, important);
		case DECL_PAGE:
			return AddPageDeclL(property, important);
		case DECL_FONTFACE:
			if (!important)
				return AddFontfaceDeclL(property);
			break;
		case DECL_VIEWPORT:
			return AddViewportDeclL(property, important);

#ifdef CSS_ANIMATIONS
		case DECL_KEYFRAMES:
			if (!important)
				return AddRulesetDeclL(property, FALSE);
			break;
#endif // CSS_ANIMATIONS

		default:
			OP_ASSERT(!"Trying to add declaration to unknown declaration block.");
			break;
		}

		return INVALID;
	}

	/** Put a token on top of the lexer stack. Used for putting start tokens
		for identifying parsing of style values for dom operations. */
	void SetNextToken(int tok) { m_next_token = tok; }

	/** Set the css property that we are currently parsing the declaration values for. */
	void SetDOMProperty(short prop) { m_dom_property = prop; }

	/** The css property that we are currently parsing the declaration values for. */
	short GetDOMProperty() { return m_dom_property; }

	/** Set current namespace used for type selector. */
	void SetCurrentNameSpaceIdx(int ns_idx) { m_current_ns_idx = ns_idx; }

	/** Get namespace used for type selector being parsed. */
	int GetCurrentNameSpaceIdx() { return m_current_ns_idx; }

	/** Reset to default namespace used for type selectors. */
	void ResetCurrentNameSpaceIdx() { m_current_ns_idx = GetDefaultNameSpaceIdx(); if (m_current_ns_idx == NS_IDX_NOT_FOUND) m_current_ns_idx = NS_IDX_ANY_NAMESPACE; }

	/** Add a namespace prefix/uri pair from an @namespace rule. */
	void AddNameSpaceL(const uni_char* name, const uni_char* uri)
	{
		m_stylesheet->AddNameSpaceL(m_hld_prof, name, uri);
		SetAllowLevel(ALLOW_NAMESPACE);
	}

	/** Get the namespace index for a prefix. Returns NS_IDX_NOT_FOUND
		if the prefix is not bound to a uri. */
	int GetNameSpaceIdx(const uni_char* prefix) { return m_stylesheet ? m_stylesheet->GetNameSpaceIdx(prefix) : NS_IDX_NOT_FOUND; }

	/** Get the default namespace index defined by @namespace rule without a prefix. */
	int GetDefaultNameSpaceIdx() { return m_stylesheet ? m_stylesheet->GetDefaultNameSpaceIdx() : NS_IDX_NOT_FOUND; }

	/** Store the current level of curly braces '{}'. Used for block level error recovery. */
	void StoreBlockLevel(int lookahead) { m_lexer.StoreBlockLevel(lookahead); }

	/** See documentation for CSS_Lexer::RecoverBlockOrSemi. */
	void RecoverBlockOrSemi(int sync_token) { m_next_token = m_lexer.RecoverBlockOrSemi(sync_token); }

	/** See documentation for CSS_Lexer::RecoverDeclaration. */
	void RecoverDeclaration(int sync_token) { m_next_token = m_lexer.RecoverDeclaration(sync_token); }

	/** See documentation for CSS_Lexer::RecoverMediaQuery. */
	void RecoverMediaQuery(int sync_token) { m_next_token = m_lexer.RecoverMediaQuery(sync_token); }

	/** Push the EOF token back onto the lexer stack to return it once more.
		Used for error recovery. */
	void UnGetEOF() { m_lexer.UnGetEOF(); }

	/** Set that a parse error occurred. */
	void SetErrorOccurred(CSS_PARSE_STATUS err) { if (m_error_occurred == CSSParseStatus::OK) m_error_occurred = err; }

	/** Set/reset the declaration parsing in @page mode. */
	void BeginPage() { OP_ASSERT(m_decl_type == DECL_RULESET); m_decl_type = DECL_PAGE; }
	void EndPage() { m_decl_type = DECL_RULESET; }

	/** Set/reset the declaration parsing in @font-face mode. */
	void BeginFontface() { OP_ASSERT(m_decl_type == DECL_RULESET); m_decl_type = DECL_FONTFACE; }
	void EndFontface() { m_decl_type = DECL_RULESET; }

	/** Set/reset the declaration parsing in @viewport mode. */
	void BeginViewport() { OP_ASSERT(m_decl_type == DECL_RULESET); m_decl_type = DECL_VIEWPORT; }
	void EndViewport() { m_decl_type = DECL_RULESET; }

	/** Empty declaration */
	void EmptyDecl() { m_last_decl = NULL; }

	/** Delete unterminated declarations (not terminated by semi-colon). */
	void TerminateLastDecl();

	/** @returns TRUE if CSS_INT_NUMBER should not be converted to CSS_NUMBER. */
	BOOL UseIntegerTokens() const { return m_lexer.InMediaRule(); }

#if defined(CSS_ERROR_SUPPORT)

	/** Output error in error console. */
	void EmitErrorL(const uni_char* message, OpConsoleEngine::Severity severity);

	/** Common method for emitting illegal value errors for declarations. */
	void InvalidDeclarationL(DeclStatus error, short property);

#endif // CSS_ERROR_SUPPORT

	/** malloc used by YYMALLOC in bison parser */
	void* YYMalloc(size_t size)
	{
		int i = 0;
		if (m_yystack[0])
		{
			// Current assumption is that the generated bison parser
			// Only use YYMALLOC for allocating stack on the heap.
			// When it allocates more mem for the stack, it will immediately
			// free the old heap allocated stack. Thus, we need two entries only.
			OP_ASSERT(m_yystack[1] == 0);
			i = 1;
		}
		m_yystack[i] = op_malloc(size);
		return m_yystack[i];
	}

	/** free used by YYMALLOC in bison parser */
	void YYFree(void* ptr)
	{
		if (ptr == m_yystack[0])
			m_yystack[0] = NULL;
		else
		{
			OP_ASSERT(m_yystack[1] == ptr);
			m_yystack[1] = NULL;
		}
		op_free(ptr);
	}

	/** An invalid block is discovered by the grammar. */
	void InvalidBlockHit();

	CSS* GetStyleSheet() const { return m_stylesheet; }

private:
	/** Returns TRUE if the output screen supports RGBA colors. If this methods returns FALSE,
		declarations with rgba() and hsla() values will be dropped. */
	BOOL SupportsAlpha() const { return !m_hld_prof || m_hld_prof->GetVisualDevice()->SupportsAlpha(); }

	/** Get the keyword at the specified array index in the the input buffer. */
	CSSValue GetKeyword(int index)
	{
		OP_ASSERT(m_val_array[index].token == CSS_IDENT);
		return m_input_buffer->GetValueSymbol(m_val_array[index].value.str.start_pos, m_val_array[index].value.str.str_len);
	}

	/** Add a declaration to a ruleset. */
	DeclStatus AddRulesetDeclL(short property, BOOL important);

	/** Add a declaration to an @page rule. */
	DeclStatus AddPageDeclL(short property, BOOL important);

	/** Add a descriptor to an @font-face rule. */
	DeclStatus AddFontfaceDeclL(short property);

	/** Add a viewport property to an @viewport rule. */
	DeclStatus AddViewportDeclL(short property, BOOL important);

	/** Helper method for parsing a single <family-name>/<generic-family>/inherit. */
	DeclStatus SetFamilyNameL(CSS_generic_value& value,
							  int& pos,
							  BOOL allow_inherit,
							  BOOL allow_generic);

	/** Helper function for AddDeclarationL for setting font family. */
	DeclStatus SetFontFamilyL(CSS_property_list* prop_list, int start_idx, BOOL important, BOOL allow_inherit);

	void FindPositionValues(int i, CSS_generic_value values[4], int& n_values) const;

	/** Helper function for parsing fill position.
	    @param i[inout] The index of the value array to start at. This is useful if you have
	           multiple backgrounds; for content leave it at 0. If parsing is successful,
	           i is incremented by the number of values found.
	    @param[out] pos the computed position.
	    @param[out] pos_type the computed position's value types.
	    @param [in] allow_reference_point allow setting the reference point. background-position uses this.
	    @param [out] reference_point the user-supplied reference point (top, bottom, left, right), if any.
		             The horizontal reference is always stored in element 0, the vertical reference always in element 1.
	    @param [out] has_reference_point if the user supplied a reference point
	    @return OK if parsing was succesful, otherwise INVALID.
	 */
	DeclStatus SetPosition(int& i, float pos[2], CSSValueType pos_type[2], BOOL allow_reference_point, CSSValue reference_point[2], BOOL& has_reference_point) const;

	/** Helper function to expand a "timing function" to the
		respective represents */
	void SetTimingKeywordL(CSSValue keyword, CSS_generic_value_list& time_arr);

	/** Helper function for AddDeclarationL for setting (text/box) shadow. */
	DeclStatus SetShadowL(CSS_property_list* prop_list, short prop, BOOL important);

	/** Helper function for AddDeclarationL for setting individual border radius properties. */
	DeclStatus SetIndividualBorderRadiusL(CSS_property_list* prop_list, short prop, BOOL important);

	/** Helper function for AddDeclarationL for setting the shorthand border radius properties. */
	DeclStatus SetShorthandBorderRadiusL(CSS_property_list* prop_list, BOOL important);

	/** Helper function for AddDeclarationL for setting the background-repeat property.

		The background-repeat propery is a generic array
		representation of what the user entered as

		<repeat> [, <repeat> ]*

		repeat := repeat-x | repeat-y | [repeat | space | round | no-repeat]{1,2}	*/
	DeclStatus SetBackgroundRepeatL(CSS_property_list* prop_list, BOOL important);

	/** Helper function for AddDeclarationL for setting background image. */
	DeclStatus SetBackgroundImageL(CSS_property_list* prop_list, BOOL important);

	/** Helper function for AddDeclarationL for setting background size. */
	DeclStatus SetBackgroundSizeL(CSS_property_list* prop_list, BOOL important);

	/** Helper function for AddDeclarationL for setting the background or
	    replaced-content position.

	    @param prop the position property to set: either CSS_PROPERTY_object_position
	    or CSS_PROPERTY_background_position. */
	DeclStatus SetPositionL(CSS_property_list* prop_list, BOOL important, short prop);

	/** Helper function for AddDeclarationL for setting the background origin/clip/attachment. */
	DeclStatus SetBackgroundListL(CSS_property_list* prop_list, short prop, BOOL important);

	/** Helper function for AddDeclarationL for setting the background shorthand. */
	DeclStatus SetBackgroundShorthandL(CSS_property_list* prop_list, BOOL important);

	/** Helper function for AddDeclarationL for setting -o-border-image. */
	DeclStatus SetBorderImageL(CSS_property_list* prop_list, BOOL important);

#ifdef SVG_SUPPORT

	/** Helper function for AddRulesetDeclL for setting SVG fill and stroke paint URIs. */
	DeclStatus SetPaintUriL(CSS_property_list* prop_list, short prop, BOOL important);

#endif

#ifdef CSS_TRANSFORMS

	DeclStatus SetTransformListL(CSS_property_list* prop_list, BOOL important);

	DeclStatus SetTransformOriginL(CSS_property_list* prop_list, BOOL important);

#endif

	CSS_decl::DeclarationOrigin GetCurrentOrigin() const;

	/** The lexical analyzer. */
	CSS_Lexer m_lexer;

	/** The object used for internal storage of the stylesheet.
		The object is populated as we parse. */
	CSS* m_stylesheet;

	/** Set to TRUE if we're parsing a user stylesheet.
		Cached from m_stylesheet->GetUserDefined(). */
	BOOL m_user;

	/** The input buffer which is the parser input. */
	CSS_Buffer* m_input_buffer;

	/** The HLDocProfile object of the owner document.
		NULL for browser and user stylesheets. */
	HLDocProfile* m_hld_prof;

	/** The array of values for the declaration currently being parsed. */
	PropertyValue m_default_val_array[CSS_VAL_ARRAY_SIZE];

	/** The array of values for the declaration currently being parsed. */
	ValueArray m_val_array;

	/** The list of declarations for the current ruleset. */
	CSS_property_list* m_current_props;

	/** The url which should be used as a base url for resolving
		relative urls within the stylesheet. */
	URL m_base_url;

	/** Are @charset, @import or @namespace rules currently allowed?
		The m_allow_min is increased as rule types are added.
		Once an @namespace rule is added, you cannot add any @charset
		or @import rules. */
	AllowLevel m_allow_min;

	/** The m_allow_max level is used for inserting
		rules through DOM. If you insert a rule between two @imports,
		it has to be an @import rule (m_allow_max will be ALLOW_IMPORT).
		If you insert a rule between an @import rule and an @namespace
		rule, it can both be an @import or an @namespace rule (m_allow_min
		will be ALLOW_IMPORT and m_allow_max will be ALLOW_NAMESPACE).

		For normal stylesheet parsing when all rules are appended at
		the end, m_allow_max will be ALLOW_STYLE. */
	AllowLevel m_allow_max;

	/** Used by AddDeclarationL to figure out if this is a
		normal ruleset declaration, a font-face descriptor
		or a viewport property. */
	DeclarationBlockType m_decl_type;

	/** Forced lexer token put on top of lexer stack by SetNextToken. */
	int m_next_token;

	/** Current list of simple selectors. */
	Head m_simple_selector_stack;

	/** Current list of selectors. */
	Head m_selector_list;

#ifdef MEDIA_HTML_SUPPORT
	/** List of selectors parsed for a ::cue sub-selector. */
	Head m_cue_selector_list;

	/** Saved state of m_current_selector before parsing a ::cue sub-selector. */
	CSS_Selector* m_saved_current_selector;

	/** Set to TRUE if currently parsing a ::cue sub-selector. */
	BOOL m_in_cue;
#endif // MEDIA_HTML_SUPPORT

	/** The selector being parsed. */
	CSS_Selector* m_current_selector;

#ifdef CSS_ANIMATIONS
	/** The current set of uncommited keyframe selectors. */
	List<CSS_KeyframeSelector> m_current_keyframe_selectors;

	/** The keyframes being parsed */
	CSS_KeyframesRule* m_current_keyframes_rule;
#endif // CSS_ANIMATIONS

	/** The conditional rule being parsed. */
	CSS_ConditionalRule* m_current_conditional_rule;

	/** The medium list being parsed. */
	CSS_MediaObject* m_current_media_object;

	/** The media query being parsed. */
	CSS_MediaQuery* m_current_media_query;

	/** Set to TRUE if m_dom_rule should be replaced. FALSE means that the parsed rule
		is supposed to be inserted before m_dom_rule. */
	BOOL m_replace_rule;

	/** The rule which the parsed rule will replace or be inserted before. */
	CSS_Rule* m_dom_rule;

	/** The css property that we are currently parsing the declaration values for. */
	short m_dom_property;

	/** Current namespace used for type selector. */
	int m_current_ns_idx;

	/** Set to TRUE if a parse error occurred. */
	CSS_PARSE_STATUS m_error_occurred;

	/** Last added CSS_decl to m_current_props. Set to NULL when terminated by ';'. */
	CSS_decl* m_last_decl;

	/** Store pointers allocated by YYMALLOC. If cssparse() LEAVEs, we need to free this memory. */
	void* m_yystack[2];

	/** For storing values when parsing local() and other functions */
	ValueArray m_function_val_array;

	/** The line number of the style tag content if embedded in markup
		or 1 in the case of a standalone CSS file. */
	unsigned m_document_line;

#ifdef SCOPE_CSS_RULE_ORIGIN
	/** Line number of the current rule. (Where the declaration block begins). */
	unsigned m_rule_line_no;
#endif // SCOPE_CSS_RULE_ORIGIN

	CSS_Condition* m_condition_list;
};

#endif // CSS_PARSER_H
