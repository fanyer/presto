/**
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef PLUGIXPARENTWINDOWLISTENER_H_
#define PLUGIXPARENTWINDOWLISTENER_H_

class OpWindow;
class UnixOpPluginAdapter;

/**
 * Listens to re-parenting events.
 *
 * Plug-ins on *nix need the X11 id of their mother window in order to create
 * and retrieve input events. Parent-changed events occur when a tab is torn
 * from its window and put into another window.
 *
 * This is part of a really, REALLY ugly hack in order to listen to window
 * re-parenting events. It stems from the fact that windows in general can
 * only have one window listener. Overwriting this listener is, of course,
 * not without fatal consequences. Desktop solved this by implementing a
 * proxy listener that other listeners can subscribe to. Since Lingogi
 * currently does not support tearing out tabs, and given the time constraint
 * imposed on us for the OOPP task, we simply do not have the resources
 * necessary to implement multiple listeners support for OpWindow.
 *
 * This bug is now filed as CORE-43867.
 *
 * Retrieving the X11 window id from a OpWindow is platform-specific and should
 * be done in this class. As an example, you can perform the following
 * operations to get the X11 id of the parent window on desktop:
 *
 * \code
 * DesktopWindow* GetParent(OpWindow* window)
 * {
 * 	MDE_OpWindow* win = static_cast<MDE_OpWindow*>(window);
 * 	MDE_OpView* view = static_cast<MDE_OpView*>(win->GetParentView());
 *  win = view->GetParentWindow();
 * 	return static_cast<DesktopOpWindow*>(win)->GetDesktopWindow();
 * }
 * \endcode
 *
 * An instance of this listener will be created by UnixOpPluginAdapter. On
 * parent changed events, UnixOpPluginAdapter::OnParentChanged() is called in
 * order to notify the plug-in process of its new parent's X11 id.
 *
 * Implementations must listen to all window events, not only parent changed
 * events. If the plug-in window is closed for any reason, it is important to
 * remove the listener instance from the window event handler.
 */
class PlugixParentWindowListener
{
public:
	static OP_STATUS Create(PlugixParentWindowListener** listener,
	                        UnixOpPluginAdapter* adapter);

	virtual ~PlugixParentWindowListener() { }

	/**
	 * Set plug-in parent window.
	 *
	 * Implementations should take the OpWindow and retrieve the "real" parent
	 * window. See class documentation.
	 *
	 * @param parent  Plug-in window's current parent window.
	 *
	 * @return See OpStatus.
	 */
	virtual OP_STATUS SetParent(OpWindow* parent) = 0;
};

#endif  // PLUGIXPARENTWINDOWLISTENER_H_
