/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DRAGPRIVATEDATA_H
# define DRAGPRIVATEDATA_H

# ifdef DRAG_SUPPORT

#  include "modules/logdoc/elementref.h"
#  include "modules/dragdrop/src/scroller.h"
#  include "modules/hardcore/timer/optimer.h"
#  include "modules/pi/OpDragObject.h"

class CoreViewDragListener;
class HTML_Document;
class HTML_Element;

/**
 * Class storing all informations core needs during drag'n'drop. It stores pointers to core internal
 * classes so those information can't be stored in OpDragObject directly.
 * This data is set in OpDragObject as OpDragSourcePrivateData using OpDragObject::SetPrivateData().
 * It may be retrieved using OpDragObject::GetPrivateData().
 *
 * @see OpDragObject
 */
class PrivateDragData : public OpCorePrivateDragData, public OpTimerListener
{
public:
	/**
	 * Document's drag'n'drop events sqeuence period.
	 * It's required by HTML5 d'n'd spec that ONDRAG + ONDRAGOVER events sequence
	 * is sent at least every 350 ms +- 200ms.
	 */
	static const unsigned DRAG_SEQUENCE_PERIOD = 350;

	/** Structure describing a document associated with the dragged element.
	 * This is a data kept in the auto hash table. When a document is deleted
	 * we must iterate through hash table's data and invalidate proper elements
	 * without changing the hash table's data itself.
	 */
	struct DocElm
	{
		HTML_Document* document;
	};

	PrivateDragData()
	  : m_source_doc(NULL)
	  , m_origin_listener(NULL)
	  , m_is_selection(FALSE)
	  , m_is_dropped(FALSE)
	  , m_target_doc(NULL)
	  , m_is_body_onenter_special(FALSE)
	{
		m_timer.SetTimerListener(this);
	}

	virtual ~PrivateDragData();

	/** Sets the source html element with its document. */
	void					SetSourceHtmlElement(HTML_Element* source, HTML_Document* document);

	/** Gets an origin drag listener (the listener the drag operation started in). */
	CoreViewDragListener*	GetOriginListener() const { return m_origin_listener; }

	/** Sets an origin drag listener (the listener the drag operation started in). */
	void					SetOriginListener(CoreViewDragListener* listener) { m_origin_listener = listener; }

	/** Sets a document's area the drag feedback bitmap was made of. */
	void					SetBitmapRect(const OpRect& rect) { m_bitmap_rect = rect; }

	/**
	 * Gets a document's area the feedback bitmap was made of.
	 * @see SetBitmapRect()
	 */
	const OpRect&			GetBitmapRect() { return m_bitmap_rect; }

	/** Adds given element to a list of dragged elements (together with its document). */
	OP_STATUS				AddElement(HTML_Element* elm, HTML_Document* document);

	/**
	 * Removes the given element from the list of dragged elements.
	 * Must be called if the element gets deleted.
	 */
	void					OnElementRemove(HTML_Element* elm);

	/**
	 * Must be called when the document gets unloaded.
	 */
	void					OnDocumentUnload(HTML_Document* document);

	/**
	 * Gets the document associated with the given element.
	 * May return NULL if the given element is not on the dragged elements list
	 * or the document got deleted in a meanwhile.
	 */
	HTML_Document*			GetElementsDocument(HTML_Element* elm);

	/** Gets a drag source as an html element. May return NULL if there's no source html element. */
	HTML_Element*			GetSourceHtmlElement() const { return m_source_elm.GetElm(); }

	/** Gets the list of the dragged elements. */
	const OpPointerHashTable<HTML_Element, DocElm>&
							GetElementsList() { return m_elements_list; }

	/** Sets the flag indicating if the dragged data comes from a text selection. */
	void					SetIsSelection(BOOL val) { m_is_selection = val; }

	/** Gets the value indicating if the dragged data comes from a text selection. */
	BOOL					IsSelection() { return m_is_selection; }

	/** Sets the flag keeping track if the dragged data was successfuly dropped. */
	void					SetIsDropped(BOOL val) { m_is_dropped = val; }

	/** Gets the value indicating if the dragged data was successfuly dropped. */
	BOOL					IsDropped() { return m_is_dropped; }

	/** Returns the target document. May return NULL. */
	HTML_Document*			GetTargetDocument() { return m_target_doc; }

	// OpTimerListener's interface
	virtual void			OnTimeOut(OpTimer*);

private:
	friend class DragDropManager;
	/** The element being drag's source. */
	AutoNullElementRef		m_source_elm;
	/** The document the source element belongs to. */
	HTML_Document*			m_source_doc;
	/** The drag listener the operation was started within. */
	CoreViewDragListener*	m_origin_listener;
	/** A document's area the bitmap was made of. */
	OpRect					m_bitmap_rect;
	/** The list of elements being dragged (with documents associated with them). */
	OpAutoPointerHashTable<HTML_Element, DocElm>
							m_elements_list;
	/** The flag indicatiing whether the dragged data is coming from a text selection. */
	BOOL					m_is_selection;
	/** Keeps track whether the data was successfuly dropped i.e. ONDROP event was handled and the drop effect is not DROP_NONE. */
	BOOL					m_is_dropped;
	/**
	 * The current d'n'd target document. The pointing device is currently over this document
	 * and the elements in it get ONDRAGENTER, ONDRAGOVER, ONDRADLEAVE and ONDROP events.
	 */
	HTML_Document*			m_target_doc;
	/**
	 * Drag'n'drop events sequence timer. Makes sure ONDRAG + ONDRAGOVER events are sent
	 * periodically even when the pointing device stands still.
	 */
	OpTimer					m_timer;
	/**
	 * Stores BOOL value indicating that ONDRAGENTER event sent to <body> element was
	 * a special event caused by the true target element not cancelling ONDRAGENTER.
	 * See http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html#drag-and-drop-processing-model
	 */
	BOOL					m_is_body_onenter_special;

	/** Instance of the class taking care of scrolling when dragging. */
	DragScroller			m_scroller;

	/**
	 * Last known drag point in document scale, relative to the upper left corner of the view. To get the full document
	 * coordinate, OpPoint(view_x, view_y) has to be added.
	 */
	OpPoint					m_last_known_drag_point;

	/** The origins allowed to be given the drag data (uri-list format). */
	OpString				m_allowed_target_origin_urls;

	/** The drag origin. */
	OpString				m_orign_url;
};

# endif // DRAG_SUPPORT

#endif // DRAGPRIVATEDATA_H
