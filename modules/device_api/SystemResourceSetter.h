/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DEVICE_API_SYSTEM_RESOURCE_SETTER_H
#define DEVICE_API_SYSTEM_RESOURCE_SETTER_H

#ifdef DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT

#include "modules/doc/frm_doc.h"

typedef int OP_SYSTEM_RESOURCE_SETTER_STATUS;
/** Utility class for asynchronous setting system resources from urls.
 *  It hanles loading a resource(as it was loading an inline
 *  element and calling apropriate platform functionality)
 */
class SystemResourceSetter
{
public:
	/// Additional error codes for set system resource.
	/// Description in SetSystemResourceCallback::OnFinished.
	class Status : public OpStatus
	{
	public:
		enum
		{
			ERR_RESOURCE_LOADING_FAILED = USER_ERROR,
			ERR_RESOURCE_FORMAT_UNSUPPORTED,
			ERR_RESOURCE_SAVE_OPERATION_FAILED,

		};
	};
	/** Callback object called to notify about finishing setting system
	 *  resource.
	 */
	class SetSystemResourceCallback
	{
	public:
		/** Called when system resource setting is finished or fails
		 *
		 * @param status
		 *  - OK if succeeded apropriate error code if failed.
		 *  - ERR_RESOURCE_LOADING_FAILED - Loading of the resource has failed.
		 *  - ERR_RESOURCE_FORMAT_UNSUPPORTED - Resource format is unsupported.
		 *  - ERR_NO_SUCH_RESOURCE or ERR_FILE_NOT_FOUND - Couldn't to the resource (resource referenced by uri is not available).
		 *  - ERR_NO_ACCESS - No access to the resource(due to security configuration).
		 *  - ERR_NO_DISK - Not enough disk space to complete operation.
		 *  - ERR_RESOURCE_SAVE_OPERATION_FAILED - Other unspecified error during SaveTo file.
		 *  - ERR_NO_MEMORY - OOM.
		 *  - ERR_NOT_SUPPORTED - the operation is not supported.
		 *  - ERR - any other error.
		 */
		virtual void OnFinished(OP_SYSTEM_RESOURCE_SETTER_STATUS status) = 0;
	};
	enum ResourceType
	{
		DESKTOP_IMAGE,
		DEFAULT_RINGTONE,
		CONTACT_RINGTONE
	};

	/** Asynchronously sets specified system resource to resource referenced by an url
	 *
	 * @param url_str - url referenceing the resource.
	 * @param resource_type - type of the resource to set
	 * @param resource_id - optional id of the resource (for example id of the contact for which we can set a ringtone).
	 *                Only valid with some resource types(currently CONTACT_RINGTONE)
	 * @param callback - callback called when the setting is over or fails.
	 * @param document - document in which context the setting is done.
	 */
	void SetSystemResource(const uni_char* url_str, ResourceType resource_type, const uni_char* resource_id, SetSystemResourceCallback* callback, FramesDocument* document);



	/// All the classes below should be private but BREW compiler doesn't handle it well
	class SetSystemResourceHandler;

	class LoadListenerImpl: public ExternalInlineListener
	{
	public:
		LoadListenerImpl(SetSystemResourceHandler& handler) : m_handler(handler) {}
		virtual void LoadingStopped(FramesDocument *document, const URL &url);
	private:
		SetSystemResourceHandler& m_handler;
	};

	class QueryDesktopBackgroundImageFormatCallbackImpl: public OpOSInteractionListener::QueryDesktopBackgroundImageFormatCallback
	{
	public:
		QueryDesktopBackgroundImageFormatCallbackImpl(SetSystemResourceHandler& handler) : m_handler(handler) {}
		virtual void OnFinished(OP_STATUS error, URLContentType content_type, const uni_char* dir_path);
	private:
		SetSystemResourceHandler& m_handler;

	};

	class SetDesktopBackgroundImageCallbackImpl : public OpOSInteractionListener::SetDesktopBackgroundImageCallback
	{
	public:
		SetDesktopBackgroundImageCallbackImpl(SetSystemResourceHandler& handler) : m_handler(handler) {}
		virtual void OnFinished(OP_STATUS error);
	private:
		SetSystemResourceHandler& m_handler;
	};

	class SetRingtoneCallbackImpl : public OpSystemInfo::SetRingtoneCallback
	{
	public:
		SetRingtoneCallbackImpl(SetSystemResourceHandler& handler) : m_handler(handler) {}
		virtual void OnFinished(OP_STATUS error, BOOL file_was_moved);
	private:
		SetSystemResourceHandler& m_handler;
	};

	class SetSystemResourceHandler
	{
	public:
		static OP_STATUS Make(SetSystemResourceHandler*& new_obj, const uni_char* url_str, ResourceType resource_type, const uni_char* resource_id, SetSystemResourceCallback* callback, FramesDocument* document);
		void Start();
		virtual ~SetSystemResourceHandler();
		OP_STATUS DumpBitmapToFile(OpFileDescriptor& file_descriptor);
		OP_STATUS DumpRawContentToFile(OpFileDescriptor& file_descriptor);
	private:
		SetSystemResourceHandler();
		void Finish(OP_STATUS status);
		void OnResourceLoaded();
		URL m_url;
		ResourceType m_resource_type;
		OpString m_resource_id;
		SetSystemResourceCallback* m_callback;
		FramesDocument* m_document;

		LoadListenerImpl m_load_listener;
		QueryDesktopBackgroundImageFormatCallbackImpl m_query_format_callback;
		SetDesktopBackgroundImageCallbackImpl m_set_desktop_image_callback;
		SetRingtoneCallbackImpl m_set_ringtone_callback;
		friend class LoadListenerImpl;
		friend class QueryDesktopBackgroundImageFormatCallbackImpl;
		friend class SetDesktopBackgroundImageCallbackImpl;
		friend class SetRingtoneCallbackImpl;
	};
	friend class SetSystemResourceHandler;

	static OP_STATUS AreFilesEqual(OpFileDescriptor& fd1, OpFileDescriptor& fd2, BOOL& equal);
};

#endif // SYSTEM_RESOURCE_SETTER_SUPPORT

#endif // DEVICE_API_JIL_SYSTEM_RESOURCE_SETTER_H
