/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 * 
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved. 
 * 
 * This file is part of the Opera web browser.  It may not be distributed 
 * under any circumstances. 
 */ 

#ifndef OP_SIMPLE_DIALOG_CONTROLLER_H
#define OP_SIMPLE_DIALOG_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/desktop_util/prefs/PrefAccessors.h"

#include "adjunct/quick_toolkit/contexts/DialogContext.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"

#include "modules/locale/locale-enum.h"


class QuickButton;

 /**
  * Makes it possible to show a simple dialog containing just an icon and a message without creating a dedicated UI definition.
  */
class SimpleDialogController: public DialogContext
{
public:

	enum DialogImage
	{
		IMAGE_NONE,
		IMAGE_WARNING,
		IMAGE_INFO,
		IMAGE_QUESTION,
		IMAGE_ERROR
	};
	
	enum DialogType
	{
		TYPE_OK,
		TYPE_CLOSE,
		TYPE_OK_CANCEL,
		TYPE_YES_NO,
		TYPE_YES_NO_CANCEL
	};

	enum DialogResultCode
	{
		DIALOG_RESULT_OK  = 1,			// OK or YES button result
		DIALOG_RESULT_CANCEL = 2,		// CANCEL button result, also NO button result for other than YES NO CANCEL dialog type
		DIALOG_RESULT_NO  = 3			// NO button result for YES NO CANCEL dialog type
	};

	SimpleDialogController(DialogType dialog_type, DialogImage dialog_image, OpStringC8 specific_name, Str::LocaleString message, Str::LocaleString title);
	SimpleDialogController(DialogType dialog_type, DialogImage dialog_image, OpStringC8 specific_name, OpStringC message, OpStringC title);
	
	virtual ~SimpleDialogController();

	/** 
	 * Sets dialog to be bloking on Show call.
	 * @param result - dialog returning value
	 */
	void SetBlocking(DialogResultCode& result) {m_result = &result;*m_result = DIALOG_RESULT_CANCEL;}

	/** 
	 * Sets dialog to use "don't show again" checkbox.
	 * @param do_not_show_again - input/output value for checkbox
	 *
	 * See also ConnectDoNotShowAgain for quick preference binding support
	 */
	void SetDoNotShowAgain(bool& do_not_show_again);
	
	/** 
	 * Sets dialog to use "don't show again" checkbox. 
	 * That's the preference binding version of SetDoNotShowAgain function.
	 *
	 * @see QuickBinder::Connect()
	 */
	template <typename C>
	OP_STATUS ConnectDoNotShowAgain(C& collection, typename C::integerpref preference,
		int selected = 0, int deselected = 1);


	/** 
	 * Sets dialog to use show "help" button in the button strip
	 */
	void SetHelpAnchor(OpStringC help_anchor) {m_help_anchor = help_anchor.CStr();}

	BOOL CanHandleAction(OpInputAction* action);
	OP_STATUS HandleAction(OpInputAction* action);
	
protected:

	// Overriding UiContext
	virtual void InitL();
	virtual void OnUiClosing();

	/**
	 * Called as a result of YES and OK buttons
	 * except "OK" and "CLOSE" dialog type.
	 */
	virtual void OnOk();

	/**
	 * Called as a result of NO button in "Yes No Cancel" dialog type.
	 */
	virtual void OnNo();

	/**
	 * Called as a result of NO, CANCEL and X button in the dialog title bar
	 * except "OK" and "CLOSE" dialog type.
	 */
	virtual void OnCancel();

	/**
	 * When using this constructor, it's recommended to use later SetTitle and SetMessage for dialog configuration
	 */
	SimpleDialogController(DialogType dialog_type, DialogImage dialog_image, OpStringC8 specific_name);

	OP_STATUS SetTitle(const OpStringC& title) {return m_dialog->SetTitle(title);}
	OP_STATUS SetMessage(const OpStringC& message);

	OP_STATUS SetIcon(DialogImage image);

	QuickButton*		m_ok_button;			// YES or OK button
	QuickButton*		m_cancel_button;		// NO or CANCEL for other than YES NO CANCEL dialog type
	QuickButton*		m_help_button;
	QuickButton*		m_no_button;			// NO button for YES NO CANCEL dialog type

private:
	const char* GetDialogImage();

	DialogImage			m_dialog_image;
	OpProperty<bool>	m_again_property;
	
	DialogType			m_dialog_type;
	DialogResultCode*	m_result;
	const uni_char*		m_help_anchor;
	OpStringC8          m_specific_dialog_name;
	Str::LocaleString   m_str_message;
	Str::LocaleString   m_str_title;
	OpStringC			m_title;
	OpStringC			m_message;
	bool*				m_do_not_show_again;
	PrefUtils::IntegerAccessor* m_dont_show_again_accessor;
	int					m_selected;
	int					m_deselected;
	bool				m_hook_called;
};


template <typename C>
OP_STATUS SimpleDialogController::ConnectDoNotShowAgain(C& collection, typename C::integerpref preference,
	int selected, int deselected)
{
	m_selected = selected;
	m_deselected = deselected;

	m_dont_show_again_accessor = OP_NEW(PrefUtils::IntegerAccessorImpl<C>, (collection, preference));
	return m_dont_show_again_accessor ? OpStatus::OK : OpStatus::ERR;
}

#endif // OP_SIMPLE_DIALOG_CONTROLLER_H
