/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Patricia Aas
 */
#include "core/pch.h"

#include "adjunct/quick/dialogs/DownloadDialog.h"
#include "modules/util/filefun.h"
#include "modules/dochand/win.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

//Widgets :
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "modules/widgets/OpDropDown.h"

#ifndef WIN_CE
#include <errno.h>
#include <sys/stat.h>
#endif

#define RUN_BUTTON_DELAY_TIME	1000  // [ms]
#define OPEN_BUTTON_DELAY_TIME	750   // [ms]

/***********************************************************************************
**
**	DownloadDialog
**
***********************************************************************************/

DownloadDialog::DownloadDialog(DownloadItem * download_item,
		INT32 default_button_index)
	: m_download_item(download_item),
	  m_save_action(FALSE),
	  m_accept_open(TRUE),
	  m_initialized(FALSE),
	  m_change_index(-1),
	  m_default_button_index(default_button_index),
	  m_filter_opera_out(FALSE),
	  m_is_executing_action(FALSE)
{
	OP_ASSERT(0 <= m_default_button_index
			&& m_default_button_index < BUTTON_COUNT);

	//Make the download item (the backend)
	if (0 != (m_timer = OP_NEW(OpTimer, ())))
		m_timer->SetTimerListener(this);

	if (0 != (m_update_icon_timer = OP_NEW(OpTimer, ())))
		m_update_icon_timer->SetTimerListener(this);

	if (0 != (m_update_progress_timer = OP_NEW(OpTimer, ())))
		m_update_progress_timer->SetTimerListener(this);
}

/***********************************************************************************
**
**	~DownloadDialog
**
***********************************************************************************/

DownloadDialog::~DownloadDialog()
{
	OP_DELETE(m_timer);
	OP_DELETE(m_update_icon_timer);
	OP_DELETE(m_update_progress_timer);
	OP_DELETE(m_download_item);
}

/***********************************************************************************
**
**	OnActivate
**
***********************************************************************************/

void DownloadDialog::OnActivate(BOOL activate,
								BOOL first_time)
{
	//The first time this is called on windows the Init function of the dialog has
	//not yet been called - meaning that the backend is not initialized either.
	if (activate && m_initialized)
	{
		m_accept_open = FALSE;

		int delay = IsFileExecutable() ? RUN_BUTTON_DELAY_TIME : OPEN_BUTTON_DELAY_TIME;
		m_timer->Start(delay);
	}

	if (!activate)
	{
		m_accept_open = FALSE;
		g_input_manager->UpdateAllInputStates();
	}

	Dialog::OnActivate(activate, first_time);
}

/***********************************************************************************
**
**	OnInitVisibility
**
***********************************************************************************/

void DownloadDialog::OnInitVisibility()
{
	OpString title;

	if (IsFileExecutable())
	{
		// D_DOWNLOAD_DLG_RUN_FILE contains a %s which was a placeholder for
		// the file name, for security reason we don't want it anymore. DSK-284600
		g_languageManager->GetString(Str::D_DOWNLOAD_DLG_RUN_FILE, title);
		title.ReplaceAll(UNI_L("%s"), UNI_L(""));

		ShowWidget("label_for_Download_dropdown", FALSE);
		ShowWidget("Open_icon",                   FALSE);
		ShowWidget("Download_dropdown",           FALSE);
		ShowWidget("Change_button",               FALSE);
		ShowWidget("Save_action_checkbox",        FALSE);

		OpLabel * warning_label = (OpLabel*) GetWidgetByName("run_warning_label");

		if(warning_label)
		{
			warning_label->SetWrap(TRUE);
			ShowWidget("run_warning_label", TRUE);
		}
	}
	else
	{
		// D_DOWNLOAD_DLG_DOWNLOADING_FILE contains a %s which was a placeholder for
		// the file name, for security reason we don't want it anymore. DSK-284600
		g_languageManager->GetString(Str::D_DOWNLOAD_DLG_DOWNLOADING_FILE, title);
		title.ReplaceAll(UNI_L("%s"), UNI_L(""));
		ShowWidget("run_warning_label", FALSE);
	}

	SetTitle(title.CStr());
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS DownloadDialog::Init(DesktopWindow* parent_window)
{
	//Call superclass init:
	Dialog::Init(parent_window);

	// force parent window to be at least as big as download dialog
	UINT32 w,h;
	OpRect r;
	parent_window->GetInnerSize(w,h);
	Dialog::GetPlacement(r);
	parent_window->SetMinimumInnerSize(r.width, r.height);
	if (w < (UINT32)r.width || h < (UINT32)r.height)
	{
		w = MAX(w, (UINT32)r.width);
		h = MAX(h, (UINT32)r.height);
		parent_window->SetInnerSize(w, h);
	}

	//Get the sizes of the icons to be supplied by the download item :
	UINT file_icon_size   = 16;
	UINT server_icon_size = 16;
	UINT open_icon_size   = 16;
	UINT type_icon_size   = 16;

	GetIconSizes(file_icon_size,
				 server_icon_size,
				 open_icon_size,
				 type_icon_size);

	//Initialize the download item:
	m_download_item->Init(file_icon_size,
						  server_icon_size,
						  open_icon_size,
						  type_icon_size);

	//Update the widgets:
	RETURN_IF_ERROR(UpdateWidgets());

	//Set accept open to FALSE:
	m_accept_open = FALSE;

	//Start timer:
	int delay = IsFileExecutable() ? RUN_BUTTON_DELAY_TIME : OPEN_BUTTON_DELAY_TIME;
	m_timer->Start(delay);

	// In UpdateWidgets Open button might be enabled, it should be disabled instead
	EnableButton(0, FALSE);

	m_initialized = TRUE;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetButtonInfo
**
***********************************************************************************/

void DownloadDialog::GetButtonInfo(INT32 index,
								   OpInputAction*& action,
								   OpString& text,
								   BOOL& is_enabled,
								   BOOL& is_default,
								   OpString8& name)
{
	switch (index)
	{
		case 0:
			action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN));

			if (IsFileExecutable())
				g_languageManager->GetString(Str::D_DOWNLOAD_DLG_RUN, text);
			else
				g_languageManager->GetString(Str::SI_OPEN_BUTTON_TEXT, text);
			name.Set(WIDGET_NAME_BUTTON_OPEN);
			break;
		case 1:
			action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SAVE));
			g_languageManager->GetString(Str::SI_SAVE_BUTTON_TEXT, text);
			name.Set(WIDGET_NAME_BUTTON_SAVE);
			break;
		case 2:
			action = GetCancelAction();
			text.Set(GetCancelText());
			name.Set(WIDGET_NAME_BUTTON_CANCEL);
			break;
		case 3:
			action = GetHelpAction();
			text.Set(GetHelpText());
			name.Set(WIDGET_NAME_BUTTON_HELP);
			break;
	}

	is_default = m_default_button_index == index;
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL DownloadDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch(child_action->GetAction())
			{
				case OpInputAction::ACTION_OPEN:
				{
					child_action->SetEnabled(m_accept_open);
					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_SAVE:
		{
			OnSave();
			return TRUE;
		}

		case OpInputAction::ACTION_OPEN:
			if (OnOpen(TRUE,FALSE))
			{
				m_is_executing_action = FALSE;
				CloseDialog(FALSE);
			}
			return TRUE;

		case OpInputAction::ACTION_CHANGE:
		{
			OnChange();
			break;
		}
	}

	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**
**	OnSave
**
***********************************************************************************/

BOOL DownloadDialog::OnSave()
{
	m_save_action = GetWidgetValue("Save_action_checkbox");

	BOOL cancelled = FALSE;

	OP_STATUS status = m_download_item->Save(m_save_action,
											 this);
	m_is_executing_action = TRUE;
	return OpStatus::IsSuccess(status) && !cancelled;
}

/***********************************************************************************
**
**	OnSaveDone
**
***********************************************************************************/

void DownloadDialog::OnSaveDone(BOOL cancel)
{
	m_is_executing_action = FALSE;
	CloseDialog(cancel);
}

/************************************************************************************
**
**	OnOpen
**
***********************************************************************************/

BOOL DownloadDialog::OnOpen(BOOL allow_direct_execute,
							BOOL from_file_type_dialog_box)
{
	m_save_action = GetWidgetValue("Save_action_checkbox");

	if (!SetSelectedApplication()) // FALSE means we are using a chooser...
		return FALSE;

	m_is_executing_action = TRUE;

	OP_STATUS status = m_download_item->Open(m_save_action,
											 allow_direct_execute,
											 from_file_type_dialog_box);
	OP_ASSERT(OpStatus::IsSuccess(status)); // FIXME: ignored status return !
	OpStatus::Ignore(status);
	return TRUE;
}

/************************************************************************************
**
**	OnChange
**
***********************************************************************************/

void DownloadDialog::OnChange(OpWidget *widget,
							  BOOL changed_by_mouse)
{
	if(widget == (OpDropDown*) GetWidgetByName("Download_dropdown"))
	{
		SetSelectedApplication();
	}
}

/************************************************************************************
**
**	SetSelectedApplication
**
***********************************************************************************/

BOOL DownloadDialog::SetSelectedApplication()
{
	OpDropDown * download_dropdown = (OpDropDown*) GetWidgetByName("Download_dropdown");

	if(!download_dropdown)
		return TRUE;

	// Get the item that was selected
	UINT selected_item = download_dropdown->GetSelectedItem();

	// Get the container of that item :
	DownloadManager::Container * container = 0;
	container = (DownloadManager::Container *) download_dropdown->GetItemUserData(selected_item);

	// If no container was chosen - then the user wants another application :
	if(!container)
	{
		m_download_item->ChooseOtherApplication(this);
		return FALSE;
	}

	// Set the container as the selected container :
	m_download_item->SetSelected(container);
	return TRUE;
}

/************************************************************************************
**
**	OnOtherApplicationChosen
**
***********************************************************************************/

void DownloadDialog::OnOtherApplicationChosen(BOOL choice_made)
{
	OpDropDown * dropdown = (OpDropDown*) GetWidgetByName("Download_dropdown");

	if(!dropdown)
		return;

	if(choice_made)
	{
		// Get the container for the user specified app :
		HandlerContainer * handler = 0;
		m_download_item->GetUserHandler(handler);

		// Get the index of the container if it is already in the dropdown :
		int index = GetIndex(handler);

		// Remove it if it is :
		if(index != -1)
			dropdown->RemoveItem(index);

		// Find the correct index to insert :
		int insert_pos = (index == -1) ? (dropdown->CountItems() - 1) : index;

		// Add the new item :
		dropdown->AddItem(handler->GetCommand().CStr(), insert_pos, &index, reinterpret_cast<INTPTR>(handler));

		// Select it :
		SelectItem(handler);
	}
	else
	{
		// Reselect the previously selected container :
		SelectItem(m_download_item->GetSelected());
	}
}

/************************************************************************************
**
**	OnChange
**
***********************************************************************************/

BOOL DownloadDialog::OnChange()
{
	//update because UpdateWidgets uses it and is called by FiletypeDialog::OnOk
	m_save_action = GetWidgetValue("Save_action_checkbox");
	OP_STATUS status = m_download_item->Change(this);
	OP_ASSERT(OpStatus::IsSuccess(status)); // FIXME: ignored status return !
	OpStatus::Ignore(status);

	return TRUE;
}

/************************************************************************************
**
**	EnableOpen
**
***********************************************************************************/

void DownloadDialog::EnableOpen()
{
	BOOL enable_open = TRUE;

	// * Get the action :

	HandlerMode action = m_download_item->GetMode();

	// * Enable/disable Open :

	if(action == HANDLER_MODE_SAVE)
	{
		enable_open = FALSE;
	}
	else
	{
		// Default behaviour is to enable open :
		enable_open = TRUE;
		// One shall be able to choose Other application regardless
		// of if there is no handlers defined for the mime-type
	}

	m_accept_open = enable_open;
	EnableButton(0, m_accept_open);
	EnableButton(1, TRUE);
	FocusButton(m_default_button_index);
	SetWidgetEnabled("Download_dropdown", m_accept_open);
}

/************************************************************************************
**
**	GetIconSizes
**
***********************************************************************************/

void DownloadDialog::GetIconSizes(UINT & file_icon_size,
								  UINT & server_icon_size,
								  UINT & open_icon_size,
								  UINT & type_icon_size)
{
	OpLabel * fileicon   = (OpLabel*) GetWidgetByName("File_icon");
	OpLabel * servericon = (OpLabel*) GetWidgetByName("Server_icon");
	OpLabel * openicon   = (OpLabel*) GetWidgetByName("Open_icon");
	OpLabel * typeicon   = (OpLabel*) GetWidgetByName("Type_icon");

	if(fileicon)
	{
		OpRect rect = fileicon->GetOriginalRect();
		file_icon_size = rect.height;
	}

	if(servericon)
	{
		OpRect rect = servericon->GetOriginalRect();
		server_icon_size = rect.height;
	}

	if(openicon)
	{
		OpRect rect = openicon->GetOriginalRect();
		open_icon_size = rect.height;
	}

	if(typeicon)
	{
		OpRect rect = typeicon->GetOriginalRect();
		type_icon_size = rect.height;
	}
}

/***********************************************************************************
**
**	UpdateWidgets
**
**  we need to be careful about the information we present to the user here;
**  when "OPEN" is pressed the action must be the same as presented
**
***********************************************************************************/

OP_STATUS DownloadDialog::UpdateWidgets()
{
	// ----------------------
	// Update the file item handler :
	// ----------------------
	m_download_item->Update();

	// ----------------------
	// UPDATE WIDGETS :
	// ----------------------

	// ---------------------- Mime Type :

	SetTypeIcon();              //Widget name : "Type_icon"
	SetTypeInfo();              //Widget name : "Type_info"
	SetTypeInfoLabel();         //Widget name : "label_for_Type_info"

	// ---------------------- Server :

	SetServerIcon();            //Widget name : "Server_icon"
	SetServerInfo();            //Widget name : "Server_info"
	SetServerInfoLabel();       //Widget name : "label_for_Server_info"

	// ---------------------- File :

	SetFileIcon();              //Widget name : "File_icon"
	SetFileInfo();              //Widget name : "File_info"
	SetFileInfoLabel();         //Widget name : "label_for_File_info"

	// ---------------------- Download :
	SetSizeInfo();              //Widget name : "Size_info"
	SetDownloadProgress();      //Widget name : "Download_progress"

	// ---------------------- Handler :

	SetOpenIcon();              //Widget name : "Open_icon"
	SetOpenInfo();              //Widget name : "Open_info"

	// SetTypeIcon() must not be called after this, as it uses
	// GetMimeType() which can cause the DownloadItem to invalidate
	// the handlers that are referenced in the dropdown items
	SetDownloadDropdown();      //Widget name : "Download_dropdown"
	SetDownloadDropdownLabel(); //Widget name : "label_for_Download_dropdown"

	// ----------------------
	// Present save settings option :
	// ----------------------

	SetWidgetValue("Save_action_checkbox", m_save_action);

	// ----------------------
	// Enable / Disable controls :
	// ----------------------

	EnableOpen(); //Will check if open should be enabled

	// ----------------------
	// DISABLE CONTROLS CASES :
	// ----------------------

	//(julienp) See bug 287201. Noone seems to know the reason why this code was there.
	// It can be removed if no one complains about it issues regarding it (today is 18th Oct 2007)
	// Additional comment: this piece of code actually fixes bug # 285358, so we need a more refined way
	// to fix 287201. rfz 20080904. 
#ifdef CONTENT_DISPOSITION_ATTACHMENT_FLAG
	// 1. Disable some controls if the content disposition flag has been set :
	// 1'. but not if there is no handler defined for this mime - type (a more real fix for 287201).
	if(m_download_item->GetUsedContentDispositionAttachment() && m_download_item->HasDefaultViewer())
	{
		SetWidgetEnabled("Save_action_checkbox", FALSE);
		SetWidgetEnabled("Change_button", FALSE);
	}
#endif //CONTENT_DISPOSITION_ATTACHMENT_FLAG

	// 2. Disable some controls if the file is a program :
	if (IsFileExecutable())
	{
		SetWidgetEnabled("Save_action_label", FALSE);
		SetWidgetEnabled("Save_action_checkbox", FALSE);
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
//Widget name : "Open_icon"
void DownloadDialog::SetOpenIcon()
{
	OpLabel * openicon = (OpLabel*) GetWidgetByName("Open_icon");

	if(!openicon)
		return;

	UINT icon_size = m_download_item->GetHandlerIconSize();

	openicon->SetSize(icon_size, icon_size);

	// * Get the action :
	HandlerMode action = m_download_item->GetMode();

	// * Set operas own icon for file to be opened by opera or a plugin :
	BOOL needs_appicon = TRUE;

	switch(action)
	{
		case HANDLER_MODE_PLUGIN   :
		case HANDLER_MODE_INTERNAL :
		{
			openicon->GetBorderSkin()->SetImage("Window Browser Icon");
			needs_appicon = FALSE;
		}
		break;
	}

	HandlerContainer * handler = m_download_item->GetSelectedHandler();

	if(handler && needs_appicon)
	{
		Image image = handler->GetImageIcon();
		openicon->GetBorderSkin()->SetBitmapImage(image);
	}

	m_update_icon_timer->Start(333);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
//Widget name : "Type_icon");
void DownloadDialog::SetTypeIcon()
{
	OpLabel * typeicon = (OpLabel*) GetWidgetByName("Type_icon");

	if(!typeicon)
		return;

	UINT icon_size = m_download_item->GetMimeTypeIconSize();

	typeicon->SetSize(icon_size, icon_size);

	Image image = m_download_item->GetMimeType().GetImageIcon();
	typeicon->GetBorderSkin()->SetBitmapImage(image);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
//Widget name : "Server_icon"
void DownloadDialog::SetServerIcon()
{
	OpLabel * servericon = (OpLabel*) GetWidgetByName("Server_icon");

	if(!servericon)
		return;

	UINT icon_size = m_download_item->GetServerIconSize();

	servericon->SetSize(icon_size, icon_size);

	Image image = m_download_item->GetServer().GetImageIcon();
	servericon->GetBorderSkin()->SetBitmapImage(image);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
//Widget name : "File_icon"
void DownloadDialog::SetFileIcon()
{
	OpLabel * fileicon = (OpLabel*) GetWidgetByName("File_icon");

	if(!fileicon)
		return;

	UINT icon_size = m_download_item->GetFileIconSize();

	fileicon->SetSize(icon_size, icon_size);

	Image image = m_download_item->GetFile().GetImageIcon();
	fileicon->GetBorderSkin()->SetBitmapImage(image);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
//Widget name : "File_info"
void DownloadDialog::SetFileInfo()
{
	OpLabel * filename_text = (OpLabel*) GetWidgetByName("File_info");

	if(!filename_text)
		return;

	OpString fileinfo;

	fileinfo.Append(m_download_item->GetFile().GetName().CStr());

	OpString bytesstr;

	if(m_download_item->GetContentSize(bytesstr))
	{
		fileinfo.AppendFormat(UNI_L(" (%s)"), bytesstr.CStr());
	}

	SetWidgetText("File_info", fileinfo.CStr());
	filename_text->SetEllipsis(ELLIPSIS_CENTER);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
//Widget name : "Server_info"
void DownloadDialog::SetServerInfo()
{
	OpLabel * servername_text = (OpLabel*) GetWidgetByName("Server_info");

	if(!servername_text)
		return;

	SetWidgetText("Server_info", m_download_item->GetServer().GetName().CStr());
	servername_text->SetEllipsis(ELLIPSIS_CENTER);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
//Widget name : "Open_info"
void DownloadDialog::SetOpenInfo()
{
	OpLabel * open_text = (OpLabel*) GetWidgetByName("Open_info");

	if(!open_text)
		return;

	HandlerContainer * handler = m_download_item->GetSelectedHandler();

	OpString open_txt;

	if(handler)
	{
		open_txt.Set(handler->GetName().CStr());
		open_txt.Append(" (");
		open_txt.Append(handler->GetCommand().CStr());
		open_txt.Append(")");
	}

	SetWidgetText("Open_info", open_txt.CStr());
}

/***********************************************************************************
**
**
**
***********************************************************************************/
//Widget name : "Size_info"
void DownloadDialog::SetSizeInfo()
{
	OpLabel * size_text = (OpLabel*) GetWidgetByName("Size_info");

	if(!size_text)
		return;

	OpString bytesstr;
	m_download_item->GetContentSize(bytesstr);

	SetWidgetText("Size_info", bytesstr.CStr());
}


/***********************************************************************************
**
**
**
***********************************************************************************/
//Widget name : "Type_info"
void DownloadDialog::SetTypeInfo()
{
	OpLabel * type_text = (OpLabel*) GetWidgetByName("Type_info");

	if(!type_text)
		return;

	const uni_char * type_name = 0;

	type_name = m_download_item->GetMimeType().GetName().CStr();

	if(!type_name)
		type_name = m_download_item->GetMimeType().GetMimeType().CStr();

	SetWidgetText("Type_info", type_name);
}

void DownloadDialog::RightAlignLabel(const OpStringC8& name)
{
	OpLabel* label = GetWidgetByName<OpLabel>(name, WIDGET_TYPE_LABEL);
	if (label)
		label->SetJustify(label->GetRTL() ? JUSTIFY_LEFT : JUSTIFY_RIGHT, FALSE);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
//Widget name : "Download_progress"
void DownloadDialog::SetDownloadProgress()
{
	OpProgressBar * progressbar = (OpProgressBar*) GetWidgetByName("Download_progress");

	if(!progressbar)
		return;

	OpFileLength bytes_downloaded = m_download_item->GetLoadedSize();
	OpFileLength bytes_total      = m_download_item->GetContentSize();

	OpString progress_label;
	m_download_item->GetPercentLoaded(progress_label);

	OpString size_string;
	m_download_item->GetContentSize(size_string);

	OpString loaded_string;
	m_download_item->GetLoadedSize(loaded_string);

	progress_label.Append(" ");
	progress_label.Append(loaded_string.CStr());
	progress_label.Append(" (");
	progress_label.Append(size_string.CStr());
	progress_label.Append(")");

	progressbar->SetProgress(bytes_downloaded, bytes_total);
	progressbar->SetLabel(progress_label.CStr());

	m_update_progress_timer->Start(333);
}



/***********************************************************************************
**
**
**
***********************************************************************************/
//Widget name : "Download_dropdown"
void DownloadDialog::SetDownloadDropdown()
{
	OpDropDown * handlers_dropdown = (OpDropDown*) GetWidgetByName("Download_dropdown");

	if(!handlers_dropdown)
		return;

	handlers_dropdown->Clear();

	// Avoid streched favicons :
	handlers_dropdown->SetRestrictImageSize(TRUE);
	handlers_dropdown->SetListener(this);

	// ----------------------
	// Add Items :
	// ----------------------

	// ---------------------- Handlers :

	AddHandlers(handlers_dropdown);

	// ---------------------- Plugins :

	AddPlugins(handlers_dropdown);

	// ---------------------- Other Application :

	AddChange(handlers_dropdown);

	// ----------------------
	// Set Selected :
	// ----------------------

	SelectItem(m_download_item->GetSelected());
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void DownloadDialog::SelectItem(DownloadManager::Container * container)
{
	OpDropDown * dropdown = (OpDropDown*) GetWidgetByName("Download_dropdown");

	if(!dropdown)
		return;

	if(!container)
		return;

	int index = GetIndex(container);

	if(index == -1)
		return;

	dropdown->SelectItem(index, TRUE);
	m_download_item->SetSelected(container);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
int DownloadDialog::GetIndex(DownloadManager::Container * container)
{
	OpDropDown * dropdown = (OpDropDown*) GetWidgetByName("Download_dropdown");

	if(!dropdown)
		return -1;

	DownloadManager::Container * c = 0;
	int count = dropdown->CountItems();

	for(int i = 0; i < count; i++)
	{
		c = (DownloadManager::Container *) dropdown->GetItemUserData(i);

		if(c == container)
		{
			return i;
		}
	}

	return -1;
}

BOOL DownloadDialog::IsFileExecutable()
{
	OP_ASSERT(NULL != m_download_item);
	return DownloadManager::GetManager()->AllowExecuteDownloadedContent()
			&& m_download_item->GetFile().IsExecutable();
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void DownloadDialog::AddPlugins(OpDropDown * dropdown)
{
	if(!dropdown)
		return;

	//Get the plugins :
	OpVector<PluginContainer> plugins;
	m_download_item->GetPlugins(plugins);

	//Add them to the menu :
	for(UINT32 j = 0; j < plugins.GetCount(); j++)
	{
		PluginContainer * plugin = plugins.Get(j);

		const uni_char * plugin_name = plugin->GetName().CStr();
		OpWidgetImage widget_image;
		widget_image.SetWidgetImage(&plugin->GetWidgetImageIcon());

		widget_image.SetRestrictImageSize(TRUE);

		OpString plugin_txt;
#if defined(_UNIX_DESKTOP_)
		const uni_char * plugin_path = plugin->GetPath().CStr();
		// Quite a number of plug-ins have "No name" as product name in unix
		plugin_txt.AppendFormat(UNI_L("%s - %s"), plugin_name, plugin_path );
#else
		plugin_txt.Set(plugin_name);
#endif

		INT32 got_index = -1;

		dropdown->AddItem(plugin_txt.CStr(), -1, &got_index, reinterpret_cast<INTPTR>(plugin));
		dropdown->ih.SetImage(got_index, "Window Browser Icon", dropdown);
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void DownloadDialog::AddHandlers(OpDropDown * dropdown)
{
	if(!dropdown)
		return;

	//Get the handlers :
	OpVector<HandlerContainer> handlers;
	m_download_item->GetFileHandlers(handlers);

	//Add them to the menu :
	for(UINT32 i = 0; i < handlers.GetCount(); i++)
	{
		HandlerContainer * handler = handlers.Get(i);

		const uni_char * handler_command = handler->GetCommand().CStr();
		const uni_char * handler_name    = handler->GetName().CStr();

		if(m_filter_opera_out && handler_name && !uni_strcmp(handler_name, UNI_L("Opera Internet Browser")))
		{
			// skip opera
			continue;
		}
		OpWidgetImage widget_image;
		widget_image.SetWidgetImage(&handler->GetWidgetImageIcon());

		widget_image.SetRestrictImageSize(TRUE);

		INT32 got_index = -1;
		dropdown->AddItem(handler_name ? handler_name : handler_command, -1, &got_index, reinterpret_cast<INTPTR>(handler));
		if (handler->GetHandlerMode() == HANDLER_MODE_INTERNAL)
			dropdown->ih.SetImage(got_index, "Window Browser Icon", dropdown);
		else
			dropdown->ih.SetImage(got_index, &widget_image, dropdown);
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void DownloadDialog::AddSeperator(OpDropDown * dropdown)
{
	if(!dropdown)
		return;

	INT32 got_index = -1;
	if (OpStatus::IsSuccess(dropdown->AddItem(NULL, -1, &got_index)))
		dropdown->ih.GetItemAtNr(got_index)->SetSeperator(TRUE);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void DownloadDialog::AddChange(OpDropDown * dropdown)
{
	if(!dropdown)
		return;

	HandlerContainer * handler = 0;
	m_download_item->GetUserHandler(handler);

	if(handler->GetCommand().HasContent())
	{
		INT32 got_index = -1;
		dropdown->AddItem(handler->GetCommand().CStr(), -1, &got_index, reinterpret_cast<INTPTR>(handler));
	}

	OpString other_application;
	g_languageManager->GetString(Str::D_DOWNLOAD_OTHER_APPLICATION, other_application);

	dropdown->AddItem(other_application.CStr(), -1, &m_change_index, 0); //TRANS
}

/***********************************************************************************
**
**	OnCancel
**
***********************************************************************************/

void DownloadDialog::OnCancel()
{
	m_download_item->Cancel();
	Dialog::OnCancel();
}

/***********************************************************************************
**
**	OnClose
**
***********************************************************************************/

void DownloadDialog::OnClose(BOOL user_initiated)
{
	m_download_item->Close();
	Dialog::OnClose(user_initiated);
}

/***********************************************************************************
**
**	OnTimeOut
**
***********************************************************************************/

void DownloadDialog::OnTimeOut(OpTimer* timer)
{
	if (timer != m_timer                  &&
		timer != m_update_progress_timer  &&
		timer != m_update_icon_timer)
	{
		Dialog::OnTimeOut(timer);
		return;
	}

	if(timer == m_timer)
	{
		if (IsActive()) // don't allow click-through from window above
		{
			UpdateWidgets();
			g_input_manager->UpdateAllInputStates();
		}
	}
	else if(timer == m_update_progress_timer)
	{
		if (IsActive()) // don't allow click-through from window above
		{
			SetDownloadProgress();
		}
	}
	else if(timer == m_update_icon_timer)
	{
		if (IsActive()) // don't allow click-through from window above
		{
			SetSelectedApplication();
			SetOpenIcon();
		}
	}
}

/***********************************************************************************
**
**	OnDocumentChanging
**
***********************************************************************************/

void DownloadDialog::OnDocumentChanging(DocumentDesktopWindow* document_window)
{
	if (GetParentDesktopWindow() 
		&& GetParentDesktopWindow()->GetType() == WINDOW_TYPE_DOCUMENT && 
		document_window == reinterpret_cast<DocumentDesktopWindow*>(GetParentDesktopWindow()))
	{
		CloseDialog(TRUE, TRUE, FALSE);
	}
}

void DownloadDialog::OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url)
{
}

void DownloadDialog::OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus, BOOL was_stopped_by_user)
{
	if (was_stopped_by_user)
	{
		CloseDialog(TRUE, FALSE, FALSE);
	}
}
