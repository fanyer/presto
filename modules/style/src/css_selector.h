/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_SELECTOR_H
#define CSS_SELECTOR_H

#include "modules/util/simset.h"
#include "modules/style/css_value_types.h"
#include "modules/logdoc/html.h"
#include "modules/logdoc/namespace.h"
#include "modules/logdoc/logdocenum.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/style/css_decl.h"
#include "modules/style/css_property_list.h"
#include "modules/style/css.h"
#include "modules/style/css_matchcontext.h"

class HLDocProfile;

#define MAKE_AB_COEFF(a, b) ((static_cast<UINT16>(a) << 16) | static_cast<UINT16>(b))
#define GET_AB_COEFF(x, a, b) (a = static_cast<INT16>(((x) >> 16) & 0xffff), b = static_cast<INT16>((x) & 0xffff))
#define AB_COEFF_MIN -32768
#define AB_COEFF_MAX 32767

enum SelAttrType
{
	CSS_SEL_ATTR_TYPE_HTMLATTR_EQUAL,
	CSS_SEL_ATTR_TYPE_HTMLATTR_INCLUDES,
	CSS_SEL_ATTR_TYPE_HTMLATTR_DEFINED,
	CSS_SEL_ATTR_TYPE_HTMLATTR_DASHMATCH,
	CSS_SEL_ATTR_TYPE_HTMLATTR_PREFIXMATCH,
	CSS_SEL_ATTR_TYPE_HTMLATTR_SUFFIXMATCH,
	CSS_SEL_ATTR_TYPE_HTMLATTR_SUBSTRMATCH,
	CSS_SEL_ATTR_TYPE_PSEUDO_PAGE,
	CSS_SEL_ATTR_TYPE_PSEUDO_CLASS,
	CSS_SEL_ATTR_TYPE_CLASS,
	CSS_SEL_ATTR_TYPE_ID,
	CSS_SEL_ATTR_TYPE_ELM,
	CSS_SEL_ATTR_TYPE_NTH_CHILD,
	CSS_SEL_ATTR_TYPE_NTH_OF_TYPE,
	CSS_SEL_ATTR_TYPE_NTH_LAST_CHILD,
	CSS_SEL_ATTR_TYPE_NTH_LAST_OF_TYPE
};

enum PseudoPageType
{
	PSEUDO_PAGE_UNKNOWN	= 0,
	PSEUDO_PAGE_FIRST	= 1,
	PSEUDO_PAGE_LEFT	= 2,
	PSEUDO_PAGE_RIGHT	= 4
};

enum PseudoClassType
{
	// CLASSES:
	//
	// link, visited, hover, active, focus, target, lang, enabled, disabled, checked, indeterminate,
	// root, first-child, last-child, first-of-type, only-child, only-of-type, empty.
	//
	// nth-* classes are stored with their own SelAttrType, not as pseudo classes.
	//
	// ELEMENTS:
	//
	// first-line, first-letter, selection, before, after, cue.
	//
	// HTML Track:
	//
	// past, future, cue.
	//
	// CSS3 Basic User Interface Module:
	//
	// valid, invalid, default, in-range, required, optional, read-only, read-write, out-of-range,
	//
	PSEUDO_CLASS_UNKNOWN,
	PSEUDO_CLASS_CUE,
	PSEUDO_CLASS_LANG,
	PSEUDO_CLASS_LINK,
	PSEUDO_CLASS_PAST,
	PSEUDO_CLASS_ROOT,
	PSEUDO_CLASS_AFTER,
	PSEUDO_CLASS_EMPTY,
	PSEUDO_CLASS_FOCUS,
	PSEUDO_CLASS_HOVER,
	PSEUDO_CLASS_VALID,
	PSEUDO_CLASS_ACTIVE,
	PSEUDO_CLASS_BEFORE,
	PSEUDO_CLASS_FUTURE,
	PSEUDO_CLASS_TARGET,
	PSEUDO_CLASS_CHECKED,
	PSEUDO_CLASS_DEFAULT,
	PSEUDO_CLASS_ENABLED,
	PSEUDO_CLASS_INVALID,
	PSEUDO_CLASS_VISITED,
	PSEUDO_CLASS_DISABLED,
	PSEUDO_CLASS_IN_RANGE,
	PSEUDO_CLASS_OPTIONAL,
	PSEUDO_CLASS_REQUIRED,
	PSEUDO_CLASS_READ_ONLY,
	PSEUDO_CLASS_SELECTION,
	PSEUDO_CLASS_FIRST_LINE,
	PSEUDO_CLASS_LAST_CHILD,
	PSEUDO_CLASS_ONLY_CHILD,
	PSEUDO_CLASS_READ_WRITE,
	PSEUDO_CLASS_FULLSCREEN,
	PSEUDO_CLASS__O_PREFOCUS,
	PSEUDO_CLASS_FIRST_CHILD,
	PSEUDO_CLASS_FIRST_LETTER,
	PSEUDO_CLASS_LAST_OF_TYPE,
	PSEUDO_CLASS_ONLY_OF_TYPE,
	PSEUDO_CLASS_OUT_OF_RANGE,
	PSEUDO_CLASS_FIRST_OF_TYPE,
	PSEUDO_CLASS_INDETERMINATE,
	PSEUDO_CLASS_FULLSCREEN_ANCESTOR
};

enum {

	/** Selector specificity for style attributes.
		Used for making style attributes beat everything else. */

	STYLE_SPECIFICITY = USHRT_MAX,
};

class CSS_SelectorAttribute : public Link
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

public:

	CSS_SelectorAttribute(unsigned short typ, unsigned short att, uni_char* val, int ns_index = NS_IDX_DEFAULT);
	CSS_SelectorAttribute(unsigned short typ, uni_char* name_and_val, int ns_index = NS_IDX_DEFAULT);
	CSS_SelectorAttribute(unsigned short typ, unsigned short att);
	CSS_SelectorAttribute(unsigned short typ, ReferencedHTMLClass* ref);
	/** :nth-* functions. */
	CSS_SelectorAttribute(unsigned short typ, int a, int b);

	virtual ~CSS_SelectorAttribute();

#ifdef MEDIA_HTML_SUPPORT
	CSS_SelectorAttribute* Clone();
	void SetCueString(uni_char* serialized_cue) { m_string = serialized_cue; }
	void SetType(unsigned short type) { m_type = (m_type & 0x8000) | type; }
	void SetAttr(unsigned short attr) { m_attr = attr; }
	void SetNsIdx(int ns_idx) { m_ns_idx = ns_idx; }
#endif // MEDIA_HTML_SUPPORT

	unsigned short GetType() { return m_type & 0x7fff; }
	unsigned short GetAttr() { return m_attr; }
	unsigned short GetPseudoClass() { return m_attr; }
	const uni_char*	GetId() { return m_string; }
	const uni_char* GetClass() { if (IsClassRef()) return m_class->GetString(); else return m_string; }
	ReferencedHTMLClass* GetClassRef() { OP_ASSERT(IsClassRef()); return m_class; }
	BOOL IsClassRef() { return m_attr != 0; }
	const uni_char*	GetValue() { return m_string; }
	int GetNsIdx() { return m_ns_idx; }

	BOOL IsNegated() { return (m_type & 0x8000) != 0; }
	void SetNegated() { m_type |= 0x8000; }

	CSS_SelectorAttribute* Suc() { return (CSS_SelectorAttribute*)Link::Suc(); }
	CSS_SelectorAttribute* Pred() { return (CSS_SelectorAttribute*)Link::Pred(); }

	OP_STATUS GetSelectorText(TempBuffer* buf);

	BOOL MatchNFunc(int x)
	{
		int a, b;
		GET_AB_COEFF(m_nth_expr, a, b);
		if (a == 0)
			return (b == x);
		else
			return ((x-b)/a >= 0 && (x-b)%a == 0);
	}

private:
	/** Attribute, or pseudo class, 0 if id or class is string, 1 if class is class reference. */
	unsigned short m_attr;

	/** Type of selector attribute with high-bit used to indicate that attribute is negated by :not. */
	unsigned short m_type;

	/** Value of attribute if attr, else first char is '.' if class or '#' if id.
		If type is lang pseudo class this is the language code string.
		If type is cue pseudo element this is the serialized selectors. */
	union
	{
		uni_char* m_string;
		ReferencedHTMLClass* m_class;
	};

	union
	{
		/** namespace index for attribute name. */
		int m_ns_idx;
		/** Coefficients in the nth-expression. */
		UINT32 m_nth_expr;
	};
};

enum CSS_CombinatorType
{
	CSS_COMBINATOR_DESCENDANT = 0,
	CSS_COMBINATOR_CHILD = 1,
	CSS_COMBINATOR_ADJACENT	= 2,
	CSS_COMBINATOR_ADJACENT_INDIRECT = 3
};

class CSS_Selector;

/** Represents a simple selector as defined in CSS21. */
class CSS_SimpleSelector : public Link
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT_L

public:
	CSS_SimpleSelector(int el);
	virtual ~CSS_SimpleSelector();

#ifdef MEDIA_HTML_SUPPORT
	CSS_SimpleSelector* Clone();
#endif

	uni_char* GetElmName() const { return m_elm_name; };

	/** Set an element name string which is already allocated. */
	void SetElmName(uni_char* val);

	int GetNameSpaceIdx() const { return m_ns_idx; }
	void SetNameSpaceIdx(int val);

	Markup::Type GetElm() const { return (Markup::Type)m_packed.elm; }
	void SetElm(Markup::Type elm_type) { m_packed.elm = elm_type; }
	unsigned short GetCombinator() { return m_packed.combinator; }

	void SetCombinator(unsigned short comb) { m_packed.combinator = comb; }
	void AddAttribute(CSS_SelectorAttribute* new_attr);

	void CountIdsAndAttrs(unsigned short& id_count,
						  unsigned short& attr_count,
						  unsigned short& elm_count,
						  unsigned int& has_class,
						  unsigned int& has_id,
						  BOOL& has_fullscreen);

	BOOL Match(CSS_MatchContext* context,
			   CSS_MatchContextElm* context_elm,
			   int& failed_pseudo) const
	{
		if (m_packed.universal)
		{
			if (m_packed.universal == UNIVERSAL_CLASS)
			{
				const ClassAttribute* attr = context_elm->GetClassAttribute();
				if (attr)
					if (UniversalIsRef())
						return attr->MatchClass(m_universal_class_ref);
					else
						return attr->MatchClass(m_universal_class, context->StrictMode());
				else
					return FALSE;
			}
			if (m_packed.universal == UNIVERSAL_ID)
			{
				const uni_char* he_id = context_elm->GetIdAttribute();
				return (he_id && (context->StrictMode() && uni_strcmp(he_id, m_universal_id) == 0 ||
								  !context->StrictMode() && uni_stricmp(he_id, m_universal_id) == 0));
			}
			return TRUE;
		}

		if (GetNameSpaceIdx() == NS_IDX_ANY_NAMESPACE && GetElm() != Markup::HTE_UNKNOWN && context_elm->GetNsIdx() == NS_IDX_HTML && context_elm->Type() != Markup::HTE_UNKNOWN)
		{
			if (GetElm() != Markup::HTE_ANY && GetElm() != context_elm->Type())
				return FALSE;
		}
		else
			if (!MatchType(context_elm, context->IsXml()))
				return FALSE;

		return (!GetFirstAttr() || MatchAttrs(context, context_elm, failed_pseudo));
	}

	CSS_SelectorAttribute* GetFirstAttr() const { return (CSS_SelectorAttribute*)m_sel_attr_list.First(); }
	CSS_SelectorAttribute* GetLastAttr() const { return (CSS_SelectorAttribute*)m_sel_attr_list.Last(); }

	CSS_SimpleSelector*	Suc() { return (CSS_SimpleSelector*)Link::Suc(); }
	CSS_SimpleSelector*	Pred() { return (CSS_SimpleSelector*)Link::Pred(); }

	OP_STATUS GetSelectorText(const CSS* stylesheet, TempBuffer* buf);

	void SetSerialize(BOOL serialize) { m_packed.dont_serialize = !serialize; }
	BOOL IsSerializable() const { return !m_packed.dont_serialize; }

	void RemoveAttrs() { m_sel_attr_list.Clear(); }

private:

	BOOL MatchAttrs(CSS_MatchContext* context, CSS_MatchContextElm* context_elm, int& failed_pseudo) const;
	BOOL MatchType(CSS_MatchContextElm* context_elm, BOOL is_xml) const;

	typedef enum {
		NOT_UNIVERSAL = 0,
		UNIVERSAL = 1,
		UNIVERSAL_CLASS = 2,
		UNIVERSAL_ID = 3
	} UniversalType;

	void SetUniversal(UniversalType type) { m_packed.universal = type; }
	void SetUniversalIsString() { m_packed.universal_is_string = 1; }
	BOOL UniversalIsRef() const { return m_packed.universal_is_string == 0; }

	union
	{
		struct
		{
			/** Html tag or '*' (universal selector) */
			unsigned int elm:16;
			/** One of CSS_CombinatorType. */
			unsigned int combinator:2;
			/** Universal selector, or universal selector with a single id or class selector attribute. */
			unsigned int universal:2;
			/** Is the universal class selector a string or a ReferencedHTMLClass? */
			unsigned int universal_is_string:1;
			/** Should this simple selector be skipped during serialization? */
			unsigned int dont_serialize:1;

			// 10 bits left.
		} m_packed;
		unsigned int m_packed_init;
	};

	/** list of selector attributes (CSS_SelectorAttribute) */
	Head m_sel_attr_list;

	union
	{
		uni_char* m_elm_name;
		const uni_char* m_universal_class;
		const uni_char* m_universal_id;
		ReferencedHTMLClass* m_universal_class_ref;
	};

	int m_ns_idx;
};

/** This class represents one selector. Each style rule may have several comma separated selectors. */
class CSS_Selector : public Link
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

public:

	enum MatchResult
	{
		NO_MATCH = 0,
		MATCH,
		MATCH_SELECTION
	};

	CSS_Selector() { m_packed_init = 0; }
	virtual ~CSS_Selector() { m_simple_selectors.Clear(); }

	void AddSimpleSelector(CSS_SimpleSelector* sel, unsigned short combinator = CSS_COMBINATOR_DESCENDANT);

#ifdef MEDIA_HTML_SUPPORT
	void Prepend(Head& simple_sels);
	void PrependSimpleSelector(CSS_SimpleSelector* sel);
#endif

	unsigned short GetSpecificity() { return m_packed.specificity; }
	void CalculateSpecificity();

	/** Returns the pseudo element type if the selector has a pseudo element in the rightmost simple selector.
		Else, returns ELM_NOT_PSEUDO. */
	PseudoElementType GetPseudoElm() const { return PseudoElementType(m_packed.pseudo_elm); }

	/** Returns TRUE if the ruleset that this selector belongs to has a content property declaration. */
	BOOL HasContentProperty() const { return m_packed.has_content_property; }

	/** This selector has at least one non-negated class selector in the selector target. */
	BOOL HasClassInTarget() const { return m_packed.has_class_in_target; }

	/** This selector has one and only one non-negated class selector in the selector target. */
	BOOL HasSingleClassInTarget() const { return m_packed.has_single_class_in_target; }

	/** This selector has at least one non-negated id selector in the selector target. */
	BOOL HasIdInTarget() const { return m_packed.has_id_in_target; }

	/** This selector has one and only one non-negated id selector in the selector target. */
	BOOL HasSingleIdInTarget() const { return m_packed.has_single_id_in_target; }

	/** @returns TRUE if this selector needs to be matched when finding URLs to prefetch. */
	BOOL MatchPrefetch() const { return m_packed.match_prefetch; }

	/** @returns TRUE if this selector has :fullscreen or :fullscreen-ancestor in other
				 simple selectors than the last one. */
	BOOL HasComplexFullscreen() const { return m_packed.complex_fullscreen; }

	CSS_SimpleSelector* FirstSelector() const { return (CSS_SimpleSelector*)m_simple_selectors.First(); }
	CSS_SimpleSelector* LastSelector() const { return (CSS_SimpleSelector*)m_simple_selectors.Last(); }

	MatchResult Match(CSS_MatchContext* context, CSS_MatchContextElm* context_elm) const;

	Markup::Type GetTargetElmType() { return static_cast<CSS_SimpleSelector*>(m_simple_selectors.First())->GetElm(); }

	int GetMaxSuccessiveAdjacent();

	OP_STATUS GetSelectorText(const CSS* stylesheet, TempBuffer* buf);

	unsigned short GetPseudoPage()
	{
		if (FirstSelector()
			&& FirstSelector()->GetFirstAttr()
			&& FirstSelector()->GetFirstAttr()->GetType() == CSS_SEL_ATTR_TYPE_PSEUDO_PAGE)
			return FirstSelector()->GetFirstAttr()->GetAttr();
		else
			return PSEUDO_PAGE_UNKNOWN;
	}

#ifdef MEDIA_HTML_SUPPORT
	BOOL HasPastOrFuturePseudo();
#endif // MEDIA_HTML_SUPPORT

	void SetHasContentProperty(BOOL has_it) { m_packed.has_content_property = !!has_it; }
	void SetMatchPrefetch(BOOL prefetch) { m_packed.match_prefetch = !!prefetch; }
	void SetSerialize(BOOL serialize) { m_packed.dont_serialize = !serialize; }

	CSS_Selector* SucActual()
	{
		CSS_Selector* next_sel = static_cast<CSS_Selector*>(Suc());
		while (next_sel && next_sel->m_packed.dont_serialize)
			next_sel = static_cast<CSS_Selector*>(next_sel->Suc());

		return next_sel;
	}

private:

	BOOL Match(CSS_MatchContext* context,
			   CSS_MatchContextElm* context_elm,
			   CSS_SimpleSelector* simple_sel,
			   BOOL& match_to_top,
			   BOOL& match_to_left) const;

	void SetPseudoElm(PseudoElementType pseudo_elm) { m_packed.pseudo_elm = pseudo_elm; }
	void SetHasClassInTarget() { m_packed.has_class_in_target = 1; }
	void SetHasIdInTarget() { m_packed.has_id_in_target = 1; }
	void SetHasSingleIdInTarget() { m_packed.has_single_id_in_target = 1; }
	void SetHasSingleClassInTarget() { m_packed.has_single_class_in_target = 1; }
	void SetHasComplexFullscreen() { m_packed.complex_fullscreen = 1; }

	Head m_simple_selectors;

	union
	{
		struct
		{
			unsigned int specificity:16;
			unsigned int pseudo_elm:3;
			unsigned int has_content_property:1;
			unsigned int has_class_in_target:1;
			unsigned int has_id_in_target:1;
			unsigned int has_single_id_in_target:1;
			unsigned int has_single_class_in_target:1;
			unsigned int match_prefetch:1;
			unsigned int complex_fullscreen:1;
			unsigned int dont_serialize:1;
		} m_packed; /* 27 bits in use. */
		unsigned int m_packed_init;
	};
};

inline BOOL MatchElmType(HTML_Element* match_elm, Markup::Type elm_type, NS_Type ns_type, const uni_char* elm_name, BOOL case_sensitive)
{
	if (match_elm->IsMatchingType(elm_type, ns_type))
	{
		if (elm_type == Markup::HTE_UNKNOWN)
		{
			OP_ASSERT(elm_name);
			const uni_char* elm_name2 = match_elm->GetTagName();
			if (case_sensitive)
				return (uni_strcmp(elm_name, elm_name2) == 0);
			else
				return (uni_stricmp(elm_name, elm_name2) == 0);
		}
		else
			return TRUE;
	}
	else
		return FALSE;
}

#endif // CSS_SELECTOR_H
