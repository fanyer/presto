/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_OPCLIPBOARD_H__
#define __X11_OPCLIPBOARD_H__

#include "modules/pi/OpClipboard.h"

#include "platforms/utilix/x11types.h"
#include "platforms/utilix/x11_all.h" // For None

#include <semaphore.h>

/**
 * Container for incremental data transfer if the full size extends
 * the maximum size supported by the server
 */
class IncrementalTransfer
{
public:
	IncrementalTransfer():data(0) {}
	~IncrementalTransfer() {Empty();}

	void Empty() { OP_DELETEA(data); data=0; }

	OP_STATUS Set(const UINT8* src, UINT32 src_size)
	{
		if (data)
			return OpStatus::ERR;
		data = OP_NEWA(UINT8,src_size);
		if (!data)
			return OpStatus::ERR_NO_MEMORY;
		memcpy(data, src, src_size);
		size = src_size;
		return OpStatus::OK;
	}
public:
	UINT8* data;
	UINT32 size;
	UINT32 offset;
	UINT32 blocksize;
	int format;
	time_t last_modified;
	X11Types::Atom property;
	X11Types::Atom target;
	X11Types::Window window;
};

/**
 * When sourcing an image we store and export the image data in BMP format
 */
class ClipboardImageItem
{
public:
	ClipboardImageItem()
		:m_buffer(0), m_size(0)
		{}
	~ClipboardImageItem() { Empty(); }

	void Empty()
	{
		OP_DELETEA(m_buffer);
		m_buffer = 0;
		m_size = 0;
	}

	OP_STATUS MakeBMP(const OpBitmap* bitmap);

public:
	UINT32* m_buffer;
	UINT32 m_size; // In bytes
};

/**
 * Shared data container used by both the Opera thread and the clipboard worker thread
 */
class X11ClipboardWorkerData
{
public:
	X11ClipboardWorkerData()
		: m_worker(NULL), m_window(None), m_is_mouse_selection(false), m_target(NULL), m_detach_failed(false)
		{}

public:
	class X11ClipboardWorker* m_worker;
	X11Types::Window m_window;
	pthread_t m_worker_id;
	int m_pipe[2];
	OpString m_text[2]; // [0] PRIMARY (mouse selection), [1] CLIPBOARD
	OpString m_html_text;
	ClipboardImageItem m_image;
	bool m_is_mouse_selection;
	OpString* m_target; // Pointer to text buffer that X11OpClipboard::GetText() returns to caller
	int m_has_text;
	bool m_detach_failed;
	sem_t m_semaphore;
	pthread_mutex_t m_mutex;
};


/**
 * The worker thread implememts the copy-paste protocol and can serve data
 * to other processes and programs without cooperation from the opera thread
 */
class X11ClipboardWorker
{
private:
	class TimeLimit
	{
	public:
		/**
		 * Ininitalize object with specified limit
		 */
		TimeLimit(UINT32 seconds):m_seconds(seconds),m_timeout(FALSE) {}

		/**
		 * Returns the limit
		 */
		UINT32 GetSeconds() const { return m_seconds; }

		/**
		 * Indicate that timeout has occured
		 */
		void SetTimedOut() { m_timeout=TRUE; }

		/**
		 * Returns TRUE if timeout has occured
		 */
		BOOL HasTimedOut() const { return m_timeout; }

	private:
		UINT32 m_seconds;
		BOOL m_timeout;
	};

public:
	/**
	 * Start point for the worker thread. Used by pthread_create() from the opera thread.
	 *
	 * @param data Must hold a pointer to X11ClipboardWorkerData where the 'm_worker' must
	 *        be valid
	 */
	static void* Init(void* data);

	/**
	 * Prepares a X11ClipboardWorker instance.
	 *
	 * @param data Reference to a X11ClipboardWorkerData object which must be valid as long as the worker exists
	 */
	X11ClipboardWorker(X11ClipboardWorkerData& data): m_data(data), m_remote_has_data(-1), m_remote_test_time(X11Constants::XCurrentTime) {}

private:
	/**
	 * The main function of the worker thread, analogous to main() in the main thread.
	 * Opens its own connection to the display and creates its own hidden window for
	 * the clipboard communication protocol.  Then runs a message loop, invoking
	 * @ref HandleEvent and @ref HandleMessage to handle the events and messages it receives.
	 *
	 * Will terminate with pthread_exit() on fatal errors (if it fails to open the display
	 * or create the window), or when instructed to do so by the main thread.
	 */
	void Exec();

	/**
	 * Performs the copy/paste handshake when a remote consumer
	 * requests data from our clipboard
	 *
	 * @param event Event data
	 *
	 * @param returns TRUE if event was consumed, otherwise FALSE
	 */
	bool HandleEvent(XEvent* event);

	/**
	 * Processes messages from the opera main thread
	 *
	 * @param message Zero terminated string
	 *
	 * @return true if the message ordered the thread to terminate, otherwise false
	 */
	bool HandleMessage(const char* message);

	/**
	 * Return true if our clipboard is the current system wide owner of the given selection
	 *
	 * @param atom The selection key. Should be XA_PRIMARY or CLIPBOARD
	 *
	 * @return See above
	 */
	bool OwnsSelection(X11Types::Atom atom);

	/**
	 * Return an atom that describes what clipboard to use. Can be XA_PRIMARY or CLIPBOARD
	 */
	X11Types::Atom GetCurrentClipboard();

	/**
	 * Converts the clipboard data given by 'selection' according to
	 * 'target' and set the property 'property' on the window 'window'
	 * to the resulting data.
	 *
	 * This will also start an incremental transfer (property type
	 * INCR) if needed.
	 *
	 * @param selection Which selection is requested (probably
	 * XA_PRIMARY or CLIPBOARD).
	 *
	 * @param target Which format to convert the clipboard data to
	 * before transferring it.  "Format" has a rather wide definition,
	 * e.g. the TARGETS target "converts" the clipboard data to a list
	 * of formats that the clipboard data can be converted to.
	 *
	 * @param property The property where the clipboard data should be
	 * written to (on the window given by 'window').
	 *
	 * @param display The display to use for setting the property.
	 *
	 * @param window The window where the clipboard data should be
	 * written to (on the property given by 'property').
	 *
	 * @param ret_target_used Returns the target which was actually
	 * used for data conversion.  Usually this will return 'target',
	 * but there may be exceptions (e.g. when 'target' is TEXT).
	 *
	 * @return A successful OP_STATUS if successful (in which case the
	 * caller probably should send a SelectionNotify to the requestor
	 * to inform about the success).  An error OP_STATUS if
	 * unsuccessful (in which case the caller probably should send a
	 * SelectionNotify to the requestor to inform about the failure).
	 */
	OP_STATUS PutSelectionProperty(X11Types::Atom selection, X11Types::Atom target, X11Types::Atom property, X11Types::Display* display, X11Types::Window window, X11Types::Atom* ret_target_used);

	/**
	 * Determine which encoding we should use when transferring the data in the clipboard
	 * from the current selection owner. Use @ref GetProperty to retrieve the actual text
	 * This function can block until specified timeout expires.
	 *
	 * @param allow_fallback If TRUE, return UTF8_STRING as a fallback if selection
	 *        owner does not support a format we recognize
	 * @param time_limit Specifies how long a request can last before timeout occurs
	 *        To check whether the method timed out, use TimeLimit::HasTimedOut() member
	 *        function on return.
	 *
	 * @return None on timeout or unsupported format, otherwise an atom that
	 *         @ref GetProperty can use
	 */
	X11Types::Atom GetTarget(BOOL allow_fallback, TimeLimit& time_limit);

	/**
	 * Waits for a SelectionNotify event and returns the property atom of that
	 * event. This function will also process SelectionRequest events as the
	 * sender of the SelectionNotify event might be Opera itself. A SelectionNotify
	 * can not be send if the SelectionRequest is not processed.
	 *
	 * This function will block UI and all other event processing
	 *
	 * @param time_limit Specifies how long a request can last before timeout occurs
	 *        To check whether the method timed out, use TimeLimit::HasTimedOut() member
	 *        function on return.
	 *
	 * @return The property atom of the SelectionNotify event or None on timeout. Note
	 *         that the property atom of the SelectionNotify event can be None as well.
	 */
	X11Types::Atom WaitForSelectionNotify(TimeLimit& time_limit);

	/**
	 * Detailed description as I (espen) tend to forget how this works.
	 *
	 * Sends a SelectRequest event to the owner of the current active clipboard
	 * selection (determined by @ref GetCurrentClipboard()) asking for an atom
	 * containing data of the type described in the target.
	 *
	 * Opera uses the atom OPERA_CLIPBOARD as the property identifier. This property
	 * is sent to the selection owner along with the target and a window identifier
	 * (m_window). The selection owner will save requested data in that property.
	 *
	 * If the target atom is of the special type TARGETS then the selection owner will
	 * send a list of supported data formats as atoms. Those atoms can in turn be used as
	 * the target argument in a second call to this function to get the actual text in
	 * the format the atom specifies.
	 *
	 * The selection owner must when receiving the request 1) store data in
	 * the property in the requesting window and 2) send a SelectionNotify event to
	 * that window. Due to the asynchronous nature there is a timeout of five seconds.
	 * Opera will block until the SelectionNotify is received or the timeout occurs.
	 *
	 * @param target An atom describing the requested format of data stored in the
	 *        returned property.
	 * @param time_limit Specifies how long a request can last before timeout occurs
	 *        To check whether the method timed out, use TimeLimit::HasTimedOut() member
	 *        function on return.
	 *
	 * @return The property identifier atom set in the SelectionNotify event. This
	 *         should be the same as OPERA_CLIPBOARD but use the value from the
	 *         event. None is returned if a timeout or error occured. The atom can
	 *         be used as the property argument in XGetWindowProperty() to retrieve
	 *         the data set by the selection owner.
	 */
	X11Types::Atom GetProperty(X11Types::Atom target, TimeLimit& time_limit);

	/**
	 * Retrive text from a remote process that owns the selection.
	 *
	 * @param text On return the text
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS GetText(OpString& text);

	/**
	 * Returns the clipboard content of the specified selection
	 *
	 * @param atom The selection key. Shold be XA_PRIMARY or CLIPBOARD
	 *
	 * @return See above
	 */
	OpString& GetSelectionContent(X11Types::Atom atom);

	/**
	 * Return true if the current clipboard data can be served as formatted html
	 */
	bool HasTextHTML();

	/**
	 * Add an item to the list of incremental transfers
	 *
	 * @param data The data the item can transfer
	 * @param size Size of data in bytes
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS AddTransfer(unsigned char* data, UINT32 size, IncrementalTransfer*& inc);

	/**
	 * Locate a transfer item in the list of incremental transfers
	 *
	 * @param window Each item is associated with a window id. The parameter
	 *        is used to find a match
	 *
	 * @return the matched item or 0 on no match
	 */
	IncrementalTransfer* FindTransfer(X11Types::Window window);

	/**
	 * Remove transfers that show no sign of activity. A transfer that has not
	 * been updated for 5 seconds is considered inactive.
	 */
	void CleanupTransfers();


private:
	X11Types::Display* m_display;
	X11ClipboardWorkerData& m_data;
	int m_remote_has_data;
	X11Types::Time m_remote_test_time;
	OpAutoVector<IncrementalTransfer> m_incremental_transfers;
};



class X11OpClipboard : public OpClipboard
{
public:
	static X11OpClipboard* s_x11_op_clipboard;

	X11OpClipboard();
	~X11OpClipboard();

	virtual BOOL HasText();
	virtual OP_STATUS PlaceText(const uni_char *text, UINT32 token);
	virtual OP_STATUS GetText(OpString &text);
	virtual BOOL HasTextHTML();
	virtual OP_STATUS PlaceTextHTML(const uni_char* htmltext, const uni_char* text, UINT32 token);
	virtual OP_STATUS GetTextHTML(OpString &text);
	virtual OP_STATUS PlaceBitmap(const class OpBitmap *bitmap, UINT32 token);
	virtual void SetMouseSelectionMode(BOOL mode);
	virtual BOOL GetMouseSelectionMode() { return m_is_mouse_selection; }
	/**
	 * Empties the clipboard. To be used in situations where
	 * we for security reasons want to prevent data to be
	 * retrieved from the clipboard.
	 *
	 * Note: System wide clipboard managers might break this form of
	 * security.
	 *
	 * @param token
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR on error
	 */
	virtual OP_STATUS EmptyClipboard(UINT32 token);


	// Implementation specific methods:

	/**
	 * Prepare the clipboard handler.
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS Init();

	/**
	 * This function is only present for developer/logging purposes. An OP_ASSERT
	 * will warn about events that arrive in the main thread instead of the
	 * intended worker thread.
	 *
	 * @param event Event data
	 *
	 * @return Always FALSE
	 */
	BOOL HandleEvent(XEvent* event);

	/**
	 * Perform code for SAVE_TARGETS handling if such a protocol
	 * is supported by the system. If supported this means talking to
	 * a remote application. That means this function might block for
	 * a short while.
	 * http://www.freedesktop.org/wiki/ClipboardManager
	 */
	void OnExit();

private:
	OP_STATUS WriteCommand(OpStringC8 cmd);
	OP_STATUS WaitForSemaphore(unsigned int timeout_in_seconds);

	/**
	 * Return the atom that is associated with current clipboard mode
	 *
	 * @return The atom. Can be XA_PRIMARY or "CLIPBOARD"
	 */
	X11Types::Atom GetCurrentClipboard();

private:
	X11ClipboardWorkerData m_data;
	UINT32 m_owner_token;
	bool m_is_mouse_selection; // Opera thread only to avoid locking
};



#endif // __X11_OPCLIPBOARD_H__
