/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cezary Kulakowski (ckulakowski)
 */

#ifndef ADDRESSBAR_OVERLAY_CONTROLLER_H
#define ADDRESSBAR_OVERLAY_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/PopupDialogContext.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick/widgets/OpProtocolButton.h"
#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"

class OpAddressDropDown;
class QuickOverlayDialog;
class QuickPagingLayout;


class AddressbarOverlayController : public PopupDialogContext
{
public:

	AddressbarOverlayController(OpAddressDropDown& dropdown,
			OpWidget& bounding_widget,
			OpWindowCommander* win_comm,
			const OpVector<OpPermissionListener::ConsentInformation>& info);

	void SetIsOnBottom();

private:

	struct Domain
	{
		Domain(OpPermissionListener::PermissionCallback::PermissionType type) : m_type(type)
		{}
		void OnChange(INT32 new_value);

		const OpPermissionListener::PermissionCallback::PermissionType m_type;
		OpProperty<INT32> m_property;
		TypedObjectCollection m_widgets;
	};

	enum Page
	{
		SECURITY_PAGE,
		PERMISSIONS_PAGE
	};

	enum
	{
		ALWAYS_ALLOW,
		ALLOW_ONCE,
		DENY
	};

	virtual void InitL();
	virtual BOOL CanHandleAction(OpInputAction* action);
	virtual BOOL SelectsAction(OpInputAction* action);
	virtual BOOL DisablesAction(OpInputAction* action);
	virtual OP_STATUS HandleAction(OpInputAction* action);
	virtual void OnKeyboardInputGained(OpInputContext* new_context, OpInputContext* old_context, FOCUS_REASON reason);
	bool GetHasPageContext();
	OP_STATUS InitSecurityInformation(bool& page_empty);
	OP_STATUS InitPermissionsPage(bool& page_empty);

	void UpdateButtonsStates();

	void AddDetailsButton();
	OpInputAction* GetActionForDetailsButton();

	OP_STATUS AddMaliciousSection();
	OP_STATUS AddTurboSection();
	OP_STATUS AddConnectionSection();
	OP_STATUS AddCertificateSection();
	OP_STATUS AddBlacklistSection();
	OP_STATUS AddTurboDetailSection();

	OP_STATUS AddSection(OpStringC8 icon_name, Str::LocaleString header,
			Str::LocaleString desc, 
			OpStringC8 head_skin = "Address Bar Overlay Header",
			OpStringC8 desc_skin = "Address Bar Overlay Security Information");
	OP_STATUS AddSection(OpStringC8 icon_name, OpStringC head_str, 
			OpStringC desc_str, 
			OpStringC8 head_skin = "Address Bar Overlay Header",
			OpStringC8 desc_skin = "Address Bar Overlay Security Information");

	OpAddressDropDown&					m_dropdown;
	OpProtocolButton&					m_proto_button;
	QuickOverlayDialog*					m_dialog;
	OpWidget&							m_bounding_widget;
	OpWindowCommander*					m_win_comm;
	QuickPagingLayout*					m_pages;
	OpProtocolButton::ProtocolStatus	m_proto_status;
	const TypedObjectCollection*		m_widgets;
	bool								m_turbo_action_handled;
	OpVector<OpPermissionListener::ConsentInformation>* m_consent_information;
	OpAutoVector<Domain>				m_domains;
	OpString							m_toplevel_domain;
};

#endif // !ADDRESSBAR_OVERLAY_CONTROLLER_H
