/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef CSS_MATCHCONTEXT_H
#define CSS_MATCHCONTEXT_H

#include "modules/logdoc/html.h"
#include "modules/logdoc/logdocenum.h"
#include "modules/logdoc/markup.h" // for Markup::Type
#include "modules/logdoc/namespace.h"
#include "modules/logdoc/logdoc_module.h" // for g_ns_manager
#include "modules/doc/css_mode.h"
#include "modules/style/css_media.h"

class ClassAttribute;
class HTML_Element;
class FramesDocument;
class LayoutProperties;

#ifdef STYLE_GETMATCHINGRULES_API
class CSS_MatchRuleListener;
#endif // STYLE_GETMATCHINGRULES_API

class CSS_MatchContextElm;

/** Cache for :nth-* selectors. */

struct CSS_NthCache
{
	/** Which cached value. Index into arrays. */

	enum Type
	{
		NTH_OF_TYPE,
		NTH_LAST_OF_TYPE,
		NTH_CHILD,
		NTH_LAST_CHILD
	};

	/** Various constants. */

	enum { UNKNOWN, NTH_ARRAY_SIZE = NTH_LAST_CHILD+1, NTH_TYPE_ARRAY_SIZE = NTH_LAST_OF_TYPE+1};

	/** Construct. Reset cache. */

	CSS_NthCache() { Reset(); }

	/** Reset cache. */

	void Reset()
	{
		unsigned int i;

		for (i=0; i<NTH_ARRAY_SIZE; i++)
		{
			m_elements[i] = NULL;
			m_counts[i] = UNKNOWN;
		}

		for (i=0; i<NTH_TYPE_ARRAY_SIZE; i++)
		{
			m_types[i] = Markup::HTE_UNKNOWN;
			m_ns_types[i] = NS_NONE;
		}
	}

	/** The elements for which the values are currently cached. */

	HTML_Element* m_elements[NTH_ARRAY_SIZE];

	/** The cached counts. */

	unsigned int m_counts[NTH_ARRAY_SIZE];

	/** Element types of the currently cached *-of-type-* values. */

	Markup::Type m_types[NTH_TYPE_ARRAY_SIZE];

	/** NS types of the currently cached *-of-type-* values. */

	NS_Type m_ns_types[NTH_TYPE_ARRAY_SIZE];
};


/** An element of the CSS_MatchContext. Each element represents an HTML_Element
	in a path down the document tree, with the most accessed properties/attributes
	of the HTML_Element cached. */

class CSS_MatchContextElm
{
public:

	/** The various link states for an element. */
	enum LinkState
	{
		/** We haven't checked if this element is a link.
		    Pref PrefsCollectionDisplay::VisitedLinksState is 0 **/
		UNRESOLVED_NO_VISITED,
		/** We haven't checked if this element is a link.
		    Pref PrefsCollectionDisplay::VisitedLinksState is 1 **/
		UNRESOLVED_VISITED_SAME_SERVER,
		/** We haven't checked if this element is a link.
		    Pref PrefsCollectionDisplay::VisitedLinksState is 2 **/
		UNRESOLVED,

		/** This element is not a link. */
		NO_LINK,
		/** This element is an unvisited link. */
		LINK,
		/** This element is a visited link. */
		VISITED
	};

	/** Fullscreen pseudo class for the element. */
	enum Fullscreen
	{
		NO_FULLSCREEN,
		FULLSCREEN,
		FULLSCREEN_ANCESTOR
	};

	/** Initialise this context element for a given HTML_Element and its
		child/parent context elements.

		@param parent The parent context element. Used in the TOP_DOWN mode.
					  In the BOTTOM_UP mode, this should be NULL.
		@param child The child context element. Used in the BOTTOM_UP mode.
					 In the TOP_DOWN mode, this should be NULL. It's also
					 NULL for sibling elements.
		@param html_elm The HTML_Element to initialize the context element for.
		@param fullscreen_elm The current fullscreen element for the document.
							  NULL if there is none.
		@param visited_links_state The UNRESOLVED state that matches the value
					               of the preference opera:config#VisitedLink|VisitedLinksState */

	void Init(CSS_MatchContextElm* parent,
			  CSS_MatchContextElm* child,
			  HTML_Element* html_elm,
			  HTML_Element* fullscreen_elm,
			  LinkState visited_links_state);

	/** Reset the HTML_Element pointer for the sibling element.
		Called when beginning a context mode. */

	void Reset() { m_html_elm = NULL; }

	/** Set the parent element for a context element. Normally this is done
		by Init from CreateParentElm, but for the sibling element, the child
		that is in the direct context path is the true child element. So,
		this method should only be called for the sibling element of the context. */

	void SetParent(CSS_MatchContextElm* parent) { m_parent = parent; }

	/** @return This context element's parent context element (if constructed). */

	CSS_MatchContextElm* Parent() const { return m_parent; }

	/** @return The HTML_Element that this context element represents. */

	HTML_Element* HtmlElement() const { return m_html_elm; }

	/** Mark this as the root element. It does not necessarily mean that this
		element's Parent() is NULL. It's the topmost element that is visible in
		the dom. For instance, Markup::HTE_DOC_ROOT type elements will never be part of
		the context. */

	void SetIsRoot() { m_packed.is_root = 1; }

	/** Is this the topmost element?

		@return TRUE if this is the topmost element that could be matched in the
					 current context. See SetIsRoot for further details. */

	BOOL IsRoot() const { return m_packed.is_root == 1; }

	/** Get the class attribute for the HTML_Element. If the class attribute
		is not cached, retrieve it from the HTML_Element itself.

		@return The class attribute object for the HTML_Element, or NULL if
				the element doesn't have a class attribute. */

	const ClassAttribute* GetClassAttribute()
	{
		if (m_packed.retrieved_class == 0)
			RetrieveClassAttribute();

		return m_class_attr;
	}

	/** Get the id attribute for the HTML_Element. If the id attribute
		is not cached, retrieve it from the HTML_Element itself.

		@return The id attribute string for the HTML_Element, or NULL if
				the element doesn't have an id attribute. */

	const uni_char* GetIdAttribute()
	{
		if (m_packed.retrieved_id == 0)
			RetrieveIdAttribute();

		return m_id_attr;
	}

	/** Check if the link state for this HTML_Element has been resolved.
		If not, resolve it.

		@param doc The owner document of this element. */

	void CacheLinkState(FramesDocument* doc)
	{
		if (!IsLinkStateResolved())
			ResolveLinkState(doc);
	}

	/** @returns TRUE if the HTML_Element is a link (matches :link or :visited). */

	BOOL IsLink() const { OP_ASSERT(IsLinkStateResolved()); return m_packed.link_state > NO_LINK; }

	/** @returns TRUE if the HTML_Element is a visited link (matches :visited). */

	BOOL IsVisited() const { OP_ASSERT(IsLinkStateResolved()); return m_packed.link_state == VISITED; }

	/** Check if the hover/active/focus bits have been retrieved.
		If not, retrieve them from the HTML_Element. */

	void CacheDynamicState()
	{
		if (m_packed.retrieved_dynamic_state == 0)
			RetrieveDynamicState();
	}

	/** @return TRUE if the element is hovered, otherwise FALSE. */

	BOOL IsHovered() const { OP_ASSERT(m_packed.retrieved_dynamic_state == 1); return m_packed.hovered; }

	/** @return TRUE if the element is active, otherwise FALSE. */

	BOOL IsActivated() const { OP_ASSERT(m_packed.retrieved_dynamic_state == 1); return m_packed.activated; }

	/** @return TRUE if the element is focused, otherwise FALSE. */

	BOOL IsFocused() const { OP_ASSERT(m_packed.retrieved_dynamic_state == 1); return m_packed.focused; }

#ifdef DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS

	/** Is this element hovered and at least one simple selector with :hover
		pseudo class was matched with this element ? */

	BOOL IsHoveredWithMatch() const { return m_packed2.hovered_with_match; }

	/** Flag this element, which is hovered, that a simple selector with
		:hover pseudo class was matched. */

	void SetIsHoveredWithMatch() { m_packed2.hovered_with_match = 1; }

	/** Is this element focused and at least one simple selector with :focus
		pseudo class was matched with this element ? */

	BOOL IsFocusedWithMatch() const { return m_packed2.focused_with_match; }

	/** Flag this element, which is focused, that a simple selector with
		:focus pseudo class was matched. */

	void SetIsFocusedWithMatch() { m_packed2.focused_with_match = 1; }

# ifdef SELECT_TO_FOCUS_INPUT

	/** Is this element pre-focused and at least one simple selector with :-o-prefocus
		pseudo class was matched with this element ? */

	BOOL IsPreFocusedWithMatch() const { return m_packed2.pre_focused_with_match; }

	/** Flag this element, which is pre-focused, that a simple selector with
		:-o-prefocus pseudo class was matched. */

	void SetIsPreFocusedWithMatch() { m_packed2.pre_focused_with_match = 1; }

# endif // SELECT_TO_FOCUS_INPUT

#endif // DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS

	/** @return The cached Markup::Type. */

	Markup::Type Type() const { return static_cast<Markup::Type>(m_packed.type); }

	/** @return The cached namespace index. */

	int GetNsIdx() const { return m_packed.ns_idx; }

	/** @return The element's namespace type based on the cached namespace index. */

	NS_Type GetNsType() const { return g_ns_manager->GetNsTypeAt(GetNsIdx()); }

	/** @return TRUE if the element is or was displayed. The returned value is
				only reliable when the computed styles have been calculated. */

	BOOL IsDisplayed() const { return !m_packed.display_none; }

	/** To be called if the old or new computed style value of display for
		this element is 'none'. */

	void SetDisplayNone() { m_packed.display_none = 1; }

	/** Set that computed styles were generated by
		CSS_MatchContext::CalculateComputedStyle for this element. */

	void SetHasComputedStyles() { m_packed.computed_styles = 1; }

	/** @return TRUE if computed styles were generated by
				CSS_MatchContext::CalculateComputedStyle for this element. */

	BOOL HasComputedStyles() const { return m_packed.computed_styles; }

	/** @return TRUE if this element matches either :fullscreen or :fullscreen-ancestor. */

	BOOL HasFullscreenStyles() const { return m_packed.fullscreen != NO_FULLSCREEN; }

	/** @return TRUE if this element matches :fullscreen. */

	BOOL IsFullscreenElement() const { return m_packed.fullscreen == FULLSCREEN; }

	/** @return TRUE if this element matches :fullscreen-ancestor. */

	BOOL IsFullscreenAncestor() const { return m_packed.fullscreen == FULLSCREEN_ANCESTOR; }

#ifdef MEDIA_HTML_SUPPORT
	/** @return TRUE if the element is part of a <track> ::cue, otherwise FALSE. */

	BOOL IsCueElement() const { return m_packed.ns_idx == NS_IDX_CUE; }
#endif // MEDIA_HTML_SUPPORT

private:

	/** Get the class attribute from the HTML_Element and cache it
		in this context element. */

	void RetrieveClassAttribute();

	/** Get the id attribute from the HTML_Element and cache it.
		in this context element. */

	void RetrieveIdAttribute();

	/**
	 * Tells whether the link_state for this context element has been resolved
	 */

	BOOL IsLinkStateResolved() const { return m_packed.link_state > UNRESOLVED; }

	/** Check if the HTML_Element of this context element is a link and if it's visited,
		and cache the result.

		@param doc The owner document of this element. */

	void ResolveLinkState(FramesDocument* doc);

	/** Get the hover/active/focus bits from the HTML_Element and cache
		them in this context element. */

	void RetrieveDynamicState();

	/** The parent context element. */

	CSS_MatchContextElm* m_parent;

	/** The HTML_Element that this context element represents. */

	HTML_Element* m_html_elm;

	/** The cached class attribute of the HTML_Element.
		The retrieved_class bit tells if NULL means that the element doesn't
		have a class attribute or if it hasn't been retrieved. */

	const ClassAttribute* m_class_attr;

	/** The cached id attribute of the HTML_Element.
		The retrieved_id bit tells if NULL means that the element doesn't
		have an id attribute or if it hasn't been retrieved. */

	const uni_char* m_id_attr;

	union
	{
		struct
		{
			/** Is this element the topmost element of the document tree or fragment? */
			unsigned int is_root:1;
			/** Has the class attribute been retrieved? */
			unsigned int retrieved_class:1;
			/** Has the id attribute been retrieved? */
			unsigned int retrieved_id:1;
			/** The LinkState for this element. */
			unsigned int link_state:3;
			/** The cached Markup::Type of the element. */
			unsigned int type:9;
			/** The cached namespace index of the element. */
			unsigned int ns_idx:8;
			/** Bit saying if the hovered/activated/focused bits have been cached. */
			unsigned int retrieved_dynamic_state:1;
			/** Is this element hovered? */
			unsigned int hovered:1;
			/** Is this element activated? */
			unsigned int activated:1;
			/** Is this element focused? */
			unsigned int focused:1;
			/** Is or was this element or an ancestor element display:none?
				Only set if computed styles are generated for transitions in
				LayoutWorkplace::ReloadCssProperties. */
			unsigned int display_none:1;
			/** TRUE if there are cascade elements (computed styles) for this
				element which have been created with CSS_MatchContext::CalculateComputedStyle.
				That is, if the cascade element was necessary because the
				computed styles may have changed, and there's a possibility for
				transitions to happen. */
			unsigned int computed_styles:1;
			/** Is this element :fullscreen or :fullscreen-ancestor? */
			unsigned int fullscreen:2;
			/* 31 bits in use - 1 available */
		} m_packed;
		unsigned int m_packed_init;
	};

#ifdef DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS
	union
	{
		struct
		{
			/** Is this element hovered and at least one simple selector with :hover
				pseudo class was matched with this element ? */
			unsigned int hovered_with_match:1;
			/** Is this element focused and at least one simple selector with :focus
				pseudo class was matched with this element ? */
			unsigned int focused_with_match:1;
# ifdef SELECT_TO_FOCUS_INPUT
			/** Is this element pre-focused and at least one simple selector with
				:-o-prefocus pseudo class was matched with this element ? */
			unsigned int pre_focused_with_match:1;
# endif // SELECT_TO_FOCUS_INPUT
		} m_packed2;
		unsigned int m_packed2_init;
	};
#endif // DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS
};

/** A class representing a path down the html tree as a linked list of CSS_MatchContextElms. */

class CSS_MatchContext
{
public:

	/** The match context has two modes of operation.
		See the documentation for the Begin() method
		for a description of the different modes. */

	enum Mode
	{
		INACTIVE,
		TOP_DOWN,
		BOTTOM_UP
	};

	class Operation
	{
	public:
		/** Set up an operation mode for a match context. This class
			can be used to conditionally set up a BOTTOM_UP operation
			if we're not already inside a TOP_DOWN operation.

			@param context The context to set up an operation for.
			@param doc The document in which the matching is done.
			@param mode The operation mode to set up.
			@param media_type The media type to match for.
			@param match_all TRUE if we should try to match all selectors, FALSE if we should break
							 after first match.
			@param mark_pseudo_bits TRUE if we should mark pseudo bits in HTML_Element. */
		Operation(CSS_MatchContext* context, FramesDocument* doc, Mode mode, CSS_MediaType media_type, BOOL match_all, BOOL mark_pseudo_bits)
			: m_context(context), m_mode(mode)
		{
			OP_ASSERT(context && mode != INACTIVE);

			if (context->GetMode() == INACTIVE)
			{
				context->Begin(doc, mode, media_type, match_all, mark_pseudo_bits);
			}
			else
				m_mode = INACTIVE;
		}

		/** Destructor. Ends the operation for the context when this object goes out of scope. */
		~Operation() { End(); }

		/** Explicitly end the operation before the Operation object goes out of scope. */
		void End()
		{
			if (m_mode != INACTIVE)
			{
				m_context->End(m_mode);
				m_mode = INACTIVE;
			}
		}

		/** Return the next HTML_Element after current, committing context leaves
			when moving up or right in the tree. This method should only be called for
			TOP_DOWN operations.

			@param current The element to get the next element for.
			@param skip_children If TRUE, skip the sub-tree below the current element.
			@return The next element after current or NULL if current is the last element of the tree. */
		HTML_Element* Next(HTML_Element* current, BOOL skip_children=FALSE) const;

	private:
		/** The context for which this is an operation. */
		CSS_MatchContext* m_context;

		/** The mode of operation. */
		Mode m_mode;
	};

	/** Constructor. */

	CSS_MatchContext();

	/** Destructor. Delete the allocated pool. */

	~CSS_MatchContext();

	/** Construct the context object - pre-allocating a fixed size context element pool.
		Leaves with OpStatus::ERR_NO_MEMORY on OOM.

		@param pool_size The number of pre-allocated CSS_MatchContextElm objects. */

	void ConstructL(size_t pool_size);

	/** Begin using the context in a given mode. The mode must not be changed for an uncommitted context.

		@param doc The document in which the matching is done.
		@param mode This sets the mode of operation for the context. The context can work in two
					different ways. In the BOTTOM_UP mode, we initiate the context at a given
					element for which we're matching selectors. The context path will be built upwards
					as we need to access ascendant elements when matching child/descendant selectors.
					In the BOTTOM_UP mode, the context can not be kept between matching of different
					elements. In the TOP_DOWN mode, we create the context path from the root node and
					extend or trim it as we move up and down the tree matching elements. Currently,
					the TOP_DOWN mode is used in ReloadCssProperties, and the BOTTOM_UP mode is
					used in SetCssProperties.
		@param media_type The media type to match for
		@param match_all TRUE if we should try to match all selectors, FALSE if we should break
						 after first match.
		@param mark_pseudo_bits TRUE if we should mark pseudo bits in HTML_Element. */

	void Begin(FramesDocument* doc, Mode mode, CSS_MediaType media_type, BOOL match_all, BOOL mark_pseudo_bits);

	/** Mark the end of a context usage. Begin/End calls must come in matching pairs,
		and may not be nested. In the BOTTOM_UP mode, this method commits the context.

		@param mode The same mode as Begin was called with. Used for consistency checking
					and committing. */

	void End(Mode mode)
	{
		OP_ASSERT(mode != INACTIVE && GetMode() == mode);

		/* Commit the whole context path from the leaf element and up. */
		if (mode == TOP_DOWN)
			CommitTopDown();
		else
			CommitBottomUp();

		m_current_root = NULL;
		m_packed.mode = INACTIVE;

#ifdef CSS_TRANSITIONS
		m_cascade[COMPUTED_OLD].Clear();
		m_cascade[COMPUTED_NEW].Clear();
		m_cascade_elm[COMPUTED_OLD] = NULL;
		m_cascade_elm[COMPUTED_NEW] = NULL;
#endif // CSS_TRANSITIONS
	}

	/** @return The current mode of operation. */

	Mode GetMode() const { return Mode(m_packed.mode); }

	/** Create and initiate the context element for an element to be matched.

		For the BOTTOM_UP mode, this method will be called once for the initial
		context element for the element being matched. The rest of the context
		will be extended towards the root with CreateParentElm() called from
		GetParentElm().

		For the TOP_DOWN mode, this method is called for extending the context
		path downwards.

		@return The new leaf context element. NULL on OOM. */

	CSS_MatchContextElm* GetLeafElm(HTML_Element* html_elm)
	{
		OP_ASSERT(m_next == 0 && m_current_leaf == NULL || m_packed.mode == TOP_DOWN && m_current_leaf);

		if (m_current_leaf && m_current_leaf->HtmlElement() == html_elm)
			return m_current_leaf;
		else
		{
			CSS_MatchContextElm* leaf = New();
			if (leaf)
			{
				leaf->Init(m_current_leaf, NULL, html_elm, m_fullscreen_elm, LinkState());

				if (!m_current_leaf && m_packed.mode == TOP_DOWN)
					leaf->SetIsRoot();

				if (!m_current_leaf)
					m_current_root = leaf;

				m_current_leaf = leaf;
			}
			return leaf;
		}
	}

	/** Method for retrieving the context element for the real HTML
		element used for pseudo element matching. In this case:

		<style>#x::first-letter {}</style>
		<div id="x"><div><span>X</span></div></div>

		The first-letter pseudo element will be inside the span, but
		the div with id x should be used for matching the selector, so
		we must search up the match context path past the span and the inner
		div to find the real context element to use. In the case where
		we run in BOTTOM_UP mode, the context path should be empty and we need
		to create the context element for the div#x element instead.

		@param html_elm The html element that we should find/create the
						context element for.
		@return The found or created context element. NULL on OOM. */

	CSS_MatchContextElm* FindOrGetLeafElm(HTML_Element* html_elm)
	{
		CSS_MatchContextElm* context_elm = m_current_leaf;
		while (context_elm && context_elm->HtmlElement() != html_elm)
			context_elm = context_elm->Parent();

		/** We're looking for a parent context elm. It should either be
			found, or we're working in BOTTOM_UP mode. */
		OP_ASSERT(context_elm || m_packed.mode == BOTTOM_UP && m_current_leaf == NULL);

		return context_elm ? context_elm : GetLeafElm(html_elm);
	}

	/** Get the context element that is the current leaf element.
		That is, the one that is being matched.

		@return The current leaf. */

	CSS_MatchContextElm* GetCurrentLeafElm() const { return m_current_leaf; }

	/** Get the parent context element for a given context element.
		If the parent already exists, return that one. Otherwise,
		create a parent element if the child is not already a root
		element.

		@return The parent context element. Returns NULL if the child
				parameter is the root element, and on OOM. */

	CSS_MatchContextElm* GetParentElm(CSS_MatchContextElm* child)
	{
		CSS_MatchContextElm* parent = child->Parent();

		/* In the TOP_DOWN mode, the context path should always be complete
		   down to the current leaf. */
		OP_ASSERT(parent || child->IsRoot() || m_packed.mode == BOTTOM_UP);

		if (parent)
			return parent;
		else if (!child->IsRoot())
			return CreateParentElm(child);
		else
			return NULL;
	}

	/** Initiate the sibling context element for a given HTML_Element.

		@param prev_sibling The previous sibling element being matched.
							This might be the single sibling element, or
							one of the context elements which represents
							a sibling to the right of the element being
							initialized.
		@param html_elm The HTML_Element to initialize the context element for.
		@return The sibling context element. */

	CSS_MatchContextElm* GetSiblingElm(CSS_MatchContextElm* prev_sibling, HTML_Element* html_elm)
	{
		if (m_sibling->HtmlElement() != html_elm)
			m_sibling->Init(prev_sibling->Parent(), NULL, html_elm, m_fullscreen_elm, LinkState());

		return m_sibling;
	}

	/** Used when moving up or right in the document tree while
		traversing the tree for reloading css properties.

		@param leaf If this is the element that the context's current leaf is for,
					or if this parameter is NULL, commit it. */

	void CommitLeaf(HTML_Element* leaf)
	{
		OP_ASSERT(m_packed.mode == TOP_DOWN);

		if (!leaf || m_current_leaf && m_current_leaf->HtmlElement() == leaf)
		{
			CSS_MatchContextElm* commit_leaf = m_current_leaf;
#ifdef CSS_TRANSITIONS
			CleanComputedStyles();
#endif // CSS_TRANSITIONS
			m_current_leaf = commit_leaf->Parent();
			if (m_next-- > m_size)
				OP_DELETE(commit_leaf);
		}
	}

	/** Commit the whole context path for a TOP_DOWN operation. */

	void CommitTopDown()
	{
		OP_ASSERT(m_packed.mode == TOP_DOWN);

		CSS_MatchContextElm* commit_leaf = m_current_leaf;
		while (commit_leaf && m_next-- > m_size)
		{
#ifdef CSS_TRANSITIONS
			CleanComputedStyles();
#endif // CSS_TRANSITIONS
			CSS_MatchContextElm* next_leaf = commit_leaf->Parent();
			OP_DELETE(commit_leaf);
			commit_leaf = next_leaf;
		}

		m_current_leaf = NULL;
		m_next = 0;
	}

	/** Commit the whole context path for a BOTTOM_UP operation. */

	void CommitBottomUp()
	{
		OP_ASSERT(m_packed.mode == BOTTOM_UP);

		size_t pool_idx = 0;
		CSS_MatchContextElm* cur_elm = m_current_leaf;

		while (cur_elm)
		{
#ifdef CSS_TRANSITIONS
			OP_ASSERT(!cur_elm->HasComputedStyles());
#endif // CSS_TRANSITIONS
			CSS_MatchContextElm* commit_elm = cur_elm;
			cur_elm = cur_elm->Parent();
			if (pool_idx++ >= m_size)
				OP_DELETE(commit_elm);
		}

		OP_ASSERT(pool_idx == m_next);

		m_current_leaf = NULL;
		m_next = 0;
	}

	/** @return TRUE if the context is committed. That is,
				there are currently no elements in the context. */

	BOOL IsCommitted() const { return (m_next == 0 && m_current_leaf == NULL); }

	/**
	 * Reads the value of the opera:config#VisitedLink|VisitedLinksState
	 * and caches it for later use
	 */
	void InitVisitedLinkPref();

	/** A TOP_DOWN mode operation can be started from another root than
		a document root by calling this method right after Begin(TOP_DOWN).
		This is for instance used for querySelector(All) on the Element
		interface.

		What it does, is to initialize context elements from the provided
		start root and up to the document/fragment root. From this initial
		context, GetLeafElm/CommitLeaf extends/commits the context path as
		for TOP_DOWN operations done completely from the root. End() will
		automatically commit the remaining initial context path.

		@param root The document node that is the parent of the first
					element we will call GetLeafElm for.
		@return ERR_NO_MEMORY on OOM. Otherwise OK. */

	OP_STATUS InitTopDownRoot(HTML_Element* root);

	/** @return the document for this match context. */

	FramesDocument* Document() const { return m_doc; }

	/** @return TRUE if author style should apply. */

	BOOL ApplyAuthorStyle() const { return m_packed.apply_author; }

	/** @return TRUE if the current ERA mode has APPLY_DOC_STYLES_SOME. */

	BOOL ApplyPartially() const { return m_packed.apply_partially; }

	/** @return TRUE if the document is an XML document. */

	BOOL IsXml() const { return m_packed.xml; }

	/** @return TRUE if we should try to match all selectors, or if we can
				break after matching at least one selector. Typically FALSE
				for querySelector. */

	BOOL MatchAll() const { return m_packed.match_all; }

	/** @return TRUE if the document is a strict mode document. */

	BOOL StrictMode() const { return m_packed.strict_mode; }

	/** @return the pseudo element type for the current leaf element. */

	PseudoElementType PseudoElm() const { return PseudoElementType(m_packed.pseudo_elm); }

	/** Set the pseudo element of the current leaf element. */

	void SetPseudoElm(PseudoElementType pseudo_elm) { m_packed.pseudo_elm = pseudo_elm; }

	/** @return the current layout mode for the document. */

	LayoutMode DocLayoutMode() const { return LayoutMode(m_packed.layout_mode); }

	/** @return TRUE if pseudo bits in HTML_Element should be set. Typically
				FALSE for querySelector() and when prefetching resources. */

	BOOL MarkPseudoBits() const { return m_packed.mark_pseudo_bits; }

	/** @return TRUE if we are matching for finding URLs to prefetch. */

	BOOL PrefetchMode() const { return m_packed.prefetch_mode; }

	/** Mark context as matching for finding URLs to prefetch. */

	void SetPrefetchMode() { m_packed.prefetch_mode = 1; }

	/** @return the media type to match for. */

	CSS_MediaType MediaType() const { return m_media_type; }

#ifdef STYLE_GETMATCHINGRULES_API
	/** Matching rules will be reported to this listener. NULL to disable. */

	void SetListener(CSS_MatchRuleListener* listener) { m_listener = listener; }

	/** @return The listener, or NULL if none is attached. */

	CSS_MatchRuleListener* Listener() const { return m_listener; }
#endif // STYLE_GETMATCHINGRULES_API

	/** @return The cache used for matching :nth-* selectors. */

	CSS_NthCache& GetNthCache() { return m_nth_cache; }

	/** Get the nth count to use for matching nth selectors for a given context element.

		@return The position of this element among its siblings. Starting from 1
				when it's the leftmost sibling (or rightmost for the *last* types).
				The type parameter decides how to count. */

	unsigned int GetNthCount(const CSS_MatchContextElm* context_elm, CSS_NthCache::Type type, BOOL case_sensitive=FALSE)
	{
		if (m_nth_cache.m_elements[type] != context_elm->HtmlElement())
			return CacheNthCount(context_elm, type, case_sensitive);
		else
			return m_nth_cache.m_counts[type];
	}

#ifdef CSS_TRANSITIONS

	/** Enumerated type used to identify new or old computed styles for
		CalculateComputed/GetComputedStyle. */

	enum ComputedStyle
	{
		COMPUTED_OLD,
		COMPUTED_NEW,
		COMPUTED_COUNT
	};

	/** Create the LayoutProperties object for the computed styles of the
		current leaf element and store it in the cascade chain.

		@param which Which cascade chain to store LayoutProperties in.
					 New or old computed styles.
		@param ignore_transitions TRUE if transitions in progress should be
								  applied to the computed styles.
		@return ERR_NO_MEMORY on OOM, otherwise OK. */

	OP_STATUS CalculateComputedStyle(ComputedStyle which, BOOL ignore_transitions);

	/** The old or new computed styles for the current leaf element.
		Used when reloading CSS properties.

		@param which Which of the computed styles to return. Old or new.
		@return The old or new computed styles for the current leaf element. */

	LayoutProperties* GetComputedStyle(ComputedStyle which)
	{
		OP_ASSERT(which == COMPUTED_OLD || which == COMPUTED_NEW);
		return m_cascade_elm[which];
	}

	/** @return TRUE if we are in a subtree where we're generating computed
				styles for checking transition changes. */

	BOOL IsGeneratingComputedStyles() { return m_current_leaf && m_current_leaf->HasComputedStyles(); }

#endif // CSS_TRANSITIONS

private:

#ifdef CSS_TRANSITIONS

	/** @return ERR_NO_MEMORY on OOM, otherwise OK. */

	OP_STATUS ConstructComputedStyleRoots();

	/** Complete m_cascade down to, and including, parent_elm.
		Recurses up the tree until we find the current end of the chain
		matching the context element we're looking at. If the parent_elm is
		NULL it means we've reachced to top.

		@param parent_elm The context element that should be the last element
			   of the computed style chain when this method returns. May be
			   NULL, in which case we will create a cascade element for the
			   initial values if missing.
		@return ERR_NO_MEMORY on OOM, otherwise OK. */

	OP_STATUS CompleteComputedStyleChain(CSS_MatchContextElm* parent_elm);

	/** Clean the current cascade element and set the predecessor cascade
		element as the current cascade element. Used when committing
		context elements to keep the current cascade chain in sync with
		the current context element chain. */

	void CleanComputedStyles();

#endif // CSS_TRANSITIONS

	/** Allocate a context element. If there is a free element
		in the pool, return that. Otherwise allocate a new element
		on the heap.

		@return The allocated context element. NULL on OOM. */

	CSS_MatchContextElm* New()
	{
		if (m_next < m_size)
			return &m_pool[m_next++];
		else
		{
			CSS_MatchContextElm* new_elm = OP_NEW(CSS_MatchContextElm, ());
			if (new_elm)
				m_next++;
			return new_elm;
		}
	}

	/** Recursively create the context path from the document root down to the
		operation root. */

	CSS_MatchContextElm* InitTopDownRootInternal(HTML_Element* html_elm);

	/** Create the parent context element for a given element. This method is
		used in the BOTTOM_UP mode only. */

	CSS_MatchContextElm* CreateParentElm(CSS_MatchContextElm* child);

	/** Calculate an nth-count for the given element using the previously cached value.

		@param context_elm The context element to calculate the count for.
		@param type The nth-count type to calculate.
		@param case_sensitive Case sensitivity of *-of-type tag name comparisons.
		@return The cached count. */

	unsigned int CacheNthCount(const CSS_MatchContextElm* context_elm, CSS_NthCache::Type type, BOOL case_sensitive);

	/** A pre-allocated pool of CSS_MatchContextElm objects. When all elements in the pool
		are in use, elements will be allocated one by one by the New method. */

	CSS_MatchContextElm* m_pool;

	/** A single context element used for candidates for sibling and adjacent sibling selectors. */

	CSS_MatchContextElm* m_sibling;

	/** The current leaf element of the context. */

	CSS_MatchContextElm* m_current_leaf;

	/** The current top-most element of the context path. */

	CSS_MatchContextElm* m_current_root;

	/** Cached count for elements matched with :nth-* selectors. */

	CSS_NthCache m_nth_cache;

	/** The index of the next CSS_MatchContextElm to available for allocation from the pool.
		If m_next is larger than or equal to m_size, the next CSS_MatchContextElm requested
		will be allocated from the heap. m_next will continuously grow with heap allocated
		elements as well. */

	size_t m_next;

	/** The number of pooled CSS_MatchContextElm objects in m_pool. */

	size_t m_size;

	/** The document for this match context. */

	FramesDocument* m_doc;

	/** The current fullscreen element for the document. */

	HTML_Element* m_fullscreen_elm;

	CSS_MatchContextElm::LinkState LinkState() const
	{
		return CSS_MatchContextElm::LinkState(m_packed.visited_links_state);
	}

	union
	{
		/** Packed members. */
		struct
		{
			/** The current mode (CSS_MatchContext::Mode) of operation.
				See the Begin() method for documentation. */

			unsigned int mode:2;

			/** Cached value of the preference opera:config#VisitedLink|VisitedLinksState
				Values: 0 - disable :visited, 1 - match same domain only, 2 - enable :visited normally.
				(the real type is CSS_MatchContextElm::LinkState). */

			unsigned int visited_links_state:3;

			/** The current layout mode (LayoutMode). */

			unsigned int layout_mode:3;

			/** Set to TRUE if the document is an XML document.
				Will cause things like element names to be matched
				case sensitively. */

			unsigned int xml:1;

			/** Set to TRUE if the document has a strict mode DOCTYPE. */

			unsigned int strict_mode:1;

			/** The pseudo element type (PseudoElementType), if any,
				for the current leaf element. */

			unsigned int pseudo_elm:3;

			/** TRUE if all selectors should be matched. */

			unsigned int match_all:1;

			/** TRUE if author style should apply. */

			unsigned int apply_author:1;

			/** TRUE if the current ERA mode has APPLY_DOC_STYLES_SOME. */

			unsigned int apply_partially:1;

			/** TRUE if pseudo bits should be set in HTML_Element.
				Typically set to FALSE for querySelector() and when
				prefetching resources. */

			unsigned int mark_pseudo_bits:1;

			/** TRUE if we are matching for finding URLs to prefetch. */

			unsigned int prefetch_mode:1;

		} m_packed;

		/** Initializer for bit field. */

		unsigned int m_packed_init;
	};

	/** Media type to match for. */

	CSS_MediaType m_media_type;

#ifdef STYLE_GETMATCHINGRULES_API
	/** If non-NULL, matches will be reported to this listener. */
	CSS_MatchRuleListener* m_listener;
#endif // STYLE_GETMATCHINGRULES_API

#ifdef CSS_TRANSITIONS
	/** Chains of LayoutProperties objects for old and new computed styles for
		elements while reloading CSS properties in LayoutWorkplace::Reload.
		The cascade is not necessarily rooted at Markup::HTE_DOC_ROOT if it's decided
		that the full chain is not needed. */
	Head m_cascade[COMPUTED_COUNT];

	/** The current last elements of the chains in m_cascade. Needed because
		the chain may have trailing unused LayoutProperties objects. */
	LayoutProperties* m_cascade_elm[COMPUTED_COUNT];
#endif // CSS_TRANSITIONS
};

#endif // !CSS_MATCHCONTEXT_H
