// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Patricia Aas
//

#ifndef __DOWNLOAD_MANAGER_H__
#define __DOWNLOAD_MANAGER_H__

#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpWindow.h"
#include "modules/skin/OpWidgetImage.h"
#include "modules/viewers/viewers.h"
#include "modules/url/url2.h"
#include "modules/util/OpTypedObject.h"

typedef enum {
	HANDLER_MODE_EXTERNAL,
	HANDLER_MODE_PASS_URL,
	HANDLER_MODE_INTERNAL,
	HANDLER_MODE_PLUGIN,
	HANDLER_MODE_SAVE,
	HANDLER_MODE_UNKNOWN
} HandlerMode;

typedef enum {
	CONTAINER_TYPE_FILE     = 0,
	CONTAINER_TYPE_HANDLER  = 1,
	CONTAINER_TYPE_MIMETYPE = 2,
	CONTAINER_TYPE_PLUGIN   = 3,
	CONTAINER_TYPE_SERVER   = 4
} ContainerType;

class DownloadManager
{
public:

//  -------------------------
//  Public member classes:
//  -------------------------

	class Container
	{
	public:

		//  -------------------------
		//  Public member functions:
		//  -------------------------

		virtual ~Container() {}

		INT32 GetID() { return m_id; }

		virtual ContainerType GetType() = 0;

		const OpStringC & GetName() { return m_name; }

		const OpWidgetImage& GetWidgetImageIcon(UINT32 pixel_width  = 16,
												UINT32 pixel_height = 16)
			{ return m_widget_image; }

		Image GetImageIcon(UINT32 pixel_width  = 16,
						   UINT32 pixel_height = 16)
			{ return m_image; }

		BOOL IsInitialized() { return m_initialized; }

	protected:

		//  ------------------------
		//  Protected member functions:
		//  -------------------------

		Container()
			{
				m_id = OpTypedObject::GetUniqueID();
				m_initialized = FALSE;
			}

		Container(OpString * item_name,
				  OpBitmap * item_icon)
			{
				m_id = OpTypedObject::GetUniqueID();
				InitContainer(item_name, item_icon);
			}

		Container(OpString * item_name,
				  Image &item_icon)
			{
				m_id = OpTypedObject::GetUniqueID();
				InitContainer(item_name, item_icon);
			}

		Container(const OpStringC & item_name,
				  const OpStringC8 & item_icon)
			{
				m_id = OpTypedObject::GetUniqueID();
				InitContainer(item_name, item_icon);
			}

		void InitContainer(OpString * item_name,
						   OpBitmap * item_icon)
			{
				SetName(item_name);
				SetIcon(item_icon);
				m_initialized = TRUE;
			}

		void InitContainer(OpString * item_name,
						   Image &item_icon)
			{
				SetName(item_name);
				SetIcon(item_icon);
				m_initialized = TRUE;
			}

		void InitContainer(const OpStringC & item_name,
						   const OpStringC8 & item_icon)
			{
				SetName(&item_name);
				m_widget_image.SetImage(item_icon.CStr());
				m_initialized = TRUE;
			}

		void SetName(const OpStringC * item_name){ if(item_name) m_name.Set(item_name->CStr());}

		void SetIcon(OpBitmap * item_icon)
			{
				//Make the icon from the bitmap :
				if(item_icon)
				{
					m_image = imgManager->GetImage(item_icon);
				}

				m_widget_image.SetBitmapImage( m_image, FALSE );
			}

		void SetIcon(Image &item_icon)
			{
				m_image = item_icon;

				m_widget_image.SetBitmapImage( m_image, FALSE );
			}

		void EmptyContainer()
			{
				m_initialized = FALSE;
				m_name.Empty();
				m_widget_image.Empty();
				m_image.Empty();
			}

		//  -------------------------
		//  Protected member variables:
		//  -------------------------

		OpString      m_name;
		OpWidgetImage m_widget_image;
		Image         m_image;
		BOOL          m_initialized;
		INT32	      m_id;
	};

public:

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	   Get method for singleton class DownloadManager
	   @return pointer to the instance of class DownloadManager
	*/
	static DownloadManager* GetManager();

	static void DestroyManager();

	/**
	   @param
	   @param
	   @return
	*/
	BOOL HasSystemRegisteredHandler( const Viewer& viewer,
									 const OpStringC& mime_type );

	/**
	   @param
	   @return
	*/
	BOOL HasApplicationHandler( const Viewer& viewer );

	/**
	   @param
	   @return
	*/
	BOOL HasPluginHandler( Viewer& viewer );

	/**
	   @param
	   @param
	   @return
	*/
    OP_STATUS GetSuggestedExtension(URL & url, OpString & extension);

	/**
	   @param
	   @param
	   @return
	*/
	OP_STATUS GetHostName(URL & url, OpString & host_name);

	/**
	   @param
	   @param
	   @return
	*/
    OP_STATUS GetFilePathName(URL & url, OpString & file_path);

	/**
	   @param
	   @param
	   @return
	*/
    OP_STATUS GetServerName(URL & url, ServerName *& servername);


	/**
	   @param
	   @param
	   @param
	   @return
	*/
	OP_STATUS GetSuggestedFilename(URL & url,
								   OpString & file_name,
								   const OpStringC & extension);

	/**
	   @param
	   @param
	   @return
	*/
	void MakeDefaultFilename(OpString & filename,
							 const OpStringC & extension);

	/**
	   @param
	   @param
	   @return
	*/
	OP_STATUS MakeSaveTemporaryFilename(const OpStringC & source_file_name,
										OpString & file_name);

	/**
	   @param
	   @param
	   @return
	*/
	OP_STATUS MakeSavePermanentFilename(const OpStringC & source_file_name,
										OpString & file_name);

	/**
	   @param
	   @return
	*/
	BOOL ExtensionExists(const OpStringC & extension);

	/**
	   @return
	*/
	BOOL AllowExecuteDownloadedContent();

private:

//  ------------------------
//  Private member functions:
//  -------------------------

	~DownloadManager(){}
	DownloadManager(){}

//  -------------------------
//  Private member variables:
//  -------------------------

	static DownloadManager* m_manager;
};

#endif //__DOWNLOAD_MANAGER_H__
