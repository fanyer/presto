/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWS_OPDRAGMANAGER_H
#define WINDOWS_OPDRAGMANAGER_H

#include "core/pch.h"

#include "modules/pi/OpDragManager.h"
#include "adjunct/desktop_pi/DesktopDragObject.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/pi/WindowsOpDragObject.h"

#include "platforms/windows/utils/sync_primitives.h"

class OpBitmap;
class WindowsOpView;
class VegaMDEScreen;
class WindowsDropTarget;
class WindowsDNDData;

/**
 * Platform's ::DoDragDrop() is blocking execution, so here's new thread to
 * handle only platform's dnd, while main thread handle redraws and such.
 * Based on wingogi's WingogiDndExternalThread
 **/
class WindowsDragDropExternalThread
{
public:
	WindowsDragDropExternalThread();
	~WindowsDragDropExternalThread();
	OP_STATUS Start(VegaMDEScreen* screen, OpStringC origin_url);	//< This method must be called from single main thread only.
	BOOL IsRunning();						//< Returns TRUE if external dnd thread is run.
	void Finish();							//< This method must be called from single main thread only.

private:
	void Run();								//< Called internally after Start().
	static void* ThreadProc(void* ptr);		//< Called internally after Start().
	HANDLE m_thread;						//< Platform thread.
	DWORD m_owner_thread_id;				//< Id of thread from which Start() has been called.
	BOOL m_is_running;						//< TRUE if external dnd thread is running
	VegaMDEScreen* m_screen;				//< The screen that initiated the drag'n'drop
	OpString		m_origin_url;			// origin of the data
};

struct WindowsDragTemporaryData
{
	WindowsDragTemporaryData() 
		: drop_x(0)
		, drop_y(0)
		, drag_x(-1)
		, drag_y(-1)
		, drag_acknowledged(false)
	{

	}
	int drop_x;
	int drop_y;
	int drag_x;
	int drag_y;
	bool drag_acknowledged;					// set when a drag is _actually_ acknowledged as being active
	WindowsDragDropExternalThread dnd_thread;
};


class WindowsOpDragManager : public OpDragManager
{
public:

			WindowsOpDragManager();
	virtual ~WindowsOpDragManager();

	void				UpdateDrag(WindowsOpWindow *window);

	/** Start a drag operation */

	virtual void	StartDrag();
	virtual void	StopDrag(BOOL cancelled);
	virtual BOOL	IsDragging() { return m_drag_object != NULL; }
	virtual DesktopDragObject* GetDragObject() { return m_drag_object; }
	virtual void SetDragObject(OpDragObject* drag_object);

	void SetNotifyStart(bool val) { m_notify_start = val; }

	// register/unregister a IDropTarget instance with the given window
	OP_STATUS	RegisterDropTarget(HWND hwnd_target);
	void		UnregisterDropTarget(HWND hwnd_target);

	static OP_STATUS PrepareDragOperation(WindowsOpWindow* window, int x, int y);

	WindowsDropTarget* GetDropTarget() { return m_drop_target; }

	WindowsDNDData& GetDragData() { return m_data; }

	DesktopWindow*		GetSourceToplevelWindow() { return m_source_toplevel_win; }
	DesktopWindow*		GetSourceDesktopWindow() { return m_source_win; }

	// hide drag window without ending the drop
	void				HideDragWindow();

private:
	OP_STATUS			OpenFilesIfNotHandled(BOOL drop_cancelled, DesktopDragObject* drag_object, WindowsOpWindow *window);
	HWND				CreateDNDVisualFeedbackWindow(HWND parentHWND, RECT* size);

	HWND				m_window;
	DesktopDragObject*	m_drag_object;
	OpPoint				m_bitmap_offset;
	BOOL				m_has_image;
	bool				m_notify_start;
	WindowsDropTarget*	m_drop_target;
	WindowsDNDData		m_data;
	DesktopWindow*		m_source_toplevel_win;	// the BrowserDesktopWindow the drag was started from
	DesktopWindow*		m_source_win;			// the DesktopWindow (eg. DocumentDesktopWindow) the drag was started from
	ATOM				m_wndclass;			// ATOM set when the drag window class has been registered
};


#endif // WINDOWS_OPDRAGMANAGER_H
