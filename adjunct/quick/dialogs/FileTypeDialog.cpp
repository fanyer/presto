/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "adjunct/quick/dialogs/FileTypeDialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpEdit.h"
#include "adjunct/quick/dialogs/DownloadDialog.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

#include <limits.h>

class MimeTypeEditError : public SimpleDialog
{

public:
	OP_STATUS Init(DesktopWindow* parent_window)
	{
		OpString szMimeMissing;
		OpString szMimeMissTitle;
		g_languageManager->GetString(Str::SI_FILETYPE_DIALOG_MIMETYPE_MISSING, szMimeMissing);
		g_languageManager->GetString(Str::SI_FILETYPE_DIALOG_MIMETYPE_MISSING_TITLE, szMimeMissTitle);

		return SimpleDialog::Init(WINDOW_NAME_MIME_TYPE_EDIT_ERROR, szMimeMissTitle, szMimeMissing, parent_window);
	}

	UINT32 OnOk()
	{
		return 1;
	}
};

/***********************************************************************************
**
**	FiletypeDialog
**
***********************************************************************************/

FiletypeDialog::FiletypeDialog(Viewer* viewer, BOOL new_entry)
	: m_viewer(viewer),
	  m_is_new_entry(new_entry),
	  m_webhandler_deleted(FALSE)
{
}

/***********************************************************************************
**
**	FiletypeDialog
**
***********************************************************************************/

FiletypeDialog::~FiletypeDialog()
{
	if (m_is_new_entry)			// adding the new viewer failed or was cancelled
		OP_DELETE(m_viewer);
}

/***********************************************************************************
**
**  OnInit
**
***********************************************************************************/

void FiletypeDialog::OnInit()
{
	OpWidget * active_radiobutton = NULL;
	switch ((int) m_viewer->GetAction())
	{
	case VIEWER_SAVE:
		active_radiobutton = GetWidgetByName("Action_save");
		break;
	case VIEWER_OPERA:
		active_radiobutton = GetWidgetByName("Action_open_Opera");
		break;
	case VIEWER_PASS_URL:
		SetWidgetValue("Action_pass_url", TRUE);
		active_radiobutton = GetWidgetByName("Action_open_other_application");
		break;
	case VIEWER_APPLICATION:
		active_radiobutton = GetWidgetByName("Action_open_other_application");
		break;
	case VIEWER_REG_APPLICATION:
		active_radiobutton = GetWidgetByName("Action_open_default_application");
		break;
	case VIEWER_WEB_APPLICATION:
		active_radiobutton = GetWidgetByName("Action_open_web_handler");
		break;
	case VIEWER_PLUGIN:
		active_radiobutton = GetWidgetByName("Action_open_plugin");
		break;
	case VIEWER_ASK_USER:
	default:
		active_radiobutton = GetWidgetByName("Action_show_downloaddialog");
		break;
	}

	active_radiobutton->SetValue(TRUE);

	SetWidgetText("Mimetype", m_viewer->GetContentTypeString());

	SetWidgetText("Fileextensions", m_viewer->GetExtensions());

	SetWidgetText("Action_open_otherapplication", m_viewer->GetApplicationToOpenWith());

	SetWidgetText("Webhandler_edit", m_viewer->GetWebHandlerToOpenWith());

	SetWidgetText("Action_save_directly_location", m_viewer->GetSaveToFolder());

#ifdef ABOUT_PLUGINS_SHOW_ARCHITECTURE
	OpString native_arch, non_native_arch;
	g_languageManager->GetString(Str::S_PLUGIN_LIST_ARCHITECTURE_NATIVE, native_arch);
	g_languageManager->GetString(Str::S_PLUGIN_LIST_ARCHITECTURE_NON_NATIVE, non_native_arch);
#endif // ABOUT_PLUGINS_SHOW_ARCHITECTURE

	OpDropDown* plugins_dropdown = (OpDropDown*)GetWidgetByName("Plugins_dropdown");
	if (plugins_dropdown && m_viewer)
	{
		UINT plugin_count = m_viewer->GetPluginViewerCount();
		INT active_index = -1;
		const PluginViewer* def_pv = m_viewer->GetDefaultPluginViewer();

		for (UINT i = 0; i < plugin_count; i++)
		{
			const PluginViewer* pv = m_viewer->GetPluginViewer(i);
			if (pv && pv->GetPath() && pv->GetProductName())
			{
				OpString item_text;
#ifdef ABOUT_PLUGINS_SHOW_ARCHITECTURE
				// Differentiate between 32 and 64bit plugins on 64bit systems.
				if (OpStatus::IsError(item_text.AppendFormat(UNI_L("(%s) "),
					pv->GetComponentType() == COMPONENT_PLUGIN ? native_arch.CStr() : non_native_arch.CStr())))
					continue;
#endif // ABOUT_PLUGINS_SHOW_ARCHITECTURE
				if (OpStatus::IsError(item_text.AppendFormat(UNI_L("%s - %s"), pv->GetProductName(), pv->GetPath())))
					continue;

				plugins_dropdown->AddItem(item_text.CStr());

				if (def_pv == pv)
					active_index = i;
			}
		}

		if (active_index != -1)
			plugins_dropdown->SelectItem(active_index, TRUE);
	}

	RefreshControls();

	if( !m_is_new_entry )
	{
		// Shall not be possible to modify mime type when modifying an existing viewer
		SetWidgetEnabled( "Mimetype", FALSE );
		OpEdit *edit = (OpEdit*)GetWidgetByName("Fileextensions");
		if( edit )
		{
			edit->SetCaretPos( edit->GetTextLength() );
			edit->SetFocus(FOCUS_REASON_OTHER);
			edit->SelectAll();
		}
	}

	// Shall not be possible to modify these values.
	SetWidgetEnabled( "Action_open_defaultapplication", FALSE );
	SetWidgetEnabled( "Webhandler_edit", FALSE );

	OnChange(active_radiobutton, FALSE);
}

/***********************************************************************************
**
**	OnCancel
**
***********************************************************************************/

void FiletypeDialog::OnCancel()
{
	Dialog::OnCancel();
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void FiletypeDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if(widget->IsNamed("Fileextensions"))
	{
		RefreshControls();
	}
	if (widget->IsNamed("Action_save"))
	{
		if (widget->GetValue())
		{
//			SetWidgetValue("Action_save_open", m_viewer->GetFlags().IsSet(ViewActionFlag::OPEN_WHEN_COMPLETE));
			SetWidgetValue("Action_save_directly", m_viewer->GetFlags().IsSet(ViewActionFlag::SAVE_DIRECTLY));
//			SetWidgetEnabled("Action_save_open", TRUE);
			SetWidgetEnabled("Action_save_directly", TRUE);
			SetWidgetEnabled("Action_save_directly_location", m_viewer->GetFlags().IsSet(ViewActionFlag::SAVE_DIRECTLY));
		}
		else
		{
//			SetWidgetValue("Action_save_open", FALSE);
			SetWidgetValue("Action_save_directly", FALSE);
//			SetWidgetEnabled("Action_save_open", FALSE);
			SetWidgetEnabled("Action_save_directly", FALSE);
			SetWidgetEnabled("Action_save_directly_location", FALSE);
		}
	}
	if (widget->IsNamed("Action_save_directly"))
	{
		SetWidgetEnabled("Action_save_directly_location", widget->GetValue() ? TRUE : FALSE);
	}
	if (widget->IsNamed("Action_open_other_application"))
	{
		if (widget->GetValue())
		{
			SetWidgetEnabled("Action_pass_url", TRUE);
			SetWidgetEnabled("Action_open_otherapplication", TRUE);
		}
		else
		{
			SetWidgetEnabled("Action_pass_url", FALSE);
			SetWidgetEnabled("Action_open_otherapplication", FALSE);
		}
	}
	if (widget->IsNamed("Action_open_plugin"))
	{
		if (widget->GetValue())
			SetWidgetEnabled("Plugins_dropdown", TRUE);
		else
			SetWidgetEnabled("Plugins_dropdown", FALSE);
	}
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 FiletypeDialog::OnOk()
{

	OpString tmp;

	//we need a type, do not allow an empty mimetype string
	GetWidgetText("Mimetype", tmp);
	tmp.Strip();
	if(!tmp.IsEmpty())
	{
		m_viewer->SetContentType(tmp);
	}
	else
	{
		OP_ASSERT(0);
	}

	GetWidgetText("Fileextensions", tmp);
	m_viewer->SetExtensions(tmp);

	GetWidgetText("Action_save_directly_location", tmp);
	m_viewer->SetSaveToFolder(tmp);

	GetWidgetText("Action_open_otherapplication", tmp);
	m_viewer->SetApplicationToOpenWith(tmp);

	if (m_webhandler_deleted)
		m_viewer->SetWebHandlerToOpenWith(OpStringC());

	ViewActionFlag flags;

/*	if (GetWidgetValue("Action_save_open"))
	{
		flags.Set(ViewActionFlag::OPEN_WHEN_COMPLETE);
	}
*/
	if (GetWidgetValue("Action_save_directly") != m_viewer->GetFlags().IsSet(ViewActionFlag::SAVE_DIRECTLY))
	{
		if (GetWidgetValue("Action_save_directly"))
			flags.Set(ViewActionFlag::SAVE_DIRECTLY);
		else
			flags.Unset(ViewActionFlag::SAVE_DIRECTLY);
	}

	m_viewer->SetFlags(flags.Get());

	ViewAction action = VIEWER_NOT_DEFINED;

	if (GetWidgetValue("Action_save"))
	{
		action = VIEWER_SAVE;
	}
	else if (GetWidgetValue("Action_open_Opera"))
	{
		action = VIEWER_OPERA;
	}
	else if (GetWidgetValue("Action_open_other_application"))
	{
		if ( GetWidgetValue("Action_pass_url"))
			action = VIEWER_PASS_URL;
		else
			action = VIEWER_APPLICATION;
	}
	else if (GetWidgetValue("Action_open_default_application"))
	{
		action = VIEWER_REG_APPLICATION;
	}
	else if (GetWidgetValue("Action_open_web_handler"))
	{
		action = VIEWER_WEB_APPLICATION;
		// Page loading hangs if type is VIEWER_WEB_APPLICATION with no web handler url
		GetWidgetText("Webhandler_edit", tmp);
		if (tmp.IsEmpty())
			action = VIEWER_ASK_USER;
	}
	else if (GetWidgetValue("Action_open_plugin"))
	{
		action = VIEWER_PLUGIN;
	}
	else if (GetWidgetValue("Action_show_downloaddialog"))
	{
		action = VIEWER_ASK_USER;
	}

	// Reset the user flag if web handler was removed. That allows core
	// to give us a notification for a new web handler
	m_viewer->SetAction(action, m_webhandler_deleted ? FALSE : TRUE);

	OpDropDown* plugins_dropdown = (OpDropDown*)GetWidgetByName("Plugins_dropdown");
	if(plugins_dropdown && plugins_dropdown->GetSelectedItem() >= 0 )
	{
		m_viewer->SetDefaultPluginViewer( plugins_dropdown->GetSelectedItem() );
	}

	if (OpStatus::IsSuccess(g_viewers->AddViewer(m_viewer)))
		m_is_new_entry = FALSE;

	if(GetParentDesktopWindow()->GetType() == DIALOG_TYPE_DOWNLOAD)
	{
		DownloadDialog* parentdownloaddialog = (DownloadDialog*)GetParentDesktopWindow();
		//do fileaction here
		switch(m_viewer->GetAction())
		{
			case VIEWER_ASK_USER:	//this is always the case when not invoked from FiletypeDialog
				{
					parentdownloaddialog->UpdateWidgets();
					return Dialog::OnOk();
				}
				break;
			case VIEWER_SAVE:
				{
					parentdownloaddialog->OnSave();
				}
				break;
			case VIEWER_OPERA:
			case VIEWER_PASS_URL:
			case VIEWER_APPLICATION:
			case VIEWER_WEB_APPLICATION:
			case VIEWER_PLUGIN:
				{
					parentdownloaddialog->OnOpen(FALSE,TRUE);
				}
				break;
			case VIEWER_REG_APPLICATION:
			case VIEWER_RUN:
				{
					parentdownloaddialog->OnOpen(TRUE,TRUE);
				}
				break;
		}
		UINT32 ret = Dialog::OnOk();
		parentdownloaddialog->CloseDialog(FALSE);
		return ret;
	}

	return Dialog::OnOk();
}

/***********************************************************************************
**
**	OnClose
**
***********************************************************************************/

void FiletypeDialog::OnClose(BOOL user_initiated)
{
	Dialog::OnClose(user_initiated);
}

/***********************************************************************************
**
**	RefreshControls
**
***********************************************************************************/

void FiletypeDialog::RefreshControls()
{
	OpString filename, extension, file_handler;

	//gives us a comma delimited list of extensions
	GetWidgetText("Fileextensions", extension);

	uni_char* e = extension.CStr();
	if( e )
	{
		//use the first extension, Viewer is not totally logical here
		while (*e && *(++e) != ',') {};
		*e = 0;
	}

	OpString tmp;
	tmp.AppendFormat(UNI_L("unknown.%s"), extension.CStr());
	filename.Set(tmp);

	OpString content_type;
	GetWidgetText("Mimetype", content_type);

	// if the handler is the same as the file don't show it
	if( OpStatus::IsSuccess(g_op_system_info->GetFileHandler(&filename, content_type, file_handler)) && filename.Compare(file_handler) != 0 )
	{
		SetWidgetText("Action_open_defaultapplication", file_handler.CStr());
	}
	else
	{
		SetWidgetText("Action_open_defaultapplication", UNI_L(""));
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL FiletypeDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
		{
			OpString szTmp;

			//we need a type, do not allow an empty mimetype string
			GetWidgetText("Mimetype", szTmp);
			szTmp.Strip();
			if(szTmp.IsEmpty())
			{
				MimeTypeEditError* dialog = OP_NEW(MimeTypeEditError, ());
				if (dialog)
					dialog->Init((DesktopWindow*)this);

				return TRUE;
			}
			break;
		}
		case OpInputAction::ACTION_DELETE:
		{
			m_webhandler_deleted = TRUE;
			SetWidgetText("Webhandler_edit", OpStringC());
			return TRUE;
		}
	}

	return Dialog::OnInputAction(action);
}
