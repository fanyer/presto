/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#ifdef GADGET_SUPPORT
#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/webserver/view/WebServerServiceSettingsDialog.h"

#include "adjunct/quick/webserver/controller/WebServerServiceController.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "adjunct/quick_toolkit/widgets/OpExpand.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"

#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**  WebServerServiceSettingsDialog::WebServerServiceSettingsDialog
**  @param service_controller
**  @param service_context
**
************************************************************************************/
WebServerServiceSettingsDialog::WebServerServiceSettingsDialog(WebServerServiceController * service_controller, WebServerServiceSettingsContext* service_context) :
	Dialog(),
	m_service_controller(service_controller),
	m_service_context(service_context),
	m_incorrect_input_field(NULL)
{
}

WebServerServiceSettingsDialog::~WebServerServiceSettingsDialog()
{
	OP_DELETE(m_service_context);
}


/***********************************************************************************
**  WebServerServiceSettingsDialog::GetType
**  @return
**
*******************************************************************************S*****/
OpTypedObject::Type WebServerServiceSettingsDialog::GetType()
{
	return DIALOG_TYPE_WEBSERVER_SERVICE_SETTINGS;
}


/***********************************************************************************
**  WebServerServiceSettingsDialog::OnInputAction
**  @param action
**  @return
**
************************************************************************************/
BOOL WebServerServiceSettingsDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
    case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_OK:
				{
					child_action->SetEnabled(HasRequiredFieldsFilled());
					return TRUE;
				}
			}
			break;
		}
	case OpInputAction::ACTION_OK:
		{
			if (OnOk() == DIALOG_RESULT_OK)
			{
				CloseDialog(FALSE);
			}
			return TRUE;
		}
	}
	return Dialog::OnInputAction(action);
}


/***********************************************************************************
**  WebServerServiceSettingsDialog::OnInit
**
************************************************************************************/
void WebServerServiceSettingsDialog::OnInit()
{
	// set icon & name
	OpIcon* service_icon = static_cast<OpIcon*>(GetWidgetByName("service_icon"));
	if (service_icon)
	{
		Image image(m_service_context->GetServiceImage());
		if (image.IsEmpty())
		{
			service_icon->SetImage("Unite Panel Default");
		}
		else
		{
			service_icon->SetBitmapImage(image);
			image.DecVisible(null_image_listener);
		}
	}

	// set values
	SetWidgetText("friendly_name_edit", m_service_context->GetFriendlyName().CStr());
	SetWidgetText("service_name_edit", m_service_context->GetServiceNameInURL().CStr());

	// set wrap for description
	OpMultilineEdit* install_intro_label = static_cast<OpMultilineEdit*>(GetWidgetByName("service_install_intro_label"));
	if (install_intro_label)
		install_intro_label->SetWrapping(TRUE);

	OpLabel* warning_label = static_cast<OpLabel*>(GetWidgetByName("warning_label"));
	if (warning_label)
		warning_label->SetWrap(TRUE);

	OpLabel* error_label = static_cast<OpLabel*>(GetWidgetByName("error_label"));
	if (error_label)
	{
		error_label->SetWrap(TRUE);
		error_label->SetForegroundColor(OP_RGB(255, 0, 0)); // todo: make skinable
	}

	OpEdit* address_edit = static_cast<OpEdit*>(GetWidgetByName("current_service_address_edit"));
	if (address_edit)
	{
		address_edit->SetFlatMode();
		address_edit->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
		address_edit->SetEllipsis(ELLIPSIS_CENTER);
		UpdateURL(m_service_context->GetServiceNameInURL());
	}

	// pre-set shared folder (description)
	if (m_service_context->GetSharedFolderHintType() == FolderHintNone)
	{
		ShowWidget("shared_folder_group", FALSE);
	}
	else
	{
		OpLabel* shared_folder_label = static_cast<OpLabel*>(GetWidgetByName("shared_folder_label"));
		if (shared_folder_label)
		{
			shared_folder_label->SetWrap(TRUE);

			OpString shared_folder;
			switch(m_service_context->GetSharedFolderHintType())
			{
			case FolderHintPictures:
				{
					g_languageManager->GetString(Str::D_SERVICE_SHARED_FOLDER_PICTURES, shared_folder);
					break;
				}
			case FolderHintMusic:
				{
					g_languageManager->GetString(Str::D_SERVICE_SHARED_FOLDER_MUSIC, shared_folder);
					break;
				}
			case FolderHintVideo:
				{
					g_languageManager->GetString(Str::D_SERVICE_SHARED_FOLDER_VIDEO, shared_folder);
					break;
				}
			case FolderHintDocuments:
				{
					g_languageManager->GetString(Str::D_SERVICE_SHARED_FOLDER_DOCUMENTS, shared_folder);
					break;
				}
			case FolderHintDownloads:
				{
					g_languageManager->GetString(Str::D_SERVICE_SHARED_FOLDER_DOWNLOAD, shared_folder);
					break;
				}
			case FolderHintPublic:
				{
					g_languageManager->GetString(Str::D_SERVICE_SHARED_FOLDER_PUBLIC, shared_folder);
					break;
				}
			case FolderHintDesktop:
				{
					g_languageManager->GetString(Str::D_SERVICE_SHARED_FOLDER_DESKTOP, shared_folder);
					break;
				}
			}
			if (shared_folder.HasContent())
			{
				OpString shared_folder_text;
				g_languageManager->GetString(Str::D_SERVICE_SELECT_SHARED_FOLDER_SPECIFIC, shared_folder_text);

				OpString complete_string;
				complete_string.AppendFormat(shared_folder_text.CStr(), shared_folder.CStr());
				shared_folder_label->SetText(complete_string.CStr());
			}
			// text for 'FolderHintDefault' is set in dialog.ini
		}

		DesktopFileChooserEdit* chooser = (DesktopFileChooserEdit*) GetWidgetByName("shared_folder_chooser");
		if( chooser )
		{
			chooser->SetInitialPath(m_service_context->GetSharedFolderHint().CStr());
			chooser->SetText(m_service_context->GetSharedFolderPath().CStr());
		}
	}

	// === ADVANCED SETTINGS ======
	OpExpand *advanced_expand = static_cast<OpExpand *>(GetWidgetByName("advanced_expand"));
	if (advanced_expand)
	{
		advanced_expand->SetShowHoverEffect(TRUE);
		advanced_expand->SetBold(TRUE);
		advanced_expand->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD); // fix DSK-257990, work around, SetBold alone doesn't work properly when the OpExpand group is initially hidden.
	}
	ShowWidget("advanced_group", FALSE);

#ifdef WEBSERVER_CAP_SET_VISIBILITY
	SetWidgetValue("webserver_asd_vis_checkbox", m_service_context->IsVisibleASD());
	SetWidgetValue("webserver_robots_vis_checkbox", m_service_context->IsVisibleRobots());
	SetWidgetValue("webserver_upnp_vis_checkbox", m_service_context->IsVisibleUPNP());

#ifdef PREFS_CAP_SERVICE_DISCOVERY_ENABLED
	if (!g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::ServiceDiscoveryEnabled))
	{
		SetWidgetEnabled("webserver_asd_vis_checkbox", FALSE);
	}
#endif // PREFS_CAP_SERVICE_DISCOVERY_ENABLED
#if defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_DISCOVERY_ENABLED) 
	if (!g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UPnPServiceDiscoveryEnabled))
	{
		SetWidgetEnabled("webserver_upnp_vis_checkbox", FALSE);
	}
#endif // defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_DISCOVERY_ENABLED) 
#else
	ShowWidget("webserver_asd_vis_checkbox", FALSE);
	ShowWidget("webserver_robots_vis_checkbox", FALSE);
	ShowWidget("webserver_upnp_vis_checkbox", FALSE);
#endif // WEBSERVER_CAP_SET_VISIBILITY

	ShowWidget("error_warning_group", FALSE);
	CompressGroups(); // make sure dialog is drawn correctly after showing/hiding groups
}


/***********************************************************************************
**  WebServerServiceSettingsDialog::OnOk
**  @return
**
************************************************************************************/
UINT32 WebServerServiceSettingsDialog::OnOk()
{
	if (IsPageChanged(0))
	{
		if (!HasValidSharedFolder())
		{
			ShowError(InvalidSharedFolderPath);
			return DIALOG_RESULT_CANCEL;
		}

		SaveSettingsToContext();

		ServiceSettingsError error = NoError;
		if (!m_service_controller->IsValidServiceNameInURL(m_service_context->GetServiceNameInURL()))
		{
			error = InvalidServiceNameInURL;
		}
		else if (!m_service_controller->IsServiceNameUnique(m_service_context->GetServiceIdentifier(), m_service_context->GetServiceNameInURL()))
		{
			error = ServiceNameAlreadyInUse;
		}

		if (error == NoError)
		{
			OpStatus::Ignore(m_service_controller->ChangeServiceSettings(*m_service_context)); // todo: handle status?
			return TRUE;
		}
		else
		{
			ShowError(error);
			return DIALOG_RESULT_CANCEL;
		}


	}
	return DIALOG_RESULT_OK;
}


/***********************************************************************************
**  WebServerServiceSettingsDialog::GetWindowName
**  @return
**
************************************************************************************/
const char* WebServerServiceSettingsDialog::GetWindowName()
{
	return "Webserver Service Settings Dialog";
}


/***********************************************************************************
**  WebServerServiceSettingsDialog::OnChange
**  @param widget
**  @param changed_by_mouse
**
************************************************************************************/
void WebServerServiceSettingsDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == GetWidgetByName("shared_folder_chooser"))
	{
		// Force state update of the ok button
		g_input_manager->UpdateAllInputStates();
	}
	else 
	{
		OpEdit* address_edit = static_cast<OpEdit*>(GetWidgetByName("service_name_edit"));
		if (widget == address_edit)
		{
			OpString service_address;
			address_edit->GetText(service_address);
			UpdateURL(service_address);

			// Force state update of the ok button
			g_input_manager->UpdateAllInputStates();
		}
	}
	Dialog::OnChange(widget, changed_by_mouse);
}


/***********************************************************************************
**  WebServerServiceSettingsDialog::SaveSettingsToContext
**  @param service_context
**
************************************************************************************/
void WebServerServiceSettingsDialog::SaveSettingsToContext()
{
	OpString service_name;
	GetWidgetText("friendly_name_edit", service_name);
	if (service_name.HasContent())
	{
		m_service_context->SetFriendlyName(service_name);
	}

	OpString url_name;
	GetWidgetText("service_name_edit", url_name);
	if (url_name.HasContent())
	{
		m_service_context->SetServiceNameInURL(url_name);
	}

	OpString shared_folder;
	GetWidgetText("shared_folder_chooser", shared_folder);
	if (shared_folder.HasContent())
	{
		m_service_context->SetSharedFolderPath(shared_folder);
	}

#ifdef WEBSERVER_CAP_SET_VISIBILITY
	m_service_context->SetVisibleASD(GetWidgetValue("webserver_asd_vis_checkbox"));
	m_service_context->SetVisibleRobots(GetWidgetValue("webserver_robots_vis_checkbox"));
	m_service_context->SetVisibleUPNP(GetWidgetValue("webserver_upnp_vis_checkbox"));
#endif // WEBSERVER_CAP_SET_VISIBILITY
}

/***********************************************************************************
**  WebServerServiceSettingsDialog::HasRequiredFieldsFilled
**
************************************************************************************/
/*virtual*/ BOOL
WebServerServiceSettingsDialog::HasRequiredFieldsFilled()
{
	OpString text;
	GetWidgetText("service_name_edit", text);
	if (text.IsEmpty())
	{
		return FALSE;
	}

	return HasSharedFolderFilled();
}


/***********************************************************************************
**  WebServerServiceSettingsDialog::HasSharedFolderFilled
************************************************************************************/
BOOL
WebServerServiceSettingsDialog::HasSharedFolderFilled()
{
	if (m_service_context->GetSharedFolderHintType() != FolderHintNone)
	{
		OpString shared_folder;
		GetWidgetText("shared_folder_chooser", shared_folder);
		return shared_folder.HasContent();
	}
	else
	{
		return TRUE;
	}
}

/***********************************************************************************
**  WebServerServiceSettingsDialog::HasValidSharedFolder
************************************************************************************/
BOOL
WebServerServiceSettingsDialog::HasValidSharedFolder()
{
	BOOL is_valid;

	if (m_service_context->GetSharedFolderHintType() != FolderHintNone)
	{
		is_valid = FALSE;

		OpString shared_folder;
		GetWidgetText("shared_folder_chooser", shared_folder);

		OpFile file;
		if (shared_folder.HasContent() 
			&& IsAbsolutePath(shared_folder) // hack: only allow absolute folders
			&& OpStatus::IsSuccess(file.Construct(shared_folder.CStr())))
		{
			OpFileInfo::Mode mode;
			BOOL exists;
			if (OpStatus::IsSuccess(file.Exists(exists)) && exists && OpStatus::IsSuccess(file.GetMode(mode)) && mode == OpFileInfo::DIRECTORY)
			{
				is_valid = TRUE;
				//g_op_system_info->PathsEqual(file.GetFullPath(), shared_folder.CStr(), &is_valid); // GetFullPath doesn't return the correct thing
			}
		}
	}
	else
	{
		is_valid = TRUE;
	}

	return is_valid;
}

/***********************************************************************************
**  WebServerServiceSettingsDialog::UpdateURL
************************************************************************************/
BOOL
WebServerServiceSettingsDialog::IsAbsolutePath(const OpStringC & shared_folder)
{
	return (shared_folder.HasContent()
		&& shared_folder.Find(".") != 0 // can't start with a dot
		);
}

/***********************************************************************************
**  WebServerServiceSettingsDialog::UpdateURL
************************************************************************************/
void
WebServerServiceSettingsDialog::UpdateURL(const OpStringC & service_address)
{
	OpEdit* url_edit = static_cast<OpEdit*>(GetWidgetByName("current_service_address_edit"));
	if (url_edit)
	{
		OpString url;
		OpStringC home_service(g_webserver_manager->GetRootServiceAddress());
		
		if (home_service.HasContent())
		{
			if(service_address.HasContent() && OpStatus::IsSuccess(url.AppendFormat(UNI_L("%s%s/"), home_service.CStr(), service_address.CStr())))
			{
				url_edit->SetText(url.CStr());
			}
			else
			{
				url_edit->SetText(home_service.CStr());
			}
		}
		else
		{
			url_edit->SetText(UNI_L(""));
		}
	}
}

/***********************************************************************************
**  WebServerServiceSettingsDialog::ShowError
************************************************************************************/
void
WebServerServiceSettingsDialog::ShowError(ServiceSettingsError error)
{
	OpString error_str;

	if (m_incorrect_input_field)
	{
		// clear old error first
		m_incorrect_input_field->GetBorderSkin()->SetState(0);
	}

	switch (error)
	{
	case InvalidServiceNameInURL:
		{
			m_incorrect_input_field = GetWidgetByName("service_name_edit");
			g_languageManager->GetString(Str::D_WEBSERVER_SERVICE_ERROR_INVALID_ADDRESS, error_str);
		}
		break;
	case ServiceNameAlreadyInUse:
		{
			m_incorrect_input_field = GetWidgetByName("service_name_edit");
			g_languageManager->GetString(Str::D_SERVICE_NAME_IN_USE, error_str);
		}
		break;
	case InvalidSharedFolderPath:
		{
			m_incorrect_input_field = GetWidgetByName("shared_folder_chooser");
			g_languageManager->GetString(Str::D_WEBSERVER_SERVICE_ERROR_INVALID_SHARED_FOLDER, error_str);
		}
		break;
	default:
		{
			OP_ASSERT(!"unimplemented folder path type");
		}
	}

	SetWidgetText("error_label", error_str.CStr());
	if (m_incorrect_input_field)
	{
		m_incorrect_input_field->GetBorderSkin()->SetState(SKINSTATE_ATTENTION);
	}

	ShowWidget("error_warning_group");
	CompressGroups();
}

/***********************************************************************************
**  WebServerServiceSettingsDialog::HideError
************************************************************************************/
void
WebServerServiceSettingsDialog::HideError()
{
	if (m_incorrect_input_field)
	{
		// clear old error first
		m_incorrect_input_field->GetBorderSkin()->SetState(0);
	}
	ShowWidget("error_warning_group", FALSE);
	CompressGroups();
}

#endif // WEBSERVER_SUPPORT
#endif // GADGET_SUPPORT
