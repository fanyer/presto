/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
**
*/

/**
 * @file navigation_api.h
 *
 * This file contains the navigation API.
 *
 * @author h2@opera.com
*/

#ifndef NAVIGATION_API_H
#define NAVIGATION_API_H

#include "modules/hardcore/base/opstatus.h"

#include "modules/spatial_navigation/sn_filter.h"
#include "modules/spatial_navigation/handler_api.h"

#define DIR_RIGHT 0
#define DIR_UP 90
#define DIR_LEFT 180
#define DIR_DOWN 270

/**
 * The identifiaction datatype for a link. 
 *
 */
typedef void* SnLinkID;

/**
 * The identifiaction datatype for a web frame. 
 *
 */
typedef void* SnFrameID;

/**
 * Additional link data.
 *
 */
typedef void* SnLinkData;

/**
 * Structure for passing all relevant link and frame data
 *
 */
struct SnAllData
{
	SnLinkID linkID;
	SnFrameID frameID;
	BOOL isCSSNavLink;
	RECT linkRect;

	/** Not used currently.
	SnLinkData linkData;
	INT32 linkNumber;
	INT32 frameNumber;
	SnFrameID parentFrameID;
	RECT totalRect;
	*/
};

/**
 * The different navigation modes
 *
 */

enum NavigationMode
{
 	NAVIGATION_ALL_LINKS = 0,		/**< Navigate all links, forms and elements with js handler */
 	NAVIGATION_LINKS = 1,			/**< Navigate links (anchors) */
 	NAVIGATION_FORMS = 2,			/**< Navigate forms objects */
 	NAVIGATION_ALL_NON_LINKS = 3,	/**< Navigate headings and paragraphs */
 	NAVIGATION_HEADINGS = 4,		/**< Navigate headings */
 	NAVIGATION_PARAGRAPHS = 5,		/**< Navigate paragraphs */
 	NAVIGATION_IMAGES = 6,			/**< Navigate images */
	NAVIGATION_PICKER_MODE = 7      /**< JUIX picker mode */
};

/** Data type and iterator for non rectangular HTML links.
 *  This is mainly or only used for HTML image maps.
 *
 */
class SnPolygon
{
public:
	/** Destructor, should never be used by the module.
	 *
	 */
	virtual ~SnPolygon() {};

	/** Get the X coordinage of the segment.
	 *
	 */
	virtual UINT32 GetX() = 0;

	/** Get the Y coordinage of the segment.
	 *
	 */
	virtual UINT32 GetY() = 0;

	/** Go back to the first entry in the coordinage list.
	 *
	 */
	virtual void GotoFirstCorner() = 0;

	/** End of data, Next() has come past the last entry in
	 *  the coordinate list or the list was empty.
	 *
	 */
	virtual BOOL IsNull() = 0;

	/** Go to next entry in the coordinate list. 
	 *
	 */
	virtual void Next() = 0;
};

/** Data type and iterator for all navigable items in a frame.
 *
 */
class SnLinkIterator
{
public:
	/** Destructor, should never be used by the module.
	 *
	 */
	virtual ~SnLinkIterator() {};

	/** Get the link's X coordinage relative to the whole page.
	 *
	 */
	virtual UINT32 GetX() const = 0;

	/** Get the link's Y coordinage relative to the whole page.
	 *
	 */
	virtual UINT32 GetY() const = 0;

	/** Get the link's width.
	 *
	 */
	virtual UINT32 GetWidth() const = 0;


	/** Get the link's height.
	 *
	 */
	virtual UINT32 GetHeight() const = 0;

	/** Get the link's rect.
	 *
	 */
	virtual void GetRect(RECT& rect) const = 0;

	/** Get the link's total rect.
	 *
	 */
	virtual void GetTotalRect(RECT& rect) const = 0;

	/** Is the link visible on the page?
	 *
	 */
	virtual BOOL IsVisible() const = 0;
	
	/** Is the link on top?
	 *
	 */
	virtual BOOL OnTop() = 0;

	/** Get a unique link identiy. 
	 * (Even if several links points to the same location,
	 * they will have different ID's).
	 */
	virtual SnLinkID GetCurrentID() const = 0;

	/** Get a unique link number. 
	 *
	 */
	virtual INT32 GetCurrentNumber() const = 0;

	/** Get the Parent of the current link if possible.
	 *
	 */
	virtual SnLinkData GetParent() const = 0;

	/** Does the link have a parent (scrollable container).
	 *
	 */
	virtual BOOL HasParent() const = 0;

	/** Is the link scrollable.
	 *
	 */
	virtual BOOL IsScrollable() const = 0;

	/** Additional data for the link
	 *
	 */
	virtual SnLinkData GetCurrentData() const = 0;

	/** Get the OpSnElement of the link
	 *
	 */
	virtual OpSnElement* GetCurrentElement() const = 0;
	
	/** The different link shapes.
	 *  
	 */
	enum SnLinkShape
	{ 
		LINKSHAPE_NORMAL,    /**< An ordinary, rectangular link */
		LINKSHAPE_ELLIPSE,   /**< An elliptical link */
		LINKSHAPE_POLYGON    /**< A link with polygon shape. */
	};

	/** Get the shape of the link
	 *
	 */
	virtual SnLinkShape GetLinkShape() = 0;

	/** Get the polygon iterator to get access to the data
	 *  about the link shape. 
	 *
	 */
	virtual SnPolygon* GetPolygon() = 0;
	
	/** Go back to the first entry of the link list.
	 *
	 */
	virtual void GotoFirstLink(OpSnNavFilter *nav_filter) = 0;

	/** Go to the last entry of the link list.
	 *
	 */
	virtual void GotoLastLink(OpSnNavFilter *nav_filter) = 0;

	/** End of link list, thus the object contains no 
	 *  information.
	 *
	 */
	virtual BOOL IsNull() = 0;

	/** Go to next link in the link list.
	 *
	 */
	virtual void Next(OpSnNavFilter *nav_filter) = 0;

	/** Go to previous link in the link list.
	 *
	 */
	virtual void Previous(OpSnNavFilter *nav_filter) = 0;

	/** Go to next link in the link list, not inside active link.
	 *
	 */
	virtual void NextLink(OpSnNavFilter *nav_filter) = 0;

	/** Go to previous link in the link list, not inside active link.
	*
	*/
	virtual void PreviousLink(OpSnNavFilter *nav_filter) = 0;

	/** Get the type for the current active element.
	 *
	 */
	virtual SnElementType GetCurrentType() const = 0;
};

/** Data type and iterator for all frames of the current document.
 *
 */
class SnFrameIterator
{
public:
	/** Destructor, should never be used by the module. 
	 *
	 */
	virtual ~SnFrameIterator() {};

	/** Get the frame's X coordinage relative to the whole page.
	 *
	 */
	virtual INT32 GetX() const = 0;

	/** Get the frame's Y coordinage relative to the whole page.
	 *
	 */
	virtual INT32 GetY() const = 0;   

	/** Get the frame's width.
	 *  
	 */
	virtual INT32 GetWidth() const = 0;

	/** Get the frame's height.
	 *  
	 */
	virtual INT32 GetHeight() const = 0;

	/** Get the view's X coordinage relative to the whole page.
	 *
	 */
	virtual INT32 GetViewX() const = 0;

	/** Get the view's Y coordinage relative to the whole page.
	 *
	 */
	virtual INT32 GetViewY() const = 0;   

	/** Get the view's width.
	 *  
	 */
	virtual INT32 GetViewWidth() const = 0;

	/** Get the view's height.
	 *  
	 */
	virtual INT32 GetViewHeight() const = 0;

	/** Get a link iterator with an abstract representation
	 *  of the list of links in the frame. The module have 
	 *  no responsibility or right to free the memory of the link iterator.
	 *
	 */
	virtual SnLinkIterator* GetLinkIterator() const = 0;

	/** Get a unique frame identiy.
	 *
	 */
	virtual SnFrameID GetCurrentID() const = 0;
 
	/** Get frame identiy of parent (in case of an i-frame).
	 *
	 */
	virtual SnFrameID GetParentID() const = 0;

	/** Get a unique frame number. 
	 *
	 */
	virtual INT32 GetCurrentNumber() const = 0;

	/** Go back to the first entry of the frame list.
	 *
	 */
	virtual void GotoFirstFrame(OpSnNavFilter* nav_filter) = 0;

	/** Go to the last entry of the frame list.
	 *
	 */
	virtual void GotoLastFrame() = 0;

	/** End of frame list, thus the object contains no 
	 *  information.
	 *
	 */
	virtual BOOL IsNull() const = 0;

	/** Go to next frame in the frame list.
	 *
	 */
	virtual void Next(OpSnNavFilter* nav_filter) = 0;

	/** Go to previous frame in the frame list.
	 *
	 */
	virtual void Previous(OpSnNavFilter* nav_filter) = 0;

	/** Go to next frame in the frame list, not inside active frame.
	 *
	 */
	virtual void NextFrame(OpSnNavFilter* nav_filter) = 0;

	/** Go to previous frame in the frame list, not inside active frame.
	 *
	 */
	virtual void PreviousFrame(OpSnNavFilter* nav_filter) = 0;

	/** Check if this frame is initialized
	 *
	 */
	virtual BOOL GetInitialized() const = 0;
};

/** Data type for all information about the current document,
 *  its frames and links and the active frame and link.
 *
 */
class SnInfo
{
public:
	/** Destructor, should never be used by the module.
	 *
	 */
	virtual ~SnInfo() {};

	/** Iterator for the active frame.
	 *
	 */
	virtual SnFrameIterator* GetActiveFrame() const = 0;

	/** Iterator for the active link.
	 *
	 */
	virtual SnLinkIterator* GetActiveLink() const = 0;
	
	/** Iterator for all frames.
	 *
	 */
	virtual SnFrameIterator* GetFrameIterator() const = 0;
	
	/** Get all data in the current state
	 *
	 */
	virtual void GetAllData(SnAllData* allData, BOOL activeFrame = FALSE) const = 0;

	/** Determine if this is a page with frames
	 *
	 */
	virtual BOOL IsFrameDoc() const = 0;

	/** Determine if frames are stacked or not
	 *
	 */
	virtual BOOL GetFramesStacked() const = 0;

	/** Determine if frames are either stacked or smart 
	 *
	 */
	virtual BOOL GetHasSpecialFrames() const = 0;

	/* Get the width of the window.
	 *
	 */
	virtual INT32 Width() const = 0;

	/* Get the height of the window.
	 *
	 */
	virtual INT32 Height() const = 0;
};


/** This class implements the spatial navigation algorithm.
 *
 *	This is the very module class that contains the spatial
 *  navigation algorithm and contains all data that the algoritm may
 *  store about the web page.
 *
 */

class SnModule
{
public:
	virtual ~SnModule() {}

	/**
	  * Let the module calculate the next link in the specified direction.
	  *
	  * @param info Data structure that provides all information the module
	  *             has access to.
	  * @param direction The direction of the move [0..359].
	  *
	  * @param resultingData The resulting link and frame data
	  *
	  * @param searchActiveFrameOnly searches only active frame when true, else all frames
	  *
	  * @param startingPoint if startingPoint is set, the module should start searching for links from this point.
	  *
	  * @param allowPartiallyVisibleLinks selection of partially visible links.
	  *
	  * @return OpStatus::OK if no error
	  *
	  */
	virtual OP_STATUS SnGetNextLink(const SnInfo* info, INT32 direction, 
									SnAllData* resultingData, 
									OpSnNavFilter *nav_filter, 
									BOOL searchActiveFrameOnly = FALSE, 
									POINT* startingPoint = NULL, 
									BOOL allowPartiallyVisibleLinks = TRUE) = 0;	

	/** Inserts initial focus
	  *
	  */
	virtual OP_STATUS InsertInitialFocus(const SnInfo* sninfo, SnAllData* resultingData, POINT* startingPoint, OpSnNavFilter* nav_filter) = 0;

	/** Order the module to release all memory.
	  *
	  */
	virtual void ClearCache() = 0;

	/** Tells the module that the information that it may have held up
	  * till now is invalid, for example caused by a reformat of the
	  * document.
	  */
	virtual void SnInfoIsInvalid() = 0;
};


#endif // NAVIGATION_API_H
