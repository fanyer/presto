/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author: Blazej Kazmierczak (bkazmierczak)
 *
 */
#include "core/pch.h"

#include "adjunct/quick/controller/ExtensionInstallController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/extensions/ExtensionInstaller.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickIcon.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"
#include "adjunct/widgetruntime/GadgetUtils.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/gadgets/OpGadgetManager.h"

ExtensionInstallGenericController::ExtensionInstallGenericController(OpGadgetClass& gclass,bool use_expand):
					m_widgets(NULL),
					m_gclass(&gclass),
					m_dialog_mode(ExtensionUtils::TYPE_FULL),
					m_use_expand(use_expand)
{
}

void ExtensionInstallGenericController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Extension Install Dialog"));
	m_widgets = m_dialog->GetWidgetCollection();
	
	m_dialog_mode = ExtensionUtils::GetAccessWindowType(m_gclass);

	return InitControlsL();
}
 

void ExtensionInstallGenericController::InitControlsL()
{
	if (OpStatus::IsSuccess(ExtensionUtils::GetExtensionIcon(m_icon_img, m_gclass)))
	{
		OP_ASSERT(!m_icon_img.IsEmpty());
		QuickIcon* icon_widget = m_widgets->GetL<QuickIcon>("Extension_icon");
		icon_widget->SetBitmapImage(m_icon_img);
	}

	QuickLabel* version_widget = m_widgets->GetL<QuickLabel>("Extension_version");
	ANCHORD(OpString, versionString);
	ANCHORD(OpString, gver);

	g_languageManager->GetStringL(Str::S_CRYPTO_VERSION, versionString);
	if (OpStatus::IsError(m_gclass->GetGadgetVersion(gver)) || gver.IsEmpty())
	{
		version_widget->Hide();
	}
	else
	{
		LEAVE_IF_ERROR(versionString.AppendFormat(UNI_L(" %s"), gver.CStr()));
		LEAVE_IF_ERROR(version_widget->SetText(versionString));
	}

	ANCHORD(OpString, extension_name);
	LEAVE_IF_ERROR(m_gclass->GetGadgetName(extension_name));
	m_widgets->GetL<QuickLabel>("Extension_title")->SetText(extension_name);

	LEAVE_IF_ERROR(SetWarningAccessLevel());
}

OP_STATUS ExtensionInstallGenericController::SetWarningAccessLevel()
{
	QuickPagingLayout * paging_layout = m_widgets->Get<QuickPagingLayout>("paging_layout");
	RETURN_VALUE_IF_NULL(paging_layout,OpStatus::ERR);

	if(m_use_expand)
	{
		if (m_dialog_mode ==  ExtensionUtils::TYPE_SIMPLE)
		{
			OpString access_list_string;
			RETURN_IF_ERROR(ExtensionUtils::GetAccessLevelString(m_gclass, access_list_string));
			RETURN_IF_ERROR(SetWidgetText<QuickMultilineEdit>("Access_message1",access_list_string));

			paging_layout->GoToPage(1);
		}
		else if (m_dialog_mode ==  ExtensionUtils::TYPE_CHECKBOXES)
		{
			RETURN_IF_ERROR(GetBinder()->Connect("Secure_conn_checkbox0", m_secure_mode));
			RETURN_IF_ERROR(GetBinder()->Connect("Private_mode_checkbox0", m_private_mode));
			paging_layout->GoToPage(0);
		}
	}
	else
	{
		if (m_dialog_mode ==  ExtensionUtils::TYPE_SIMPLE)
		{
			OpString access_list_string;
			RETURN_IF_ERROR(ExtensionUtils::GetAccessLevelString(m_gclass, access_list_string));
			RETURN_IF_ERROR(SetWidgetText<QuickMultilineEdit>("Access_message3",access_list_string));

			paging_layout->GoToPage(3);
		}
		else if (m_dialog_mode ==  ExtensionUtils::TYPE_CHECKBOXES)
		{
			RETURN_IF_ERROR(GetBinder()->Connect("Secure_conn_checkbox2", m_secure_mode));
			RETURN_IF_ERROR(GetBinder()->Connect("Private_mode_checkbox2", m_private_mode));
			paging_layout->GoToPage(2);
		}
	}
	return OpStatus::OK;
}


ExtensionInstallController::ExtensionInstallController(ExtensionInstaller* installer,OpGadgetClass& gclass):
ExtensionInstallGenericController(gclass),
					m_installer(installer)
					
{
	m_private_mode.Set(false);
	m_secure_mode.Set(true); // Secure page access is ON by default
}
			
void ExtensionInstallController::OnOk()
{	
	m_installer->InstallAllowed(m_secure_mode.Get() ? TRUE : FALSE, m_private_mode.Get()? TRUE : FALSE);
}

void ExtensionInstallController::OnCancel()
{
	m_installer->InstallCanceled();
}
	
ExtensionPrivacyController::ExtensionPrivacyController(OpGadgetClass& gclass,const uni_char* ext_wuid, bool private_access, bool secure_access):
					ExtensionInstallGenericController(gclass,false),
					m_ext_wuid(ext_wuid)                  
{
	m_private_mode.Set(private_access);
	m_secure_mode.Set(secure_access);
}

void ExtensionPrivacyController::OnOk()
{	
    g_desktop_extensions_manager->SetExtensionSecurity(m_ext_wuid, m_secure_mode.Get()?TRUE:FALSE,m_private_mode.Get()?TRUE:FALSE);	
}


void ExtensionPrivacyController::InitL()
{
	ExtensionInstallGenericController::InitL();

	m_widgets->GetL<QuickButton>("button_OK")->SetText(Str::DI_ID_OK);

	ANCHORD(OpString, main_label);
	g_languageManager->GetStringL(Str::D_EXTENSION_PRIVACY_DIALOG_TITLE, main_label);
	LEAVE_IF_ERROR(m_dialog->SetTitle(main_label));

	QuickLabel* unneeded = m_widgets->GetL<QuickLabel>("Extension_install_query");
	unneeded->Hide();
}


ExtensionUpdateController::ExtensionUpdateController(OpGadgetClass& gclass,const OpStringC& gadget_id):
					ExtensionInstallGenericController(gclass),					
					m_gclass_new(NULL)
{
	m_gadget_id.Set(gadget_id);
}

void ExtensionUpdateController::InitL()
{
	ExtensionInstallGenericController::InitL();
	LEAVE_IF_ERROR(UpdateControls());
}
 
OP_STATUS ExtensionUpdateController::UpdateControls()
{
	OpString main_label;
	OpStatus::Ignore(g_languageManager->GetString(Str::D_EXTENSION_UPDATE_TITLE, main_label));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Extension_install_query",main_label));
	RETURN_IF_ERROR(m_dialog->SetTitle(main_label));

	RETURN_IF_ERROR(SetWidgetText<QuickButton>("button_OK",Str::D_MAIL_INDEX_PROPERTIES_UPDATE));
	RETURN_IF_ERROR(SetWidgetText<QuickButton>("button_Cancel",Str::D_WAND_NOTNOW));

	OpString update_file_path;
	OpGadget *gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_PATH,m_gclass->GetGadgetPath());
	if (!gadget)
		return OpStatus::ERR;
	
	RETURN_IF_ERROR(g_desktop_gadget_manager->GetUpdateFileName(*gadget,update_file_path));
	RETURN_IF_ERROR(ExtensionUtils::CreateExtensionClass(update_file_path,&m_gclass_new));
	
    m_dialog_mode = ExtensionUtils::GetAccessWindowType(m_gclass_new);

	if (OpStatus::IsSuccess(ExtensionUtils::GetExtensionIcon(m_icon_img, m_gclass)))
	{
		OP_ASSERT(!m_icon_img.IsEmpty());
		QuickIcon* icon_widget = m_widgets->Get<QuickIcon>("Extension_icon");
		if (!icon_widget)
			return OpStatus::ERR;

		icon_widget->SetBitmapImage(m_icon_img);
	}

	OpString old_gadget_version, new_gadget_version, version, version_tmp; 
	RETURN_IF_ERROR(old_gadget_version.Set(m_gclass->GetGadgetVersion()));
	RETURN_IF_ERROR(new_gadget_version.Set(m_gclass_new->GetGadgetVersion()));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_EXTENSION_UPDATE_VERSION,version_tmp));
		
	if (old_gadget_version.HasContent() && new_gadget_version.HasContent())
	{
		RETURN_IF_ERROR(version.AppendFormat(version_tmp.CStr(),old_gadget_version.CStr(),new_gadget_version.CStr()));	
		RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Extension_version",version));		
	}		

	OpString extension_name;
	RETURN_IF_ERROR(m_gclass->GetGadgetName(extension_name));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Extension_title",extension_name));

	BOOL secure_mode;
	BOOL private_mode;
	RETURN_IF_ERROR(g_desktop_extensions_manager->GetExtensionSecurity(m_gadget_id, secure_mode, private_mode));

	m_secure_mode.Set(secure_mode == TRUE);
	m_private_mode.Set(private_mode == TRUE);
	RETURN_IF_ERROR(SetWarningAccessLevel());

	return OpStatus::OK;
}


void ExtensionUpdateController::OnOk()
{	
	//make sure that gadget still exist, it might have been deleted via other opera window
	OpGadget *gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_PATH,m_gclass->GetGadgetPath());
	if (gadget)
	{
		g_desktop_extensions_manager->UpdateExtension(gadget->GetIdentifier());
		g_desktop_extensions_manager->SetExtensionSecurity(gadget->GetIdentifier(), m_secure_mode.Get(), m_private_mode.Get());
	}
}
	
