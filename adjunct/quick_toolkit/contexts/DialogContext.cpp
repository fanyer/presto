/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/contexts/DialogContext.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/bindings/DefaultQuickBinder.h"
#include "adjunct/quick_toolkit/contexts/DialogContextListener.h"
#include "adjunct/quick_toolkit/readers/UIReader.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

namespace DefaultUIPaths
{
	const uni_char Dialogs[] = UNI_L("dialogs.yml");
};

OpAutoPtr<DialogReader> DialogContext::s_reader;

DialogContext::~DialogContext()
{
	OP_DELETE(m_binder);
	OP_DELETE(m_dialog);
}

OP_STATUS DialogContext::SetDialog(const OpStringC8& dialog_name)
{
	QuickDialog* dialog = OP_NEW(QuickDialog, ());
	RETURN_OOM_IF_NULL(dialog);
	return SetDialog(dialog_name, dialog);
}

OP_STATUS DialogContext::SetDialog(const OpStringC8& dialog_name, QuickDialog* dialog)
{
	OpAutoPtr<QuickDialog> dialog_holder(dialog);

	if (!s_reader.get())
		RETURN_IF_ERROR(InitReader());

	RETURN_IF_ERROR(s_reader->CreateQuickDialog(dialog_name, *dialog));
	RETURN_VALUE_IF_NULL(dialog->GetWidgetCollection(), OpStatus::ERR_NULL_POINTER);

	return SetDialog(dialog_holder.release());
}

OP_STATUS DialogContext::SetDialog(QuickDialog* dialog)
{
	RETURN_VALUE_IF_NULL(dialog, OpStatus::ERR_NULL_POINTER);

	OP_DELETE(m_dialog);
	m_dialog = dialog;
	m_dialog->SetContext(this);

	OP_DELETE(m_binder);
	m_binder = CreateBinder();
	RETURN_OOM_IF_NULL(m_binder);
	m_binder->SetWidgetCollection(m_dialog->GetWidgetCollection());

	return OpStatus::OK;
}

QuickBinder* DialogContext::CreateBinder()
{
	return OP_NEW(DefaultQuickBinder, ());
}

OP_STATUS DialogContext::ShowDialog(DesktopWindow* parent_window)
{
	struct ContextCloser
	{
		UiContext* context;
		ContextCloser(UiContext* _context) : context(_context) {}
		~ContextCloser() { if (context) context->OnUiClosing(); }
	} closer(this);

	RETURN_IF_LEAVE(InitL());

	OP_ASSERT(m_dialog);
	m_dialog->SetParentWindow(parent_window);

	RETURN_IF_ERROR(m_dialog->Show());
	closer.context = 0;

	return OpStatus::OK;
}

void DialogContext::CancelDialog()
{
	if (m_dialog)
	{
		OpInputAction action(OpInputAction::ACTION_CANCEL);
		if (CanHandleAction(&action) && OpStatus::IsSuccess(HandleAction(&action)))
			return;
	}

	OnUiClosing();
}

bool DialogContext::IsDialogVisible() const
{
	return m_dialog && m_dialog->GetDesktopWindow() && m_dialog->GetDesktopWindow()->IsVisible();
}

void DialogContext::OnUiClosing()
{
	OP_DELETE(m_binder);
	m_binder = 0;

	if (m_listener)
		m_listener->OnDialogClosing(this);

	UiContext::OnUiClosing();
}

OP_STATUS DialogContext::InitReader()
{
	OpAutoPtr<DialogReader> reader (OP_NEW(DialogReader, ()));
	if (!reader.get())
		return OpStatus::ERR_NO_MEMORY;

	CommandLineArgument* log_argument = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::UIParserLog);
	OpStringC log_filename = log_argument ? log_argument->m_string_value : OpStringC(0);

	RETURN_IF_ERROR(reader->Init(DefaultUIPaths::Dialogs, OPFILE_UI_INI_FOLDER, log_filename));
	s_reader.reset(reader.release());

	return OpStatus::OK;
}


TestDialogContext::~TestDialogContext()
{
	g_application->Exit(TRUE);
}


OP_STATUS ShowDialog(DialogContext* dialog_context, UiContext* parent_context,
		DesktopWindow* parent_window)
{
	OP_ASSERT(dialog_context != NULL);
	OP_ASSERT(parent_context != NULL);
	if (dialog_context == NULL || parent_context == NULL)
	{
		OP_DELETE(dialog_context);
		return OpStatus::ERR_NULL_POINTER;
	}

	parent_context->AddChildContext(dialog_context);
	return dialog_context->ShowDialog(parent_window);
}
