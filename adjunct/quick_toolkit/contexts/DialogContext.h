/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef DIALOG_CONTEXT_H
#define DIALOG_CONTEXT_H

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick_toolkit/contexts/UiContext.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"

class DesktopWindow;
class DialogContextListener;
class DialogReader;
class QuickBinder;


/** @brief a generic context for dialogs
  * Derive from this class to make a context for a specific dialog,
  * or use NullDialogContext to get a context that doesn't handle anyting.
  */
class DialogContext : public UiContext
{
public:
	DialogContext() : m_dialog(0), m_binder(0), m_listener(0) {}
	virtual ~DialogContext();

	/** Set the listener to be notified of events related to this context.
	  *
	  * @param listener the listener, may be @c NULL
	  */
	void SetListener(DialogContextListener* listener) { m_listener = listener; }

	/** Set the dialog that this context should be associated with.
	  * The context will take ownership of the dialog.
	  */
	OP_STATUS SetDialog(QuickDialog* dialog);

	/** Show the dialog.
	  * @note for implementors: this function will call the Init() function
	  * before showing the dialog. If the user closes the dialog (or an error
	  * occurs), it will call OnUiClosing().
	  *
	  * @param parent_window the dialog window's parent window
	  */
	OP_STATUS ShowDialog(DesktopWindow* parent_window);

	/** Initiate closing of the dialog previously shown with ShowDialog().
	  *
	  * @note Once the dialog is actually closed, this DialogContext object
	  * will get deleted.  If you need to clear a reference to it, either do it
	  * right after calling CancelDialog() or implement
	  * DialogContextListener::OnDialogClosing().
	  */
	void CancelDialog();

	// Overriding OpInputContext
	virtual const char* GetInputContextName() { return "Dialog"; }

	/** Checks whether dialog is visible and if so returns true.
	  */
	bool IsDialogVisible() const;

	// Overriding UiContext
	virtual void OnUiClosing();

protected:
	/** Set the dialog that this context should be associated with.
	  * The context will take ownership of the dialog.
	  *
	  * @param dialog_name the name identifying the dialog definition
	  */
	OP_STATUS SetDialog(const OpStringC8& dialog_name);

	/** Set the dialog that this context should be associated with.
	  * The context will take ownership of the dialog.
	  *
	  * @param dialog_name the name identifying the dialog definition
	  * @param dialog a pre-created QuickDialog instance.  Ownership is
	  * 		transferred.
	  */
	OP_STATUS SetDialog(const OpStringC8& dialog_name, QuickDialog* dialog);

	/** Set text on a widget within this dialog.
	  *
	  * @param W widget type
	  * @param T text type (any type accepted as argument type of @c W::SetText())
	  * @param widget_name the widget name
	  * @param widget_text the text to display
	  */
	template <typename W, typename T>
	OP_STATUS SetWidgetText(const OpStringC8& widget_name, const T& widget_text);

	/** Obtain a QuickBinder to be used to bind dialog UI elements to data.
	  *
	  * @return a QuickBinder instance.  The context manages its lifetime.
	  */
	QuickBinder* GetBinder() { return m_binder; }

	/** Override to change the QuickBinder implementation used by the context.
	  *
	  * @return a QuickBinder instance, caller gets ownership
	  */
	virtual QuickBinder* CreateBinder();

	QuickDialog* m_dialog;

private:
	/** Initialization function, will be called by ShowDialog()
	  * If nothing has to be done before showing the dialog, {}
	  * is a valid implementation.
	  */
	virtual void InitL() = 0;

	QuickBinder* m_binder;
	DialogContextListener* m_listener;

	static OP_STATUS InitReader();
	static OpAutoPtr<DialogReader> s_reader;
};


template <typename W, typename T>
OP_STATUS DialogContext::SetWidgetText(const OpStringC8& widget_name, const T& widget_text)
{
	W* widget = m_dialog->GetWidgetCollection()->Get<W>(widget_name);
	RETURN_VALUE_IF_NULL(widget, OpStatus::ERR_NULL_POINTER);
	return widget->SetText(widget_text);
}


class NullDialogContext : public DialogContext
{
public:
	virtual void InitL() {}
};


/** A dummy dialog context class that can be used to test dialog layouts.
  *
  * This is normally activated via the -dialogtest "Dialog To Test" command
  * line argument.
  */
class TestDialogContext : public NullDialogContext
{
public:
	virtual ~TestDialogContext();

	using DialogContext::SetDialog;
};


/** Utility function to show a dialog and hook its DialogContext.
  *
  * @param dialog_context the dialog context.  Ownership is transferred.
  * 		This object will be destroyed when the dialog closes (or fails to
  * 		open).
  * @param parent_context the context to become the parent (and owner) of
  * 		@a dialog_context
  * @param parent_window the dialog's parent window.  Can be @c NULL.
  * @return status.  On error, @a dialog_context is destroyed automatically,
  * 		too.
  */
OP_STATUS ShowDialog(DialogContext* dialog_context, UiContext* parent_context,
		DesktopWindow* parent_window);

#endif // DIALOG_CONTEXT_H
