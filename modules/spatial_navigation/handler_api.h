/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 */

/**
 * @file handler_api.h
 *
 * This file contains the handler API.
 *
 * @author h2@opera.com
*/

#ifndef HANDLER_API_H
#define HANDLER_API_H

#include "modules/spatial_navigation/sn_filter.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"


/** Enum that describes the type of a spatial navigation element.
 *
 * These types can be combined using bitwise and to indicate the complete
 * type of an element.
 *
 */
enum SnElementType
{
	ELEMENT_TYPE_NONE            = 0x00000000,
	ELEMENT_TYPE_LINK            = 0x00000001,
	ELEMENT_TYPE_JS              = 0x00000002,
	ELEMENT_TYPE_FORM            = 0x00000004,
	ELEMENT_TYPE_HEADING         = 0x00000008,
	ELEMENT_TYPE_PARAGRAPH       = 0x00000010,
	ELEMENT_TYPE_LINE            = 0x00000020,
	ELEMENT_TYPE_WORD            = 0x00000040,
	ELEMENT_TYPE_IMAGE           = 0x00000080,
	ELEMENT_TYPE_OBJECT          = 0x00000100,
	ELEMENT_TYPE_CONTAINER       = 0x00000200,
	ELEMENT_TYPE_SELECT          = 0x00000400,
	ELEMENT_TYPE_TEXTAREA        = 0x00000800,
	ELEMENT_TYPE_TEXT            = 0x00001000,
	ELEMENT_TYPE_PASSWORD        = 0x00002000,
	ELEMENT_TYPE_CHECKBOX        = 0x00004000,
	ELEMENT_TYPE_RADIO           = 0x00008000,
	ELEMENT_TYPE_SUBMIT          = 0x00010000,
	ELEMENT_TYPE_RESET           = 0x00020000,
	ELEMENT_TYPE_FILE            = 0x00040000,
	ELEMENT_TYPE_INPUT_IMAGE     = 0x00080000,
	ELEMENT_TYPE_BUTTON          = 0x00100000,
	ELEMENT_TYPE_INPUT_BUTTON    = 0x00200000,
	ELEMENT_TYPE_TABBABLE        = 0x00400000,
#ifdef WEBFORMS2_SUPPORT
	ELEMENT_TYPE_EMAIL           = 0x00800000,
	ELEMENT_TYPE_URI             = 0x01000000,
	ELEMENT_TYPE_NUMBER          = 0x02000000,
	ELEMENT_TYPE_RANGE           = 0x04000000,
	ELEMENT_TYPE_DATE            = 0x08000000,
	ELEMENT_TYPE_TIME            = 0x10000000,
	ELEMENT_TYPE_SEARCH          = 0x20000000,
	ELEMENT_TYPE_TEL             = 0x40000000,
	ELEMENT_TYPE_COLOR           = 0x80000000,
#endif  // WEBFORMS2_SUPPORT
};

class DocListElm;
class FramesDocument;
class OpInputAction;

class OpSnHandler
{
public:
	enum NavigationMode
	{
		NavigateDefault		= 0x01,
		NavigateHeaders		= 0x02,
		NavigateParagraphs	= 0x04,
		NavigateForms		= 0x08,
		NavigateLinks		= 0x10,
		NavigateImages		= 0x20,
		NavigateNonLinks	= 0x40
	};

	/** Destructor.
	 *
	 */
	virtual ~OpSnHandler() {}

	/** Initialize the handler.
	 * @param win  The window the handler belongs to
	 */
	virtual OP_STATUS		Init(Window* win) = 0;

	/** Handle the supplied OpInputAction
	 *   @param action  the action to handle
	 *	 @return TRUE if the action was handled, FALSE otherwise
	 */
	virtual BOOL			OnInputAction(OpInputAction* action) = 0;

	/** Mark the next heading in the choosen direction.
	 *  @param direction direction to navigate.  One of DIR_UP, DIR_DOWN, DIR_LEFT or DIR_RIGHT
	 *  @param startingPoint  point to navigate from.  If NULL navigation will
	 *               be from current navigation element (NavigationElement in HTML_Document)
	 *  @param scroll    If TRUE will scroll if unable to find an element to
	 *                   navigate to.
	 *  @return TRUE if able to find an element to navigate to or was able to
	 *          scroll (if scroll parameter is TRUE).  FALSE if unable to find
	 *          an element, or upon error.
	 */
	virtual BOOL			MarkNextHeadingInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE) = 0;

	/** Mark the next paragraph in the choosen direction.
	 *  @param direction direction to navigate.  One of DIR_UP, DIR_DOWN, DIR_LEFT or DIR_RIGHT
	 *  @param startingPoint  point to navigate from.  If NULL navigation will
	 *               be from current navigation element (NavigationElement in HTML_Document)
	 *  @param scroll    If TRUE will scroll if unable to find an element to
	 *                   navigate to.
	 *  @return TRUE if able to find an element to navigate to or was able to
	 *          scroll (if scroll parameter is TRUE).  FALSE if unable to find
	 *          an element, or upon error.
	 */
	virtual BOOL			MarkNextParagraphInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE) = 0;

	/** Mark the next non link in the choosen direction.
	 *  @param direction direction to navigate.  One of DIR_UP, DIR_DOWN, DIR_LEFT or DIR_RIGHT
	 *  @param startingPoint  point to navigate from.  If NULL navigation will
	 *               be from current navigation element (NavigationElement in HTML_Document)
	 *  @param scroll    If TRUE will scroll if unable to find an element to
	 *                   navigate to.
	 *  @return TRUE if able to find an element to navigate to or was able to
	 *          scroll (if scroll parameter is TRUE).  FALSE if unable to find
	 *          an element, or upon error.
	 */
	virtual BOOL			MarkNextNonLinkInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE) = 0;

	/** Mark the next link, form or element with js handler in the choosen direction.
	 *  @param direction direction to navigate.  One of DIR_UP, DIR_DOWN, DIR_LEFT or DIR_RIGHT
	 *  @param startingPoint  point to navigate from.  If NULL navigation will
	 *               be from current navigation element (NavigationElement in HTML_Document)
	 *  @param scroll    If TRUE will scroll if unable to find an element to
	 *                   navigate to.
	 *  @return TRUE if able to find an element to navigate to or was able to
	 *          scroll (if scroll parameter is TRUE).  FALSE if unable to find
	 *          an element, or upon error.
	 */
	virtual BOOL			MarkNextLinkInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE) = 0;

	/** Mark the next form object in the choosen direction.
	 *  @param direction direction to navigate.  One of DIR_UP, DIR_DOWN, DIR_LEFT or DIR_RIGHT
	 *  @param startingPoint  point to navigate from.  If NULL navigation will
	 *               be from current navigation element (NavigationElement in HTML_Document)
	 *  @param scroll    If TRUE will scroll if unable to find an element to
	 *                   navigate to.
	 *  @return TRUE if able to find an element to navigate to or was able to
	 *          scroll (if scroll parameter is TRUE).  FALSE if unable to find
	 *          an element, or upon error.
	 */
	virtual BOOL			MarkNextFormInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE) = 0;

	/** Mark the next link (anchor) in the choosen direction.
	 *  @param direction direction to navigate.  One of DIR_UP, DIR_DOWN, DIR_LEFT or DIR_RIGHT
	 *  @param startingPoint  point to navigate from.  If NULL navigation will
	 *               be from current navigation element (NavigationElement in HTML_Document)
	 *  @param scroll    If TRUE will scroll if unable to find an element to
	 *                   navigate to.
	 *  @return TRUE if able to find an element to navigate to or was able to
	 *          scroll (if scroll parameter is TRUE).  FALSE if unable to find
	 *          an element, or upon error.
	 */
	virtual BOOL			MarkNextATagInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE) = 0;

	/** Mark the next image in the choosen direction.
	 *  @param direction direction to navigate.  One of DIR_UP, DIR_DOWN, DIR_LEFT or DIR_RIGHT
	 *  @param startingPoint  point to navigate from.  If NULL navigation will
	 *               be from current navigation element (NavigationElement in HTML_Document)
	 *  @param scroll    If TRUE will scroll if unable to find an element to
	 *                   navigate to.
	 *  @return TRUE if able to find an element to navigate to or was able to
	 *          scroll (if scroll parameter is TRUE).  FALSE if unable to find
	 *          an element, or upon error.
	 */
	virtual BOOL			MarkNextImageInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE) = 0;

	/** Mark the next item in the choosen direction.
	 *
	 *  Which items are included are decided by the navigation filter, which can be set with
	 *  SetNavigationFilter(). If no filter is set, a default filter is used.
	 *
	 *  The navigaion mode can also be set with SetNavigationMode, in this case the sn_hanlder will
	 *  take care of filter creation/destruction for you.
	 *
	 *  @param direction direction to navigate.  One of DIR_UP, DIR_DOWN, DIR_LEFT or DIR_RIGHT
	 *  @param startingPoint  point to navigate from.  If NULL navigation will
	 *               be from current navigation element (NavigationElement in HTML_Document)
	 *  @param scroll    If TRUE will scroll if unable to find an element to
	 *                   navigate to.
	 *  @return TRUE if able to find an element to navigate to or was able to
	 *          scroll (if scroll parameter is TRUE).  FALSE if unable to find
	 *          an element, or upon error.
	 */
	virtual BOOL            MarkNextItemInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE, INT32 scrollValue = 0) = 0;

	/** Called when a document has been destructed so that spatnav can remove
	 *  any pointers it has to it.
	 * @param doc  The document which has been destructed
	 */
	virtual void			DocDestructed(HTML_Document* doc) = 0;

	/** Called when a document has been destructed so that spatnav can remove
	 *  any pointers it has to it.
	 * @param doc  The document which has been destructed
	 */
	virtual void			DocDestructed(FramesDocument* doc) = 0;

	/** An HTML_Element was destroyed, let's see if it affects us
	 * @param element the element being destroyed
	 */
	virtual void			ElementDestroyed(HTML_Element* element) = 0;

	/** A HTML_Element was focused or highlighted not by the spatial navigation.
	 * In case of both focus and highlight taking place, it is enough to call
	 * the method just once, however calling it multiple times won't do any bad.
	 * @param doc The HTML_Document of the element.
	 * @param element The focused or highlighted (navigated) element.
	 */
	virtual void			ElementNavigated(HTML_Document* doc, HTML_Element* element) = 0;

	/** Highlight the chosen element.  Just calls the relevant
	 *  functions in HTML_Document, so might as well call them directly
	 *  instead of going through spatnav.  For form elements it will also
	 *  focus (prefocus if FEATURE_SELECT_TO_FOCUS_INPUT is enabled) the
	 *  element.
	 * @param doc The HTML_Document the element belongs to
	 * @param helm The element to hightlight/focus
	 * @param showMouseOver If TRUE will send mouseover and mousemove events to
	 * 		  the new element, and mouseout to the previous element
	 * @param focus_document If TRUE, then will call SetFocus on the FramesDocument
	 *		  of the doc when performing other highlighting related operations.
	 *		  When FALSE, the method asserts that the FramesDocument of the doc is the
	 *		  currently active one.
	 */
	virtual void			Highlight(HTML_Document* doc, HTML_Element* helm, BOOL showMouseOver = TRUE, BOOL focusDocument = TRUE) = 0;

	/** Focus and highlight the chosen link
	 * @param doc The HTML_Document the element belongs to
	 * @param helm The element to hightlight
	 * @param showMouseOver If TRUE will send mouseover and mousemove events to
	 */
	virtual void			HighlightLink(HTML_Document* doc, HTML_Element* helm, BOOL showMouseOver = TRUE) = 0;

	/** Focus and highlight the chosen form
	 * @param doc The HTML_Document the element belongs to
	 * @param helm The element to hightlight
	 */
	virtual void			HighlightFormObject(HTML_Document* doc, HTML_Element* helm) = 0;

	/** Focus the currently selected form object
	 * @return TRUE if it actually did anything
	 */
	virtual BOOL            FocusCurrentForm() = 0;

	/** Unfocus the currently selected form object
	 * Puts the element back in the prefocus state if
	 * FEATURE_SELECT_TO_FOCUS_INPUT is enabled.
	 * @return TRUE if it actually did anything
	 */
	virtual BOOL            UnfocusCurrentForm() = 0;

	/** Check if a form element has focus
	 * @return always FALSE
	 * @todo maybe it should return something more useful?
	 */
	virtual BOOL            FormElementHasFocus() = 0;

	/**
	* Scrolling in specified direction with some amount to scroll.
	*
	* @param direction to scroll
	* @param amount of scroll (in pixels)
	* @return TRUE if we were able to scroll in specified direction.
	*		FALSE otherwise.
	*
	* @see ScrollDocumentInternalAmount()
	*/
	virtual BOOL	ScrollAmount(INT32 direction, INT32 amount) = 0;
	
	/**
	* Scrolling in specified direction with some amount to scroll.
	* Scrolling only currently navigated frame.
	*
	* @param direction to scroll
	* @param amount of scroll (in pixels)
	* @return TRUE if we were able to scroll in specified direction.
	*		 FALSE otherwise.
	*
	* @see PointerScrollAmountInternal()
	*/
	virtual BOOL	ScrollAmountCurrentFrame(INT32 direction, INT32 amount) = 0;

	/** Re-highlight the current link
	 * @param scroll Scroll if no navigatable element found
	 * @param isNavigation If TRUE method will send OnLinkNavigated callback to
	 * 		  windowmanager
	 */
	virtual OP_STATUS		HighlightCurrentElement(BOOL scroll = TRUE,
													BOOL isNavigation = TRUE) = 0;

	/** Does nothing
	 * @todo maybe do something if this function is still needed?
	 */
	virtual OP_STATUS		HighlightCurrentElementIfNeeded() = 0;

	/**
	 *  @return The image's URL, or empty URL if there is no current navigation
	 *  		element or it has no image.
	 */
	virtual URL				GetImageURL() = 0;

	/**
	 * @return A string of the href attribute of the current navigation element
	 * 		   or NULL if it has none.
	 */
	virtual const uni_char*	GetA_HRef() = 0;

	/**
	 * @return the document of the active frame
	 */
	virtual FramesDocument*		GetActiveFrame() = 0;

	/**
	 * @return the current navigation element, except for image maps it will
	 *  	   return the current area element instead of the image.
	 */
	virtual HTML_Element*	GetActiveLink() = 0;

	/**
	 * @return the type of the active link.
	 */
	virtual SnElementType GetActiveLinkType() = 0;

	/** Sets the current navigation element.  Basicly just does
	 *  HTML_Document::SetNavigationElement().
	 * @param activeLink the element to set as current navigation element.
	 */
	virtual void			SetActiveLink(HTML_Element* activeLink) = 0;

	/** Set the navigation mode, the handler will create/destroy appropriate
	 *  navigation filters for you.
	 *
	 *  Calling this method with no parameter will make the handler use the default mode
	 */
	virtual void SetNavigationMode(int mode = NavigateDefault) = 0;

	/**
	 * @return the current navigation mode
	 */
	virtual int GetNavigationMode() = 0;

	/** Set the navigation filter used to determine which elements can be navigated to.
	 *
	 *  If change_focus: if the new filter accepts the current active element, focus is
	 *  kept on that element. If the new filter doesn't accept the current active element,
	 *  focus is changed to the next element. If parameter scroll is TRUE, document is
	 *  scrolled.
	 *
	 *  Calling this method with nav_filter = NULL will make the handler use the default filter.
	 */
	virtual void SetNavigationFilter(OpSnNavFilter *nav_filter, BOOL change_focus, BOOL scroll) = 0;

	/**
	 * @return the navigation filter
	 */
	virtual OpSnNavFilter* GetNavigationFilter() = 0;

	/** Called when the document is scrolled
     *
     */
	virtual void            OnScroll() = 0;

	/** Called when the document has finished loading
     *
     */
	virtual void            OnLoadingFinished() = 0;

	/** Set if we should highlight link on load or not
     *
     */
	virtual void			SetHighlightOnLoad(BOOL highlight) = 0;

	/** Enable low memory mode
	 *
	 *  After calling this method, spatial navigation uses a lot less memory.
	 *  It is still possible to navigate, but navigation will be strange at times.
     *
     */
	virtual void			SetLowMemoryMode(BOOL lowMemory) = 0;
};

#endif // HANDLER_API_H
