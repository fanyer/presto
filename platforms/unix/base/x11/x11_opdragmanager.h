/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/
#ifndef __X11_OPDRAGMANAGER_H__
#define __X11_OPDRAGMANAGER_H__

#include "modules/pi/OpDragManager.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/util/adt/oplisteners.h"

#include "adjunct/desktop_pi/DesktopDragObject.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "platforms/utilix/x11types.h"


extern class X11OpDragManager* g_x11_op_dragmanager;

class X11DragWidget;
class X11Widget;

/**
 * Note: For our widgets to become drop aware, use X11Widget::SetAcceptDrop(bool)
 */



// Helper class for data interchange
class DragByteArray
{
public:
	DragByteArray() { m_buffer = 0; m_size=0; }
	~DragByteArray() { OP_DELETEA(m_buffer); }


	OP_STATUS Set(const unsigned char* data, unsigned int size)
	{
		OP_DELETEA(m_buffer);
		m_size = 0;

		m_buffer = OP_NEWA(unsigned char, size);
		if (!m_buffer)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		m_size = size;

		if (data)
		{
			op_memcpy(m_buffer, data, m_size);
		}

		return OpStatus::OK;
	}

	OP_STATUS SetWithTerminate(const unsigned char* data, unsigned int size)
	{
		OP_DELETEA(m_buffer);
		m_size = 0;

		m_buffer = OP_NEWA(unsigned char, size+1);
		if (!m_buffer)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		m_size = size;

		if (data)
		{
			op_memcpy(m_buffer, data, m_size);
		}
		m_buffer[m_size]=0;

		return OpStatus::OK;
	}


	unsigned char* GetData() const { return m_buffer; }
	unsigned int GetSize() const { return m_size; }

private:
	unsigned char* m_buffer;
	unsigned int m_size;
};



class X11OpDragObject : public DesktopDragObject
{
public:
	X11OpDragObject(OpTypedObject::Type type)
		: DesktopDragObject(type){}

	virtual DropType GetSuggestedDropType() const;
};

class X11OpDragManager
	: public OpDragManager
	, private OpTimerListener
	, private DesktopWindowCollection::Listener
{

public:
	class X11OpDragManagerListener
	{
	public:
		virtual ~X11OpDragManagerListener() {}
		virtual void OnDragObjectDeleted(DesktopDragObject* drag_object) = 0;
		virtual void OnDragEnded(DesktopDragObject* dragobject, bool canceled) = 0;
	};

private:
	class OpExtRect : public OpRect
	{
	public:
		OpExtRect()
			:OpRect()
			,valid(FALSE)
			{}
		void SetValid(BOOL v) { valid = v; }
		BOOL IsValid() const { return valid; }

	public:
		BOOL valid;
	};

	class OpExtPoint : public OpPoint
	{
	public:
		OpExtPoint()
			:OpPoint()
			,valid(FALSE)
			,screen(0)
			{}
		void Set(int x, int y, int s) {OpPoint::Set(x,y); screen = s; }
		void SetValid(BOOL v) { valid = v; }
		BOOL IsValid() const { return valid; }

	public:
		BOOL valid;
		int screen;
	};

	enum TimerAction
	{
		Idle,
		Start,
		WaitForStatus_OrSendLeave,
		WaitForStatus_OrExit,
		WaitForFinish_OrExit
	};

public:
	X11OpDragManager();

	/**
	 * Returns our supported version of the Xdnd protocol
	 */
	static int GetProtocolVersion() { return 5; }

	BOOL IsDragging();
	BOOL StateMachineRunning();

	void StartDrag();
	void StopDrag(BOOL canceled);
	void SetDragObject(OpDragObject* drag_object);

	// OpTimerListener

	void OnTimeOut(OpTimer* timer);

	// DesktopWindowCollection::Listener

	void OnDesktopWindowAdded(DesktopWindow* window);
	void OnDesktopWindowRemoved(DesktopWindow* window);
	void OnDocumentWindowUrlAltered(DocumentDesktopWindow* document_window, const OpStringC& url) {}
	void OnCollectionItemMoved(DesktopWindowCollectionItem* item,
							   DesktopWindowCollectionItem* old_parent,
							   DesktopWindowCollectionItem* old_previous) {}

	// Implementation specific methods:

	/**
	 * Starts a drag sequence. This function starts a modal event loop
	 * and will not return until the drag has been completed or aborted.
	 * Use @ref GetIsDragging() elsewhere if needed to determine if a
	 * drag is active
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS StartDrag(X11Types::Window window);

	/**
	 * Tests window for drop proxy. Returns the proxy window if the
	 * window specifies one, otherwise None
	 *
	 * @param window Window to test
	 *
	 * @return The proxy window or None
	 */
	X11Types::Window GetProxy(X11Types::Window window);

	/**
	 * Inspects events and deals with the accordingly if event is
	 * part og drag-and-drop handshake
	 *
	 * @return TRUE if event is not intended for anyone else but the
	 *         drag-and-drop code. Otherwise FALSE
	 */
	BOOL HandleEvent(XEvent *event);

	/**
	 * Looks up the incoming event queue and handles one event of the specified
	 * type if any.
	 *
	 * @param type Event type
	 *
	 * @return TRUE if there was an event that got processed properly
	 */
	BOOL HandleOneEvent(int type);

	/**
	 * Returns the current dragged object
	 */
	DesktopDragObject* GetDragObject();

	/**
	 * Adds drag manager listener.
	 *
	 * @param listener The listener
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS AddListener(X11OpDragManagerListener* listener) { return m_listeners.Add(listener); }

	DocumentDesktopWindow* GetSourceWindow() const { return m_drag_source_window; }

	/**
	 * Removes drag manager listener
	 *
	 * @param listener The listener
	 */
	OP_STATUS RemoveListener(X11OpDragManagerListener* listener) { return m_listeners.Remove(listener); }

	/**
	 * Determines the child window that contains the mouse pointer and sends
	 * a MotionNotify (mouse move) to that window. This will update the cursor
	 * shape should the window be an Opera window.
	 */
	static void GenerateMouseMove();

private:
	/**
	 * Starts a drag and does not return until oprations has been completed or stopped
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS Exec();

	/**
	 * Reset state, inform listener and clean up. Called every time drag has ended, either
	 * with a drop or after being canceled.
	 *
	 * @param canceled. If true, the drag was aborted
	 */
	void OnStopped(BOOL canceled);

	/**
	 * Called every time a SelectionRequest with a selection member set to the XdndSelection atom
	 * Prepates mime data and assigns it to the window property that is specified in the event
	 *
	 * @param event The selection event
	 */
	void HandleSelectionRequest(XEvent* event);

	/**
	 * Called every time the mouse has been moved while a drag is in progress. Send an
	 * XdndPosition request to the target when appropriate
	 *
	 * @param x Global X mouse coordiate
	 * @param y Global Y mouse coordiate
	 * @param screen The screen the mouse moved upon
	 * @param Sends an XdndPosition request to target when we normally would not have done so
	 */
	void HandleMove(int x, int y, int screen, BOOL force);

	/**
	 * Perform a tab drop (detach the tab) if the dragged type is OpTypedObject::DRAG_TYPE_WINDOW
	 * as the specified position
	 *
	 * @param x Global X mouse coordiate
	 * @param y Global Y mouse coordiate
	 *
	 * @return TRUE if one of more tabs were detached, otherwise FALSE
	 */
	BOOL HandleWindowDrop(int x, int y);

	/**
	 * Terminate the current drag
	 */
	void OnEscapePressed();

	/**
	 * Sets the XdndActionList on the source window
	 */
	void SetActionList();

	/**
	 * Called every time a new drop window is detected
	 *
	 * @param pos The global mouse position
	 */
	void SendXdndEnter(const OpPoint& pos);

	/**
	 * Called every time the mouse position must be transferred to the target
	 *
	 * @param pos The global mouse position
	 */
	void SendXdndPosition(const OpPoint& pos);

	/**
	 * Called every time the mouse leaves the current drop window
	 */
	void SendXdndLeave();

	/**
	 * Called when source wants target to use data
	 */
	void SendXdndDrop();

	/**
	 * Examines window for XdndAware atom and if present return the Xdnd version
	 * number it holds
	 *
	 * @param window The window to test
	 *
	 * @return The version number or -1 if no XdndAware atom was present
	 */
	int GetXdndVersion(X11Types::Window window);

	/**
	 * Creates the drag widget that is shown near the mouse cursor.
	 * The drag is stopped should the window creation fail.
	 *
	 * @param screen The screen where the window shall be shown.
	 *
	 * @return OpStatus::OK on success, otherwise an error
	 */
	OP_STATUS MakeDragWidget(int screen);

	/**
	 * Sets the cursor shape
	 */
	void UpdateCursor(BOOL accept);

	/**
	 * Saved current cursor shape
	 */
	void SaveCursor();

	/**
	 * Restores cursor shape
	 */
	void RestoreCursor();

	/**
	 * Returns supported mime types of the current dragged object
	 *
	 * @param mime_types On return the list of mime types
	 *
	 * @return OpStatus::OK of success, otherwise an error code
	 */
	OP_STATUS GetProvidedMimeTypes(OpVector<OpString8>& mime_types);

	/**
	 * Returns the provided data for the mime_type
	 *
	 * @param mime_type The requested mime type
	 * @param On return the data in the dragged object associated with the mime type
	 *
	 * @return OpStatus::OK of success, otherwise an error code
	 */
	OP_STATUS GetMimeData(const char* mime_type, DragByteArray& buf);

private:
	OpAutoPtr<DesktopDragObject> m_drag_object;
	OpAutoPtr<X11DragWidget> m_drag_widget;
	int m_remote_version;
	OpAutoPtr<OpTimer> m_timer;
	TimerAction m_timer_action;
	BOOL m_canceled_with_keypress;
	BOOL m_wait_for_status;
	BOOL m_force_send_position;
	BOOL m_has_ever_received_status;
	BOOL m_always_send_position;
	BOOL m_drop_sent;
	BOOL m_drag_accepted;
	BOOL m_send_drop_on_status_received;
	BOOL m_send_leave_on_status_received;
	BOOL m_can_start_drag;
	BOOL m_is_stopping;
	bool m_is_dragging;
	X11Types::Window m_current_target;
	X11Types::Window m_current_proxy_target;
	X11Types::Atom m_current_action;
	int m_start_screen;
	int m_current_screen;
	OpExtRect m_nochange_rect;
	OpExtPoint m_cached_pos;
	OpListeners<X11OpDragManagerListener> m_listeners;
	DocumentDesktopWindow* m_drag_source_window;
};

#endif // __X11_OPDRAGMANAGER__
