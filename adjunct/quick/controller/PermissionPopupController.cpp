/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cezary Kulakowski (ckulakowski)
 */

#include "core/pch.h"

#include "adjunct/quick/controller/PermissionPopupController.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickIcon.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/NullWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickSeparator.h"
#include "adjunct/quick_toolkit/widgets/QuickRichTextLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickDynamicGrid.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/desktop_util/string/i18n.h"

#include "modules/locale/oplanguagemanager.h"


void PermissionPopupController::InitL()
{
	QuickOverlayDialog* dialog = OP_NEW_L(QuickOverlayDialog, ());
	LEAVE_IF_ERROR(SetDialog("Permission Popup", dialog));
	m_widgets = dialog->GetWidgetCollection();
	if (m_anchor_widget)
	{
		CalloutDialogPlacer* placer = OP_NEW_L(CalloutDialogPlacer, (*m_anchor_widget, CalloutDialogPlacer::LEFT, "Permission Popup Skin"));
		dialog->SetDialogPlacer(placer);
	}
	dialog->SetBoundingWidget(m_bounding_widget);
	dialog->SetAnimationType(QuickOverlayDialog::DIALOG_ANIMATION_CALLOUT);

	const char* device_icon_image = NULL;
	ANCHORD(OpString, device_label_str);
	ANCHORD(OpString, permission_label_str);
	ANCHORD(OpString, highlighted_hostname);
	highlighted_hostname.SetL("<b>");
	highlighted_hostname.AppendL(m_hostname);
	highlighted_hostname.AppendL("</b>");

	switch (m_type)
	{
		case OpPermissionListener::PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST:
		{
			device_icon_image = "Geolocation Indicator Icon";
			g_languageManager->GetStringL(Str::D_PERMISSION_TYPE_GEOLOCATION, device_label_str);
			LEAVE_IF_ERROR(I18n::Format(permission_label_str, Str::D_GEOLOCATION_ACCESS_REQUEST_LABEL, highlighted_hostname));
			break;
		}
		case OpPermissionListener::PermissionCallback::PERMISSION_TYPE_CAMERA_REQUEST:
		{
			device_icon_image = "Camera Indicator Icon";
			g_languageManager->GetStringL(Str::D_PERMISSION_TYPE_CAMERA, device_label_str);
			LEAVE_IF_ERROR(I18n::Format(permission_label_str, Str::D_CAMERA_ACCESS_REQUEST_LABEL, highlighted_hostname));
			break;
		}
		default:
		{
			OP_ASSERT(!"Unknown permission type");
			LEAVE(OpStatus::ERR);
		}
	}

	LEAVE_IF_ERROR(m_widgets->GetL<QuickRichTextLabel>("Permission_label")->SetText(permission_label_str));
	m_widgets->GetL<QuickIcon>("Device_icon")->SetImage(device_icon_image);
	LEAVE_IF_ERROR(m_widgets->GetL<QuickLabel>("Device_label")->SetText(device_label_str));

	if (m_toplevel_domain != m_hostname)
	{
		LEAVE_IF_ERROR(m_widgets->GetL<QuickLabel>("Accessing_host_label")->SetText(m_hostname));
		m_widgets->GetL<QuickStackLayout>("Iframe_host_stack")->Show();
		m_widgets->GetL<QuickStackLayout>("Toplevel_host_stack")->Show();
		LEAVE_IF_ERROR(m_widgets->GetL<QuickLabel>("Toplevel_host_label")->SetText(m_toplevel_domain));
		m_widgets->GetL<QuickIcon>("Addressbar_vertical_separator")->Show();
	}

	m_deny_permission_on_closing = true;
}

void PermissionPopupController::OnKeyboardInputGained(OpInputContext* new_context, OpInputContext* old_context, FOCUS_REASON reason)
{
	if (new_context == this)
	{
		QuickButton* ok_button = m_widgets->Get<QuickButton>("button_OK");
		if (ok_button->IsVisible())
		{
			ok_button->GetOpWidget()->SetFocus(reason);
		}
	}
}

void PermissionPopupController::OnCancel()
{
	if (m_deny_permission_on_closing)
	{
		m_desktop_window->HandleCurrentPermission(false, OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_NONE);
	}
}

void PermissionPopupController::OnOk()
{
	QuickDropDown* dropdown = m_widgets->Get<QuickDropDown>("Domain_dropdown");
	switch (dropdown->GetOpWidget()->GetValue())
	{
		case ALWAYS_ALLOW:
		{
			m_desktop_window->HandleCurrentPermission(true, OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS);
			break;
		}
		case ALLOW_ONCE:
		{
			m_desktop_window->HandleCurrentPermission(true, OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_RUNTIME);
			break;
		}
		case DENY:
		{
			m_desktop_window->HandleCurrentPermission(false, OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS);
			break;
		}
		default:
		{
			OP_ASSERT(!"Unknown dropdown value");
			break;
		}
	}
}

void PermissionPopupController::SetIsOnBottom()
{
		m_dialog->GetDesktopWindow()->GetSkinImage()->SetType(SKINTYPE_BOTTOM);
}
