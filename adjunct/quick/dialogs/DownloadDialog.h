/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOWNLOADDIALOG_H
#define DOWNLOADDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/url/url_man.h"
#include "modules/viewers/viewers.h"
#include "modules/windowcommander/src/WindowCommander.h"

#include "adjunct/desktop_util/handlers/DownloadItem.h"

/***********************************************************************************
**
**  DownloadDialog
**
**  Supports the following widgets:
**
**	Widget name : 					Widget type : 			Comment :
**  "Open_icon"						OpLabel					Icon of handler
**	"Type_icon"						OpLabel					Icon of mime type
**	"Server_icon"					OpLabel					Favicon of server
**	"File_icon"						OpLabel					Thumbnail of file (currently not supported)
**	"File_info"						OpLabel					Name of file
**  "label_for_File_info"			OpLabel					Title for the File_info field
**	"Server_info"					OpLabel					Name of server
**  "label_for_Server_info" 		OpLabel					Title for the Server_info field
**	"Open_info"						OpLabel					Name of handler
**	"Size_info"						OpLabel					Size of file
**	"Type_info"						OpLabel					Name of mime type
**  "label_for_Type_info"			OpLabel					Title for the Type_info field
**	"Download_progress"				OpProgressBar			Progress bar showing download
**	"Download_dropdown"				OpDropDown				Dropdown with handlers and plugins
**  "label_for_Download_dropdown" 	OpLabel					Title for the Download_dropdown
**
**  Strings:
**
**  DI_IDM_DOWNDLG_FILE			=17020	"File"
**  DI_IDM_DOWNDLG_SERVER		=17021	"Server"
**  SI_IDSTR_TRANSWIN_TYPE_COL	=22080	"Type"
**  DI_IDL_SIZE					=11001	"Size"
**  D_DOWNLOAD_OPENSWITH		=67179	"Opens with"
***********************************************************************************/

class DownloadDialog
	: public Dialog,
	  public DownloadItem::DownloadItemCallback
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
       @param
       @param
       @return
    */
	DownloadDialog(DownloadItem * download_item, INT32 default_button_index = 1);

	/**

    */
	~DownloadDialog();

	/**
       @return
    */
	OP_STATUS UpdateWidgets();

	/**
       @return
    */
	BOOL OnSave();

	/**
       @param
       @param
       @return
    */
	BOOL OnOpen(BOOL allow_direct_execute,BOOL from_file_type_dialog_box);

	/**
       @param
       @param
       @return
    */
	virtual OP_STATUS Init(DesktopWindow* parent_window);

	virtual BOOL		GetModality() { return TRUE; }
	virtual BOOL		GetOverlayed() { return TRUE; }
	virtual BOOL		GetDimPage() { return TRUE; }

	// == OpWidgetListener ======================

	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);

	// == DownloadItemCallback ======================

	virtual void OnSaveDone(BOOL cancel);
	virtual void OnOtherApplicationChosen(BOOL choice_made);
	virtual OpWindow * GetParentWindow() {return GetOpWindow();}

	// == DocumentWindowListener ======================
	virtual void OnDocumentChanging(DocumentDesktopWindow* document_window);
	virtual void OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url);
	virtual void OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus, BOOL was_stopped_by_user);

	// Filter Opera in the external applications dialog
	void SetFilterOpera(BOOL filter) { m_filter_opera_out = filter; }


protected:

//  ---------------------------
//  Protected member functions:
//  ---------------------------

	virtual void				OnActivate(BOOL activate, BOOL first_time);
	virtual void				OnInitVisibility();
	virtual void				OnCancel();
	virtual void				OnClose(BOOL user_initiated);
	BOOL 						OnChange();
	virtual void				OnSetFocus() {FocusButton(m_default_button_index);}
	virtual Type				GetType()				{return DIALOG_TYPE_DOWNLOAD;}
	virtual const char*			GetWindowName()			{return "Download Dialog";}
	virtual const char*			GetHelpAnchor()			{return "downloads.html";}
	virtual INT32				GetButtonCount()		{return BUTTON_COUNT;}
	virtual void				GetButtonInfo(INT32 index,
											  OpInputAction*& action,
											  OpString& text,
											  BOOL& is_enabled,
											  BOOL& is_default,
											  OpString8& name);
	virtual BOOL				OnInputAction(OpInputAction* action);

	/**
	 * Determines whether the file being downloaded can be executed.
	 * @return @c TRUE iff the file can be executed
	 */
	virtual BOOL IsFileExecutable();

	//  -------------------------
	//  Protected member variables:
	//  -------------------------
	DownloadItem * 	m_download_item;

private:

//  ---------------------------
//  Private member functions:
//  ---------------------------

	// Help functions in enabling, updating or initializing the ui :

	void EnableOpen();
	BOOL SetSelectedApplication(); // return FALSE when calling chooser for help (by use of DownloadItem..)
	void AddPlugins(OpDropDown * dropdown);
	void AddHandlers(OpDropDown * dropdown);
	void AddSeperator(OpDropDown * dropdown);
	void AddChange(OpDropDown * dropdown);
	void SelectItem(DownloadManager::Container * container);
	int  GetIndex(DownloadManager::Container * container);

	// Setting up the widgets :

	void SetOpenIcon();	             //Widget name : "Open_icon"
	void SetTypeIcon();              //Widget name : "Type_icon"
	void SetServerIcon();            //Widget name : "Server_icon"
	void SetFileIcon();              //Widget name : "File_icon"
	void SetFileInfo();              //Widget name : "File_info"
	void SetFileInfoLabel() { RightAlignLabel("label_for_File_info"); }
	void SetServerInfo();            //Widget name : "Server_info"
	void SetServerInfoLabel() { RightAlignLabel("label_for_Server_info"); }
	void SetOpenInfo();              //Widget name : "Open_info"
	void SetSizeInfo();              //Widget name : "Size_info"
	void SetTypeInfo();              //Widget name : "Type_info"
	void SetTypeInfoLabel() { RightAlignLabel("label_for_Type_info"); }
	void SetDownloadProgress();      //Widget name : "Download_progress"
	void SetDownloadDropdown();      //Widget name : "Download_dropdown"
	void SetDownloadDropdownLabel() { RightAlignLabel("label_for_Download_dropdown"); }

	void RightAlignLabel(const OpStringC8& name);

	void GetIconSizes(UINT & file_icon_size,
					  UINT & server_icon_size,
					  UINT & open_icon_size,
					  UINT & type_icon_size);

	// Implementation of the OpTimerListener interface :
	void OnTimeOut(OpTimer* timer);

private:

//  -------------------------
//  Private member variables:
//  -------------------------

	static const INT32 BUTTON_COUNT = 4;
	
	BOOL			m_save_action;
	OpTimer*		m_timer;
	OpTimer*		m_update_icon_timer;
	OpTimer*		m_update_progress_timer;
	BOOL			m_accept_open;
	BOOL			m_initialized;
	int				m_change_index;
	const INT32		m_default_button_index;
	BOOL			m_filter_opera_out;			// If TRUE then Opera will not be shown in the "Open with" dropdown. 
	BOOL			m_is_executing_action;		// Flag indicating whether save or open operation is completed (primary
												// use is to check whether file picker dialog is still displaying)
};

#endif //DOWNLOADDIALOG_H
