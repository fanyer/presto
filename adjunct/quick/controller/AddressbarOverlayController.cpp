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

#include "adjunct/quick/controller/AddressbarOverlayController.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/managers/OperaTurboManager.h"
#include "adjunct/quick/managers/DesktopSecurityManager.h"
#include "adjunct/quick/dialogs/SecurityInformationDialog.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickIcon.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickSeparator.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickDynamicGrid.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/windowcommander/OpWindowCommander.h"


AddressbarOverlayController::AddressbarOverlayController(OpAddressDropDown& dropdown,
		OpWidget& bounding_widget,
		OpWindowCommander* win_comm,
		const OpVector<OpPermissionListener::ConsentInformation>& info) :
	PopupDialogContext(dropdown.GetProtocolButton(), &dropdown),
	m_dropdown(dropdown),
	m_proto_button(*dropdown.GetProtocolButton()),
	m_dialog(NULL),
	m_bounding_widget(bounding_widget),
	m_win_comm(win_comm),
	m_pages(NULL),
	m_turbo_action_handled(false),
	m_consent_information(NULL)
{
	// m_consent_information can't be const as we will clear it after initialization phase is finished
	m_consent_information = &const_cast<OpVector<OpPermissionListener::ConsentInformation>&>(info);
}

BOOL AddressbarOverlayController::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_SHOW_SECURITY_INFORMATION:
		case OpInputAction::ACTION_SHOW_PERMISSIONS_PAGE:
		case OpInputAction::ACTION_SHOW_SECURITY_PAGE:
		case OpInputAction::ACTION_OPEN_OPERA_WEB_TURBO_DIALOG:
		{
			return TRUE;
		}
	}
	return PopupDialogContext::CanHandleAction(action);
}

BOOL AddressbarOverlayController::SelectsAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_SHOW_PERMISSIONS_PAGE:
		{
			return m_pages->GetActivePagePos() == PERMISSIONS_PAGE;
		}
		case OpInputAction::ACTION_SHOW_SECURITY_PAGE:
		{
			return m_pages->GetActivePagePos() == SECURITY_PAGE;
		}
	}
	return PopupDialogContext::SelectsAction(action);
}

BOOL AddressbarOverlayController::DisablesAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_SHOW_SECURITY_PAGE:
		{
			return m_proto_button.GetStatus() == OpProtocolButton::Local;
		}
		case OpInputAction::ACTION_OPEN_OPERA_WEB_TURBO_DIALOG:
		{
			return m_turbo_action_handled;
		}
	}
	return PopupDialogContext::DisablesAction(action);
}

OP_STATUS AddressbarOverlayController::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_SHOW_SECURITY_INFORMATION:
		{
			SecurityInformationDialog* dialog = OP_NEW(SecurityInformationDialog, ());
			RETURN_OOM_IF_NULL(dialog);
			if (OP_STATUS err = dialog->Init(
						g_application->GetActiveDesktopWindow(), m_win_comm, 0))
			{
				OP_DELETE(dialog);
				return err;
			}
			CancelDialog();
			break;
		}
		case OpInputAction::ACTION_OPEN_OPERA_WEB_TURBO_DIALOG:
		{
			m_turbo_action_handled = true;
			g_input_manager->InvokeAction(action);
			CancelDialog();
			break;
		}
		case OpInputAction::ACTION_SHOW_SECURITY_PAGE:
		{
			m_pages->GoToPage(SECURITY_PAGE);
			UpdateButtonsStates();
			break;
		}
		case OpInputAction::ACTION_SHOW_PERMISSIONS_PAGE:
		{
			m_pages->GoToPage(PERMISSIONS_PAGE);
			UpdateButtonsStates();
			break;
		}
	}

	return PopupDialogContext::HandleAction(action);
}

void AddressbarOverlayController::OnKeyboardInputGained(OpInputContext* new_context, OpInputContext* old_context, FOCUS_REASON reason)
{
	if (new_context == this)
	{
		QuickButton* details_button = m_widgets->Get<QuickButton>("addressbar_overlay_detail_button");
		if (details_button && details_button->IsVisible())
		{
			details_button->GetOpWidget()->SetFocus(reason);
		}
	}
}

void AddressbarOverlayController::InitL()
{
	m_dialog = OP_NEW_L(QuickOverlayDialog, ());
	LEAVE_IF_ERROR(SetDialog("Addressbar Overlay Dialog", m_dialog));
	CalloutDialogPlacer* placer = OP_NEW_L(CalloutDialogPlacer, (m_proto_button, CalloutDialogPlacer::LEFT, "Addressbar Overlay Dialog Skin"));

	const uni_char* current_url = m_win_comm->GetCurrentURL(FALSE);
	unsigned starting_position = 0;
	LEAVE_IF_ERROR(StringUtils::FindDomain(current_url, m_toplevel_domain, starting_position));

	m_dialog->SetDialogPlacer(placer);
	m_dialog->SetBoundingWidget(m_bounding_widget);
	m_dialog->SetAnimationType(QuickOverlayDialog::DIALOG_ANIMATION_CALLOUT);

	m_widgets = m_dialog->GetWidgetCollection();
	m_pages = m_widgets->Get<QuickPagingLayout>("Info_pages");

	m_proto_status = m_proto_button.GetStatus();
	m_pages->SetCommonHeightForTabs(false);
	bool security_page_empty = false;
	LEAVE_IF_ERROR(InitSecurityInformation(security_page_empty));
	bool permissions_page_empty = false;
	LEAVE_IF_ERROR(InitPermissionsPage(permissions_page_empty));

	if (security_page_empty)
	{
		if (!permissions_page_empty)
		{
			m_pages->GoToPage(PERMISSIONS_PAGE);
		}
	}
	else
	{
		m_pages->GoToPage(SECURITY_PAGE);
	}

	if (security_page_empty && permissions_page_empty)
		LEAVE(OpStatus::ERR);

	if (!security_page_empty && !permissions_page_empty)
	{
		QuickStackLayout* tab_stack = m_widgets->GetL<QuickStackLayout>("Tab_stack");
		tab_stack->Show();
	}
}

void AddressbarOverlayController::SetIsOnBottom()
{
		m_dialog->GetDesktopWindow()->GetSkinImage()->SetType(SKINTYPE_BOTTOM);
}

OP_STATUS AddressbarOverlayController::InitPermissionsPage(bool& page_empty)
{
	page_empty = true;
	QuickDynamicGrid* permissions_grid = m_widgets->Get<QuickDynamicGrid>("Permissions_grid");
	RETURN_VALUE_IF_NULL(permissions_grid, OpStatus::ERR);

	unsigned number_of_hosts = m_consent_information->GetCount();

	for (unsigned i = 0; i < number_of_hosts; ++i)
	{
		OpStringC hostname = m_consent_information->Get(i)->hostname;
		OpPermissionListener::PermissionCallback::PermissionType permission_type = m_consent_information->Get(i)->permission_type;
		const char* device_icon_str = NULL;
		OpString device_label_str;

		switch (permission_type)
		{
			case OpPermissionListener::PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST:
			{
				device_icon_str = "Geolocation Indicator Icon";
				RETURN_IF_ERROR(g_languageManager->GetString(Str::D_PERMISSION_TYPE_GEOLOCATION, device_label_str));
				break;
			}
			case OpPermissionListener::PermissionCallback::PERMISSION_TYPE_CAMERA_REQUEST:
			{
				device_icon_str = "Camera Indicator Icon";
				RETURN_IF_ERROR(g_languageManager->GetString(Str::D_PERMISSION_TYPE_CAMERA, device_label_str));
				break;
			}
			default:
			{
				OP_ASSERT(!"Unexpected permission type");
				return OpStatus::ERR;
			}
		}

		OpAutoPtr<Domain> domain(OP_NEW(Domain,(permission_type)));
		RETURN_OOM_IF_NULL(domain.get());
		TypedObjectCollection* row = &(domain->m_widgets);
		RETURN_IF_ERROR(permissions_grid->Instantiate(*row));

		QuickIcon* device_icon = row->Get<QuickIcon>("Device_icon");
		RETURN_VALUE_IF_NULL(device_icon, OpStatus::ERR);
		device_icon->SetImage(device_icon_str);

		row->GetL<QuickLabel>("Device_label")->SetText(device_label_str);
		row->GetL<QuickLabel>("Accessing_host_label")->SetText(hostname);

		if (hostname != m_toplevel_domain)
		{
			row->GetL<QuickStackLayout>("Iframe_host_stack")->Show();
			row->GetL<QuickStackLayout>("Toplevel_host_stack")->Show();
			row->GetL<QuickIcon>("Addressbar_vertical_separator")->Show();
			RETURN_IF_ERROR(row->GetL<QuickLabel>("Toplevel_host_label")->SetText(m_toplevel_domain));
		}

		QuickDropDown* dropdown = row->Get<QuickDropDown>("Domain_dropdown");
		RETURN_VALUE_IF_NULL(dropdown, OpStatus::ERR);
		if (m_consent_information->Get(i)->is_allowed)
		{
			if (m_consent_information->Get(i)->persistence_type == OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS)
			{
				dropdown->GetOpWidget()->SetValue(ALWAYS_ALLOW);
			}
			else
			{
				dropdown->GetOpWidget()->SetValue(ALLOW_ONCE);
			}
		}
		else
		{
			dropdown->GetOpWidget()->SetValue(DENY);
		}
		domain->m_property.Set(dropdown->GetOpWidget()->GetValue());
		RETURN_IF_ERROR(GetBinder()->Connect(*dropdown, domain->m_property));
		RETURN_IF_ERROR(domain->m_property.Subscribe(MAKE_DELEGATE(*domain, &Domain::OnChange)));
		RETURN_IF_ERROR(m_domains.Add(domain.get()));
		domain.release();
		page_empty = false;
	}

	// m_consent_information is valid only on initialization phase
	m_consent_information = NULL;

	return OpStatus::OK;
}

void AddressbarOverlayController::UpdateButtonsStates()
{
	QuickButton* access_button = m_widgets->Get<QuickButton>("Access_button");
	QuickButton* security_button = m_widgets->Get<QuickButton>("Security_button");

	// There is no need to check if any of the pointers is not NULL.
	// If any button is missing dialog initialization would fail.
	access_button->GetOpWidget()->SetUpdateNeeded(TRUE);
	access_button->GetOpWidget()->UpdateActionStateIfNeeded();
	security_button->GetOpWidget()->SetUpdateNeeded(TRUE);
	security_button->GetOpWidget()->UpdateActionStateIfNeeded();
}

OP_STATUS AddressbarOverlayController::InitSecurityInformation(bool& page_empty)
{
	page_empty = true;
	switch (m_proto_status)
	{
		case OpProtocolButton::Fraud:
		case OpProtocolButton::Malware:
			RETURN_IF_ERROR(AddMaliciousSection());
			page_empty = false;
			break;
		case OpProtocolButton::Trusted:
		case OpProtocolButton::Secure:
		case OpProtocolButton::Web:
			RETURN_IF_ERROR(AddTurboSection());
			RETURN_IF_ERROR(AddConnectionSection());
			RETURN_IF_ERROR(AddCertificateSection());
			RETURN_IF_ERROR(AddBlacklistSection());
			page_empty = false;
			break;
		case OpProtocolButton::Turbo:
			RETURN_IF_ERROR(AddTurboSection());
			RETURN_IF_ERROR(AddTurboDetailSection());
			if (GetHasPageContext())
			{
				RETURN_IF_ERROR(AddConnectionSection());
				RETURN_IF_ERROR(AddBlacklistSection());
			}
			page_empty = false;
			break;
		case OpProtocolButton::Local:
			break;
		default:
			OP_ASSERT(!"unknown status");
			return OpStatus::ERR;
	}

	if (m_proto_status != OpProtocolButton::Fraud &&
			m_proto_status != OpProtocolButton::Malware &&
			m_proto_status != OpProtocolButton::Local)
	{
		AddDetailsButton();
		page_empty = false;
	}

	return OpStatus::OK;
}

OpInputAction* AddressbarOverlayController::GetActionForDetailsButton()
{
	OpInputAction* action = NULL; 

	switch (m_proto_status)
	{
		case OpProtocolButton::Fraud:
			if (m_win_comm && m_win_comm->GetCurrentURL(FALSE) && 
					uni_stricmp(UNI_L("opera:site-warning"), m_win_comm->GetCurrentURL(FALSE)) == 0)
			{
				break;
			}
		case OpProtocolButton::Trusted:
		case OpProtocolButton::Secure:
		case OpProtocolButton::Web:
			OpInputAction::CreateInputActionsFromString("Show security information", action);
			break;
		case OpProtocolButton::Turbo:
			OpInputAction::CreateInputActionsFromString("Open Web Turbo Dialog", action);
			break;
		default:
			break;
	}

	return action;
}

void AddressbarOverlayController::AddDetailsButton()
{
	m_widgets->GetL<QuickStackLayout>("Details_button_stack")->Show();
	m_widgets->GetL<QuickButton>("addressbar_overlay_detail_button")->GetOpWidget()->SetAction(GetActionForDetailsButton());
}

OP_STATUS AddressbarOverlayController::AddMaliciousSection() 
{
	QuickButton* button = m_widgets->Get<QuickButton>("Security_button");
	RETURN_VALUE_IF_NULL(button, OpStatus::ERR);
	button->GetOpWidget()->GetForegroundSkin()->SetImage("Addressbar Overlay Security Button Warning Image");
	Str::LocaleString detail(Str::S_ADDRESSBAR_OVERLAY_BLACKLISTED_DETAIL);
	if (m_proto_status != OpProtocolButton::Fraud)
		detail = Str::S_BLOCKED_MALWARE_SITE_WARNING;

	RETURN_IF_ERROR(
			AddSection("Addressbar Fraud", Str::S_ADDRESSBAR_OVERLAY_BLACKLISTED, 
				detail, "Address Bar Overlay Header Fraud",	
				"Address Bar Overlay Security Information"));

	return OpStatus::OK;
}

OP_STATUS AddressbarOverlayController::AddTurboSection() 
{
	if (g_opera_turbo_manager->GetTurboMode() != OperaTurboManager::OperaTurboOff)
	{
		if (!GetHasPageContext())
		{
			RETURN_IF_ERROR(
					AddSection("Addressbar Turbo Enabled", 
						Str::S_OPERA_WEB_TURBO_ENABLED, 
						Str::S_ADDRESSBAR_OVERLAY_TURBO_DETAIL));
		}
		else if (m_win_comm->GetPrivacyMode())
		{
			// Inactive for private.

			RETURN_IF_ERROR(
					AddSection("Addressbar Turbo Inactive",
						Str::S_ADDRESSBAR_OVERLAY_TURBO_INACTIVE,
						Str::S_ADDRESSBAR_OVERLAY_TURBO_INACTIVE_PRIVATE));
		}
		else if (m_win_comm->GetTurboMode())
		{
			if (m_win_comm->IsIntranet())
			{
				// Inactive for intranet.

				RETURN_IF_ERROR(
						AddSection("Addressbar Turbo Inactive",
							Str::S_ADDRESSBAR_OVERLAY_TURBO_INACTIVE,
							Str::S_ADDRESSBAR_OVERLAY_TURBO_INACTIVE_INTRANET));
			}
			else if (g_desktop_security_manager->IsHighSecurity(m_win_comm->GetSecurityMode()))
			{
				// Inactive for secure.

				RETURN_IF_ERROR(
						AddSection("Addressbar Turbo Inactive",
							Str::S_ADDRESSBAR_OVERLAY_TURBO_INACTIVE,
							Str::S_ADDRESSBAR_OVERLAY_TURBO_INACTIVE_SECURE));
			}
			else if (m_win_comm->GetURLType() == DocumentURLType_FTP)
			{
				// Inactive for intranet.

				RETURN_IF_ERROR(
						AddSection("Addressbar Turbo Inactive",
							Str::S_ADDRESSBAR_OVERLAY_TURBO_INACTIVE,
							Str::S_ADDRESSBAR_OVERLAY_TURBO_INACTIVE_FTP));
			}
			else if (m_win_comm->GetSecurityMode() != OpDocumentListener::NO_SECURITY
					|| m_win_comm->GetURLType() == DocumentURLType_HTTPS)
			{
				// Only active for some unencrypted contents (if any) in this 
				// insecure https page.

				RETURN_IF_ERROR(
						AddSection("Addressbar Turbo Enabled", 
							Str::S_OPERA_WEB_TURBO_ENABLED, 
							Str::S_ADDRESSBAR_OVERLAY_TURBO_ACTIVE_ON_INSECURE));
			}
			else
			{
				RETURN_IF_ERROR(
						AddSection("Addressbar Turbo Enabled", 
							Str::S_OPERA_WEB_TURBO_ENABLED, 
							Str::S_ADDRESSBAR_OVERLAY_TURBO_DETAIL));
			}
		}
		else 
		{
			RETURN_IF_ERROR(
					AddSection("Addressbar Turbo Inactive",
						Str::S_ADDRESSBAR_OVERLAY_TURBO_INACTIVE, 
						Str::D_OPERA_TURBO_NOTIFICATION_FAST_NET_AUTO));
		}
	}

	return OpStatus::OK;
}

OP_STATUS AddressbarOverlayController::AddConnectionSection() 
{
	// Get domain.
	const uni_char* current_url = m_win_comm->GetCurrentURL(FALSE);
	URL url = g_url_api->GetURL(current_url);
	ServerName* server = url.GetServerName();
	OpString detail;
	const char* header_skin = "Address Bar Overlay Unencrypted Header";
	const char* icon = "Addressbar Unencrypted Connection";
	RETURN_IF_ERROR(detail.Set(server ? server->UniName() : NULL));

	Str::LocaleString header_id(Str::S_ADDRESSBAR_OVERLAY_CONNECTION_UNENCRYPTED);
	OpDocumentListener::SecurityMode sec_mode = m_win_comm->GetSecurityMode();
	if (g_desktop_security_manager->IsHighSecurity(sec_mode))
	{
		header_id = Str::S_ADDRESSBAR_OVERLAY_CONNECTION_SECURE;
		icon = "Addressbar Securely Connected";
		header_skin = "Address Bar Overlay Secure Header";
	}
	else if (sec_mode == OpDocumentListener::FLAWED_SECURITY ||
			sec_mode == OpDocumentListener::LOW_SECURITY)
	{
		header_id = Str::S_ADDRESSBAR_OVERLAY_CONNECTION_NOT_SECURE;
		icon = "Addressbar Insecure Connection";
		header_skin = "Address Bar Overlay Insecure Header";
	}

	OpString header;
	RETURN_IF_ERROR(g_languageManager->GetString(header_id, header));
	RETURN_IF_ERROR(AddSection(icon, header, detail, header_skin));

	return OpStatus::OK;
}

OP_STATUS AddressbarOverlayController::AddCertificateSection() 
{
	OpDocumentListener::SecurityMode sec_mode = m_win_comm->GetSecurityMode();
	if (g_desktop_security_manager->IsEV(sec_mode))
	{
		OpString header, details, orgnization;
		m_dropdown.GetSecurityInformation(orgnization);

		g_languageManager->GetString(Str::S_ADDRESSBAR_OVERLAY_SITE_CERTIFIED, header);
		g_languageManager->GetString(Str::S_ADDRESSBAR_OVERLAY_SITE_CERTIFIED_DETAIL, details);
		details.ReplaceAll(UNI_L("%s"), orgnization);

		RETURN_IF_ERROR(
				AddSection("Addressbar Site Certified", header, details, "Address Bar Overlay Certified Header"));
	}

	return OpStatus::OK;
}

OP_STATUS AddressbarOverlayController::AddBlacklistSection() 
{
	// Make sure we don't show this for an empty window.

	const uni_char* url = m_win_comm->GetLoadingURL(FALSE);
	if (url && *url)
	{
		OpDocumentListener::TrustMode trust_mode = m_win_comm->GetTrustMode();
		if (trust_mode == OpDocumentListener::PHISHING_DOMAIN ||
				trust_mode == OpDocumentListener::MALWARE_DOMAIN)
		{
			OP_ASSERT(false);
		}
		else if (trust_mode == OpDocumentListener::TRUST_NOT_SET)
		{
			if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableTrustRating))
			{
				RETURN_IF_ERROR(
						AddSection("Addressbar Spinner", 
							Str::S_ADDRESSBAR_OVERLAY_BLACKLIST_CHECKING, 
							Str::S_ADDRESSBAR_OVERLAY_BLACKLIST_CHECKING_DETAIL,
							"Address Bar Overlay Blacklist Checking Header"));
			}
			else
			{
				RETURN_IF_ERROR(
						AddSection("Addressbar Blacklist Unknown",
							Str::S_ADDRESSBAR_OVERLAY_BLACKLIST_CHECK_FAILED,
							Str::S_ADDRESSBAR_OVERLAY_BLACKLIST_CHECK_FAILED_DISABLED,
							"Address Bar Overlay Blacklist Check Failed Header"));
			}
		}
		else
		{
			RETURN_IF_ERROR(
					AddSection("Addressbar Not Blacklisted",
						Str::S_ADDRESSBAR_OVERLAY_NOT_BLACKLISTED, 
						Str::S_ADDRESSBAR_OVERLAY_NOT_BLACKLISTED_DETAIL,
						"Address Bar Overlay Not Blacklisted Header"));
		}
	}

	return OpStatus::OK;
}

OP_STATUS AddressbarOverlayController::AddTurboDetailSection() 
{
	OpString saved, str, str2;
	float ratio = g_opera_turbo_manager->GetCompressionRate();
	UINT bytes_saved = g_opera_turbo_manager->GetBytesSaved();

	saved.Reserve(20);
	StrFormatByteSize(saved, bytes_saved, 0);

	if (bytes_saved > 0)
	{
		g_languageManager->GetString(Str::S_ADDRESSBAR_OVERLAY_TURBO_STATS_DETAIL, str);
		str.ReplaceAll(UNI_L("%s"), saved);

		OpString r;
		r.AppendFormat(UNI_L("%d%%"), (UINT)(100-100/ratio));
		g_languageManager->GetString(Str::S_ADDRESSBAR_OVERLAY_TURBO_STATS, str2);
		str2.ReplaceAll(UNI_L("%s"), r);

		RETURN_IF_ERROR(
				AddSection("Addressbar Turbo Detail", str2, str));
	}

	return OpStatus::OK;
}

OP_STATUS AddressbarOverlayController::AddSection(OpStringC8 icon_name, 
		OpStringC head_str, OpStringC desc_str, OpStringC8 head_skin,
		OpStringC8 desc_skin)
{
	TypedObjectCollection row;
	QuickDynamicGrid* grid = m_widgets->Get<QuickDynamicGrid>("Security_page_grid");
	RETURN_VALUE_IF_NULL(grid, OpStatus::ERR);
	RETURN_IF_ERROR(grid->Instantiate(row));

	row.GetL<QuickIcon>("Security_info_icon")->SetImage(icon_name.CStr());
	RETURN_IF_ERROR(row.GetL<QuickMultilineLabel>("Security_info_head")->SetText(head_str));
	row.GetL<QuickMultilineLabel>("Security_info_head")->SetSkin(head_skin);
	RETURN_IF_ERROR(row.GetL<QuickMultilineLabel>("Security_info_description")->SetText(desc_str));
	row.GetL<QuickMultilineLabel>("Security_info_description")->SetSkin(desc_skin);

	return OpStatus::OK;
}

OP_STATUS AddressbarOverlayController::AddSection(OpStringC8 icon_name,
		Str::LocaleString head_id, Str::LocaleString desc_id,
		OpStringC8 head_skin, OpStringC8 desc_skin)
{
	OpString head_str, desc_str;
	RETURN_IF_ERROR(g_languageManager->GetString(head_id, head_str));
	RETURN_IF_ERROR(g_languageManager->GetString(desc_id, desc_str));

	return AddSection(icon_name, head_str, desc_str, head_skin, desc_skin);
}

bool AddressbarOverlayController::GetHasPageContext()
{
	DocumentDesktopWindow* active = g_application->GetActiveDocumentDesktopWindow();
	if (active)
	{
		if (active->IsSpeedDialActive())
			return false;

		OpWindowCommander* comm = m_dropdown.GetWindowCommander();
		const uni_char* url = comm->GetCurrentURL(FALSE);

		return url && *url;
	}

	return false;
}

void AddressbarOverlayController::Domain::OnChange(INT32 new_value)
{
	QuickLabel* hostname_label = m_widgets.Get<QuickLabel>("Accessing_host_label");
	OpString hostname;
	RETURN_VOID_IF_ERROR(hostname_label->GetText(hostname));
	OpPermissionListener::PermissionCallback::PersistenceType persistence;
	QuickDropDown* dropdown = m_widgets.Get<QuickDropDown>("Domain_dropdown");
	BOOL3 allowed = NO;
	switch (dropdown->GetOpWidget()->GetValue())
	{
		case ALWAYS_ALLOW:
		{
			allowed = YES;
			persistence = OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS;
			break;
		}
		case DENY:
		{
			allowed = NO;
			persistence = OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS;
			break;
		}
		case ALLOW_ONCE:
		{
			allowed = YES;
			persistence = OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_RUNTIME;
			break;
		}
		default:
		{
			OP_ASSERT(!"Unknown dropdown value");
			return;
		}
	}

	DocumentDesktopWindow* window = g_application->GetActiveDocumentDesktopWindow();
	window->SetUserConsent(hostname, m_type, allowed, persistence);
}
