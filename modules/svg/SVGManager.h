/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SVG_MANAGER_H
#define SVG_MANAGER_H

#ifdef SVG_SUPPORT

class VisualDevice;
class OpBitmap;
class LayoutProperties;
class URL;
class SVGContext;
class SVGPath;
class TempBuffer;
class ComplexAttr;
class OpInputAction;
class LogicalDocument;
class SVGImage;
class HTML_Element;
class OpRect;
class SVGImageRef;
class SVGTreeIterator;
class OpFile;
class SVGFilterInputImageGenerator;
class SvgProperties;
class CSS_property_list;

#include "modules/svg/svg_external_types.h" // for SVGUrlReference
#include "modules/logdoc/logdocenum.h"
#include "modules/logdoc/namespace.h"
#include "modules/dom/domeventtypes.h"
#include "modules/display/cursor.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/doc/html_doc.h" // For HTML_Document::SCROLL_ALIGN, not really needed soon
#include "modules/doc/doctypes.h"

#ifndef SVG_DOCUMENT_CLASS
# define SVG_DOCUMENT_CLASS FramesDocument
#endif // !SVG_DOCUMENT_CLASS

class SVG_DOCUMENT_CLASS;

/**
 * Main SVG external API. This is the entry point for most SVG
 * operations.
 *
 * @section conventions Conventions used in this interface
 *
 * If a method accepts a argument of type HTML_Element named 'root'
 * then the Type() of the HTML_Element must be Markup::SVGE_SVG. If a method
 * accepts a argument of type HTML_Element named 'element' then the
 * namespace of the HTML_Element must be svg (HTML_Element::GetNsType()
 * == NS_SVG).
 *
 * @author Erik Dahlstrom (ed@opera.com) & David Vest (davve@opera.com)
 *
 */
class SVGManager
{
public:
	/**
	 * This is needed for destruction to work
	 */
	virtual ~SVGManager() {}

	/**
	 * This class is a container for events that is sent to the SVG module.
	 */
	class EventData
	{
	public:
		/**
		 * Constructor. Initializes all members to zero or default
		 * values.
		 */
		EventData();

		/**
		 * This is the DOM_EventType enumerator representing the event if one
		 * exists.
		 */
		DOM_EventType type;

		HTML_Element *target;			/**< Target. */

		SVG_DOCUMENT_CLASS* frm_doc;	/**< The current frames document of the target */

		int modifiers;					/**< Shift key state. */

		/* UIEvent */
		int detail;						/**< Detail (repeat count for ONCLICK). */

		/* MouseEvent */
		int screenX;					/**< MouseEvent: screen X position. */
		int screenY;					/**< MouseEvent: screen Y position. */
		int offsetX;					/**< MouseEvent: offset X position. */
		int offsetY;					/**< MouseEvent: offset Y position. */
		int clientX;					/**< MouseEvent: client X position. */
		int clientY;					/**< MouseEvent: client Y position. */
		int button;						/**< MouseEvent: button. */
	};

	/**
	 * Initiate the handling of an event.
	 * @param data The data of the event. @see EventData.
	 *
	 * @return OpBoolean::IS_TRUE means that the event has been
	 * handled. OpBoolean::IS_FALSE means that the event hasn't been
	 * handled. OpStatus::ERR_NO_MEMORY is return if a OOM occurs.
	 */
	virtual OP_BOOLEAN      HandleEvent(const EventData& data) = 0;

	/**
	 * Initialize a root element to be a SVG root.
	 *
	 * Must be called on every SVG root node before rendering it.
	 *
	 * @param root The root element of the SVG. Should not be NULL.
	 * @param logdoc The document containing the SVG. Should not be NULL.
	 */
	virtual SVGContext*		CreateDocumentContext(HTML_Element* root, LogicalDocument* logdoc) = 0;

	/**
	 * Parse a SVG attribute
	 *
	 * This method parses a string attribute into the corresponding SVG object
	 * @param element The element the attribute is associated to
	 * @param doc The current frames document
	 * @param attr_name The attribute type
	 * @param ns The attribute namespace index
	 * @param str The string to parser
	 * @param strlen The length of the string to parse
	 * @param outValue The resulting value
	 * @param outValueType The type of the resulting value
	 * @return OpBoolean::IS_TRUE if the attribute was parsed
	 * successfully. OpBoolean::IS_FALSE if the string couldn't be
	 * parsed. OpStatus::ERR_NO_MEMORY if OOM occured.
	 */
	virtual OP_BOOLEAN      ParseAttribute(HTML_Element* element, SVG_DOCUMENT_CLASS* doc, int attr_name, int ns, const uni_char* str, unsigned strlen, void** outValue, ItemType &outValueType) = 0;

	/**
	 * Navigate to element. If the URL is external to the SVG
	 * document, the URL is saved in outurl and expected to be
	 * navigated to by the caller.
	 *
	 * @param elm The element to get the URL for. Can be any SVG element.
	 * @param doc The current frames document
	 * @param outurl A pointer to a pointer where the external URL will be
	 * saved if that is the case. It will be set to NULL otherwise.
	 * @param event The event that has trigged the navigation.
	 * @param window_target The adress of a pointer that will point to
	 * the name of the window that should be target of the link, for
	 * instance _blank or _parent. Can be NULL.
	 * @param modifiers The current modifier keys
	 * @return OpBoolean::IS_TRUE if it was an internal URL.
	 * OpBoolean::IS_FALSE If it is an external url OR the url could
	 * not be retreived. Note that OpBoolean::IS_FALSE is not a
	 * guarantee that the URL was external only that it wasn't
	 * internal. OpStatus::ERR_NO_MEMORY if OOM occured
	 */
	virtual OP_BOOLEAN      NavigateToElement(HTML_Element *elm, SVG_DOCUMENT_CLASS* doc, URL** outurl, DOM_EventType event = ONCLICK, const uni_char** window_target = NULL, ShiftKeyState modifiers = SHIFTKEY_NONE) = 0;

	/**
	 * Ask the SVG module what graphic element, if any, is intersected
	 * at a coordinate (x, y). The coordinates are relative to
	 * upper-right corner of the SVG image. 'target' is set to NULL if
	 * no graphical element matches the coordinates. This variant will
	 * save the offset of the element within the viewport.
	 *
	 * @param root The root element of the SVG to find the element in
	 * @param doc The current frames document
	 * @param x The x coordinate @param y The y coordinate
	 * @param element The matched element is saved here. It is set to
	 * NULL if no graphics element matches.
	 * @param offset_x Reference to where the offset in x will be saved
	 * @param offset_y Reference to where the offset in y will be saved
	 */
	virtual OP_BOOLEAN      FindSVGElement(HTML_Element* root, SVG_DOCUMENT_CLASS* doc, int x, int y, HTML_Element** element, int& offset_x, int& offset_y) = 0;

	/**
	* Transform coordinates from the SVGDocument coordinate system to the element coordinate system.
	* The element coordinate system's origo is at its boundingbox top-left corner.
	*
	* @param element The element to transform coordinates to
	* @param doc The frames document that has the element
	* @param io_x The x coordinate (in SVGDocument coordinates, out element coordinates)
	* @param io_y The y coordinate (in SVGDocument coordinates, out element coordinates)
	*/
	virtual OP_BOOLEAN		TransformToElementCoordinates(HTML_Element* element, SVG_DOCUMENT_CLASS* doc, int& io_x, int& io_y) = 0;

	/**
	 * Invalidate element
	 *
	 * The element to invalidate caches for. This removes cached data
	 * as bounding boxes and other temporal calculations.
	 *
	 * @param element The element to invalidate caches for.
	 * @param doc The current frames document
	 */
	virtual void            InvalidateCaches(HTML_Element* element, SVG_DOCUMENT_CLASS* doc = NULL) = 0;

	/**
	 * Start an animation
	 *
	 * @param root The root element
	 * @param doc The current frames document
	 * @return OpStatus::ERR_NO_MEMORY if OOM. OpStatus::OK otherwise.
	 */
	virtual OP_STATUS		StartAnimation(HTML_Element* root, SVG_DOCUMENT_CLASS* doc) = 0;

	/**
	 * Stop an animation
	 *
	 * @param root The root element
	 * @param doc The current frames document
	 * @return OpStatus::ERR_NO_MEMORY if OOM. OpStatus::OK otherwise.
	 */
	virtual OP_STATUS		StopAnimation(HTML_Element* root, SVG_DOCUMENT_CLASS* doc) = 0;

	/**
	 * Pause an animation
	 *
	 * @param root The root element
	 * @param doc The current frames document
	 * @return OpStatus::ERR_NO_MEMORY if OOM. OpStatus::OK otherwise.
	 */
	virtual OP_STATUS		PauseAnimation(HTML_Element* root, SVG_DOCUMENT_CLASS* doc) = 0;

	/**
	 * Pause an animation
	 *
	 * @param root The root element
	 * @param doc The current frames document
	 * @return OpStatus::ERR_NO_MEMORY if OOM. OpStatus::OK otherwise.
	 */
	virtual OP_STATUS		UnpauseAnimation(HTML_Element* root, SVG_DOCUMENT_CLASS* doc) = 0;

#ifdef ACCESS_KEYS_SUPPORT
	/**
	 * Process the press of an access key on a element
	 *
	 * @param doc The current frames document
	 * @param element The element that was activated
	 * @param key The access key used
	 */
	virtual OP_STATUS       HandleAccessKey(SVG_DOCUMENT_CLASS* doc, HTML_Element* element, uni_char access_key) = 0;
#endif // ACCESS_KEYS_SUPPORT

	/**
     * Allocate and create an SVGPath.
	 *
	 * @param path The path will be returned here
	 * @return OpStatus::ERR_NO_MEMORY if OOM. OpStatus::OK otherwise.
	 */
	virtual OP_STATUS       CreatePath(SVGPath** path) = 0;

	/**
	* Handle a specific action, returns TRUE if handled by svg.
	*
	* @param action The action
	* @param clicked_element The svg element that the action is related to
	* @param doc The Document that the svg root element is in
	* @return TRUE if handled, FALSE otherwise
	*/
	virtual BOOL			OnInputAction(OpInputAction* action, HTML_Element* clicked_element, SVG_DOCUMENT_CLASS* doc) = 0;

	/**
	 * Called when something has changed in an SVG document.
	 *
	 * @param is_addition TRUE if node was just inserted, FALSE if node will be removed.
	 * @param doc The document where the SVG resides.
	 * @param parent The parent of the element that was changed (needed since the child may have been removed already).
	 * @param child The element that changed.
	 * @param is_addition TRUE if the child was added to the tree, FALSE otherwise.
	 */
	virtual OP_STATUS		OnSVGDocumentChanged(SVG_DOCUMENT_CLASS *doc, HTML_Element *parent, HTML_Element *child, BOOL is_addition) = 0;

	/**
	 * This function must be called when styles change on an SVG element. This can for instance be caused by
	 * applying new pseudo classes as when hovering the element.
	 *
	 * @param doc The document where the SVG resides.
	 * @param element The element that have had new CSS styles applied.
	 * @param changes Change flags generated during property loading. SVG has its special PROPS_CHANGED_SVG_* namespace.
	 *
	 * @return OpStatus::OK if there was no problem, OpStatus::ERR_NO_MEMORY if an OOM situation was encountered.
	 */
    virtual OP_STATUS		HandleStyleChange(SVG_DOCUMENT_CLASS *doc, HTML_Element* element, unsigned int changes) = 0;

	/**
	 * This function must be called when a SVG or XLink attribute changes on a SVG element.
	 *
	 * @param doc The document where the SVG resides.
	 * @param element The element whose attribute has changed.
	 * @param attr The attribute that has changed. It's exact type depends on the namespace. See ns.
	 * @param ns The namespace of the attribute in attr. Must be NS_SVG or NS_XLINK.
	 * @param was_removed TRUE if the attribute was removed (and thus cannot be read on the element). FALSE if it was added or modified.
	 *
	 * @return OpStatus::OK if there was no problem, OpStatus::ERR_NO_MEMORY if an OOM situation was encountered.
	 */
	virtual OP_STATUS		HandleSVGAttributeChange(SVG_DOCUMENT_CLASS *doc, HTML_Element* element, Markup::AttrType attr, NS_Type ns, BOOL was_removed) = 0;

	/**
	 * This function must be called when text in a SVG document changes.
	 *
	 * @param doc The document where the SVG resides.
	 * @param element The element
	 *
	 * @return OpStatus::OK if there was no problem, OpStatus::ERR_NO_MEMORY if an OOM situation was encountered.
	 */
	virtual OP_STATUS		HandleCharacterDataChanged(SVG_DOCUMENT_CLASS *doc, HTML_Element* element) = 0;

	/**
	 * This function must be called when an inline object inside an SVG document is ignored.
	 * Inline objects include for example raster images and foreignObject elements.
	 *
	 * @param doc The document where the SVG resides.
	 * @param element The element corresponding to the ignored inline (resource)
	 *
	 * @return OpStatus::OK if there was no problem, OpStatus::ERR_NO_MEMORY if an OOM situation was encountered.
	 */
	virtual OP_STATUS		HandleInlineIgnored(SVG_DOCUMENT_CLASS* doc, HTML_Element* element) = 0;
	/**
	 * This function must be called when an inline object inside an SVG document changes.
	 * Inline objects include for example raster images and foreignObject elements.
	 *
	 * @param doc The document where the SVG resides.
	 * @param element The changed element
	 * @param is_content_changed Content changed flag, used to decide for example if shadow trees need to be recreated
	 *
	 * @return OpStatus::OK if there was no problem, OpStatus::ERR_NO_MEMORY if an OOM situation was encountered.
	 */
	virtual OP_STATUS		HandleInlineChanged(SVG_DOCUMENT_CLASS* doc, HTML_Element* element, BOOL is_content_changed=FALSE) = 0;

	/**
	 * This function repaints the given element if it's inside an svg document fragment. Only the area which the
	 * element was last painted to is repainted. This can be used for e.g text selections where nothing changed
	 * except the selection, and the painted area is unaffected.
	 *
	 * @param doc The document where the SVG resides
	 * @param element The element to repaint
	 *
	 * @return OpStatus::OK if there was no problem, OpStatus::ERR_NO_MEMORY if an OOM situation was encountered.
	 */
	virtual OP_STATUS		RepaintElement(SVG_DOCUMENT_CLASS* doc, HTML_Element* element) = 0;

	/**
	 * Informs the svg module that a font is about to be deleted so
	 * that data related to it can be removed from internal caches.
	 *
	 * @param font_about_to_be_deleted The OpFont that is going to be deleted
	 */
	virtual void			HandleFontDeletion(OpFont* font_about_to_be_deleted) = 0;

	/**
	 * Returns the element that has the event handlers for a svg node. This is normally the node
	 * itself, except when having use tags. This method never returns NULL.
	 *
	 * @param element The traversed node that an event should pass. Must not be null.
	 * @return The event target element
	 */
	virtual HTML_Element* GetEventTarget(HTML_Element* element) = 0;

	/**
	 * This calculates the effective cursor for an element and should only
	 * be called when the HasCursor flag is set on the HTML_Element.
	 *
	 * @param elm The element that may have a cursor set
	 * @return The cursortype to use
	 */
	virtual CursorType GetCursorForElement(HTML_Element* elm) = 0;

	/**
	 * Get an URL from a xlink::href attribute
	 *
	 * @param elm The element to extract the url from
	 * @param root_url The document's URL. Needed for correct resolving of relative URLs.
	 * @return A pointer to the URL. NULL if no URL was found.
	 */
	virtual URL* GetXLinkURL(HTML_Element* elm, URL* root_url = NULL) = 0;

	/**
	 * Check if the given svg element is visible. The element must have been layouted
	 * and must have a trustworthy/non-dirty non-empty screen boundingbox to be considered
	 * visible.
	 *
	 * @param element The element to check
	 * @return TRUE if the element is visible, FALSE otherwise
	 */
	virtual BOOL IsVisible(HTML_Element* element) = 0;

	/**
	 * Checks whether the element is one of the text class elements
	 *
	 * @param element The element to be checked, must not be NULL.
	 * @return TRUE if the element is a text content element, FALSE otherwise.
	 */
	virtual BOOL IsTextContentElement(HTML_Element* element) = 0;

	/**
	 * Given a LogicalDocument and a svg:svg node in that document,
	 * this returns the SVGImage for that SVG or NULL if it wasn't a
	 * real SVG image.
	 *
	 * @param log_doc The LogicalDocument to look in
	 * @param element The root svg element
	 * @return The corresponding SVGImage, or NULL
	 */
	virtual SVGImage*		GetSVGImage(LogicalDocument* log_doc,
										HTML_Element* element) = 0;

	/**
	 * Check if the SVGManager is currently selecting text.
	 *
	 * @return TRUE if selecting text, FALSE otherwise
	 */
	virtual BOOL			IsInTextSelectionMode() const = 0;

	/**
	 * If the element is contained in an SVG fragment with a non-empty
	 * text selection, clear out the selection.
	 *
	 * @param element The element, assumed to be non-NULL and in the SVG namespace.
	 */
	virtual void			ClearTextSelection(HTML_Element* element) = 0;

	/**
	 * If the element is editable and has non-empty text selection, deletes the selected content.
	 *
	 * @param element The element, assumed to be non-NULL and in the SVG namespace.
	 */
	virtual void			RemoveSelectedContent(HTML_Element* element) = 0;

	/**
	 * If the element under the cursor is editable, shows the caret at cursor position.
	 * This should be used if the caret should be shown without focusing the element
	 * (focusing the element would cause the caret to show).
	 *
	 * @param doc The document where the SVG resides
	 * @param element The element, assumed to be non-NULL and in the SVG namespace.
	 */
	virtual void			ShowCaret(SVG_DOCUMENT_CLASS* doc, HTML_Element* element) = 0;

	/**
	 * Hides the editable element's caret.
	 *
	 * @param doc The document where the SVG resides
	 * @param element The element containing the caret, assumed to be non-NULL and in the SVG namespace.
	 */
	virtual void			HideCaret(SVG_DOCUMENT_CLASS* doc, HTML_Element* element) = 0;

	/**
	 * If the element is editable inserts the given text at the caret position.
	 *
	 * @param element the element, assumed to be non-NULL and in the SVG namespace.
	 * @param text The text to be inserted.
	 */
	virtual void			InsertTextAtCaret(HTML_Element* element, const uni_char* text) = 0;

	/**
	 * Checks if given point (in document coordinates) is over a text selection.
	 *
	 * @param doc The document the element with selection is within.
	 * @param elm The element containing the selection.
	 * @param doc_point - The point to be checked if it's over the selection.
	 */
	virtual BOOL			IsWithinSelection(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm, const OpPoint& doc_point) = 0;

	/**
	 * If the element is contained in an SVG fragment with a non-empty
	 * text selection, assign 'length' the character length of that
	 * text selection and return TRUE.
	 *
	 * @param element The element, assumed to be non-NULL and in the SVG namespace.
	 * @param length The result length, altered iff the method succeeds.
	 * @return TRUE if there is a valid text selection, FALSE otherwise
	 */
	virtual BOOL			GetTextSelectionLength(HTML_Element* element, unsigned& length) = 0;

	/**
	 * If the element is contained in an SVG fragment with a non-empty
	 * text selection, copy the text selection to 'buffer' and return
	 * TRUE.
	 *
	 * @param element The element, assumed to be non-NULL and in the SVG namespace.
	 * @param buffer The result buffer, altered iff the method succeeds.
	 * @return TRUE if there is a valid text selection and copying of
	 * the selected text to 'buffer' is successful, FALSE otherwise.
	 */
	virtual BOOL			GetTextSelection(HTML_Element* element, TempBuffer& buffer) = 0;

	/**
	 * If the element is contained in an SVG fragment with a non-empty
	 * text selection, retrieves a list of rectangles enclosing the selection.
	 *
	 * @param element The element, assumed to be non-NULL and in the SVG namespace.
	 * @param list A list of rectangles to be filled in (rectangles are in document coordinates).
	 */
	virtual void			GetSelectionRects(SVG_DOCUMENT_CLASS* doc, HTML_Element* element, OpVector<OpRect>& list) = 0;

	/**
	 * Returns a tooltip for a node in the string, or leaves it empty
	 * if there is none suitable. This is basically the textcontent of
	 * any existing title element.
	 *
	 * @param elm The element to get a svg tooltip for.
	 * @param result The string object to insert the tooltip into.
	 * @return OpStatus::OK normally, or OpStatus::ERR_NO_MEMORY if OOM.
	 */
	virtual OP_STATUS		GetToolTip(HTML_Element* elm, OpString& result) = 0;

	/**
	 * Returns TRUE if the given element is focusable.
	 * To be used for tabbing among elements inside an SVG.
	 *
	 * Does not consider the visibility of the element (e.g. display:none)
	 *
	 * @param doc The SVG_DOCUMENT_CLASS that contains the element
	 * @param elm The element to check
	 * @return TRUE if the element is focusable, FALSE if not.
	 */
	virtual BOOL			IsFocusableElement(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm) = 0;

	/**
	 * Returns TRUE if the given element is editable.
	 *
	 * Does not consider the visibility of the element (e.g. display:none)
	 *
	 * @param doc The SVG_DOCUMENT_CLASS that contains the element
	 * @param elm The element to check
	 * @return TRUE if the element is editable, FALSE if not.
	 */
	virtual BOOL			IsEditableElement(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm) = 0;

	/**
	 * Enter editing mode for the specified element.
	 *
	 * @param doc The SVG_DOCUMENT_CLASS that contains the element
	 * @param elm Root element for the editing operation
	 */
	virtual void			BeginEditing(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm, FOCUS_REASON reason) = 0;

	/**
	 * Get navigation data for element.
	 * The navigation rectangle is relative to SVG root element's content box.
	 *
	 * @param elm Element to query for navigational data.
	 * @param elm_box If successful, contains a rect describing the extents of the element.
	 * @return OpStatus::OK if navigational data could be acquired.
	 */
	virtual OP_STATUS		GetNavigationData(HTML_Element* elm, OpRect& elm_box) = 0;

	/**
	 * Get element's rectangle. The rectangle is in document coordinates.
	 *
	 * @param elm Element to query for the rectangle.
	 * @param elm_box If successful, contains a rect describing the extents of the element.
	 * @return OpStatus::OK if the rectangle could be acquired.
	 */
	virtual OP_STATUS		GetElementRect(HTML_Element* elm, OpRect& elm_box) = 0;

	/**
	 * Query element for navigational target (AKA. userspecified navigation/nav-*).
	 *
	 * @param elm Element to query.
	 * @param direction Direction of interrest in degrees (0=right,90=up, etc).
	 * @param nway Number of directions to consider (2, 4, etc).
	 * @param preferred_next_elm If TRUE is returned, contains target element.
	 * @return TRUE if a specified target (in the direction) was found, otherwise FALSE
	 */
	virtual BOOL			HasNavTargetInDirection(HTML_Element* elm, int direction, int nway, HTML_Element*& preferred_next_elm) = 0;

	/**
	 * Get an iterator that will iterate through the navigatable element in a SVG document.
	 *
	 * When done with the iterator, release it with the ReleaseIterator() method.
	 *
	 * @param elm Element where iteration will start.
	 * @param search_area Document area to consider when looking for navigatable elements. If NULL, all elements are considered.
	 * @param layout_props LayoutProperties corresponding to elm, or NULL if none available.
	 * @param nav_iterator The returned iterator.
	 * @return OpStatus::OK if successful.
	 */
	virtual OP_STATUS		GetNavigationIterator(HTML_Element* elm, const OpRect* search_area, LayoutProperties* layout_props, SVGTreeIterator** nav_iterator) = 0;

#ifdef SEARCH_MATCHES_ALL
	/**
	 * Get an iterator that will iterate through the highlightable elements in a SVG document.
	 *
	 * When done with the iterator, release it with the ReleaseIterator() method.
	 *
	 * @param elm Element where iteration will start.
	 * @param layout_props LayoutProperties corresponding to elm, or NULL if none available.
	 * @param first_hit The first selection element
	 * @param iterator The returned iterator.
	 * @return OpStatus::OK if successful.
	 */
	virtual OP_STATUS		GetHighlightUpdateIterator(HTML_Element* elm, LayoutProperties* layout_props, SelectionElm* first_hit, SVGTreeIterator** iterator) = 0;
#endif // SEARCH_MATCHES_ALL

#ifdef RESERVED_REGIONS
	/**
	 * Get an iterator that will iterate through the elements of an SVG document
	 * that define reserved regions. A reserved region is an area of the document
	 * where the document reacts to a subset of DOM events. This subset is defined
	 * by tweaks, but is usually restricted to touch events.
	 *
	 * This iterator is used by layout to capture the bounding boxes of these
	 * elements on requests from platforms that would like to know which input
	 * events they need to send to document, and which they can handle without
	 * synchronizing with Core.
	 *
	 * When done with the iterator, release it with the ReleaseIterator() method.
	 *
	 * @param elm Element where iteration will start.
	 * @param search_area Area to consider when looking for navigatable elements,
	 * in document coordinates. If NULL, all elements are considered.
	 * @param region_iterator The returned iterator.
	 * @return OpStatus::OK if successful.
	 */
	virtual OP_STATUS		GetReservedRegionIterator(HTML_Element* elm, const OpRect* search_area, SVGTreeIterator** region_iterator) = 0;
#endif // RESERVED_REGIONS

	/**
	 * Release a SVGTreeIterator.
	 *
	 * @param iter Iterator to release.
	 */
	virtual void			ReleaseIterator(SVGTreeIterator* iter) = 0;

	/**
	 * Scroll to the given rect.
	 *
	 * @param rect The rect to scroll to
	 * @param align How to align the rect
	 * @param scroll_to The element that is being scrolled to
	 */
	virtual void			ScrollToRect(OpRect rect, SCROLL_ALIGN align, HTML_Element *scroll_to) = 0;

	/**
	 * Instruct the SVG module to update the highlight information for an element.
	 *
	 * @param vd VisualDevice where updating will take place.
	 * @param elm Element to update.
	 * @param in_paint Specify whether to paint (TRUE) or invalidate (FALSE) the highlight.
	 */
	virtual void			UpdateHighlight(VisualDevice* vd, HTML_Element* elm, BOOL in_paint) = 0;

	/**
	 * Handle event finished, also see FramesDocument::HandleEventFinished.
	 *
	 * @param event The event that finished
	 * @param target The target element
	 */
	virtual void			HandleEventFinished(DOM_EventType event, HTML_Element *target) = 0;

	/**
	 * Returns TRUE if the given element is allowed to create a frame (see FramesDocument::GetNewIFrame).
	 * For elements that aren't in the svg namespace this method will return FALSE.
	 *
	 * @param elm The element to check
	 * @return TRUE if a frame is allowed to be created for the given element, FALSE if not
	 */
	virtual BOOL			AllowFrameForElement(HTML_Element* elm) = 0;

	/**
	 * Returns TRUE if the given element may receive events (see for instance HTML_Document::MouseAction and MouseListener::GetMouseHitView).
	 * For elements that aren't in the svg namespace this method will return FALSE.
	 *
	 * @param elm The element to check
	 * @return TRUE if a the element may receive events, FALSE if not
	 */
	virtual BOOL			AllowInteractionWithElement(HTML_Element* elm) = 0;

	/**
	 * Checks if scripting should be enabled for the given document.
	 * This is meant to be used to prevent script execution on svg and any embedded content inside
	 * an svg when the svg is referenced from an html:img element.
	 *
	 * @param doc The SVG_DOCUMENT_CLASS to check
	 * @return TRUE if scripting should be enabled, FALSE if not
	 */
	virtual BOOL			IsEcmaScriptEnabled(SVG_DOCUMENT_CLASS* doc) = 0;

	/**
	 * Gets the string from the SVG class attribute.
	 *
	 * @param elm The element to read from
	 * @param baseVal If TRUE then the base value will be returned, otherwise the animated value will be returned
	 * @return The ClassAttribute representation of the class attribute
	 */
	virtual const ClassAttribute* GetClassAttribute(HTML_Element* elm, BOOL baseVal) = 0;

	/**
	 * This is a hook from HLDocProfile::InsertElement.
	 *
	 * @param elm The element that was inserted
	 * @return The status
	 */
	virtual OP_STATUS InsertElement(HTML_Element* elm) = 0;

	/**
	 * This is a hook from HLDocProfile::EndElement.
	 *
	 * @param elm The element that was ended
	 * @return The status
	 */
	virtual OP_STATUS EndElement(HTML_Element* elm) = 0;

	/**
	 * This applies an svg filter chain using the given filter input image generator which must be implemented
	 * by the caller, see svg_filters.h. Requires VegaOpPainter.
	 *
	 * @param ref The SVGURLReference from the CSS cascade (or otherwise) that points to a <filter> element
	 * @param doc The FramesDocument that wants to resolve the URL reference, may be the same as the document that contains the <filter> element, but need not be.
	 * @param generator The input image generator, memory is owned by the caller
	 * @param screen_targetbbox The boundingbox in screen units of the content to filter
	 * @param painter The painter that will be used to apply the filter effect
	 * @param affected_area The area that the painter touched, in screen units
	 * @return The status
	 */
	virtual OP_STATUS		ApplyFilter(SVGURLReference ref, SVG_DOCUMENT_CLASS* doc,
										SVGFilterInputImageGenerator* generator,
										const OpRect& screen_targetbbox,
										OpPainter* painter, OpRect& affected_area) = 0;

	/**
	  * Creates the OpFont that corresponds to the given OpFontInfo.
	  *
	  * @param doc The SVG_DOCUMENT_CLASS that contains the element
	  * @param out_opfont A newly allocated OpFont that corresponded to the OpFontInfo
	  * @return OpStatus::ERR_NO_MEMORY for OOM, OpStatus::ERR_NULL_POINTER if any of the parameters were NULL,
	  *         OpStatus::ERR if other errors such as wrong type of element or parsing errors.
	  *         OpStatus::OK if successful. The OpFontInfo is only set if the return value was successful.
	  */
	 virtual OP_STATUS GetOpFont(HTML_Element* fontelm, UINT32 size, OpFont** out_opfont) = 0;

	 /**
	  * Gets the OpFontInfo that corresponds to the given SVGFont.
	  *
	  * @param doc The SVG_DOCUMENT_CLASS that contains the element
	  * @param font_element The element that is the root of the SVGFont, must be of type Markup::SVGE_FONT or Markup::SVGE_FONT_FACE
	  * @param out_fontinfo A newly allocated OpFontInfo that describes the font, the caller of the method owns this memory
	  * @return OpStatus::ERR_NO_MEMORY for OOM, OpStatus::ERR_NULL_POINTER if any of the parameters were NULL,
	  *         OpStatus::ERR if other errors such as wrong type of element or parsing errors.
	  *         OpStatus::OK if successful. The OpFontInfo is only set if the return value was successful.
	  */
	 virtual OP_STATUS GetOpFontInfo(HTML_Element* font_element, OpFontInfo** out_fontinfo) = 0;

	 /**
	  * Draw a string to the VisualDevice.
	  */
	 virtual OP_STATUS DrawString(VisualDevice* vis_dev, OpFont* font, int x, int y, const uni_char* txt, int len, int char_spacing_extra = 0) = 0;

#ifdef SVG_GET_PRESENTATIONAL_ATTRIBUTES
	 /**
	  * Get the equivalent CSS properties for the presentational attributes
	  * of an SVG element.
	  *
	  * @param [in] elm The SVG element to to get attributes for. May not be NULL, and must
	  *                 be in the SVG namespace.
	  * @param [in] hld_profile The HLDocProfile for the document the element belongs to.
	  * @param [out] list CSS declarations will be stored here. May not be NULL.
	  * @return OpStatus::OK on success, or OpStatus::ERR_NO_MEMORY.
	  */
	 virtual OP_STATUS GetPresentationalAttributes(HTML_Element* elm, HLDocProfile* hld_profile, CSS_property_list* list) = 0;
#endif // SVG_GET_PRESENTATIONAL_ATTRIBUTES

	/**
	 * Switch the LogicalDocument associated with an SVG root's SVGDocumentContext.
	 *
	 * If the SVGDocumentContext of @c root is already associated with the
	 * specified LogicalDocument, nothing happens.
	 *
	 * @param logdoc The LogicalDocument to switch to. May not be NULL.
	 * @param root The root of the SVG fragment. May not be NULL.
	 */
	 virtual void SwitchDocument(LogicalDocument* logdoc, HTML_Element* root) = 0;

#ifdef USE_OP_CLIPBOARD
	/**
	 * If the element is a SVG editable element it should paste the contents
	 * of the clipboard at the current caret position.
	 *
	 * @param element The element, assumed to be non-NULL and in the SVG namespace.
	 * @param clipboard The instance of the clipboard the data from should be pasted.
	 */
	virtual void Paste(HTML_Element* elm, OpClipboard* clipboard) = 0;

	/**
	 * If the element is a SVG editable element and has a non-empty selection it
	 * should place the selected content in the clipboard and then remove it from
	 * itself.
	 *
	 * @param element The element, assumed to be non-NULL and in the SVG namespace.
	 * @param clipboard The instance of the clipboard the data from should be placed in.
	 */
	virtual void Cut(HTML_Element* elm, OpClipboard* clipboard) = 0;

	/**
	 * If the element is a SVG element and has a non-empty selection it
	 * should place the selected content in the clipboard. This operation must not
	 * affect the selection in any way.
	 *
	 * @param element The element, assumed to be non-NULL and in the SVG namespace.
	 * @param clipboard The instance of the clipboard the data from should be placed in.
	 */
	virtual void Copy(HTML_Element* elm, OpClipboard* clipboard) = 0;

	/**
	 * If the element is a SVG editable element it should be informed about the end
	 * of the clipboard operation.
	 *
	 * @param element The element, assumed to be non-NULL and in the SVG namespace.
	 */
	virtual void ClipboardOperationEnd(HTML_Element* elm) = 0;
#endif // USE_OP_CLIPBOARD
};

#endif // SVG_SUPPORT
#endif // _SVG_MANAGER_
