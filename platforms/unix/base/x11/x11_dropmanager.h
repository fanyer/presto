/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_OPDROPMANAGER_H__
#define __X11_OPDROPMANAGER_H__

#include "platforms/unix/base/x11/x11_opdragmanager.h"
#include "platforms/unix/base/x11/x11_mdescreen.h"
#include "platforms/utilix/x11_all.h"
#include "modules/pi/OpView.h"

class X11Widget;
class DesktopDragObject;
class MDE_OpView;
class DragByteArray;

class X11DropManager : public X11OpDragManager::X11OpDragManagerListener, public X11MdeScreen::Listener
{
public:
	static OP_STATUS Create();
	static void Destroy();

	/**
	 * Inspects events and deals with them accordingly if event is
	 * part og drag-and-drop handshake
	 *
	 * @return TRUE if event is not intended for anyone else but the
	 *         drag-and-drop code. Otherwise FALSE
	 */
	BOOL HandleEvent(XEvent* event);

	/**
	 * Return the active drop object.
	 */
	DesktopDragObject* GetDropObject() const { return m_drop_object; }

	static OP_STATUS ConvertURL(OpString& src);
	/**
	 * Reads a desktop file and returns the url it specifies if any
	 *
	 * @param filename The desktop filename
	 * @param url On return the url read from the desktop file
	 *
	 * @return TRUE on success, otherwise FALSE
	 */
	static BOOL GetURLFromDesktopFile(const OpStringC& filename, OpString8& url);

	// X11OpDragManager::X11OpDragManagerListener
	virtual void OnDragObjectDeleted(DesktopDragObject* drag_object);
	virtual void OnDragEnded(DesktopDragObject* dragobject, bool cancelled);

	// X11MdeScreen::Listener
	virtual void OnDeleted(X11MdeScreen* mde_screen);

private:
	enum MimeDataState
	{
		MimeOk,
		MimeError,
		MimeTimeout
	};

private:
	X11DropManager();
	~X11DropManager();

	/**
	 * Collect provided MIME types from drag source
	 */
	void GetMimeTypes( XClientMessageEvent* event );

	/**
	 * Prepares internal drag object with data from the incoming drag message
	 *
	 * @return OpStatus::OK on success ortherwise and error. The drag object
	 *         should not be used in that case
	 */
	OP_STATUS MakeDropObject();

	/**
	 * Returns TRUE if mime_type has been received from drag source
	 *
	 * @param mime_type The requested mime type
	 *
	 * @return TRUE of received, otherwise FALSE
	 */
	BOOL HasMimeType(const OpStringC8& mime_type);

	/**
	 * Collect data from source based on mine type. The function will
	 * block for the specified number of milliseconds
	 *
	 * @param mime_type The mime type to query soure about
	 * @param data On return, received data from source
	 *
	 * @return MimeOk on success, MimeError on an error, MimeTimeout if the timeout was exceeded
	 */
	MimeDataState GetMimeData(const OpStringC8& mime_type, DragByteArray& data, UINT32 timeout_ms);

	/**
	 * Scans event queue until a SelectionNotify event arrives
	 *
	 * @return The property or the proerty from SelectionNotify, or X11Constants::None on timeout
	 */
	X11Types::Atom GetProperty(X11Types::Window window, UINT32 timeout_ms);

	/**
	 * Clear internal state
	 */
	void Reset();

	/**
	 * Called every time a position request is processed
	 */
	void SendXdndStatus(long action, BOOL always_send_position, const OpRect& rect);

	/**
	 * Called every time a drop has completed
	 */
	void SendXdndFinished(BOOL drop_accepted);

private:
	X11Types::Window m_drag_source;
	X11Widget* m_target;
	X11MdeScreen* m_mde_screen;
	X11Types::Time m_time;
	BOOL m_owns_drop_object;
	DesktopDragObject* m_drop_object;
	BOOL m_drop_accepted;
	BOOL m_url_drop_accepted;
	bool m_drag_is_outside_opera;
	ShiftKeyState m_shift_state;
	OpPoint m_drop_point;
	OpVector<OpString8> m_mime_types;
};


extern X11DropManager* g_drop_manager;


#endif // __X11_OPDROPMANAGER_H__
