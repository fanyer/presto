/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _CASCADE_H_
#define _CASCADE_H_

#include "modules/layout/layoutprops.h"

#include "modules/doc/frm_doc.h"
#include "modules/util/str.h"

class MultiColumnContainer;
class TableContent;
class FlexContent;
class SpaceManager;
class StackingContext;
class ZElement;
class TraversalObject;

#ifdef SVG_SUPPORT
class SVGImage;
#endif // SVG_SUPPORT

class LayoutProperties
  : public Link
{
public:

	/** Result of layout box creation operation. */
	enum LP_STATE
	{
		OUT_OF_MEMORY, ///< Stop layout due to out of memory.
		YIELD, ///< Stop layout due to yield.
		INSERTED_PARENT, ///< Anonymous parent table-* layout box inserted.
		ELEMENT_MOVED_UP, ///< Element and all its successive siblings were moved up one level (became a sibling of its anonymous table-* parent).
		CONTINUE, ///< The typical result. Everything went well and no special action needs to be taken.
		STOP ///< Stop layout because we reached the last element to reflow (cf. LayoutInfo::stop_before).
	};

	HTML_Element*	html_element;

public:

	/** Inherit properties with first-line pseudo properties from this
		element */

	BOOL			use_first_line_props;

private:

	HTMLayoutProperties
					props;

	HTMLayoutProperties*
					cascading_props;

	LP_STATE		LayoutElement(LayoutInfo& info);

	/** Check if this element will generate child elements to represent the content property.

		@return TRUE if there will be generated child elements of this element
		because of the content property. Returns FALSE if there will be no
		elements. */

	BOOL			HasRealContent();

	/** Create a pseudo element for ::before or ::after under this element.

		This will remove any old ::before (if 'before' is TRUE) or ::after (if
		'before' is FALSE) pseudo element first.

		@param info Layout info
		@param before TRUE if we're creating a ::before element, FALSE if we're
		creating an ::after element.
		@return FALSE on OOM, TRUE otherwise. */

	BOOL			CreatePseudoElement(LayoutInfo& info, BOOL before);

	/** Create a list item marker pseudo element.

		@param info The LayoutInfo of the current reflow.
		@return FALSE on OOM, TRUE otherwise. */

	BOOL			CreateListItemMarker(LayoutInfo& info);

	/** Inserts a child text node for the list item marker pseudo element.

		@return FALSE on OOM, TRUE otherwise. */

	BOOL			InsertListItemMarkerTextNode();

	enum ListMarkerContentType
	{
		/** No content, used when list-style-type is 'none' and no list-style-image is specified,
			or list-style-image is specified, but the image (or SVG) is not yet ready. */
		LIST_MARKER_NONE,
		/** Used for numeration list markers. */
		LIST_MARKER_TEXT,
		/** Used for images, svg and simple glyph markers. */
		LIST_MARKER_BULLET
	};

	/** Gets the suitable content type for list marker pseudo element.
		The result depends on list-style properties and whether the images (if specified) are available.

		@param doc The document being reflowed.
		@return The suitable list marker content type. */

	ListMarkerContentType
					GetListMarkerContentType(FramesDocument* doc);

	/** Create the layout box and the content for list marker pseudo element.
		Special version of
		@see LayoutProperties::CreateBox
		for list markers.

		@param type The needed list marker content type.
		@return CONTINUE or OUT_OF_MEMORY. */

	LP_STATE		CreateListMarkerBox(ListMarkerContentType type);

	/** Add generated content for this element.

		This will generate child elements for this element's 'content'
		property.

		@param info Layout information
		@return FALSE on OOM, TRUE otherwise */

	BOOL			AddGeneratedContent(LayoutInfo& info);

	LP_STATE		CreateTextBox(LayoutInfo& info);

	LP_STATE		CreateBox(LayoutInfo& info, Markup::Type element_type, CSSValue display_type, BOOL& may_need_reflow);

	OP_LOAD_INLINE_STATUS
					LoadInlineURL(LayoutInfo& info,
								  URL* inline_url,
								  InlineResourceType inline_type,
								  BOOL from_user_css,
								  BOOL& may_need_reflow);

#ifdef MEDIA_HTML_SUPPORT
	/** Insert structural elements for <video>. */

	OP_STATUS		InsertVideoElements(LayoutInfo& info);

	/** Insert background box for a <track> cue. */

	OP_STATUS		InsertCueBackgroundBox(LayoutInfo& info);

#endif

	/** Check and insert missing table elements both in element tree and cascade.
		Returns INSERTED_PARENT if element was inserted. */

	LP_STATE		CheckAndInsertMissingTableElement(LayoutInfo& info, Markup::Type create_type);

	LP_STATE		InsertMapAltTextElements(LayoutInfo& info);

#ifdef ALT_TEXT_AS_TEXT_BOX
	/** Insert a Text element that contains the alt text as a child to the current image element */

	LP_STATE        InsertAltTextElement(LayoutInfo& info);
#endif

	/** Create cascade for specified element. Recursive method used internally. */

	static LayoutProperties*
					CreateCascadeInternal(HTML_Element* html_element,
										  Head& prop_list,
										  HLDocProfile* hld_profile,
										  BOOL dont_normalize = FALSE);

	/** Clone an existing cascade chain.

		This is a helper method for CloneCascade().

		Works recursively by walking all the way too the root and clones each
		cascade entry on its way back down.

		@param prop_list[out] Put the new cascade here
		@param old_layout_props The cascade to clone.
		@param hld_profile The HLDocProfile for the document.

		@return OK if the cascade was cloned, or ERR_NO_MEMORY on OOM. */

	static OP_STATUS
					CloneCascadeInternal(Head& prop_list, const LayoutProperties* old_layout_props, HLDocProfile* hld_profile);

	/** Check if the protocol of the specified inline URL is trusted, which
		means that we may pass it to another application for loading. */

	OP_BOOLEAN		IsTrustedInline(URL* inline_url);

public:

	/** Create the cascade for the specified element and put it in prop_list.

		@param html_element The element to create the cascade for. Cascade elements
							are created for the initial properties and for every
							ascendant of html_element. If html_element is NULL,
							only the cascade element for the initial properties will
							be created.
		@param prop_list The list to add the created cascade elements to. The caller
						 is responsible for deleting the elements and clear this list.
		@param hld_profile The HLDocProfile for the document.
		@param dont_normalize Set to TRUE for currentStyle, otherwise FALSE.

		@return The last element in the cascade, or NULL on allocation failure.
				If NULL is returned, prop_list is guaranteed to be empty. */

	static LayoutProperties*
					CreateCascade(HTML_Element* html_element,
								  Head& prop_list,
								  HLDocProfile* hld_profile
#ifdef CURRENT_STYLE_SUPPORT
								  , BOOL dont_normalize = FALSE
#endif
						);

	/* Correct some values that are invalid for the layout engine but should still
	   be retained for getComputedStyle and currentStyle. This is done by cloning
	   the props using WantToModifyProps(TRUE). */

	BOOL			RectifyInvalidLayoutValues();

	/** Remove elements that are inserted by layout.

		Remove those children of the specified element that are inserted by
		layout. Also remove the specified element, if it is inserted by
		layout. Children of such elements will be promoted. If the specified
		element was inserted by layout, the 'element' parameter will be
		changed, and the new 'element's children inserted by layout will also
		be removed.

		When a layout box is recreated during reflow, we first have to get rid
		of any layout-inserted elements, because part of the job of creating a
		layout box for an element is to insert such elements, i.e. missing
		anonymous table ancestors and elements resulting from processing the
		'content' property and the ::before and ::after selectors. The reason
		for the box recreation is typically that the element changed its
		display type, and the old anonymous table element structure will then
		be wrong.

		As for generated content (via the 'content' property), we could in
		theory keep the layout-inserted elements even during box recreation,
		but our code currently doesn't support this.

		It may be worth noting here that HTML_Element::MarkExtraDirty() on an
		element marks any contiguous chain of layout-inserted parents (+ the
		non-inserted parent of that chain) extra dirty. */

	static void		RemoveElementsInsertedByLayout(LayoutInfo& info, HTML_Element*& element);

	Container*		container;
	Container*		multipane_container;
	TableContent*	table;
	FlexContent*	flexbox;
	SpaceManager*	space_manager;
	StackingContext*
					stacking_context;

public:

					LayoutProperties()
					  : html_element(NULL),
						use_first_line_props(FALSE),
						cascading_props(NULL),
						container(NULL),
						multipane_container(NULL),
						table(NULL),
						flexbox(NULL),
						space_manager(NULL),
						stacking_context(NULL) {}

	virtual			~LayoutProperties() { delete cascading_props; }

	void*			operator new(size_t size) OP_NOTHROW;
	void			operator delete(void* ptr, size_t size);


	inline HTMLayoutProperties*
					GetProps() { return &props; }

	OP_STATUS		InitProps() { return OpStatus::OK; }

	/** Finish laying out boxes. */

	OP_STATUS		Finish(LayoutInfo* info);

	/** Remove cascade from layout structure. */

	void			RemoveCascade(LayoutInfo& info);

	/** Wipe element. */

	void			Clean() { html_element = NULL; container = NULL; multipane_container = NULL; table = NULL; flexbox = NULL; use_first_line_props = FALSE; delete cascading_props; cascading_props = NULL; }

	/** Check if table or table cell in cascade has a specified desired width. */

	BOOL			HasTableSpecifiedWidth() const;

	/** Gets the image or svg image that should be used for list item bullet.
		May return an empty image or NULL SVG image pointer if the resources are
		not available yet.

		@param doc The document being reflowed.
		@param[out] svg_image The SVGImage pointer. Assigned a valid value if the SVGImage of the marker is ready.
		@return The marker Image. May be empty (in case the image is not yet available or SVGImage pointer is valid, but
				it wasn't yet painted to a buffer.) */

	Image			GetListMarkerImage(FramesDocument* doc
#ifdef SVG_SUPPORT
									   , SVGImage*& svg_image
#endif // SVG_SUPPORT
									   );

	/** Search cascade for active table row group. */

	TableRowGroupBox*
					FindTableRowGroup() const;

	/** Find active parent for this element. */

	HTML_Element*	FindParent() const;

	/** Find containing element for this absolute positioned element. */

	HTML_Element*   FindContainingElementForAbsPos(BOOL fixed) const;

	/** Find the nearest multi-pane container.

		@param find_paged If TRUE, look for a paged container. If FALSE, look
		for a multicol container. */

	Container*		FindMultiPaneContainer(BOOL find_paged) const;

	/** Find the offsetParent of an element. */

	LayoutProperties*
					FindOffsetParent(HLDocProfile *hld_profile);

	/** Clean succeeding LayoutProperties until one with html_element = NULL is reached. */

	void			CleanSuc() const;

	/** Remove first-line properties and illegal terminations within the first line. */

	void			RemoveFirstLineProperties();

	/** Get properties as determined by the cascade. */

	HTMLayoutProperties&
					GetCascadingProperties() { return cascading_props ? *cascading_props : props; }

	/** Skip over branch and terminate parents that might have
	    terminators in this branch. Also process list item numbering if we're skipping a list item element. */

	BOOL			SkipBranch(LayoutInfo& info, BOOL skip_space_manager, BOOL skip_z_list);

	/** We want to modify properties, make a copy for the cascade. */

	BOOL			WantToModifyProperties(BOOL copy = FALSE);

	/** Used to temporarily insert the first-line element in to the tree.
	    Will automatically remove the first-line element on destruction. */
	class AutoFirstLine
	{
	public:

		/** Create a new AutoFirstLine, but don't actually insert the element
		    until AutoFirstLine::Insert() is called. This is useful if want
		    to insert the first-line element only if a certain condition is
		    met.

		    The first-line element will be automatically removed when the
		    object is destroyed.

		    @param hld_profile The document the insertion should apply to.
		                       May not be NULL. */
		AutoFirstLine(HLDocProfile* hld_profile);

		/** Create a new AutoFirstLine, and automatically insert it before
		    the specified element.

		    The first-line element will be automatically removed when the
		    object is destroyed.

		    @param hld_profile The document the insertion should apply to.
		                       May not be NULL.
		    @param html_element Insert the first-line element before this
		                        element. May not be NULL. */
		AutoFirstLine(HLDocProfile* hld_profile, HTML_Element* html_element);

		/** If a first-line element has been inserted, the destructor will
		    remove it from the tree. If a first-line element has not been
		    inserted, nothing happens. */
		~AutoFirstLine();

		/** Insert the first-line element before the specified element.

		    @param html_element Insert the first-line element before this
		                        element. May not be NULL. */
		void Insert(HTML_Element* html_element);

		/** @return The actual first-line element, or NULL if the
		            first-line element has noe been inserted. */
		HTML_Element* Elm() const { return m_first_line; }

	private:

		/** If the first-line element has been inserted, it points to the
		    inserted element. Otherwise NULL. */
		HTML_Element* m_first_line;

		/** The document the insertion applies to. */
		HLDocProfile* m_hld_profile;
	};

	/** Add first-line pseudo properties to cascade. */

	OP_STATUS		AddFirstLineProperties(HLDocProfile* hld_profile);

	/** Get the layout properties for child during layout.

		@param info LayoutInfo used for this reflow.
		@param child The element to create LayoutProperties for.
		@param invert_run_in If TRUE, convert run-in between inline and block.
		@return The LayoutProperties for the child HTML_Element. NULL on OOM. */

	LayoutProperties*
					GetChildCascade(LayoutInfo& info, HTML_Element* child, BOOL invert_run_in = FALSE);

	/** Clone the cascade chain that ends with this element.

		@param prop_list[out] Put the new cascade here
		@param hld_profile The HLDocProfile for the document.

		@return The end of the cascade chain (the clone), or NULL on OOM. */

	LayoutProperties*
					CloneCascade(Head& prop_list, HLDocProfile* hld_profile) const;

	/** Get the layout properties for child.

		Computes the complete cascade from the closest ancestor of child available
		in the cascade chain, down to child.

		@param hld_profile The HLDocProfile for the document.
		@param child The element to get cascade for.

		@return The LayoutProperties for the child HTML_Element. NULL on OOM. */

	LayoutProperties*
					GetChildProperties(HLDocProfile* hld_profile, HTML_Element* child);

	/** Get the layout properties for child. To be used while reloading CSS
		properties to detect computed style transitions/animations only
		(via LayoutWorkplace::ReloadCssProperies).

		@param hld_profile The document's HLDocProfile.
		@param child The element to get computed style for.
		@param ignore_transitions Get computed style for the element as if there
								  was no ongoing transitions.
		@return The new computed styles. NULL on OOM. */

	LayoutProperties*
					GetChildPropertiesForTransitions(HLDocProfile* hld_profile, HTML_Element* child, BOOL ignore_transitions);

	/** Convenience overloading. */

	LayoutProperties*
					Suc() const { return (LayoutProperties*) Link::Suc(); }

	/** Convenience overloading. */

	LayoutProperties*
					Pred() const { return (LayoutProperties*) Link::Pred(); }

	/** Flags to be used when creating cascade. */

	enum GetCascadeFlags
	{
		/** No flags set. */
		NOFLAGS,
		/** Switch between inline and block for run-ins. */
		INVERT_RUN_IN,
		/** Don't apply transition properties. */
		IGNORE_TRANSITIONS
	};

	/** Calculate computed styles by inheriting from parent LayoutProperties
		and call GetCssProperties to retrieve specified styles on this
		element.

		@param hld_profile HLDocProfile for the document.
		@param parent The LayoutProperties for the element we're inheriting from.
		@param flags Or'ed GetCascadeFlags.
		@return The LayoutProperties for the child HTML_Element. NULL on OOM. */

	BOOL			Inherit(HLDocProfile* hld_profile, LayoutProperties* parent, int flags = NOFLAGS);

	/** Get open or close quotation mark at given level. */

	const uni_char* GetQuote(unsigned int quote_level, BOOL open_quote);

	/** Determine whether page/column breaking inside this element should be avoided. */

	void			GetAvoidBreakInside(const LayoutInfo& info, BOOL& avoid_page_break_inside, BOOL& avoid_column_break_inside);

	/** Attempt to create (or recreate) a layout box for a child, and lay it out.

		This method will create a cascade (LayoutProperties) entry for the
		specified child element, attach it to the chain and create the correct
		type of layout box for the element (see CreateLayoutBox()). It will also
		lay out the box and the entire subtree.

		@param info Layout info
		@param child The child for which to create a layout box. */

	LP_STATE		CreateChildLayoutBox(LayoutInfo& info, HTML_Element* child);

	/** Attempt to create (or recreate) a layout box for this element, and lay it out.

		Based on CSS properties (such as display, position, float and width),
		node type (such IMG, INPUT and DIV element, or a text node) and parent,
		create an appropriate layout box for this element. A layout box
		consists of an object derived from the Box class, and in most cases
		also an object derived from the Content class. The Box object is
		connected to the HTML_Element.

		In addition to merely creating the layout box, this method will also
		lay it out.

		This method may insert a table* parent anonymous element, to complete
		the CSS table model, as defined in 17.2.1 of the CSS 2.1 spec. If that
		happens, no box creation will occur. It will simply return, and signal
		that the (new) parent's layout box needs to be created first (return
		value INSERTED_PARENT).

		Inserting an anonymous table* parent anonymous element will move the
		child and all its consecutive siblings under this anonymous element. It
		may later turn out that one of the siblings doesn't belong under the
		anonymous element (for example, a real table-row DOM element should not
		be a child of a layout-inserted anonymous table-row element). This
		method will also detect such situations, and if necessary, move an
		element up one level. If this happens, no box creation will occur. It
		will simply return, and signal that the element has been moved up
		(return value ELEMENT_MOVED_UP).

		This method is also responsible for creating and inserting
		pseudo-elements caused by the 'content' property, either on the actual
		DOM element or through the ::before and ::after selectors.

		@param info Layout info
		@return Any value of LP_STATE. See above for INSERTED_PARENT and
		ELEMENT_MOVED_UP. See LP_STATE documentation for the other values. */

	LP_STATE		CreateLayoutBox(LayoutInfo& info);

#ifdef SUPPORT_VISUAL_ADBLOCK

	/** Check if an element has some content blocked by content_filter.

		If it has, CreateLayoutBox() has to return CONTINUE, to prevent the
		visualization of the element.

		@param info Layout info
		@return TRUE if the element has to be blocked, FALSE if it can be displayed	*/

	BOOL			IsElementBlocked(LayoutInfo& info);

#endif // SUPPORT_VISUAL_ADBLOCK

	BOOL			ConvertToInlineRunin(LayoutInfo& info, Container* old_container);

	/** Change font and update some of the font attributes (ascent, descent ...) */

	void			ChangeFont(VisualDevice* visual_device, int font_number);

	LayoutProperties*
					GetCascadeAndLeftAbsEdge(HLDocProfile* hld_profile, HTML_Element* element, LayoutCoord& left_edge, BOOL static_position);

	/** Is this element to be laid out with auto width? This is true when the CSS width is auto.

		The special width property -o-content-size is also treated as 'auto' since in all cases
		except iframes, they are to be treated the same. */

	BOOL			IsLayoutWidthAuto() const
	{
		return props.content_width == CONTENT_WIDTH_AUTO || props.content_width == CONTENT_WIDTH_O_SIZE;
	}

	/** Extract properties from this element that are to be stored as "global document properties".

		A document has a concept of "document background", "document font", etc. Normally
		these are extracted from HTML and BODY elements. Will cause the entire document
		area to be repainted when an element sets "global document background". */

	OP_STATUS		SetDocRootProps(HLDocProfile* hld_profile, Markup::Type element_type) const;

	/** Add ZElement to stacking context, if there is any */

	void			AddZElement(ZElement *z_element);

	/** Compute a color declaration from a COLORREF value.

		If `is_current_style' is TRUE, the declaration will have the
		format for currentStyle.  This is only supported when
		FEATURE_CURRENT_STYLE is enabled.

		Usually a internal helper method for `GetComputedDecl()', but
		used externally though a round-trip to the SVG module. */
	static CSS_decl* GetComputedColorDecl(short property, COLORREF color_val, BOOL is_current_style = FALSE);

	/** Get a computed style property for an element.

		The cascade is computed to the element `elm' and the
		`property' used to select field in the cascade. The computed
		value in the cascade will be used to construct a CSS
		declaration matching the computed value.

		A clean tree is required before the cascade is created, so a
		reflow will be issued first.

		`psuedo' can be used to point to a generated element. This is
		needed because a reflow can cause the generated elements to be
		re-generated.

		Only avaliable when scripting is available since the only
		use case is access from DOM.*/
	static CSS_decl* GetComputedDecl(HTML_Element* elm, short property, short pseudo, HLDocProfile *hld_profile);

	/** Like the above function, but requires the cascade to be precomputed. */
	static CSS_decl* GetComputedDecl(HTML_Element* elm, short property, short pseudo, HLDocProfile *hld_profile, LayoutProperties *lprops);

#ifdef CURRENT_STYLE_SUPPORT
	/** Get a current style property for an element. Corresponds to
		the .currentStyle object in DOM.

		The cascade is computed to the element `elm' in
		currentStyle-mode and the `property' used to select field in
		the cascade. The computed value in the cascade will be used to
		construct a CSS declaration matching the currentStyle value.

		A clean tree is required before the cascade is created, so a
		reflow will be issued first.

		`psuedo' can be used to point to a generated element. This is
		needed because a reflow can cause the generated elements to be
		re-generated.

		Only avaliable when scripting is available since the only
		use case is access from DOM.*/
	static CSS_decl *GetCurrentStyleDecl(HTML_Element *elm, short property, short pseudo, HLDocProfile *hld_profile);
#endif // CURRENT_STYLE_SUPPORT

	/** Calculate the number of pixel that a CSS declaration `decl`
		corresponds to. */
	static int CalculatePixelValue(HTML_Element* elm, CSS_decl *decl, FramesDocument *frm_doc, BOOL is_horizontal);

#ifdef CSS_TRANSITIONS
	/** Increase reference count for CSS_decl objects which may be dereferenced
		when removed for CssPropertyItem while this cascade element is kept for
		transitions in ReloadCssProperties. */
	void ReferenceDeclarations();

	/** Decrease reference count for CSS_decl objects which have their reference
		counts increased by RefDeclarations. */
	void DereferenceDeclarations();
#endif // CSS_TRANSITIONS

private:
	static CSS_decl* GetComputedDecl(HTML_Element* elm, short property, short pseudo, HLDocProfile *hld_profile, BOOL is_current_style);
	static CSS_decl* GetComputedDecl(HTML_Element* elm, short property, short pseudo, HLDocProfile *hld_profile, LayoutProperties *lprops, BOOL is_current_style);
};

/** Return TRUE if the text has to be laid out, or FALSE otherwise (e.g. it only contains collapsable white-space). */

inline BOOL			TextRequiresLayout(FramesDocument* doc, const uni_char* text, CSSValue white_space)
{
	return text && (!IsWhiteSpaceOnly(text) ||
#ifdef DOCUMENT_EDIT_SUPPORT
					// Create layoutbox for empty textelement in editable mode.
					doc->GetDocumentEdit() && !*text ||
#endif // DOCUMENT_EDIT_SUPPORT
					white_space == CSS_VALUE_pre || white_space == CSS_VALUE_pre_wrap);
}

#endif /* _CASCADE_H_ */
