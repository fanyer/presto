// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Patricia Aas
//

#ifndef __DOWNLOAD_ITEM_H__
#define __DOWNLOAD_ITEM_H__

#include "modules/dochand/docman.h"
#include "modules/pi/OpWindow.h"
#include "modules/skin/OpWidgetImage.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/src/TransferManagerDownload.h"
#include "adjunct/desktop_util/handlers/HandlerContainer.h"
#include "adjunct/desktop_util/handlers/MimeTypeContainer.h"
#include "adjunct/desktop_util/handlers/ServerContainer.h"
#include "adjunct/desktop_util/handlers/FileContainer.h"
#include "adjunct/desktop_util/handlers/PluginContainer.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/desktop_pi/desktop_file_chooser.h"

typedef enum {
	FILE_CHOOSING_SAVE,
	FILE_CHOOSING_OTHER_APP,
	FILE_CHOOSING_NONE
} FileChoosingAction;

class DownloadItem :
	public DesktopFileChooserListener
{
public:

	class DownloadItemCallback
	{
	public:
		virtual ~DownloadItemCallback(){}

		/**
		 * This function should be used to call CloseDialog with appropriate parameters
		 * @param cancel signifies that the OnCancel method shall  be called
		 */
		virtual void OnSaveDone(BOOL cancel) {}

		/**
		 *
		 * @param handler
		 */
		virtual void OnOtherApplicationChosen(BOOL choice_made) {}

		/**
		 * This function returns the parent of the eventual filechooser
		 * called when saving (aka, the window of this dialog)
		 */
		virtual OpWindow* GetParentWindow() { return NULL; }
	};

//  -------------------------
//  Public member functions:
//  -------------------------

	//  -------------------------
	//  INITIALIZING :
	//  -------------------------

	DownloadItem(URLInformation * url_information, 
				DesktopFileChooser* chooser,
				BOOL is_download = FALSE,
				BOOL is_privacy_mode = FALSE);

    DownloadItem(OpDocumentListener::DownloadCallback * dcb,
				 DesktopFileChooser* chooser,
				 BOOL is_download = FALSE,
				 BOOL is_privacy_mode = FALSE);
    /**
       Init function used in the DownloadDialog to set up the ItemHandler
       @return OpStatus::OK if the initialization was successful
    */
    OP_STATUS Init(UINT file_icon_size     = 16,
				   UINT server_icon_size   = 16,
				   UINT handler_icon_size  = 16,
				   UINT mimetype_icon_size = 16);

	//  -------------------------
	//  DESTRUCTOR :
	//  -------------------------

    /**
       Destroys the item
    */
    ~DownloadItem();

	//  -------------------------
	//  DOWNLOAD FUNCTIONS :
	//  -------------------------

	OP_STATUS Save(BOOL	save, DownloadItemCallback * listener_callback = NULL);

	OP_STATUS SaveDone(BOOL	save, OpString & saveto_filename);

    OP_STATUS Open(BOOL save, BOOL allow_direct_execute, BOOL from_file_type_dialog_box);

    OP_STATUS Cancel();

    OP_STATUS Close();

	OP_STATUS ChooseOtherApplication(DownloadItemCallback * listener_callback);

	//  -------------------------
	//  ALL AROUND FUNCTIONS :
	//  -------------------------

    /**
	   Launches the FileTypeDialog so that the user can change the settings in
	   the viewer manually.

       @param doc_man
       @return OpStatus::OK if the close was successful
    */
    OP_STATUS Change(DesktopWindow* parent_window);

    /**
	   Updates the DownloadItem - reads some values back from the url.

       @return OpStatus::OK if the update was successful

	   NOTE : The mimetype can change in the URL when some data has been read,
	   that's why it's important to call update before reading from the
	   DownloadItem.
    */
    OP_STATUS Update();

    /**
       @return the total content size (in bytes)
    */
    OpFileLength GetContentSize();

	/**
	   @param
	   @param

	   @return TRUE if a valid size was found
    */
	BOOL GetContentSize(OpString & size_string, UINT32 format_flags=SFBS_DEFAULT);

    /**
       @return the number of bytes downloaded so far
    */
	OpFileLength GetLoadedSize();

	/**
	   @param
	   @param
    */
	void GetLoadedSize(OpString & size_string, UINT32 format_flags=SFBS_DEFAULT);

	/**
	   Makes a string with the percent loaded so far (ie : "25%") and places
	   it in the parameter. Can be used for labels on progressbars and so on.

	   @param percent_string - string where the percent string will be placed
    */
	void GetPercentLoaded(OpString & percent_string);

    /**
       @return the mode of the download item
    */
    HandlerMode GetMode();

	/**
	   @param filename - the string where the filename will be placed
       @return OpStatus::OK if successful

	   NOTE : This funcion should only be used where the filename/url is
	   passed directly to the application (like in OpenWith) - in all other
	   cirmumstances the GetName function of the FileContainer should be used.
    */
	OP_STATUS GetFileName(OpString & filename);

#ifdef CONTENT_DISPOSITION_ATTACHMENT_FLAG
    /**
       @return
    */
    BOOL GetUsedContentDispositionAttachment();
#endif //CONTENT_DISPOSITION_ATTACHMENT_FLAG

    /**
		@return the container of the selected handler/plugin
    */
	DownloadManager::Container * GetSelected();

	/**
	   Set the selected handler/plugin

	   @param container - the container of the selected handler/plugin
	 */
    OP_STATUS SetSelected(DownloadManager::Container * container);

	//  -------------------------
	//  External Handlers :
	//  -------------------------

    /**
		@return TRUE if the file has a handler at the specified index
    */
    BOOL HasHandler() { return HasHandler(0); }

   /**
       @return the number of handlers
    */
	UINT GetHandlerCount() { return m_handlers.GetCount(); }

    /**
       @param handlers - where all the handlers (in the right order) will be placed
       @return OpStatus::OK if successful
    */
    OP_STATUS GetFileHandlers(OpVector<HandlerContainer>& handlers);

	/**
	   Get the user selected handler

	   @param user_handler - HandlerContainer pointer which will be set to the user
	                         handler container, note that this container can be empty.

	   @return OpStatus::OK if no error occured
	*/
	OP_STATUS GetUserHandler(HandlerContainer *& user_handler);

    /**
       @return pointer to the handler at the selected position
    */
    HandlerContainer * GetSelectedHandler();

	//  -------------------------
	//  Plugins :
	//  -------------------------

	/**
	   @return TRUE if the file has a plugin handler at the specified index

    */
	BOOL HasPlugin() { return HasPlugin(0); }

  	/**
       @return the number of plugins
    */
	UINT GetPluginCount() { return m_plugins.GetCount(); }

    /**
       @param plugins - where all the plugins (in the right order) will be placed
       @return OpStatus::OK if successful
    */
    OP_STATUS GetPlugins(OpVector<PluginContainer>& plugins);

    /**
       @return pointer to the plugin at the selected position
    */
    PluginContainer * GetSelectedPlugin();

	//  -------------------------
	//  Containers :
	//  -------------------------

    /**
       @return reference to the MimeTypeContainer
    */
    MimeTypeContainer & GetMimeType();

    /**
       @return reference to the GetServer
    */
    ServerContainer & GetServer(BOOL force_init = FALSE);

    /**
       @return reference to the FileContainer
    */
	FileContainer & GetFile(BOOL force_init = FALSE);

	/**
       @return TRUE is item is downloaded from private window 
    */
	BOOL GetPrivacyMode() { return m_is_privacy_mode; }

	UINT GetFileIconSize()     { return m_file_icon_size; }
	UINT GetServerIconSize()   { return m_server_icon_size; }
	UINT GetHandlerIconSize()  { return m_handler_icon_size; }
	UINT GetMimeTypeIconSize() { return m_mimetype_icon_size; }

	BOOL HasDefaultViewer();

	void SetOperaAsViewer()		{ m_viewer.SetAction(VIEWER_OPERA); }

	// == DesktopFileChooserListener ======================

	virtual void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);

 private:

	/**
	 * Common initialization code for DownloadItem.
	 *
	 * @param download_callback Download callback.
	 * @param is_download Is download.
	 * @param is_privacy_mode Privacy mode
	 * @param chooser DownloadItem doesn't create its own DesktopFileChooser,
	 * you have to provide one. Not all of its functionality needs a chooser, 
	 * so by default this object is NULL. DownloadItem doesn't take ownership 
	 * of this object!
	 */
    void Init(TransferManagerDownloadAPI* download_callback,
			BOOL is_download, BOOL is_privacy_mode, DesktopFileChooser* chooser);

    /**
		@return TRUE if the file has a handler at the specified index

		NOTE : If selected_handler is 0 this will return TRUE if there is
		at least one handler.
    */
    BOOL HasHandler(UINT selected_handler);

    /**
	   @param index - the index of the handler to be selected
       @return OpStatus::OK if successful
    */
    OP_STATUS SetSelectedHandlerIndex(int index);

    /**
	   @param handler - the handler to be selected
       @return OpStatus::OK if successful
    */
    OP_STATUS SetSelectedHandler(HandlerContainer * handler);

    /**
		@return TRUE if the file has a plugin handler at the specified index

		NOTE : If selected_plugin is 0 this will return TRUE if there is
		at least one plugin.
    */
	BOOL HasPlugin(UINT selected_plugin);

   /**
	   @param index - the index of the plugin to be selected
       @return OpStatus::OK if successful
    */
    OP_STATUS SetSelectedPluginIndex(UINT index);

    /**
	   @param plugin - the plugin to be selected
       @return OpStatus::OK if successful
    */
    OP_STATUS SetSelectedPlugin(PluginContainer * plugin);

    BOOL ViewerExists();
    BOOL IsLoading();
    BOOL IsLoaded();
    OP_STATUS CommitViewer();
    OP_STATUS OpenLocalFile();
	OP_STATUS ChangeMimeType(OpString & mime_type);

	OP_STATUS Invalidate();
	OP_STATUS ResetVars();
    OP_STATUS Init(OpString & type);
    OP_STATUS GetMimeType(OpString & type);

    void SetMimeTypeOnExtension();
	void MakeMimeType(const uni_char * mime_type);
	BOOL MakeFormatedSize(OpString & size_string,
						  OpFileLength size,
						  UINT32 format_flags);

    OP_STATUS      CreateViewer();
    OP_STATUS      CopyViewer();
    const Viewer & GetViewer();
    ViewAction     GetAction();

	BOOL IsLocalFile();

    Viewer * FindViewer();
	int GetId();
    URLType GetType();
    const MimeTypeContainer & GuessMimeType();
	BOOL IsViewerSelectedPlugin(PluginContainer * plugin);

	OP_STATUS GetFinalFilename(OpWindow * parent,
							   OpString * finalname,
							   BOOL save,
							   DownloadItemCallback * osdc);

	FileChoosingAction GetFileChoosingAction() { return m_file_choosing_action; }
	OP_STATUS SetFileChoosingAction(FileChoosingAction action);
	void ResetFileChoosingAction() { m_file_choosing_action = FILE_CHOOSING_NONE; }

//  -------------------------
//  Private member variables:
//  -------------------------
	TransferManagerDownloadAPI * 	m_download_callback;

    BOOL      m_keep_temp_viewer;

    Viewer m_viewer;
    Viewer * m_original_viewer;

	OpString m_user_command;

	OpAutoVector<PluginContainer>  m_plugins;
	OpAutoVector<HandlerContainer> m_handlers;
	HandlerContainer m_user_handler;

	INT  m_selected_handler;
	UINT m_selected_plugin;

	BOOL m_user_handler_loaded;
	BOOL m_handlers_loaded;
	BOOL m_plugins_loaded;
	BOOL m_viewer_loaded;
	BOOL m_save;

    MimeTypeContainer m_mime_type;
	ServerContainer   m_server;
	FileContainer     m_file;

	UINT m_file_icon_size;
	UINT m_server_icon_size;
	UINT m_handler_icon_size;
	UINT m_mimetype_icon_size;

    HandlerMode m_mode;

	BOOL m_initialized;
	BOOL m_is_download;
	BOOL m_is_privacy_mode;

	DesktopFileChooser* m_chooser;
	DesktopFileChooserRequest m_request;
	DownloadItemCallback * m_listener_callback;
	FileChoosingAction m_file_choosing_action;
	OpAutoPtr<DesktopFileChooserListener> m_chooser_listener;
};

#endif //__DOWNLOAD_ITEM_H__
