/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
**
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _SN_HANDLER_H_
#define _SN_HANDLER_H_

#ifdef _SPAT_NAV_SUPPORT_

#include "modules/spatial_navigation/sn_capabilities.h"
#include "modules/spatial_navigation/handler_api.h"
#include "modules/spatial_navigation/sn_algorithm.h"
#include "modules/spatial_navigation/navigation_api.h"

class Window;
class FramesDocElm;

/** This class contains data about a frame (or iframe).  Used when navigating
 *  between (i)frames.
 */
class FrameElement
	: public Link
{
public:
	FrameElement() : frmDoc(NULL), scrollable(NULL)
	{
		iframeRect.left = 0;
		iframeRect.right = 0;
		iframeRect.top = 0;
		iframeRect.bottom = 0;
	}
	~FrameElement() {}

	/** Initialise the new frame
	 *  @param doc    The frame we are entering
	 *  @param docElm The FramesDocElm belonging to this frame
	 *  @param iframe_position  The position of an iframe inside parent frame (only used for iframes)
	 */
	OP_STATUS		Init(FramesDocument* doc, FramesDocElm* docElm, RECT rect, HTML_Element* lastScrollable);

	/** @return Previous (i)frame.  I.e. a child of this frame which currently has
	 *          navigation focus itself or one of its children.  Will be NULL
	 *          if this frame itself is the current navigation frame. */
	FrameElement*	Pred() const { return (FrameElement*) Link::Pred(); }
	/** @return Next (i)frame.  I.e. the parent of this (i)frame.  NULL if 
	 *          this is the top frame */
	FrameElement*	Suc() const { return (FrameElement*) Link::Suc(); }

	/** @return	The FramesDocument belonging to this frame */
	FramesDocument*	GetFramesDocument() { return frmDoc; }
	/** @return	The FramesDocElm document belonging to this frame */
	FramesDocElm*	GetFrmDocElm() { return frmDocElm; }
	/** @return the pointer to the position/dimension of the current iframe within its parent
	 *          frame.  Only valid for iframes. */
	const RECT*		GetIframeRect() { return &iframeRect; }

	/** Sets the iframe rect for this frame. Should be used only for iframes.
	*/
	void			SetIframeRect(const RECT& rect)	{iframeRect = rect;}

	/** @return TRUE if this frame is an iframe and not a regular frame */
	BOOL			IsIFrame() { return iframeRect.right-iframeRect.left > 0; }

	/** @return The scrollable which was last active in the frame. A ScrollableContainer
	 *          or PagedContainer is associated with the element, unless it is NULL. */
	HTML_Element*	GetScrollable() { return scrollable; }

	/** Sets new scrollable element. May be NULL. If not NULL, this element should be
	 *  associated with a ScrollableContainer or PagedContainer. */
	void 			SetScrollable(HTML_Element* elm) { scrollable = elm; }
private:
	FramesDocument* frmDoc;
	FramesDocElm*	frmDocElm;
	RECT			iframeRect;
	HTML_Element*	scrollable;
};

/**
 * This class is used as a stack of frames (and iframes) we have entered, so that
 * we know where to continue navigation when exiting the current frame.
 * 
 * Limitation: It is only able to keep track of which Frames we have entered/exited
 * by spatial navigation, so if the navigation element has been moved to some
 * other Frame by a method other than spatnav, spatnav will become a little
 * confused.  This should be improved.
 * 
 * Possibly we can get rid of the entire FramePath, and only use the 
 * Frames-tree as it is in doc without keeping a separate stack here.
 * The only piece of information we're missing in doc is the RECT of iframes
 * used for adjusting coordinates.  For ordinary Frames we have all the info
 * we need in doc.
 */
class FramePath
	: public Head
{
public:
	FramePath() : m_store_point(NULL) {}
	~FramePath() { Clear(); }

	/// @return Last frame (i.e. top frame)
    FrameElement*	Last() const { return (FrameElement*)Head::Last(); }
    /// @return First frame (i.e. the frame we are currently navigating).
    FrameElement*	First() const { return (FrameElement*)Head::First(); }

	/** Called when navigating into a new frame
	 *  @param doc    The frame we are entering
	 *  @param docElm The FramesDocElm belonging to this frame
	 *  @param iframe_position  The position of an iframe inside parent frame (only used for iframes)
	 *  @param insert_last  Insert at last in frames stack, that is this frame is parent of every other frame on stack
	 */
	OP_STATUS AddFrameToPath(FramesDocument* doc, FramesDocElm* docElm, RECT iframe_position, HTML_Element* lastScrollable, BOOL insert_last=FALSE);
	/** Called when navigation out of a frame */
	void RemoveFrameFromPath();

	/**
	 * Sets a store point.
	 * If we later call RestorePreviousPath() then the FramePath will be restored
	 * to what it looked like when CreateStorePoint() was called.
	 */
	void CreateStorePoint() { m_store_point = First(); }

	BOOL IsOnStoredPoint() { OP_ASSERT(m_store_point); return First() == m_store_point; }

	/** If we have navigated out of the frame we started from, without finding anything
	 *  we want to restore the frame path to what it was, so that we can keep the active
	 *  element and the path we used to get there.
	 * 
	 *  This should be called every time we've navigating and failed to find another element
	 *  to navigate to 
	 * 
	 *  This or DiscardPreviousPath() should be called after each navigation.*/

	void RestorePreviousPath();

	/** This discard the previous path we stored while looking for another element
	 *  to navigate to.  If we find another element then we don't need the path anymore,
	 *  so this should be called whenever we do find another element by navigating.
	 * 
	 *  This or RestorePreviousPath() should be called after each navigation.
	 */
	void DiscardPreviousPath();

private:
	FrameElement*		m_store_point;   // The point in the frame path where current path and stored path separate
	AutoDeleteHead		m_stored_path;   // This is the path from store_point which was before we started changing the current
};


class SnHandler
  : public OpSnHandler
{
public:
	// The various results an attampt at navigating can have
	enum NavigationResult {
		NO_ACTION,     // unable to find anything nor scroll (if allowed to scroll)
		FOUND_ELEMENT, // found a navigatable element
		SCROLLED,      // scrolled
		SCROLL_OUTER,  // nothing found in visible part of an iframe, but it is partly out of view, so caller should scroll containing frame so more of iframe is visible
		NEW_FRAME,     // switched to next frame, or caller should switch to next frame
		NEW_SCROLLABLE, // entered a new ScrollableContainer or PagedContainer, that has no inner navigable elements visible
						// and can't be scrolled in nav direction (but perhaps can be in some other direction)
		NAVIGATION_ERROR,
		NAVIGATION_OOM
	};

	SnHandler() :
			window(NULL),
			currentDoc(NULL),
			activeFrameDoc(NULL),
			activeHTML_Doc(NULL),
			highlightOnLoad(TRUE),
			initial_highlight(FALSE),
			scrollX(0),
			scrollY(0),
			allowDocScrolling(FALSE),
			scrolledOnce(FALSE),
			lowMemoryMode(FALSE),
			nav_filter(NULL),
			default_nav_filter(NULL),
#ifdef SELECT_TO_FOCUS_INPUT
			currentForm(NULL),
			currentFormDoc(NULL),
#endif // SELECT_TO_FOCUS_INPUT
			heading_nav_filter(NULL),
			paragraph_nav_filter(NULL),
			nonlink_nav_filter(NULL),
			form_nav_filter(NULL),
			atag_nav_filter(NULL),
			image_nav_filter(NULL),
			configurable_nav_filter(NULL),
			nav_mode(NavigateDefault),
			currentFramePath(NULL),
			fourway(TRUE),
			activeScrollable(NULL),
			previous_link(NULL),
			previous_scrollable(NULL),
			navigate_from(NULL),
			lastElement(NULL)
		{
			allData.linkID = NULL;
			allData.frameID = NULL;
		}
	
	~SnHandler();

	OP_STATUS		Init(Window* win); 	///< @overload OpSnHandler
	BOOL			OnInputAction(OpInputAction* action);	///< @overload OpSnHandler
	BOOL			MarkNextHeadingInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE); 	///< @overload OpSnHandler
	BOOL			MarkNextParagraphInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE); 	///< @overload OpSnHandler
	BOOL			MarkNextNonLinkInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE);	///< @overload OpSnHandler
	BOOL			MarkNextLinkInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE);	///< @overload OpSnHandler
	BOOL			MarkNextFormInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE);	///< @overload OpSnHandler
	BOOL			MarkNextATagInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE);	///< @overload OpSnHandler
	BOOL			MarkNextImageInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE);	///< @overload OpSnHandler
	BOOL            MarkNextItemInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = TRUE, INT32 scrollValue = 0);	///< @overload OpSnHandler

	void			DocDestructed	(HTML_Document* doc);	///< @overload OpSnHandler
	void			DocDestructed	(FramesDocument* doc);	///< @overload OpSnHandler
	void			ElementDestroyed(HTML_Element* element);	///< @overload OpSnHandler
	void			ElementNavigated(HTML_Document* doc, HTML_Element* element);	///< @overload OpSnHandler
	void			Highlight(HTML_Document* doc, HTML_Element* helm, BOOL showMouseOver = TRUE, BOOL focusDocument = TRUE);  ///< @overload OpSnHandler
	void			HighlightLink(HTML_Document* doc, HTML_Element* helm, BOOL showMouseOver = TRUE);  ///< @overload OpSnHandler
	void			HighlightFormObject(HTML_Document* doc, HTML_Element* helm);   ///< @overload OpSnHandler
#ifdef _PLUGIN_NAVIGATION_SUPPORT_
	/**
	 * Calls method in plugin-code for handling navigation of plugins
	 * @param direction  Direction to navigate
	 * @return TRUE if it was able to navigate in plugin.
	 */
	BOOL            NavigateInsidePlugin(INT32 direction);
#endif

#ifdef MEDIA_SUPPORT
	/**
	 * Handle internal keyboard navigation in media controls
	 * @param direction Direction to navigate
	 * @param focus_gained TRUE if the called for a element that just gained
	 *					   focus, FALSE if the element already had focus.
	 * @return TRUE if the element could navigate internally, FALSE if the element
	 *		   wants to drop focus.
	 */
	BOOL			NavigateInsideMedia(INT32 direction, BOOL focus_gained);
#endif // MEDIA_SUPPORT

	BOOL            FocusCurrentForm();   ///< @overload OpSnHandler
	BOOL            UnfocusCurrentForm();   ///< @overload OpSnHandler
	BOOL			FormElementHasFocus() { return FALSE; }   ///< @overload OpSnHandler

	OP_STATUS		HighlightCurrentElement(BOOL scroll = TRUE, 
													BOOL isNavigation = TRUE);   ///< @overload OpSnHandler
	OP_STATUS		HighlightCurrentElementIfNeeded() { return OpStatus::OK; }   ///< @overload OpSnHandler

	URL				GetImageURL();   ///< @overload OpSnHandler
	const uni_char*	GetA_HRef();   ///< @overload OpSnHandler

	FramesDocument*	GetActiveFrame() { return activeFrameDoc; }   ///< @overload OpSnHandler
	HTML_Element*	GetActiveLink();   ///< @overload OpSnHandler
	
	SnElementType	GetActiveLinkType();   ///< @overload OpSnHandler
	void			SetActiveLink(HTML_Element* activeLink);   ///< @overload OpSnHandler

	void			SetNavigationMode(int mode = NavigateDefault);   ///< @overload OpSnHandler
	int				GetNavigationMode() { return nav_mode; }   ///< @overload OpSnHandler

	void            SetNavigationFilter(OpSnNavFilter *nav_filter, BOOL scroll, BOOL change_focus);   ///< @overload OpSnHandler
	OpSnNavFilter*  GetNavigationFilter() { return nav_filter; }   ///< @overload OpSnHandler

	void            OnScroll();   ///< @overload OpSnHandler
	void            OnLoadingFinished();   ///< @overload OpSnHandler
	void			SetHighlightOnLoad(BOOL highlight) { highlightOnLoad = highlight; }   ///< @overload OpSnHandler

	void			SetLowMemoryMode(BOOL lowMemory);   ///< @overload OpSnHandler

#ifdef SELECT_TO_FOCUS_INPUT
	/** Used for setting prefocus for Form elements.  For elements which
	 * doesn't steal input focus (like e.g. buttons) it just sets normal
	 * focus, while for elements which does steal focus (i.e. input fields)
	 * it just draws highlighting around the element, allowing it to be properly
	 * focused by a later action.
	 * @param doc the document the element belongs to
	 * @param helm the element to give (pre)focus
	 * @return TRUE if it was able to set (pre)focus
	 */
	BOOL			FocusElement(HTML_Document* doc, HTML_Element* helm);
#endif

	/**
	Interface function which calls doc->FocusElement.
	@param doc: current document
	@param helm: element to focus
	*/
	BOOL Doc_FocusElement(HTML_Document* doc, HTML_Element* helm);

	/**
	Interface function to doc module highlight.
	@param doc: current document
	@param helm: element to highlight
	Balance parameters are optional. See HTML_Document::HighlightElement for info on these parameters
	*/
	BOOL Doc_HighlightElement(HTML_Document* doc, HTML_Element* helm, BOOL send_event = TRUE, BOOL focusFormElm = FALSE, BOOL scroll = TRUE);

	/**
	* @overload OpSnHandler::ScrollAmount
	*/
	BOOL	ScrollAmount(INT32 direction, INT32 amount);
	
	/**
	* @overload OpSnHandler::ScrollAmountCurrentFrame
	*/
	BOOL	ScrollAmountCurrentFrame(INT32 direction, INT32 amount);
	
protected:
	/** Finds next navigatable element and sets it as current navigation element
	 *  If unable to find any element it will scroll if possible and allowed by
	 *  the scroll parameter.
	 *  @param direction direction to navigate.  One of DIR_UP, DIR_DOWN, DIR_LEFT or DIR_RIGHT
	 *  @param startingPoint  point to navigate from.  If NULL navigation will 
	 *               be from current navigation element (NavigationElement in HTML_Document)
	 *  @param scroll    If TRUE will scroll if unable to find an element to 
	 *                   navigate to.
	 *  @return TRUE if able to find an element to navigate to or was able to 
	 *          scroll (if scroll parameter is TRUE).  FALSE if unable to find
	 *          an element, or upon error
	 */
	BOOL	MarkNext(INT32 direction, POINT* startingPoint, BOOL scroll = TRUE, INT32 scrollValue = 0);

	/** Finds next navigatable element in the current frame and sets it as 
	 *  current navigation element
	 *  If unable to find any element it will scroll if possible and allowed by
	 *  the scroll parameter.
	 *  @param direction direction to navigate.  One of DIR_UP, DIR_DOWN, DIR_LEFT or DIR_RIGHT
	 *  @param startingPoint  point to navigate from.  If NULL navigation will 
	 *               be from current navigation element (NavigationElement in HTML_Document)
	 *  @return TRUE if able to find an element to navigate to or was able to 
	 *          scroll (if scroll parameter is TRUE).  FALSE if unable to find
	 *          an element, or upon error
	 */
	BOOL	MarkNextItemInActiveFrame(INT32 direction, POINT* startingPoint = NULL);

	BOOL	InitNavigation(INT32 direction, BOOL scroll, INT32 scrollValue);
	BOOL	GetNextLink(INT32 direction, POINT* startingPoint, BOOL scroll = TRUE);
	void	SetActiveLinkAndHighlight();

	/** Resets the variables containing info about which links, frames etc. if needed */
	void	UpdateNavigationData();

	void			OnLinkNavigated();
	
	/** Will scroll the frame in direction (if possible)
	 *  returns TRUE if able to scroll.  If frame is NULL then the current frame
	 * will be scrolled.
	 * Last param is an optional limit of scroll amount in pixels in document scale. Negative value does not
	 * affect the actual scroll value depending on SnHandler's setting. */
	BOOL			Scroll(INT32 direction, FrameElement* frame = NULL, INT32 limit = -1);
	void			SetScrollX(INT32 value) { scrollX = value; }
	void			SetScrollY(INT32 value) { scrollY = value; }
	void			ScrollToIFrame(SnFrameIterator* frames, HTML_Document* doc, BOOL refocus = FALSE);

	/** Fakes ONMOUSEOUT and ONMOUSELEAVE events on the previously
	 * navigated element.
	 * @param doc HTML_Document of the new navigated element.
	 * @param last_elm The last navigated element by spatnav. May be NULL.
	 *		  It is also used as a control flag. Should be NULL if called after some
	 *		  spatnav action. Must be valid if called from notification that something
	 *		  apart from spatnav has changed highlight or focus.
	 * @param navigated_elm The newly navigated/highlighted/focused element.
	 * @param[out] common_ancestor May be assigned a pointer to the common ancestor of
	 *				navigated_elm and previously hovered element.
	 */
	void			FakeMouseOutOnPreviousElement(HTML_Document* doc, HTML_Element* last_elm, HTML_Element* navigated_elm, HTML_Element*& common_ancestor);

	void			ShowMouseOver(HTML_Document* doc, HTML_Element* helm);
	
	Window*			window;			///< The window this Handler belongs to

	// The next variables will all be reset from the current document in the
	// window before each navigation, so they are really only valid during navigation
	FramesDocument*	currentDoc;		///< The top frame of the document currently being active
	FramesDocument*	activeFrameDoc;      ///< The specific frame currently being navigated.  The same as currentDoc if no frames.
	HTML_Document*	activeHTML_Doc;      ///< The HTML_Document currently being navigated

	BOOL			highlightOnLoad;    ///< If the first navigatable element should be highlighted when a page is first loaded
	BOOL			initial_highlight;  ///< TRUE when performing the first automatic highlight after load (only happens if highlightOnLoad is TRUE)
	INT32			scrollX;			///< The amount to scroll in horizontal direction as percent of view size
	INT32			scrollY;			///< The amount to scroll in vertical direction as percent of view size
	BOOL            allowDocScrolling;
	BOOL			scrolledOnce;

	BOOL			lowMemoryMode;   ///< currently unused

	/** Given a frameset document will enter the first subframe in the frameset
	 *  @param  parent_frame  the frameset document
	 *  @return TRUE if there was any subframe to enter */
	BOOL			EnterFirstSubFrame(FramesDocument* parent_frame);

	/** Enter a specific frame (of a frameset)
	 * @param frame  the frame to enter
	 */
	OP_STATUS		EnterFrame(FramesDocElm* frame);
	/** Enter an IFrame, that is find it's frames document and set it as the
	 *  current frame.  Also focuses it.  Does not navigate the iframe.
	 *  @param iframe_element   The HTML_Element which contains the iframe.  Must be an element of iframe or object type
	 *  @param parent_doc       The FramesDocument of the document which contains the iframe
	 *  @param iframe_rect      The size/position of the iframe in parent document
	 *  @return   OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR if unable to 
	 *            enter the iframe for some other reason (e.g. it wasn't done 
	 *            loading yet, empty document etc.), otherwise OpStatus::OK
	 */
	OP_STATUS		EnterIFrame(HTML_Element* iframe_element, FramesDocument* parent_doc, const RECT& iframe_rect);

	/** Exit an iframe that is set as the current frame being navigated, and move
	 * focus to the parent doc (or further ancestor in some cases).
	 * @param exit_after_try TRUE when exiting an iframe just after navigating
	 *		  to it (because no action could be carried out inside it).
	 */
	void			ExitIFrame(BOOL exit_after_try = FALSE);

	void			RestorePreviousLink();

	/** The navigation filter currently used */
	OpSnNavFilter*  nav_filter;
	/** Default navigation filter, which navigates to all clickable things */
	OpSnNavFilter*  default_nav_filter;
#ifdef SELECT_TO_FOCUS_INPUT
	HTML_Element* currentForm;
	HTML_Document* currentFormDoc;
#endif
	struct SnAllData allData;   ///< temporary data while navigating

private:

#ifdef SELFTEST
	friend class ST_spatnavoverflow;
	friend class ST_spatnavcss3navigation;
	friend class ST_spatnaviframes;
#endif // SELFTEST

	/** Gets the better hover/unfocus target for the elm
	 * that would most likely get the mouse events if it was
	 * really affected. Hackish.
	 * @param elm The HTML_Element to process. May be NULL.
	 * @return elm or the better target or NULL if elm was NULL.
	 */
	HTML_Element*	GetBetterTarget(HTML_Element* elm);

	/**
	* Scrolls the document in given direction by amount, which is calculated upon SnHandler's scroll step and
	* optional limit.
	*
	* @param direction to scroll
	* @param doc which should be scrolled
	* @param limit an optional limit of scroll amount in pixels in document scale. Negative value does not
	*	affect the actual computed scroll amount
	*
	* @return TRUE if scrolling was made succesfully.
	*		FALSE otherwise.
	*/
	BOOL			ScrollDocumentInternal(INT32 direction, FramesDocument* doc, INT32 limit = -1);
	
	/**
	* Scrolling document in direction with specified amount to scroll.
	* It works in the same way as ScrollDocumentInternal, except scroll step is specified and not calculated.
	*
	* @param direction to scroll
	* @param doc which should be scrolled
	* @param amount of scroll step
	*
	* @return TRUE if scrolling was made succesfully.
	*		FALSE otherwise.
	*/
	BOOL			ScrollDocumentInternalAmount(INT32 direction, FramesDocument* doc, INT32 amount);
	
	/**
	* Scrolling document in direction with specified amount to scroll.
	* It works in the same way as ScrollDocumentInternalAmount,
	* except when scrolling fails it tries to scroll parent of specified document.
	*
	* @param direction to scroll
	* @param doc which should be scrolled
	* @param amount of scroll step
	*
	* @return TRUE if scrolling was made succesfully.
	*		FALSE otherwise.
	*/
	BOOL			PointerScrollAmountInternal(FramesDocument* doc, INT32 direction, INT32 amount);
		
	OpSnNavFilter*	GetFilter(NavigationMode mode);

	/* Special filters for just navigating to e.g. images. These are only created when they are
	   used the first time, otherwise they remain NULL. */
	OpSnNavFilter*  heading_nav_filter;
	OpSnNavFilter*  paragraph_nav_filter;
	OpSnNavFilter*  nonlink_nav_filter;
	OpSnNavFilter*  form_nav_filter;
	OpSnNavFilter*  atag_nav_filter;
	OpSnNavFilter*	image_nav_filter;
	OpSnNavFilter*	configurable_nav_filter;

	int				nav_mode;

	// magellan_3 stuff
	BOOL			SetInitialFrame();

	/**
	 * 	Searches for a navigatable element in the current frame, or scroll
	 *  if none found.
	 *  
	 *  @param direction	Direction to search.  0 = right, 90 = up, 180 = left, 270 = down
	 *  @param startingPoint  Where it should start searching from.  If NULL it 
	 *  					  starts from previous element navigated to
	 *  @param scroll       The function won't scroll itself, but if it finds (i)frames then
	 *                      it will call FindLinkInFrameSet which might scroll if allowed to by this parameter
	 *	@param scroll_amount	When we are inside an iframe and the navigation attempt returns SCROLL_OUTER, then the maximum value, 
								such that the outer view won't jump over the iframe edge in given direction is stored under this pointer.
								This value is scaled to document. May be NULL, then no information given.
	 *  @param new_scrollable_rect_ptr the pointer to the rect of the new scrollable (ScrollableContainer or PagedContainer).
									   Not NULL when we have entered a new scrollable just before the call of this method, NULL otherwise.
	 *  @return 	The result of the navigation attempt
	 */
	NavigationResult		FindLinkInCurrentFrame(INT32 direction,
												   POINT* startingPoint,
												   BOOL scroll,
												   INT32* scroll_amount = NULL,
												   const RECT* new_scrollable_rect_ptr = NULL);

	/**
	 * 	Searches all subframes in a frameset (returning after the first one it is able to
	 *  do something useful with).  If the current frame is not a frameset then
	 *  it just calls FindLinkInCurrentFrame.
	 *  
	 *  @param direction	Direction to search.  0 = right, 90 = up, 180 = left, 270 = down
	 *  @param startingPoint  Where it should start searching from.  If NULL it 
	 *  					  starts from previous element navigated to
	 *  @return 	The result of the navigation attempt
	 */
	NavigationResult			FindLinkInFrameSet(INT32 direction, POINT* startingPoint, BOOL scroll);

	/**
	 *  Gets the next subframe in a frameset, if there is any
	 * 
	 *  @param direction   the direction to look for a frame in
	 *  @param parent      the FramesDocument for the frameset
	 *  @param oldDoc      the frame we're currently in
	 *  @param checkVisibility used in special frames mode. When TRUE, method will perform extra check
		                   whether new found frame is in view
	 *  @return            the best frame to navigate to, or NULL if none
	 */
	FramesDocElm*	GetNextFrame(INT32 direction, FramesDocument* parent, FramesDocElm* oldDoc, BOOL checkVisibility = FALSE);

	/** Set the active frame document
	 *  Unlikely to have much effect as it will be reset to the
	 *  window's active frame as soon as navigation occurs
	 */
	void			SetActiveFrame(FramesDocument* frameDoc);

	FramePath*		currentFramePath;
	BOOL			fourway;
	HTML_Element*	activeScrollable; // active ScrollableContainer or PagedContainer
	HTML_Element*   previous_link;  // only used while navigating to remember where we started
	HTML_Element*	previous_scrollable; // used to store the activeScrollable, in the frame, that we started from

	HTML_Element*	navigate_from;   // used during navigating in/out of scrollable containers, paged containers and iframes to remember the position of the container/iframe for further navigation

	RECT            lastRect; // Used to remember which line of a line-breaked link we should navigate from.
	/** Used in various places to compare the current focused/highlighed or hovered
		element to the one that was last navigated by spatnav and therefore know
		whether some data is valid or some action should be carried out. */
	HTML_Element*   lastElement;
	OpVector<HTML_Element> exclusions; // Exclusions list - used to skip some elements when it's known there's no point inspecting them
};


#endif // _SPAT_NAV_SUPPORT_

#endif // _SN_HANDLER_H_


