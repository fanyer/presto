/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_COLLECTION_H
#define CSS_COLLECTION_H

#include "modules/layout/layout_coord.h"
#include "modules/style/cssmanager.h"
#include "modules/style/css_matchcontext.h"
#include "modules/util/simset.h"
#include "modules/style/src/css_values.h"
#include "modules/style/css_media.h"
#include "modules/style/css_viewport.h"
#include "modules/logdoc/logdocenum.h"
#include "modules/logdoc/class_attribute.h"
#include "modules/style/css_animations.h"
#include "modules/util/OpHashTable.h"
#include "modules/hardcore/timer/optimer.h"

class CSS_WebFont;

#ifdef CSS_VIEWPORT_SUPPORT
class CSS_ViewportMeta;
#endif // CSS_VIEWPORT_SUPPORT

class CSS;
class CSS_Properties;
class CSS_DOMRule;
class CSS_property_list;
class CSS_MatchContext;
class CSS_MatchContextElm;

#ifdef STYLE_GETMATCHINGRULES_API

/** Represents an element used in the CSS_MatchRuleListener API.

    The CSS_MatchElement may refer to an actual HTML element in the tree, or
    it may refer to a pseudo-element related to that element.

    Because some pseudo-elements are only present in the tree during layout,
    we need a way to refer to a pseudo-element which does not require the
    actual pseudo-elements to be present. This class solves this problem
    by referring to an element (either regular or pseudo) using a pair of
    one real element, and optionally one pseudo-element type. This pair
    can than be resolved into an actual HTML_Element object by the style
    module, at a time this element is known to be present. */
class CSS_MatchElement
{
public:

	/** Create a new (empty) CSS_MatchElement. This will not refer to any
	    element, and is therefore invalid (IsValid will return FALSE).

	    The default constructor is required for arrays, however. */
	CSS_MatchElement();

	/** Create a new CSS_MatchElement which refers to the specified (real,
	    non-pseudo) element.

	    @param html_element The element this object should refer to. */
	CSS_MatchElement(HTML_Element* html_element);

	/** Create a new CSS_MatchElement which refers to the specified pseudo-
	    element of the specified (real) element.

	    If you attempt to refer to a pseudo-element which the target (real)
	    element does not actually have, then the CSS_MatchElement is invalid
	    (IsValid will return FALSE), and may not be used with the
	    CSS_MatchRuleListener API.

	    Note that only the following pseudo-elements are currently supported:

	     - ELM_PSEUDO_BEFORE
	     - ELM_PSEUDO_AFTER
	     - ELM_PSEUDO_FIRST_LETTER
	     - ELM_PSEUDO_FIRST_LINE

	    Attempting to use any other pseudo-element will result in an invalid
	    CSS_MatchElement.

	    @param html_element The element this object should refer to.
	    @param pseudo A supported pseudo-element, or ELM_NOT_PSEUDO. */
	CSS_MatchElement(HTML_Element* html_element, PseudoElementType pseudo);

	/** @return The pseudo-element type this CSS_MatchElement refers to. This may
	    be ELM_NOT_PSEUDO, if the CSS_MatchElement does not refer to a pseudo-
	    element. */
	PseudoElementType GetPseudoElementType() const { return m_pseudo; }

	/** @return TRUE if this CSS_MatchElement refers to a pseudo-element, and
	            FALSE otherwise. */
	BOOL IsPseudo() const { return m_pseudo != ELM_NOT_PSEUDO; }

	/** @return The (real, non-pseudo) element this CSS_MatchElement refers to.
	            If the CSS_MatchElement is valid, this never returns NULL. */
	HTML_Element* GetHtmlElement() const { return m_html_element; }

	/** Get the parent CSS_MatchElement for this CSS_MatchElement. This may yield
	    an invalid CSS_MatchElement if this CSS_MatchElement has no parent, or if
	    selector matching is not applicable to the parent element.

	    If this CSS_MatchElement refers to a (valid) pseudo-element, the parent
	    CSS_MatchElement returned from this function will be the real element
	    associated with the pseudo-element.

	    @return The parent CSS_MatchElement, which may or may not be invalid. */
	CSS_MatchElement GetParent() const;

	/** Check whether this CSS_MatchElement is 'valid'.

	    A CSS_MatchElement is valid if it refers to an element that exists, and
	    if selector matching is applicable for the target element. (For instance,
	    Markup::HTE_DOC_ROOT is not applicable).

	    If the CSS_MatchElement refers to a pseudo-element for a certain real
	    element, then the real element must have the specified pseudo-element
	    for the CSS_MatchElement to be valid.

	    @return TRUE if valid, FALSE if invalid. */
	BOOL IsValid() const;

	/** Check if this CSS_MatchElement is equal to another CSS_MatchElement.

	    This function does not check if the involved CSS_MatchElements are
	    invalid, and two invalid CSS_MatchElements *can* be equal.

	    @return TRUE if equal, FALSE otherwise. */
	BOOL IsEqual(const CSS_MatchElement& elm) const;

private:

	/** The real, non-pseudo element this CSS_MatchElement is referring to. */
	HTML_Element* m_html_element;

	/** If ELM_NOT_PSEUDO, indicates that we are referring to a pseudo-element. */
	PseudoElementType m_pseudo;
};

/** Listener interface for rules that matched in the GetMatchingStyleRules call. */
class CSS_MatchRuleListener
{
public:
	virtual ~CSS_MatchRuleListener() {}

	/** Emit a style rule that matched for an element during GetMatchingStyleRules.
		See GetMatchingStyleRules documentation for matching rule order.

		@param element The element for which the rule matched. If include_inherited
					   is TRUE for GetMatchingStyleRules, this element may also be an
					   ascendent element of GetMatchingStyleRules' element parameter.
		@param rule The rule that matched.
		@param specificity The specificity of the rule. This is the internal value used
						   for specificity, so don't trust it to match some formula in
						   the CSS spec.

		@return You may return OpStatus::ERR_NO_MEMORY on OOM. Otherwise, you should return
				OpStatus::OK. */
	virtual OP_STATUS RuleMatched(const CSS_MatchElement& element, CSS_DOMRule* rule, unsigned short specificity) = 0;

# ifdef STYLE_MATCH_LOCAL_API
	/** Emit a style rule from a local stylesheet that matched for an element during
		GetMatchingStyleRules. See GetMatchingStyleRules documentation for matching
		rule order.

		@param element The element for which the rule matched. If include_inherited
					   is TRUE for GetMatchingStyleRules, this element may also be an
					   ascendent element of GetMatchingStyleRules' element parameter.
		@param rule The rule that matched. You may not create a dom object from this
					rule object, because it is not associated with a specific document.
					You can, however, use the methods on CSS_DOMRule to retrieve the
					text representation.
		@param specificity The specificity of the rule. This is the internal value used
						   for specificity, so don't trust it to match some formula in
						   the CSS spec.

		@return You may return OpStatus::ERR_NO_MEMORY on OOM. Otherwise, you should return
				OpStatus::OK. */
	virtual OP_STATUS LocalRuleMatched(const CSS_MatchElement& element, CSS_DOMRule* rule, unsigned short specificity) = 0;

# endif // STYLE_MATCH_LOCAL_API

# ifdef STYLE_MATCH_INLINE_DEFAULT_API
	/** Emit the inline style (specified via style attribute or set on style object)
		for the element being matched.

		@param element The element being matched. If include_inherited is TRUE for
					   GetMatchingStyleRules, this element may also be an ascendent
					   element of GetMatchingStyleRules' element parameter.
		@param style A CSS_property_list representation of the declarations set
					 on the style object. Don't store this parameter. The caller might
					 delete it after this callback returns.
		@return You may return OpStatus::ERR_NO_MEMORY on OOM. Otherwise, you should return
				OpStatus::OK. */
	virtual OP_STATUS InlineStyle(const CSS_MatchElement& element, CSS_property_list* style) = 0;

	/** Emit the default style for the element being matched. E.g. an H1 element will
		have font-size set from the default stylesheet for html.

		@param element The element being matched. If include_inherited is TRUE for
					   GetMatchingStyleRules, this element may also be an ascendent
					   element of GetMatchingStyleRules' element parameter.
		@param style A CSS_property_list representation of the declarations set
					 by the default stylesheet. This might not be entirely correct
					 since not all default styling can be expressed in CSS.
					 Don't store this parameter. The caller might delete it after
					 this callback returns. The declarations in this list MUST be
					 resolved before they are used, see ResolveDeclaration().
		@return You may return OpStatus::ERR_NO_MEMORY on OOM. Otherwise, you should return
				OpStatus::OK. */
	virtual OP_STATUS DefaultStyle(const CSS_MatchElement& element, CSS_property_list* style) = 0;

	/** Match a rule with the specified pseudo-class or pseudo-element, even if that
		rule isn't currently applied.

		The pseudo classes are ORed together, see CSSPseudoClassType.

		@param pseudo The pseudo classes which the listener may choose to match.
		@return TRUE to match, FALSE to not match.
	 */
	virtual BOOL MatchPseudo(int pseudo) = 0;

	/** Return the actual declaration.  For use for declarations
		returned for the DefaultStyle callback where some declarations
		can't be put directly into the property list returned.

		@param decl The declaration (which may be a proxy declaration)
		@return The actual declaration.	*/
	static CSS_decl* ResolveDeclaration(CSS_decl* decl);

# endif // STYLE_MATCH_INLINE_DEFAULT_API
};

#endif // STYLE_GETMATCHINGRULES_API

class CSSCollection;

/** A base class for CSSCollection elements. */
class CSSCollectionElement : public ListElement<CSSCollectionElement>
{
public:
	/** Enumerated element type. */
	enum Type
	{
		STYLESHEET,
#ifdef CSS_VIEWPORT_SUPPORT
		VIEWPORT_META,
#endif // CSS_VIEWPORT_SUPPORT
		SVGFONT
	};

	/** @return TRUE if element is a stylesheet (class CSS) object. */
	BOOL IsStyleSheet() { return GetType() == STYLESHEET; }

	/** @return TRUE if element is a CSS_WebFont object. */
	BOOL IsSvgFont() { return GetType() == SVGFONT; }

#ifdef CSS_VIEWPORT_SUPPORT
	/** @return TRUE if element is a CSS_ViewportMeta object. */
	BOOL IsViewportMeta() { return GetType() == VIEWPORT_META; }
#endif // CSS_VIEWPORT_SUPPORT

	/** @return the collection element type. */
	virtual Type GetType() = 0;

	/** Returns the HTML element for this collection element.
		For stylesheets, it's the link/style element. For svg fonts,
		it's the svg:font element. */
	virtual HTML_Element* GetHtmlElement() = 0;

	/** Called after the collection element has been added to a collection.

		@param collection The CSSCollection.
		@param doc The document the collection belongs to. */
	virtual void Added(CSSCollection* collection, FramesDocument* doc) = 0;

	/** Called after the collection element has been removed from a collection.

		@param collection The CSSCollection.
		@param doc The document the collection belongs to. */
	virtual void Removed(CSSCollection* collection, FramesDocument* doc) = 0;
};

/** A collection of CSS (stylesheet) objects for a document. */

class CSSCollection : public OpTimerListener, PrefsCollectionFilesListener
{
public:

	/** Iterator class for iterating through the stylesheets of the collection. */
	class Iterator
	{
	public:

		/** Type of elements to be included in an iteration. */
		typedef enum {
			ALL, /** Include all elements in the collection. */
			STYLESHEETS, /** Include all stylesheet objects in the collection, including imported stylesheets in the cascading order. */
			STYLESHEETS_NOIMPORTS, /** Include stylesheet objects from the top level (no imports). */
#ifdef CSS_VIEWPORT_SUPPORT
			VIEWPORT, /** CSS_ViewportMeta elements. */
#endif // CSS_VIEWPORT_SUPPORT
			WEBFONTS /** SVG font elements. */
		} Type;

		/** Constructor. Construct Iterator for a collection based on type. */
		Iterator(CSSCollection* coll, Type type);

		/** Find out if the iterator has more elements left.

			@return TRUE if Next will return an element different from NULL, otherwise FALSE. */
		BOOL HasNext() { return m_next != NULL; }

		/** Returns the next element in the iteration. The order of the returned elements
			will be from first to last in the cascading order according to the CSS spec. */
		CSSCollectionElement* Next()
		{
			CSSCollectionElement* ret = m_next;
			if (m_next)
				m_next = SkipElements(static_cast<CSSCollectionElement*>(m_next->Suc()));
			return ret;
		}

	private:
		/** Skip elements with wrong type for iterator. */
		CSSCollectionElement* SkipElements(CSSCollectionElement* cand);

		/** Next element to be returned by Next(). */
		CSSCollectionElement* m_next;

		/** Type of elements to be included in this iteration. */
		Type m_type;
	};

	/** Constructor */
	CSSCollection() : m_doc(NULL),
#ifdef CSS_ANIMATIONS
		m_animation_manager(this),
#endif // CSS_ANIMATIONS
#ifdef PREFS_HOSTOVERRIDE
		m_overrides_loaded(FALSE),
#endif // PREFS_HOSTOVERRIDE
#ifdef CSS_TRANSITIONS
		m_has_transitions(FALSE),
		m_has_animations(FALSE),
#endif // CSS_TRANSITIONS
		m_match_first_child(FALSE),
		m_font_prefetch_limit(-1)
	{
		m_match_context = g_css_match_context;
		m_match_context->InitVisitedLinkPref();
	}

	/** Destructor which clears the collection. */
	virtual ~CSSCollection();

	/** Set current FramesDocument for matching. This method is called from HLDocProfile.
		The document will typically be changed for printing, but this method is also used
		to set the FramesDocument initially. */
	void SetFramesDocument(FramesDocument* doc);

	/** Get current FramesDocument. */
	FramesDocument* GetFramesDocument() const { return m_doc; }

	/** Add a new collection element to the collection. Ownership for the collection
		element is retained by the caller. The caller is responsible for calling
		RemoveCollectionElement before it deletes the collection element, and also
		before the CSSCollection is deleted.

		@param new_elm CollectionElement to be added.
		@param commit If TRUE, the element is added into the correct place in the
					  cascading order, ready for use. If FALSE, the element is only
					  added to the collection without being used. Stylesheets added with
					  commit=FALSE can be put into the cascading order and used by
					  calling CommitAdded(). */
	void AddCollectionElement(CSSCollectionElement* new_elm, BOOL commit=TRUE);

	/** Remove the CSSCollectionElement whose GetHtmlElement() matches the
		input parameter.

		@param he The HTML_Element to remove the collection element for.
		@return The removed collection element. NULL if not found. */
	CSSCollectionElement* RemoveCollectionElement(HTML_Element* he);

	/** Check if a CSSCollectionElement is part of this collection,
		either committed, or in the list of pending elements.

		@param collection_elm The element to check for.
		@return TRUE if part of elements or pending elements. */
	BOOL IsInCollection(CSSCollectionElement* collection_elm)
	{
		return m_element_list.HasLink(collection_elm) ||
			m_pending_elements.HasLink(collection_elm);
	}

	/** Call AddCollectionElement for all collection elements that has been added with
		AddCollectionElement with commit=FALSE. */
	void CommitAdded();

	/** Change bits used for the StyleChanged() method. */
	enum StyleChange
	{
		/** No changes. */
		CHANGED_NONE = 0x0,
		/** Properties need to be reloaded because rulesets have been added/removed/modified. */
		CHANGED_PROPS = 0x1,
		/** Page properties need to be reloaded because page rules have been added/removed/modified. */
		CHANGED_PAGEPROPS = 0x2,
		/** Properties need to be reloaded because webfonts have been added/removed/loaded/unloaded. */
		CHANGED_WEBFONT = 0x4,
		/** Viewport properties need to be reloaded because @viewport or meta viewport rules have been added. */
		CHANGED_VIEWPORT = 0x8,
		/** Animations properties need to be reloaded because keyframes rules have been added. */
		CHANGED_KEYFRAMES = 0x10,
		/** MediaQueryList objects may have changed their matches state. */
		CHANGED_MEDIA_QUERIES = 0x20,

		/** All bits set. */
		CHANGED_ALL = 0x3f
	};

	/** Notify collection of changes in style.

		@param changes A combination of change bits from the StyleChange enum. */
	void StyleChanged(unsigned int changes);

	/** Match the elements in the document (hld_profile) with the selectors from a stylesheet (css)
		to find the elements which match selectors from the given stylesheet. Used to find out which
		elements we need to reload CSS properties for when a stylesheet is added or removed.

		@param css The stylesheet to match for. */
	void MarkAffectedElementsPropsDirty(CSS* css);

	/** Prefetch URLs to resources currently being applied to any element in
		the document by matching selectors that belong to rulesets that have
		declarations for properties that may contain URL values.

		@return ERR_NO_MEMORY on OOM, otherwise OK. */
	OP_STATUS PrefetchURLs();

	/** Retrieve the collection of CSS declarations which apply to a given HTML_Element. */
	OP_STATUS GetProperties(HTML_Element* he, double runtime_ms, CSS_Properties* css_properties, CSS_MediaType media_type);

#ifdef PAGED_MEDIA_SUPPORT
	/** Retrieve the collection of CSS declarations which apply to a given page. */
	void GetPageProperties(CSS_Properties* css_properties, BOOL first_page, BOOL left_page, CSS_MediaType media_type = CSS_MEDIA_TYPE_ALL);
#endif // PAGED_MEDIA_SUPPORT

	/** Return the maximum number of successive adjacent combinators in all selectors of
		all stylesheets in this collection. */
	int GetSuccessiveAdjacent() const;

	/** Find a web-font by font family and media type using cascading order. Returns NULL if not found. */
	CSS_WebFont* GetWebFont(const uni_char* family_name, CSS_MediaType media_type);

	/** Adds a font family to be considered for font prefetching

		New additions are ignored when font prefetching is disabled.

		If the font family corresponds to a web font an attempt to load it
		is done when CheckFontPrefetchCandidates() is called.

		@param[in] family_name The family name of the font. */
	void AddFontPrefetchCandidate(const uni_char* family_name);

	/** Disables font prefetching

		Clears the list of font prefetch candidates and stops further attempts to add more fonts to the list. */
	void DisableFontPrefetching();

	/** Attempts to load the fonts added by AddFontPrefetchCandidate().

		Note that candidates that have previously been checked will be rechecked.
		It is necessary, since a candidate that was not seen as a webfont in previous
		attempts might resurface as a webfont if a stylesheet with a matching
		font-face declaration is loaded. */
	void CheckFontPrefetchCandidates();

	/** Registers that a reevaluation of the webfonts is needed after the specified period of time.

		The timer triggers painting of timed out webfont text with fallback fonts.

		There is only one timer. If the new timeout is set to occur later than the existing
		timeout it will be ignored.  When the timer triggers the webfonts are reevaluated and
		the timeout reset to reflect any remaining (not yet timed out) webfonts.

		@param timeout The timeout in ms */
	void RegisterWebFontTimeout(int timeout);

	/** For the webfont timer */
	virtual void OnTimeOut(OpTimer* timer);

#ifdef CSS_VIEWPORT_SUPPORT
	/** Get the viewport object for the document. If it is dirty, the properties
		are cascaded first.

		@returns the pointer to the documents viewport object. Never NULL. */
	CSS_Viewport* GetViewport()
	{
		if (m_viewport.IsDirty())
			CascadeViewportProperties();

		return &m_viewport;
	}
#endif // CSS_VIEWPORT_SUPPORT

	/** Notify the collection about changed dimensions.

		Happens when the layout viewport is changed due to changed window size,
		@viewport change, css/device pixel ratio change, etc. In that case
		evaluation of media queries might change.

		@param old_width The previous width in CSS px.
		@param old_height The previous height in CSS px.
		@param new_width The current width in CSS px.
		@param new_height The current height in CSS px. */
	void DimensionsChanged(LayoutCoord old_width,
						   LayoutCoord old_height,
						   LayoutCoord new_width,
						   LayoutCoord new_height);

	/** Notify the collection about changed device dimensions.

		Could happen if css/device pixel ratio changes, or when the device
		changes orientation. In that case evaluation of media queries might
		change.

		@param old_device_width The previous device width in CSS px.
		@param old_device_height The previous device height in CSS px.
		@param new_device_width The current device width in CSS px.
		@param new_device_height The current device height in CSS px. */
	void DeviceDimensionsChanged(LayoutCoord old_device_width,
								 LayoutCoord old_device_height,
								 LayoutCoord new_device_width,
								 LayoutCoord new_device_height);

#ifdef STYLE_GETMATCHINGRULES_API

	/** Match style rules for a given element and emit the matched rules through the provided
		listener object. The order of which the rules will be emitted are (cf CSS 2.1 - 6.4.1 Cascading order):

		- The rules are emitted in the reverse order of which they are specfied.
		- The rules are NOT sorted for importance or specificity.
		- The rules are sorted in the reverse order of origin (that is, author before user before user agent).

		For each element being matched, the inline style and default style will also be emitted.

		@param element The element to match rules for. Must be a valid CSS_MatchElement.
		@param css_properties The resulting array of declarations for each property.
		@param media_type The media type of the canvas.
		@param include_inherited Also emit the matching rules for the parent elements. The matching will be done
								 from the element parameter and up to the top of the document tree.
		@param listener The listener object for matched rules. May not be NULL.

		@see CSS_MatchElement

		@return OpStatus::OK on success; OpStatus::ERR if @a element was invalid; or OpStatus::ERR_NO_MEMORY.*/
	OP_STATUS GetMatchingStyleRules(const CSS_MatchElement& element, CSS_Properties* css_properties, CSS_MediaType media_type, BOOL include_inherited, CSS_MatchRuleListener* listener);

#endif // STYLE_GETMATCHINGRULES_API

#ifdef CSS_ANIMATIONS
	CSS_AnimationManager* GetAnimationManager() { return &m_animation_manager; }
#endif // CSS_ANIMATIONS

	/** Return the match context object that is used for matching selectors in this CSS_Collection. */
	CSS_MatchContext* GetMatchContext() { return m_match_context; }

#ifdef PREFS_HOSTOVERRIDE
	virtual void FileHostOverrideChanged(const uni_char* hostname);
#endif // PREFS_HOSTOVERRIDE

	/** Populate the m_stylesheet_array with the currently used stylesheets. */
	OP_STATUS GenerateStyleSheetArray(CSS_MediaType media_type);

	/** @return TRUE if we have ever tried to match :first-child or :not(:first-child)
				for this CSSCollection. */
	BOOL MatchFirstChild() const { return m_match_first_child; }

	/** Called from selector matching to say that we have tried to match :first-child. */
	void SetMatchFirstChild() { m_match_first_child = TRUE; }

#ifdef CSS_TRANSITIONS
	/** Mark the stylesheet as having transition properties. */
	void SetHasTransitions() { m_has_transitions = TRUE; }

	/** @return TRUE if this collection's document has or has had transition properties. */
	BOOL HasTransitions() const { return m_has_transitions; }

	/** Mark the stylesheet as having animation properties. */
	void SetHasAnimations() { m_has_animations = TRUE; }

	/** @return TRUE if this collection's document has or has had animation properties. */
	BOOL HasAnimations() const { return m_has_animations; }
#endif // CSS_ANIMATIONS

#ifdef DOM_FULLSCREEN_MODE
	/** Apply property changes when the fullscreen element changes.

		@param old_elm The previous fullscreen element. May be NULL.
		@param new_elm The new fullscreen element. May be NULL. */
	void ChangeFullscreenElement(HTML_Element* old_elm, HTML_Element* new_elm);
#endif // DOM_FULLSCREEN_MODE

	/** Construct a CSS_MediaQueryList from a media query list string.

		@param media_string The media query list string.
		@param listener Listener object, typically in the dom code to trigger
						MediaQueryListListener callbacks.
		@return A new CSS_MediaQueryList object which has been added to this
				collection/document. The caller is responsible for deleting it
				using OP_DELETE. The CSS_MediaQueryList destructor will make
				sure it is removed from the collection. A NULL return value
				means that we went OOM. */
	CSS_MediaQueryList* AddMediaQueryList(const uni_char* media_string, CSS_MediaQueryList::Listener* listener);

private:

	friend class Iterator;

	/** Check if a resize of the window changes the result of a media query
		present in the stylesheets for this collection. */
	BOOL HasMediaQueryChanged(LayoutCoord old_width,
							  LayoutCoord old_height,
							  LayoutCoord new_width,
							  LayoutCoord new_height);

	/** Check if a change in the device dimensions changes the result of a
		media query present in the stylesheets for this collection. */
	BOOL HasDeviceMediaQueryChanged(LayoutCoord old_device_width,
									LayoutCoord old_device_height,
									LayoutCoord new_device_width,
									LayoutCoord new_device_height);

	/** Re-evaluate media queries from window.matchMedia and notify listeners
		of any changes to the match states. */
	void EvaluateMediaQueryLists();

	/** Get the default style for anchors (HTML::A, WML::ANCHOR, WML::DO). */
	void GetAnchorStyle(CSS_MatchContextElm* context_elm, BOOL set_color, COLORREF& color, int& decoration_flags, CSSValue& border_style, short& border_width);

	/** Get the default (browser) stylesheet properties. This method is called from GetProperties. */
	OP_STATUS GetDefaultStyleProperties(CSS_MatchContextElm* context_elm, CSS_Properties* css_properties);

#ifdef MEDIA_HTML_SUPPORT
	/** Get the default stylesheet properties for <track> cue elements. */
	void GetDefaultCueStyleProperties(HTML_Element* element, CSS_Properties* css_properties);
#endif // MEDIA_HTML_SUPPORT

#ifdef DOM_FULLSCREEN_MODE
	/** Get the default (browser) styles for :fullscreen and :fullscreen-ancestor elements. */
	void ApplyFullscreenProperties(CSS_MatchContextElm* context_elm, CSS_Properties* css_properties);

	/** @return TRUE if there is a selector in this collection which has
				:fullscreen or :fullscreen-ancestor in other places
				than the rightmost simple selector. */
	BOOL HasComplexFullscreenStyles() const;
#endif // DOM_FULLSCREEN_MODE

	/** A linked list of the CSSCollectionElement objects in this collection */
	List<CSSCollectionElement> m_element_list;

	/** A linked list of the collection elements added with commit=FALSE */
	List<CSSCollectionElement> m_pending_elements;

	/** Constants defining indices into the m_stylesheet_array. */
	enum StyleSheetIndex
	{
#ifdef _WML_SUPPORT_
		WML_STYLESHEET_IDX,
#endif // _WML_SUPPORT_
#if defined(CSS_MATHML_STYLESHEET) && defined(MATHML_ELEMENT_CODES)
		MATHML_STYLESHEET_IDX,
#endif // CSS_MATHML_STYLESHEET && MATHML_ELEMENT_CODES
		FIRST_AUTHOR_STYLESHEET_IDX
	};

	/** The current array of stylesheets. */
	OpVector<CSS> m_stylesheet_array;

	/** Store the FramesDocument so that we don't need to pass it to every method. */
	FramesDocument* m_doc;

#ifdef CSS_ANIMATIONS
	CSS_AnimationManager m_animation_manager;
#endif // CSS_ANIMATIONS

	/** The match context to use when loading properties from this collection. */
	CSS_MatchContext* m_match_context;

#ifdef PREFS_HOSTOVERRIDE

	/** Load the host overrides. */
	void LoadHostOverridesL(const uni_char* hostname);

	struct LocalStylesheet
	{
		HTML_Element* elm;
		CSS* css;

		LocalStylesheet() : elm(NULL), css(NULL) {};
		~LocalStylesheet();
	};

	/** An array of LINK HTML_Element object pointers for the host overrides
		for the local stylesheets and and their CSS objects. */
	LocalStylesheet m_host_overrides[CSSManager::FirstUserStyle];

	/** Initially FALSE. Set to TRUE the first time we try to load host overrides. */
	BOOL m_overrides_loaded;

#endif // PREFS_HOSTOVERRIDE

#ifdef CSS_VIEWPORT_SUPPORT
	/** Cascade viewport properties from @viewport rules and meta viewport elements. */
	void CascadeViewportProperties();

	/** The viewport object for constraining viewport properties for this document. */
	CSS_Viewport m_viewport;
#endif // CSS_VIEWPORT_SUPPORT

#ifdef CSS_TRANSITIONS
	/** TRUE if the current m_stylesheet_array may contain transition properties. */
	BOOL m_has_transitions;
#endif // CSS_TRANSITIONS

#ifdef CSS_ANIMATIONS
	/** TRUE if the current m_stylesheet_array may contain animations properties. */
	BOOL m_has_animations;
#endif // CSS_ANIMATIONS

	/** Set to TRUE if we have ever tried to match :first-child or :not(:first-child).
		Used for MarkPropsDirty optimization. */
	BOOL m_match_first_child;

	/** A list of active MediaQueryList objects in DOM. */
	AutoDeleteList<CSS_MediaQueryList> m_query_lists;

	/** Limit on how many webfonts may be prefetched. */
	int m_font_prefetch_limit;

	/** Names of font families to be considered for prefetching. */
	OpStringHashTable<OpString> m_font_prefetch_candidates;

	/** Timer for when webfonts are to be reevaluated for use of fallback fonts. */
	OpTimer m_webfont_timer;
};

/**
   returns the index of the form style to be used for elm_type with
   input type itype. this is to make sure the same form style is used
   in GetDefaultStyleProperties as is used in
   HTMLayoutProperties::GetCssProperties when applying writing
   system-specific defaults for font family and/or font size.

   @param elm_type the type of the html element for which style is to
   be fetched.
   @param itype the input type of said element.
   @return the index of the style, to be passed to GetStyleEx
 */
int GetFormStyle(const Markup::Type elm_type, const InputType itype);

#endif // CSS_COLLECTION_H
