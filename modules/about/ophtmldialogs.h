/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef HTML_DIALOGS_H
#define HTML_DIALOGS_H

#ifdef ABOUT_HTML_DIALOGS

#include "modules/dom/domenvironment.h"
#include "modules/url/url2.h" // for URL

class HTML_Dialog;


class HTMLDialogData
{
public:
	/** Constructor which will set everything to a known value */
	HTMLDialogData() : identifier(0), modal(TRUE),
		width(0), height(0) {}

	/** Destructor that will delete all allocated data */
	virtual ~HTMLDialogData();

	/** Will copy the member data of the object */
	virtual OP_STATUS DeepCopy(HTMLDialogData *src);

	/** Message ID, used for localization. */
	int			identifier;

	/** URL to the dialog HTML template. */
	URL			html_template;

	/** Set to TRUE if the dialog is to be modal to the document */
	BOOL		modal;

	/** The preferred width of the content of the dialog. */
	int			width;

	/** The preferred height of the content of the dialog. */
	int			height;
};

/**
 * \brief Callback function which Opera passes to asynchronous dialogs so that they can
 * return values. Set result to NULL if they are not to contain any values.
 */
class AsyncHTMLDialogCallback
{
public:
	typedef enum
	{
		HTML_DIALOG_TYPE_JS,
		HTML_DIALOG_TYPE_UNKNOWN
	} HTMLDialogCallbackType;

	virtual OP_STATUS	OnClose(const uni_char *result) = 0;

	virtual HTMLDialogCallbackType	GetCallbackType() = 0;
};

class AsyncHTMLDialogData : public HTMLDialogData
{
public:
	/** Will copy the member data of the object */
	virtual OP_STATUS DeepCopy(AsyncHTMLDialogData *src);

	/** Callback handle. */
	AsyncHTMLDialogCallback*	callback;
};

/**
 * Class for managing open dialogs.
 *
 * Hold a list of all open dialogs and provides utility functions for creating dialogs.
 */
class HTMLDialogManager
{
public:
	HTMLDialogManager() {}
	~HTMLDialogManager();

    void			Add(HTML_Dialog *dlg);
	/**
	 * Closes the dialog and its window and removes it from the list of dialogs
	 */
	void			Remove(HTML_Dialog *dlg);

	/**
	 * Finds the dialog related to window.
	 * @returns A dialog related to window, NULL if none could be found.
	 */
	HTML_Dialog*	FindDialog(Window *win);

	/**
	 * Activates window child dialogs and deactivates other windows child dialogs.
	 * @param win Window whos child dialogs should be activated.
	 */
	void			ActivateDialogs(Window *win);

	/**
	 * Cancels and destroys open dialogs that has window as parent.
	 * @param win Dialogs with this window as parent will be cancelled and destroyed, if NULL all dialogs will
	 *			  be cancelled and destroyed.
	 */
	void			CancelDialogs(Window *win = NULL);


	/** Utility function for creating asyn dialog */
	OP_STATUS		OpenAsyncDialog(Window *opener, AsyncHTMLDialogData *data);

private:
	AutoDeleteHead	m_dialoglist;   //< List of open dialogs
};

/**
 *
 */
class HTML_Dialog : public Link
{
public:

	HTML_Dialog() : m_opener(NULL), m_win(NULL), m_result(NULL) {}
	virtual	~HTML_Dialog();

	OP_STATUS			Construct(URL &template_url,
								Window *opener,
								HTMLDialogData *data);

	Window*				GetWindow() { return m_win; }
	Window*				GetOpener() { return m_opener; }
	const uni_char*		GetResult() { return m_result; }
	OP_STATUS			SetResult(const uni_char *result);

	virtual OP_STATUS	CloseDialog() = 0;

protected:

	uni_char*			m_result;
	HTMLDialogData		m_data;

private:

	Window*				m_opener;
	Window*				m_win;
};

/**
 * An asyncronous dialog. This includes standard message boxes, password dialogs,
 * certificate dialogs and filechooser dialogs.
 */
class HTML_AsyncDialog : public HTML_Dialog
{
public:
	HTML_AsyncDialog() : HTML_Dialog() {};
	virtual ~HTML_AsyncDialog() {}

	OP_STATUS		Construct(URL &template_url, Window *opener, AsyncHTMLDialogData *data);

	virtual OP_STATUS	CloseDialog();

	/**
	 * Returns the callback set when created with
	 */
	AsyncHTMLDialogCallback* GetCallback() { return m_callback; }

private:

	/** Callback handle. */
	AsyncHTMLDialogCallback*	m_callback;
};

#define g_html_dialog_manager	(g_opera->about_module.m_html_dialog_manager)

#endif // ABOUT_HTML_DIALOGS

#endif // HTML_DIALOGS_H
