/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef AUTOCOMPLETEPOPUP_H
#define AUTOCOMPLETEPOPUP_H

#include "modules/forms/form_his.h"
#include "modules/widgets/WidgetWindow.h"
#include "modules/hardcore/timer/optimer.h"

#include "modules/url/url2.h"
#include "modules/hardcore/mh/messobj.h"

class AutoCompleteListboxListener;
class OpView;
class OpWindow;
class OpFont;
class OpInputAction;
class OpListBox;
class AutoCompletePopup;

enum AUTOCOMPL_TYPE {
	AUTOCOMPLETION_OFF,
	/** Fetches data from the personal info database */
	AUTOCOMPLETION_PERSONAL_INFO,
	/** Fetches data from history */
	AUTOCOMPLETION_HISTORY,
	AUTOCOMPLETION_CONTACTS,
	AUTOCOMPLETION_ACTIONS,
	AUTOCOMPLETION_GOOGLE,
	/** Suggest something good for a form on a web page */
	AUTOCOMPLETION_FORM,
	AUTOCOMPLETION_M2_MESSAGES
};

/** Window that comes up with list of choices when user types. */

class AutoCompleteWindow : public WidgetWindow
{
public:
	AutoCompleteWindow(AutoCompletePopup* autocomp);
	~AutoCompleteWindow();

	// Make this method public
	static OpRect GetBestDropdownPosition(OpWidget *widget, INT32 width, INT32 height) { return WidgetWindow::GetBestDropdownPosition(widget, width, height); }

private:
	AutoCompletePopup* autocomp;
};

/**
	Crossplatform class for showing the popup with autocompleting items while user types in editboxes.
	It can fetch items from personal info, contacts, history etc. (See AUTOCOMPL_TYPE)
*/

class AutoCompletePopup
#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE
 : public OpTimerListener, public MessageObject
#endif
{
public:
	AutoCompletePopup(OpWidget* edit);
	~AutoCompletePopup();

	/** Set the type of data that should autocomplete. */
	void SetType(AUTOCOMPL_TYPE type);

	/** Open the menu manually (not by typing in the editfield).
		This does things slightly differently:
		- No filtering (all items are shown)
		- Automatically select the item that match the current text in the editfield. */
	void OpenManually();

	/** Set a target OpWidget if the AutoCompletePopup needs to use the width of, and align to
		another widget than the OpEdit it normally uses */
	void SetTargetWidget(OpWidget* new_target) { target = new_target; }

	BOOL EditAction(OpInputAction* action, BOOL changed);

	void UpdatePopup();
	void UpdatePopupLookAndPlacement(BOOL only_if_font_change = FALSE);
	OP_STATUS CreatePopup();
	void ClosePopup(BOOL immediately = FALSE);
	void FreeItems();

	/** Returns TRUE if any autocompletionbox is currently visible. */
	static BOOL IsAutoCompletionVisible();

	/** Returns TRUE if any autocompletion item is highlighted */
	static BOOL IsAutoCompletionHighlighted();

	/** Closes any visible autocompletionpopup */
	static void CloseAnyVisiblePopup();

#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE
	virtual void	OnTimeOut(OpTimer* timer);
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void	StopGoogle();
#endif

public:
	OpWidget* edit;

	AUTOCOMPL_TYPE type;

	OpString user_string; ///< The string the user has typed in the editbox so far.
	int current_item;	  ///< -1 if user_string should be in the textbox.
	int num_items;
	int num_columns;
	uni_char** items;
	BOOL opened_manually;

	AutoCompleteListboxListener* listener;
	OpListBox* listbox;
	AutoCompleteWindow* window;

#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE
	URL m_google_url;
	OpTimer* m_google_timer;
#endif
private:
	/** TRUE if the editfields value should be updated when the highlighted item in the popup changes.
		That might be a security risk in forms that can be constantly read by javascript.
		If FALSE, the item must be selected to set the value in the editfield. */
	BOOL change_field_while_browsing_alternatives;

	OpString last_selected_string;

	OpWidget* target; ///< used to calculate the popup width and alignment, if NULL, it uses the edit member

	/**
	   sends OnChange message to edit if the string has changed since the last message was sent
	 */
	void SendEditOnChange(BOOL changed_by_mouse = FALSE);
	OpString m_last_edit_text; ///< string in edit when last was sent to it

	void CommitLastSelectedValue();
	/** Select current highlighted item (if any item is highlighted) and close.
		Returns TRUE if there was a item to select. */
	BOOL SelectAndClose();
	void UpdateEdit();
	void SetEditTextWithCaretAtEnd(const uni_char* new_text);

	friend class AutoCompleteListboxListener;
};

#endif // AUTOCOMPLETEPOPUP_H
