/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ST_WEB_SERVER_SETUP_CONTROLLER_H
#define ST_WEB_SERVER_SETUP_CONTROLLER_H

#ifdef SELFTEST

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/quick_toolkit/bindings/DefaultQuickBinder.h"
#include "adjunct/quick_toolkit/contexts/DialogContext.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickGrid.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"


struct ST_WebServerSettings
{
	OpProperty<OpString> m_device_name;
};


/**
 * A demonstration of an MVC-style approach using QuickWidgets and QuickBinder.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski@opera.com)
 */
class ST_WebServerSetupController : public NullDialogContext
{
public:
	ST_WebServerSetupController() : m_binder(NULL)  {}
	virtual ~ST_WebServerSetupController() { OP_DELETE(m_binder); }

	void InitL()
	{
		// This part normally automated via declarative UI.

		OpStackAutoPtr<QuickStackLayout> stack(OP_NEW_L(QuickStackLayout,
				(QuickStackLayout::VERTICAL)));

		QuickLabel* label = OP_NEW_L(QuickLabel, ());
		LEAVE_IF_ERROR(label->Init());
		LEAVE_IF_ERROR(label->SetText(UNI_L("Customize your computer name")));
		LEAVE_IF_ERROR(stack->InsertWidget(label));

		QuickLabel* address_label = OP_NEW_L(QuickLabel, ());
		LEAVE_IF_ERROR(address_label->Init());
		LEAVE_IF_ERROR(stack->InsertWidget(address_label));

		QuickEdit* edit = OP_NEW_L(QuickEdit, ());
		LEAVE_IF_ERROR(edit->Init());
		LEAVE_IF_ERROR(stack->InsertWidget(edit));

		QuickDialog* dialog = OP_NEW_L(QuickDialog, ());
		LEAVE_IF_ERROR(SetDialog(dialog));
		LEAVE_IF_ERROR(dialog->Init());
		dialog->SetContent(stack.release());

		// Initialize the model to default values.

		m_model.m_device_name.Set(UNI_L("home"));

		// Set up the bindings.

		m_binder = OP_NEW_L(DefaultQuickBinder, ());
		LEAVE_IF_ERROR(m_binder->Connect(*edit, m_model.m_device_name));
		LEAVE_IF_ERROR(m_model.m_device_name.Subscribe(MAKE_DELEGATE(*this, &ST_WebServerSetupController::ToAddress)));
		LEAVE_IF_ERROR(m_binder->Connect(*address_label, m_address));

		// TODO: Upon closing the dialog, the controller will pass the
		// ST_WebServerSettings object on for processing by some Unite logic.
	}

	void ToAddress(const OpStringC& device_name)
	{
		OpString address;
		address.AppendFormat(UNI_L("http://%s.unite.opera.com "),
				device_name.CStr());
		m_address.Set(address);
	}

private:
	ST_WebServerSettings m_model;
	OpProperty<OpString> m_address;
	QuickBinder* m_binder;
};

#endif // SELFTEST
#endif // ST_WEB_SERVER_SETUP_CONTROLLER_H
