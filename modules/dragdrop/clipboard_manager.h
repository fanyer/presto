/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CLIPBOARDMANAGER_H
# define CLIPBOARDMANAGER_H

# ifdef USE_OP_CLIPBOARD

class OpClipboard;

#  include "modules/pi/OpDragObject.h"
#  include "modules/dom/domeventtypes.h"
#  include "modules/util/OpHashTable.h"
#  include "modules/inputmanager/inputaction.h"

/**
 * The clipboard listener interface.
 * Has to be implemented by classes which need access the clipboard.
 *
 * @see Copy()
 * @see Cut()
 * @see Paste()
 * @see OpClipboard
 */
class ClipboardListener
{
public:
	virtual				~ClipboardListener() {}

	/**
	 * This is called by ClipboardManager in order to give
	 * the listener the possibility to paste the clipboard's
	 * content.
	 *
	 * @param clipboard - the clipboard to take the data to paste from.
	 */
	virtual void		OnPaste(OpClipboard* clipboard) = 0;

	/**
	 * This is called by ClipboardManager in order to give the
	 * listener the possibility to put its data into the clipboard.
	 * Moreover, if it's possible, the listener must remove the content
	 * it put in the clipboard from itself.
	 *
	 * @param clipboard - the clipboard to place the data in.
	 */
	virtual void		OnCut(OpClipboard* clipboard) = 0;

	/**
	 * This is called by ClipboardManager in order to give the
	 * listener the possibility to put its data into the clipboard.
	 *
	 * @param clipboard - the clipboard to place the data in.
	 */
	virtual void		OnCopy(OpClipboard* clipboard) = 0;

	/**
	 * This is called by ClipboardManager in order to inform the
	 * listener the clipboard operation has ended. The listener may
	 * clean up its internal state if required.
	 *
	 * @param clipboard - OpClipboard instance to access.
	 */
	virtual void		OnEnd() {}
};

/**
 * Clipboard manager to be used by core.
 * It hides OpClipboard inside and exposes its own clipboard API.
 * It's also responsible for sending clipboard DOM events (oncopy, oncut nad onpaste).
 */
class ClipboardManager
{
private:
	// Internal event data structure.
	struct ClipboardEventData
	{
		ClipboardEventData()
			: data_object(NULL)
			, listener(NULL)
			, token(0)
			, in_use(FALSE)
			, cleared(FALSE)
#  ifdef _X11_SELECTION_POLICY_
			, mouse_buffer(FALSE)
#  endif // _X11_SELECTION_POLICY_
		{}

		~ClipboardEventData()
		{
			OP_DELETE(data_object);
			if (listener)
				listener->OnEnd();
		}

		/** The event's data object. @see OpDragObject. */
		OpDragObject*		data_object;
		/** The clipboard listener. @see ClipboardListener. */
		ClipboardListener*	listener;
		/** The clipboard's token. */
		UINT32				token;
		/** Keeps track if the this ClipboardEventData is being used. */
		BOOL				in_use;
		/** The flag indicating if the content of the cleaboard should be cleared. */
		BOOL				cleared;
#  ifdef _X11_SELECTION_POLICY_
		/** The flag indicating if the clipboard is in the mouse buffer mode. @see SetMouseSelectionMode() */
		BOOL				mouse_buffer;
#  endif // _X11_SELECTION_POLICY_
	};

	/** Platform's clipboard. */
	OpClipboard*		m_pi_clipboard;
	/** Events data. */
	OpAutoVector<ClipboardEventData>
						m_data;

	/**
	 * Sends clipboard event to the given target element.
	 *
	 * @param type - The type of the event. One of ONCOPY, ONCUT or ONPASTE.
	 * @param doc - The document elm lives in.
	 * @param elm - The target element.
	 * @param token - Used to identify the content so that
	 * it could be used later to empty the clipboard content associated with it.
	 * @param listener - The listener to notify about the operation.
	 *
	 * @return OpStatus::ERR_NO_MEMORY in case of OOM. OpStatus::OK otherwise.
	 */
	OP_STATUS			SendEvent(DOM_EventType type, FramesDocument* doc, HTML_Element* elm, unsigned int token, ClipboardListener* listener = NULL);

	/** If all events data are unused it clears the data array. */
	void				DeleteAllIfUnused();
public:
	ClipboardManager() : m_pi_clipboard(NULL) {}
	~ClipboardManager();

	/** Initializes the manager i.e. creates an instance of OpClipboard. */
	OP_STATUS			Initialize();

	/**
	 * Executes oncopy clipboard action, i.e.:
	 * - if the passed in document is valid and the target element may be determined [1]
	 *   ONCOPY event is sent to it. In such case the passed in listener gets OnEnd() notification
	 *   synchronously and it's replaced with the global listener i.e. Window.
	 *   ONCOPY's default calls the globl listener's OnCopy() (synchronously or asynchronously)
	 *   and the global listener is responsible for propagating this event further to the active listener
	 *   (e.g. the listener of the focused form element). Note that the active listener may be different
	 *   than the passed in one due to e.g. focus changing in the ONCOPY handler.
	 *
	 * - if the passed in document is NULL or the target element is NULL and the proper target
	 *   can not be determined [1] the OnCopy() is called on the passed in listener synchronously
	 *   and it should put the selected content in the clipboard.
	 *
	 * [1] If the passed in element is not NULL it's used. Otherwise the target element
	 * is tried to be determined using the algorithm described in
	 * http://dev.w3.org/2006/webapi/clipops/clipops.html, paragraph 5.4.
	 *
	 * @param listener - The clipboard listener requesting the copy action.
	 * @param token - Used to identify the content so that
	 * it could be used later to empty the clipboard content associated with it.
	 * @param doc - The document the action is meant to be performed in.
	 * @param elm - The target element the action is meant to be performed on.
	 *
	 * @return OpStatus::ERR_NO_MEMORY in case of OOM. OpStatus::OK otherwise.
	 *
	 * @see ClipboardListener
	 */
	OP_STATUS			Copy(ClipboardListener* copy_listener, UINT32 token = 0, FramesDocument* doc = NULL, HTML_Element* elm = NULL);

	/**
	 * Executes oncut clipboard action, i.e.:
	 * - if the passed in document is valid and the target element may be determined [1]
	 *   ONCUT event is sent to it. In such case the passed in listener gets OnEnd() notification
	 *   synchronously and it's replaced with the global listener i.e. Window.
	 *   ONCUT's default calls the globl listener's OnCut() (synchronously or asynchronously)
	 *   and the global listener is responsible for propagating this event further to the active listener
	 *   (e.g. the listener of the focused form element). Note that the active listener may be different
	 *   than the passed in one due to e.g. focus changing in the ONCUT handler.
	 *
	 * - if the passed in document is NULL or the target element is NULL and the proper target
	 *   can not be determined [1] the OnCut() is called on the passed in listener synchronously
	 *   and it should put the selected content in the clipboard and remove it from itself.
	 *
	 * [1] If the passed in element is not NULL it's used. Otherwise the target element
	 * is tried to be determined using the algorithm described in
	 * http://dev.w3.org/2006/webapi/clipops/clipops.html, paragraph 5.4.
	 *
	 * @param listener - The clipboard listener requesting the cut action.
	 * @param token - Used to identify the content so that
	 * it could be used later to empty the clipboard content associated with it.
	 * @param doc - The document the action is meant to be performed in.
	 * @param elm - The target element the action is meant to be performed on.
	 *
	 * @return OpStatus::ERR_NO_MEMORY in case of OOM. OpStatus::OK otherwise.
	 *
	 * @see ClipboardListener
	 */
	OP_STATUS			Cut(ClipboardListener* cut_listener, UINT32 token = 0, FramesDocument* doc = NULL, HTML_Element* elm = NULL);

	/**
	 * Executes onpaste clipboard action, i.e.:
	 * - if the passed in document is valid and the target element may be determined [1]
	 *   ONPASTE event is sent to it. In such case the passed in listener gets OnEnd() notification
	 *   synchronously and it's replaced with the global listener i.e. Window.
	 *   ONPASTE's default calls the globl listener's OnPaste() (synchronously or asynchronously)
	 *   and the global listener is responsible for propagating this event further to the active listener
	 *   (e.g. the listener of the focused form element). Note that the active listener may be different
	 *   than the passed in one due to e.g. focus changing in the ONPASTE handler.
	 *
	 * - if the passed in document is NULL or the target element is NULL and the proper target
	 *   can not be determined [1] the OnPaste() is called on the passed in listener synchronously
	 *   and it should insert the clipboard's content to itself.
	 *
	 * [1] If the passed in element is not NULL it's used. Otherwise the target element
	 * is tried to be determined using the algorithm described in
	 * http://dev.w3.org/2006/webapi/clipops/clipops.html, paragraph 5.4.
	 *
	 * @param listener - The clipboard listener requesting the paste action.
	 * @param token - Used to identify the content so that
	 * it could be used later to empty the clipboard content associated with it.
	 * @param doc - The document the action is meant to be performed in.
	 * @param elm - The target element the action is meant to be performed on.
	 *
	 * @return OpStatus::ERR_NO_MEMORY in case of OOM. OpStatus::OK otherwise.
	 *
	 * @see ClipboardListener
	 */
	OP_STATUS			Paste(ClipboardListener* paste_listener, FramesDocument* doc = NULL, HTML_Element* elm = NULL);

	/** Unregisters the given listener. Must be called when the listener gets destroyed. */
	void				UnregisterListener(ClipboardListener* listener);

	/**
	 * Places the given url in the clipboard.
	 *
	 * @url - The url string.
	 * @param token - Used to identify the content so that
	 * it could be used later to empty the clipboard content associated with it.
	 * @param listener - The listener to notify about the operation.
	 *
	 * @return OpStatus::ERR_NO_MEMORY in case of OOM. OpStatus::OK otherwise.
	 */
	OP_STATUS			CopyURLToClipboard(const uni_char* url, UINT32 token = 0);

	/**
	 * Copies the image specified by the url to the clipboard.
	 *
	 * @param url - The url of the image to copy to the clipboard.
	 * @param token - Used to identify the content so that
	 * it could be used later to empty the clipboard content associated with it.
	 *
	 * @return OpStatus::ERR_NO_MEMORY in case of OOM. OpStatus::ERR if the url
	 * does not point to an image. OpStatus::OK otherwise.
	 */
	OP_STATUS			CopyImageToClipboard(URL& url, UINT32 token = 0);

	/**
	 * Handles the clipboard event's default action.
	 * It's called by HTML_Element::HandleEvent().
	 *
	 * @param event - The event's type.
	 * @param cancelled - TRUE if the event's default action was prevented by a script.
	 * @param id - The event's id.
	 *
	 * @return OpStatus::ERR_NO_MEMORY in case of OOM. OpStatus::OK otherwise.
	 */
	void				HandleClipboardEventDefaultAction(DOM_EventType event, BOOL cancelled, unsigned int id);

#  ifdef _X11_SELECTION_POLICY_
	/** Enables or disables mouse selection mode (set active clipboard).
	 *
	 * Some systems (e.g. X11 environments) sort of use two different
	 * clipboards; one used for current mouse selection (which
	 * automatically puts text on the clipboard when text is selected
	 * with the mouse), and another for other/regular selection
	 * mechanisms (the user selects text, and manually copies it to
	 * the clipboard by pressing Ctrl+C, selecting "Copy" from the
	 * menu, etc.).
	 *
	 * This method is used to specify which clipboard is to be the
	 * active one, i.e. which of the clipboards to manipulate when
	 * calling PlaceText(), GetText(), etc.
	 *
	 * Initially, mouse selection mode is disabled.
	 *
	 * @param mode If TRUE, select mouse selection clipboard. If FALSE
	 */
	void SetMouseSelectionMode(BOOL val);

	/**
	 * Is mouse selection enabled?
	 *
	 * @return The active clipboard, i.e. what has been set by
	 * SetMouseSelectionMode(). Return TRUE if mouse selection is
	 * enabled. Initially, mouse selection mode is disabled.
	 */
	BOOL GetMouseSelectionMode();
#  endif // _X11_SELECTION_POLICY

	/** Return TRUE if there is text in the clipboard. */
	BOOL				HasText();

#  ifdef CLIPBOARD_HTML_SUPPORT
	/**
	 * Is the current text in the clipboard HTML formatted?
	 *
	 * @return TRUE if there is HTML text on the clipboard.
	 */
	BOOL				HasTextHTML();
#  endif // CLIPBOARD_HTML_SUPPORT

	/**
	 * Empties the clipboard if the current content is associated with the token.
	 *
	 * @param token - the token of the clipboard content which should be cleared.
	 */
	OP_STATUS			EmptyClipboard(UINT32 token);

	/**
	 * Places a bitmap in the clipboard.
	 *
	 * @param bitmap - The bitmap to place in the clipboard.
	 * @param token - Used to identify the content so that
	 * it could be used later to empty the clipboard content associated with it.
	 */
	OP_STATUS			PlaceBitmap(const class OpBitmap* bitmap, UINT32 token = 0);

	/**
	 * It's called by DOM when DataTransfer.clearData() is called. It's needed to know
	 * whether the clipboard content is meant to be cleared.
	 *
	 * @param object - The event's data object.
	 */
	void				OnDataObjectClear(OpDragObject* object);

	/**
	 * It's called by DOM when DataTransfer.setData() is called. It's needed to know
	 * that the clipboard content isn't meant to be cleared.
	 *
	 * @param object - The event's data object.
	 */
	void				OnDataObjectSet(OpDragObject* object);

	/** Gets OpDragObject associated with the clipboard event based on event's id. */
	OpDragObject*		GetEventObject(unsigned int id);

	/**
	 * Returns TRUE if clipboard's input action must be enabled. This should be called
	 * when checking accessibility of copy, cut and paste input actions.
	 *
	 * @param action - Must be one of:
	 * - OpInputAction::ACTION_COPY,
	 * - OpInputAction::ACTION_CUT,
	 * - OpInputAction::ACTION_PASTE.
	 * @param doc - The document for which the action is checked.
	 * @param elm - The elm for which the action is checked.
	 * If it's NULL the proper element is tried to be be determined
	 * using the algorithm described in paragraph 5.4 in the clipboard events
	 * specification: http://dev.w3.org/2006/webapi/clipops/clipops.html.
	 */
	BOOL				ForceEnablingClipboardAction(OpInputAction::Action action, FramesDocument* doc, HTML_Element* elm = NULL);
};

# endif // DRAG_SUPPORT

#endif // OPDRAGMANAGER_H
