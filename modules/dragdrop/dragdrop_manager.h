/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DRAGDROPMANAGER_H
# define DRAGDROPMANAGER_H

# ifdef DRAG_SUPPORT

#  include "modules/dragdrop/src/events_manager.h"
#  include "modules/hardcore/keys/opkeys.h"

class OpDragObject;
class OpDragManager;
class HTML_Element;
class FramesDocument;
class HTML_Document;
class CoreViewDragListener;
class URL;
class FileDropConfirmationCallback;

/** Drag and drop manager to be used by core. */
class DragDropManager
{
private:
	/** Platform's drag manager. */
	OpDragManager*	m_pi_drag_manager;

	/**
	 * The flag indicating in the drag'n'drop operation is blocked.
	 *
	 * @see Block()
	 * @see Unblock()
	 * @see IsBlocked()
	 */
	BOOL			m_is_blocked;

	/** The flag indicating if the current drag was cancelled. */
	BOOL			m_is_cancelled;

	/** Enumaration of possible ONMOUSEDOWN states */
	enum MouseDownStatus
	{
		/** A neutral state */
		NONE,
		/** The ONMOUSEDOWN has been queued and will be sent at some later point. */
		DELAYED,
		/** The ONMOUSEDOWN has been sent. */
		SENT,
		/** A default action of ONMOUSEDOWN has been prevented. */
		CANCELLED,
		/** A default action of ONMOUSEDOWN hasn't been prevented. */
		ALLOWED
	}
	;
	/**
	 * The flag indicating what's the status of ONMOUSEDOWN event.
	 * We need to keep track of it in order to know if the drag is allowed because
	 * preventing the default action of ONMOUSEDWON prevents dragging as well.
	 *
	 * @see OnSendMouseDown()
	 * @see OnRecordMouseDown()
	 * @see OnMouseDownDefaultAction()
	 * @see MouseDownStatus
	 */
	MouseDownStatus	m_mouse_down_status;

	friend class FileDropConfirmationCallback;
	/** The files drop confirmation dialog callback which is displayed when droping a file will cause its data will be passed to a page. */
	FileDropConfirmationCallback*
					m_drop_file_cb;

	/** Cancels the file drop dialog in the passed in document. */
	void			CancelFileDropDialog(HTML_Document* doc);

	/**
	 * Shows the file drop dialog (the dialog asking if it's allowed to pass file's data to a page)
	 * in the passed in document.
	 *
	 * @param doc - the dialog will be shown on behalf of this document.
	 * @param x - the x coordinate the file drop occured at (in document coordinates).
	 * @param y - the y coordinate the file drop occured at (in document coordinates).
	 * @param[in] visual_viewport_x The x offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's x offset and not the rendering view port's one.
	 * @see FramesDocument::GetVisualViewport()
	 * @see FramesDocument::GetVisualViewportX()
	 * @param[in] visual_viewport_y The y offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's y offset and not the rendering view port's one.
	 * @see FramesDocument::GetVisualViewport()
	 * @see FramesDocument::GetVisualViewportY()
	 * @param offset_x - the x offset inside the target element. 0 for
	 * the left edge.
	 * @param offset_y - the y offset inside the target element. 0 for
	 * the top edge.
	 * @param modifiers - the flag that says which shift, control and
	 * alt keys were pressed when the event was triggered.
	 *
	 * @note x, y, offset_x, offset_y, modifiers will be cached by the dialog
	 * and passed to the drop event at confirmation.
	 */
	void			ShowFileDropDialog(HTML_Document* doc, int x, int y, int visual_viewport_x, int visual_viewport_y, int offset_x, int offset_y, ShiftKeyState modifiers);

	/**
	 * Drag'n'drop events recorder.
	 * @see DragDropEventsManager.
	 */
	DragDropEventsManager
					m_actions_recorder;

	/**
	 * This is the proper method to send a d'n'd event (ONDRAGSTART for
	 * instance) inside Opera. It will send the event through the
	 * script system (if enabled) and to the relevant document part.
	 *
	 * <p>This method is for d'n'd events.
	 *
	 * @param doc - the document the event is meant to be fired in.
	 *
	 * @param event - the event type.
	 *
	 * @param target - the element that should receive the
	 * event. Not NULL.
	 *
	 * @param document_x - the x position of the d'n'd event in
	 * document coordinates.
	 *
	 * @param document_y - the y position of the d'n'd event in
	 * document coordinates.
	 *
	 * @param offset_x - the offset inside the target element. 0 for
	 * the left edge.
	 *
	 * @param offset_y - the offset inside the target element. 0 for
	 * the top edge.
	 *
	 * @param modifiers - the flag that says which shift, control and
	 * alt keys were pressed when the event was triggered.
	 *
	 * @param[in] visual_viewport_x The x offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's x offset and not the rendering view port's one.
	 * If NULL, the viewport x position must be got from the document.
	 * @see FramesDocument::GetVisualViewport()
	 * @see FramesDocument::GetVisualViewportX()
	 *
	 * @param[in] visual_viewport_y The y offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's y offset and not the rendering view port's one.
	 * If NULL, the viewport y position must be got from the document.
	 * @see FramesDocument::GetVisualViewport()
	 * @see FramesDocument::GetVisualViewportY()
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 *
	 * @see DragAction()
	 * @see HTML_Element::HandleEvent()
	 *
	 */
	OP_STATUS		HandleDragEvent(HTML_Document* doc, DOM_EventType event, HTML_Element* target,
	                                int document_x, int document_y, int offset_x, int offset_y,
	                                ShiftKeyState modifiers, int* visual_viewport_x = NULL, int* visual_viewport_y = NULL);

	/**
	 * Sets proper drop type based on d'n'd event to be fired and
	 * OpDragObject's effects allowed and suggested drop type.
	 *
	 * @see DropType
	 * @see OpDragObject::SetDropType()
	 */
	void			SetProperDropType(DOM_EventType event);

	/**
	 * Finds the currently intersected element based on passed x, y (in document coordinates).
	 *
	 * @param[in] doc - the document the element is supposed to be looked for in.
	 * @param[in] x - the x coordinate (in document coordinates) of a point where the cursor currently is.
	 * @param[in] y - the y coordinate (in document coordinates) of a point where the cursor currently is.
	 * @param[out] offset_x - the x offset within the element which was found
	 *                        (relative to the element's bounding box top,left corner).
	 * @param[out] offset_y - the y offset within the element which was found
	 *                        (relative to the element's bounding box top,left corner).
	 *
	 * @return The element found or NULL if there's no HTML_Element under (x,y) point in given document.
	 */
	HTML_Element*	FindIntersectingElement(HTML_Document* doc, int x, int y, int& offset_x, int& offset_y);

	/** Helper method taking care of all actions which must be performed on drop. */
	OP_STATUS		DragDrop(HTML_Document* doc, int x, int y, int visual_viewport_x, int visual_viewport_y, int offset_x, int offset_y, ShiftKeyState modifiers);

	/**
	 * It's responsible for scrolling a viewport/scrollable during d'n'd if it's
	 * detected that the drag is falling into d'n'd scroll margin.
	 *
	 * @param doc - the document which is meant to be scrolled.
	 * @param x - the current d'n'd cursor's x - in document coordinates.
	 * @param y - the current d'n'd cursor's y - in document coordinates.
	 *
	 * See also DND_SCROLL_MARGIN, DND_SCROLL_INTERVAL, DND_SCROLL_DELTA_MIN and DND_SCROLL_DELTA_MAX
	 * tweaks.
	 *
	 * @return TRUE if anything was scrolled.
	 */
	BOOL			ScrollIfNeededInternal(FramesDocument* doc, int x, int y);

	/**
	 * Simple wrapper for OpDragManager::SetDragObject().
	 * @see OpDragManager::SetDragObject()
	 */
	void			SetDragObject(OpDragObject* drag_object);

	/**
	 * Adds the passed in url to the list of allowed URLs in the data store.
	 * Allowed URL is a URL which is allowed to be given the data.
	 *
	 * @param drag_object - the data store the allowed target url should be added to.
	 * @param url - the origin url.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM. OpStatus::OK otherwise.
	 */
	static OP_STATUS
					AddAllowedTargetURL(OpDragObject* drag_object, const uni_char* url);

public:
	DragDropManager();

	~DragDropManager();
	/** Initializes the manager. */
	OP_STATUS		Initialize();

	// OpDragManager wrapper.
	/**
	 * Simple wrapper for OpDragManager::StartDrag().
	 * @see OpDragManager::StartDrag()
	 */
	void			StartDrag();

	/**
	 * Simple wrapper for OpDragManager::StopDrag().
	 * @see OpDragManager::StopDrag()
	 */
	void			StopDrag(BOOL cancelled = FALSE);

	/** Simple wrapper for OpDragManager::IsDragging().
	 * @see OpDragManager::IsDragging()
	 */
	BOOL			IsDragging();

	/**
	 * Simple wrapper for OpDragManager::GetDragObject().
	 * @see OpDragManager::GetDragObject()
	 */
	OpDragObject*	GetDragObject();

	/**
	 * The generic "handle drag'n'drop events" code. This may not be
	 * synchronous. In case of scripts, events may be recorded here
	 * for replaying them later on.
	 *
	 * @param doc - the document the d'n'd event should be fired in.
	 *
	 * @param event - the type of d'n'd event.
	 *
	 * @param x - the x coordinate of the d'n'd action in document
	 * coordinates.
	 *
	 * @param y - the y coordinate of the d'n'd action in document
	 * coordinates.
	 *
	 * @param[in] visual_viewport_x The x offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's x offset and not the rendering view port's one.
	 * @see FramesDocument::GetVisualViewport()
	 * @see FramesDocument::GetVisualViewportX()
	 *
	 * @param[in] visual_viewport_y The y offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's y offset and not the rendering view port's one.
	 * @see FramesDocument::GetVisualViewport()
	 * @see FramesDocument::GetVisualViewportY()
	 *
	 * @param offset_x - the x offset within the target element (relative to its top, left corner).
	 *
	 * @param offset_y - the y offset within the target element (relative to its top, left corner).
	 *
	 * @param shift_pressed TRUE if the shift key was pressed when
	 * doing the d'n'd action.
	 *
	 * @param control_pressed TRUE if the control key was pressed when
	 * doing the d'n'd action.
	 *
	 * @param alt_pressed TRUE if the alt key was pressed when doing
	 * the d'n'd action.
	 *
	 * @return TRUE if the action was handled immediately or FALSE
	 * if the action could not be handled at the moment (and most likely was recorded to
	 * be replayed later).
	 *
	 */
	BOOL			DragAction(HTML_Document* doc, DOM_EventType event, int x, int y, int visual_viewport_x, int visual_viewport_y, int offset_x, int offset_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed);

	/**
	 * Another version of StartDrag() which should be called when the drag starts outside of a document.
	 *
	 * @param object - the drag object to be set in OpDragManager.
	 * @param origin_listener - the origin drag listener.
	 * @param is_selection - if the data comes from a text selection.
	 *
	 * @see CoreViewDragListener
	 */
	void			StartDrag(OpDragObject* object, CoreViewDragListener* origin_listener, BOOL is_selection);

	/**
	 * Must be called when the drag'n'drop operation progress in blocked e.g.
	 * a drag'n'drop event thread is interrupted by the JS alert.
	 */
	void			Block();

	/**
	 * Must be called when the drag'n'drop operation progress is unblocked
	 * e.g. the JS alert which blocked a drag'n'drop event thread gets dismissed.
	 *
	 * @see Block()
	 */
	void			Unblock();

	/**
	 * Returns TRUE if the drag'n'drop operation progress is blocked.
	 *
	 * @see Block()
	 * @see Unblock()
	 */
	BOOL			IsBlocked();

	/**
	 * Should be called by display module (DragListener) when it has detected
	 * that the drag'n'drop operation was started e.g. due to the mouse move
	 * with its left button pressed.
	 *
	 * Checks whether the d'n'd is valid i.e. d'n'd started on a text selection
	 * or a draggable element. Creates and fills proper d'n'd data, create the
	 * d'n'd visual feedback and send ONDRAGSTART event.
	 *
	 * @param origin_drag_listener - the drag listener the drag was started in.
	 * @param doc - the document the drag started in.
	 * @param start_x - the x coordinate where d'n'd started in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_x has to be added.
	 * @param start_y - the y coordinate where d'n'd started in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_y has to be added.
	 * @param modifiers - the flag that says which shift, control and
	 * alt keys were pressed when the drag start was triggered.
	 * @param current_x - the x coordinate a pointing device is currently at in document
	 * scale, relative to the upper left corner of the view. To get the full document
	 * coordinate, view_x has to be added.
	 * @param current_y - the y coordinate a pointing device is currently at in document
	 * scale, relative to the upper left corner of the view. To get the full document
	 * coordinate, view_x has to be added.
	 *
	 * @note [start_x, start_y] point might be the same as [current_x, current_y] point
	 * if a drag start does not depend on pointing device movement e.g. a long click.
	 * Otherwise - if the drag start is detected e.g. by mouse move with the left buffon pressed -
	 * [start_x, start_y] point will be where the mouse button was pressed and [current_x, current_y]
	 * point will be the point the mouse is currently at.
	 *
	 * @see CoreViewDragListener
	 */
	void			OnDragStart(CoreViewDragListener* origin_drag_listener, HTML_Document* doc, int start_x, int start_y, ShiftKeyState modifiers, int current_x, int current_y);

	/** Should be called when the drag enters document. */
	void			OnDragEnter();

	/**
	 * Should be called by display module (DragListener) whenever d'n'd moves.
	 *
	 * Propagates the ONDRAG/ONDRAGENTER/ONDRAGOVER/ONDRAGLEAVE events.
	 *
	 * @param doc - the document in which the drag moved.
	 * @param x - the x coordinate where d'n'd moved to in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_x has to be added.
	 * @param y - the y coordinate where d'n'd moved to in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_y has to be added.
	 * @param modifiers - the flag that says which shift, control and
	 * alt keys were pressed when the drag move was triggered.
	 * @param force - If TRUE the move events sequence is executed even if the passed in coordinates
	 * are the same as the last coordinates the sequence was executed for. It's needed in order to
	 * be able to execute the sequence periodically even if the drag pointer stands still.
	 */
	void			OnDragMove(HTML_Document* doc, int x, int y, ShiftKeyState modifiers, BOOL force = FALSE);

	/**
	 * Should be called by display module (DragListener) when the drop occurs
	 * (e.g. the left mouse button was released).
	 *
	 * Propagates the ONDROP/ONDRAGLEAVE+ONDRAGEND events or cleans d'n'd up if
	 * it's impossible to send the events.
	 *
	 * Note that all operations are done in the current target document.
	 *
	 * @param x - the x coordinate where drop occured in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_x has to be added.
	 * @param y - the y coordinate where drop occured to in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_y has to be added.
	 * @param modifiers - the flag that says which shift, control and
	 * alt keys were pressed when the drop was triggered.
	 */
	void			OnDragDrop(int x, int y, ShiftKeyState modifiers);

	/**
	 * Should be called by display module (DragListener) when d'n'd finishes outside of Opera.
	 *
	 * Propagates the ONDRAGEND event or cleans d'n'd up if it's impossible to send the event.
	 *
	 * Note that all operations are done in the source document.
	 *
	 * @param x - the x coordinate where d'n'd ended in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_x has to be added. Can be negative.
	 * @param y - the y coordinate where d'n'd ended to in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_y has to be added. Can be negative.
	 * @param modifiers - the flag that says which shift, control and
	 * alt keys were pressed when the drag was ended.
	 */
	void			OnDragEnd(int x, int y, ShiftKeyState modifiers);

	/**
	 * Should be called by display module (DragListener) when d'n'd leaves this
	 * document as e.g. a result of mouse move.
	 *
	 * Propagates the ONDRAGLEAVE event and clears document specific d'n'd data.
	 *
	 * Note that all operations are done in the current target document.
	 *
	 * @param modifiers - the flag that says which shift, control and
	 * alt keys were pressed when the drag left the document.
	 */
	void			OnDragLeave(ShiftKeyState modifiers);

	/**
	 * Should be called by display module (DragListener) when d'n'd gets
	 * cancelled e.g. because ESC key was pressed.
	 *
	 * Propagates the ONDRAGLEAVE+ONDRAGEND events or cleans d'n'd up if it's
	 * impossible to send these events.
	 *
	 * Note that all operations are done in the current target document.
	 *
	 * @param x - the x coordinate where d'n'd got aborted in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_x has to be added.
	 * @param y - the y coordinate where d'n'd got aborted in document scale, relative
	 * to the upper left corner of the view. To get the full document
	 * coordinate, view_y has to be added.
	 * @param modifiers - the flag that says which shift, control and
	 * alt keys were pressed when the drag was cancelled.
	 */
	void			OnDragCancel(int x, int y, ShiftKeyState modifiers);

	/** ONDRAGSTART's default action. Should be called from HTML_Element::HandleEvent(ONDRAGSTART).
	 * @see HTML_Element::HandleEvent()
	 */
	void			HandleOnDragStartEvent(BOOL cancelled);

	/** ONDRAG's default action. Should be called from HTML_Element::HandleEvent(ONDRAG).
	 * @see HTML_Element::HandleEvent()
	 */
	void			HandleOnDragEvent(HTML_Document* doc, int document_x, int document_y, int offset_x, int offset_y, ShiftKeyState modifiers, BOOL cancelled);

	/** ONDRAGENTER's default action. Should be called from HTML_Element::HandleEvent(ONDRAGENTER).
	 * @see HTML_Element::HandleEvent()
	 */
	void			HandleOnDragEnterEvent(HTML_Element* this_elm, HTML_Document* doc, int document_x, int document_y, int offset_x, int offset_y, ShiftKeyState modifiers, BOOL cancelled);

	/** ONDRAGOVER's default action. Should be called from HTML_Element::HandleEvent(ONDRAGOVER).
	 * @see HTML_Element::HandleEvent()
	 */
	void			HandleOnDragOverEvent(HTML_Document* frames_doc, int document_x, int document_y, int offset_x, int offset_y, BOOL cancelled);

	/** ONDRAGLEAVE's default action. Should be called from HTML_Element::HandleEvent(ONDRAGLEAVE).
	 * @see HTML_Element::HandleEvent()
	 */
	void			HandleOnDragLeaveEvent(HTML_Element* this_elm, HTML_Document* doc);

	/** ONDROP's default action. Should be called from HTML_Element::HandleEvent(ONDROP).
	 * @see HTML_Element::HandleEvent()
	 */
	void			HandleOnDropEvent(HTML_Document* doc, int document_x, int document_y, int offset_x, int offset_y, ShiftKeyState modifiers, BOOL cancelled);

	/** ONDRAGEND's default action. Should be called from HTML_Element::HandleEvent(ONDRAGEND).
	 * @see HTML_Element::HandleEvent()
	 */
	void			HandleOnDragEndEvent(HTML_Document* doc);

	/** Must be called when an element gets removed from the tree. */
	void			OnElementRemove(HTML_Element* elm);

	/** Must be called when a document gets unloaded. */
	void			OnDocumentUnload(HTML_Document* doc);

	/**
	 * Scrolls a viewport/scrollable during d'n'd if it has detected that the
	 * drag is falling into d'n'd scroll margin.
	 *
	 * Note: operates on the target document.
	 *
	 * @param x - the last d'n'd move x - in document coordinates.
	 * @param y - the last d'n'd move y - in document coordinates.
	 *
	 * See also DND_SCROLL_MARGIN, DND_SCROLL_INTERVAL, DND_SCROLL_DELTA_MIN and DND_SCROLL_DELTA_MAX
	 * tweaks.
	 *
	 * @return TRUE if anything was scrolled.
	 */
	BOOL			ScrollIfNeeded(int x, int y);

	/** Tries to move to the next events sequence. Must be called when one of d'n'd driving event is going to be lost. */
	void			MoveToNextSequence();

	/**
	 * Gets the drag listener of the drag origin view.
	 * @see CoreViewDragListener
	 */
	CoreViewDragListener*
					GetOriginDragListener();

	/**
	 * Sets the drag listener of the drag origin view.
	 * @see CoreViewDragListener
	 */
	void			SetOriginDragListener(CoreViewDragListener* origin_listener);

	/**
	 * Adds the given element to the list of dragged elements and to the feedback bitmap.
	 *
	 * @param doc - the target document.
	 * @param elm - the element to be added.
	 * @param elm_doc - the document the element is in.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR_NULL_POINTER on invalid parameter. OpStatus::OK otherwise.
	 */
	OP_STATUS		AddElement(HTML_Document* doc, HTML_Element* elm, HTML_Document* elm_doc);

	/**
	 * Sets the given element as the feedback element and create the feedback bitmap out of it.
	 *
	 * @param doc - the target document.
	 * @param elm - the element to be set.
	 * @param elm_doc - the document the element is in.
	 * @param point - bitmap's drag point (relative to bitmap's top left corner
	 * indicating a place within the bitmap the pointing device's cursor should be kept in
	 * while the bitmap is moved together with the cursor during the drag'n'drop operation.)
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR_NULL_POINTER on invalid parameter. OpStatus::OK otherwise.
	 */
	OP_STATUS		SetFeedbackElement(HTML_Document* doc, HTML_Element* elm, HTML_Document* elm_doc, const OpPoint& point);

	/** Checks if a given mime type is Opera's special one. */
	static BOOL		IsOperaSpecialMimeType(const uni_char* type);

	/**
	 * Sets the drag origin url in the passed in data store.
	 *
	 * @param drag_object - the data store where the origin url should be set.
	 * @param url - the origin url.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM. OpStatus::OK otherwise.
	 */
	static OP_STATUS
					SetOriginURL(OpDragObject* drag_object, URL& url);

	/**
	 * Gets the drag origin url from the passed in data store.
	 *
	 * @param drag_object - the data store the origin url should be got from.
	 *
	 * @return th origin url string.
	 */
	static const uni_char*
					GetOriginURL(OpDragObject* drag_object);

	/**
	 * Adds the passed in URL to the list of allowed URLs in the data store.
	 * The allowed URL is the URL which is allowed to be given the data.
	 *
	 * @param drag_object - the data store the allowed target url should be set in.
	 * @param url - the origin url.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM. OpStatus::OK otherwise.
	 */
	static OP_STATUS
					AddAllowedTargetURL(OpDragObject* drag_object, URL& url);

	/**
	 * Removes any limitation where dragged data can be dropped.
	 *
	 * @param drag_object - the data store where target url
	 * should be unlimited.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM. OpStatus::OK otherwise.
	 */
	static OP_STATUS
					AllowAllURLs(OpDragObject* drag_object);

	/**
	 * Checks if the passed in URL is on the list of allowed URLs in the data store.
	 *
	 * @param drag_object - the data store to be checked.
	 * @param url - the origin url.
	 *
	 * @return TRUE if url is allowed target or there's no limitation of URLs.
	 *         Otherwise FALSE.
	 */
	static BOOL		IsURLAllowed(OpDragObject* drag_object, URL& url);

	/**
	 * Gets offset relative to element's padding box.
	 *
	 * @param[in] elm - the element to which padding box the offset should be returned.
	 * @param[in] doc - the element's document.
	 * @param[in] x - the x document coordinate.
	 * @param[in] y - the y document coordinate.
	 * @param[out] offset_x - the x offset relative to element's padding box.
	 * @param[out] offset_y - the y offset relative to element's padding box.
	 */
	void			GetElementPaddingOffset(HTML_Element* elm, HTML_Document* doc, int x, int y, int& offset_x, int& offset_y);

	/**
	 * This must be called by HTML_Element::HandleEvent when ONMOUSEDOWN event is processed.
	 *
	 * @param doc - The document elm lives in.
	 * @param elm - ONMOUSEDOWN target element.
	 * @param cancelled - TRUE if the ONMOUSEDOWN's default action was prevented. FALSE Otherwise.
	 */
	void			OnMouseDownDefaultAction(FramesDocument* doc, HTML_Element* elm, BOOL cancelled);

	/** This must be called when ONMOUSEDOWN event is about to be sent to a page. */
	void			OnSendMouseDown();

	/** This must be called when ONMOUSEDOWN event is queued to be sent later on. */
	void			OnRecordMouseDown();
};

# endif // DRAG_SUPPORT

#endif // OPDRAGMANAGER_H
