/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT
#include "modules/dom/src/extensions/domextensionuiitem.h"
#include "modules/dom/src/extensions/domextensionuipopup.h"
#include "modules/dom/src/extensions/domextensionuibadge.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"
#include "modules/dochand/winman.h"
#include "modules/img/src/imagerep.h"
#include "modules/doc/frm_doc.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/dom/domutils.h"

/* static */ int
DOM_ExtensionUIItem::HandleItemAddedStatus(DOM_Object *this_object, OpExtensionUIListener::ItemAddedStatus call_status, ES_Value *return_value)
{
	switch (call_status)
	{
	case OpExtensionUIListener::ItemAddedUnknown:
	case OpExtensionUIListener::ItemAddedSuccess:
		return ES_FAILED;

	case OpExtensionUIListener::ItemAddedResourceExceeded:
		return DOM_CALL_DOMEXCEPTION(QUOTA_EXCEEDED_ERR);

	case OpExtensionUIListener::ItemAddedFailed:
	case OpExtensionUIListener::ItemAddedContextNotSupported:
	case OpExtensionUIListener::ItemAddedNotSupported:
	default:
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	}
}

/* static */ int
DOM_ExtensionUIItem::HandleItemRemovedStatus(DOM_Object *this_object, OpExtensionUIListener::ItemRemovedStatus call_status, ES_Value *return_value)
{
	switch (call_status)
	{
	case OpExtensionUIListener::ItemRemovedUnknown:
	case OpExtensionUIListener::ItemRemovedSuccess:
		return ES_FAILED;

	case OpExtensionUIListener::ItemRemovedFailed:
	default:
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	}
}

/* static */ OP_STATUS
DOM_ExtensionUIItem::ReportImageLoadFailure(const uni_char *resource, unsigned int id, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	/* Ignore the unbound/invalid favicon, but post error. */
	OpConsoleEngine::Message console_message(OpConsoleEngine::EcmaScript, OpConsoleEngine::Error, id);
	OpString &message = console_message.message;

	RETURN_IF_ERROR(message.Append("Failed to load icon: "));
	RETURN_IF_ERROR(message.Append(resource));

	ES_Value message_value;
	if (return_value->type == VALUE_OBJECT)
	{
		ES_Object *error_object = return_value->value.object;
		OP_BOOLEAN result;
		RETURN_IF_ERROR(result = origining_runtime->GetName(error_object, UNI_L("message"), &message_value));
		if (result == OpBoolean::IS_TRUE && message_value.type == VALUE_STRING)
		{
			RETURN_IF_ERROR(message.Append(", reason: "));
			RETURN_IF_ERROR(message.Append(message_value.value.string));
		}
	}

	RETURN_IF_ERROR(origining_runtime->GetDisplayURL(console_message.url));

	TRAPD(status, g_console->PostMessageL(&console_message));
	return status;
}

/* static */ OP_STATUS
DOM_ExtensionUIItem::Make(DOM_ExtensionUIItem *&new_obj, DOM_ExtensionSupport *extension_support, DOM_Runtime *origining_runtime)
{
	return DOMSetObjectRuntime(new_obj = OP_NEW(DOM_ExtensionUIItem, (extension_support)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_UIITEM_PROTOTYPE), "UIItem");
}

/* virtual */
DOM_ExtensionUIItem::~DOM_ExtensionUIItem()
{
	OP_NEW_DBG("DOM_ExtensionUIItem::~DOM_ExtensionUIItem()", "extensions.dom");
	OP_DBG(("this: %p title: %s is_attached: %d", this, m_title, m_is_attached));

	if (m_is_attached)
		OpStatus::Ignore(NotifyItemRemove(FALSE));

	OP_DELETEA(m_title);
	OP_DELETEA(m_favicon);
	OP_DELETE(m_favicon_bitmap);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	OP_DELETE(m_favicon_hires_bitmap);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	Out();
}

class DOM_ExtensionImageLoader
	: public ExternalInlineListener,
	  public DOM_Object,
	  public ES_ThreadListener
{
public:
	static OP_STATUS Make(DOM_ExtensionImageLoader *&loader, URL url, ES_Thread *thread, DOM_Runtime *runtime)
	{
		return DOMSetObjectRuntime((loader = OP_NEW(DOM_ExtensionImageLoader, (url, thread))), runtime);
	}

	/* From ExternalInlineListener. */
	void LoadingProgress(const URL &url)
	{
	}

	void LoadingStopped(const URL &url)
	{
		GetFramesDocument()->StopLoadingInline(img_url, this);
		Finish();
	}

	void LoadingRedirected(const URL &from, const URL &to)
	{
	}

	/* From ES_ThreadListener. */
	OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		switch (signal)
		{
		case ES_SIGNAL_CANCELLED:
		case ES_SIGNAL_FINISHED:
		case ES_SIGNAL_FAILED:
			GetFramesDocument()->StopLoadingInline(img_url, this);
			break;
		default:
			break;
		}

		dependent_loader = NULL;
		has_blocked_thread = FALSE;
		ES_ThreadListener::Remove();
		return OpStatus::OK;
	}

	void ThreadNowBlocked()
	{
		thread->AddListener(this);
		if (!dependent_loader || !dependent_loader->has_blocked_thread)
		{
			thread->Block();
			has_blocked_thread = TRUE;
		}
	}

	void Finish()
	{
		/* If there's another loader, delegate unblocking to it.
		   Otherwise, unblock this thread if needed. */
		if (dependent_loader)
			dependent_loader->Detach(has_blocked_thread);
		else if (has_blocked_thread)
			OpStatus::Ignore(thread->Unblock());

		dependent_loader = NULL;
		has_blocked_thread = FALSE;
		ES_ThreadListener::Remove();
	}

	void Cancel()
	{
		GetFramesDocument()->StopLoadingInline(img_url, this);
		if (has_blocked_thread)
			thread->Unblock();

		if (dependent_loader)
		{
			dependent_loader->SetDependentLoader(NULL);
			dependent_loader = NULL;
		}

		ES_ThreadListener::Remove();
	}

	DOM_ExtensionImageLoader *GetDependentLoader()
	{
		return dependent_loader;
	}

	void SetDependentLoader(DOM_ExtensionImageLoader *loader)
	{
		if (loader != dependent_loader)
		{
			dependent_loader = loader;
			if (loader)
				loader->SetDependentLoader(this);
		}
	}

	void Detach(BOOL unblock_on_stop)
	{
		dependent_loader = NULL;
		has_blocked_thread = has_blocked_thread || unblock_on_stop;
	}

	URL GetImageURL()
	{
		return img_url;
	}

	virtual void GCTrace()
	{
		GCMark(dependent_loader);
	}

private:
	DOM_ExtensionImageLoader(URL img_url, ES_Thread *thread)
		: img_url(img_url),
		  thread(thread),
		  has_blocked_thread(FALSE),
		  dependent_loader(NULL)
	{
	}

	URL img_url;
	ES_Thread *thread;
	BOOL has_blocked_thread;

	DOM_ExtensionImageLoader *dependent_loader;
};

/* static */ int
DOM_ExtensionUIItem::LoadImage(OpGadget *extension_gadget, DOM_ExtensionUIItem *item, BOOL notify_update, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	OpString extension_url_str;

	CALL_FAILED_IF_ERROR(extension_gadget->GetGadgetUrl(extension_url_str, FALSE, TRUE));
	URL extension_url = g_url_api->GetURL(extension_url_str.CStr(), extension_gadget->UrlContextId());

	URL favicon_url = g_url_api->GetURL(extension_url, item->m_favicon);
	if (favicon_url.Type() == URL_NULL_TYPE || favicon_url.Type() == URL_UNKNOWN || DOM_Utils::IsOperaIllegalURL(favicon_url))
		return item->CallDOMException(SYNTAX_ERR, return_value);

	if (FramesDocument *frames_doc = extension_gadget->GetWindow() ? extension_gadget->GetWindow()->GetCurrentDoc() : NULL)
	{
		/* Load the favicon URL in the context of the gadget/extension. */
		ES_Thread *thread = GetCurrentThread(origining_runtime);
		DOM_Runtime *runtime = static_cast<DOM_Runtime*>(frames_doc->GetDOMEnvironment()->GetRuntime());

		DOM_ExtensionImageLoader *img_loader;
		CALL_FAILED_IF_ERROR(DOM_ExtensionImageLoader::Make(img_loader, favicon_url, thread, runtime));

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		OpStringC fav_ic_wrapper(item->m_favicon);
		OpString  favicon_hires;
		int last_dot_pos = fav_ic_wrapper.FindLastOf(UNI_L('.'));
		if (last_dot_pos != KNotFound)
		{
			RETURN_IF_ERROR(favicon_hires.Append(item->m_favicon, last_dot_pos));
			RETURN_IF_ERROR(favicon_hires.Append(UNI_L("@2x")));
			RETURN_IF_ERROR(favicon_hires.Append(item->m_favicon+last_dot_pos));

			URL favicon_hires_url = g_url_api->GetURL(extension_url, favicon_hires);
			if (favicon_hires_url.Type() == URL_NULL_TYPE || favicon_hires_url.Type() == URL_UNKNOWN || DOM_Utils::IsOperaIllegalURL(favicon_hires_url))
				return item->CallDOMException(SYNTAX_ERR, return_value);

			DOM_ExtensionImageLoader* img_hires_loader;
			CALL_FAILED_IF_ERROR(DOM_ExtensionImageLoader::Make(img_hires_loader, favicon_hires_url, thread, runtime));

			OP_LOAD_INLINE_STATUS hires_status = frames_doc->LoadInline(favicon_hires_url, img_hires_loader);
			if (OpStatus::IsMemoryError(hires_status))
				return ES_NO_MEMORY;
			else if (hires_status == LoadInlineStatus::USE_LOADED)
			{
				item->m_favicon_hires_loader = img_hires_loader;
				int result = CopyIconImage(item, item, return_value);
				if (result == ES_NO_MEMORY)
					return result;
			}
			else if (hires_status == LoadInlineStatus::LOADING_STARTED)
			{
				img_hires_loader->ThreadNowBlocked();
				img_hires_loader->SetDependentLoader(img_loader);
				item->m_favicon_hires_loader = img_hires_loader;
			}
			// If the inline load reported a failure, that's acceptable.
			// The hires resource loading is optional and might well not be available.
			// Hence, fall through and continue loading the 'normal' favicon resource.
		}

#endif // PIXEL_SCALE_RENDERING_SUPPORT

		OP_LOAD_INLINE_STATUS status = frames_doc->LoadInline(favicon_url, img_loader);
		if (OpStatus::IsMemoryError(status))
			return ES_NO_MEMORY;
		else if (status == LoadInlineStatus::USE_LOADED)
		{
			/* We will use this icon, cancel any dependent inline loads. */
			if (DOM_ExtensionImageLoader *dependent_loader = img_loader->GetDependentLoader())
			{
				dependent_loader->Cancel();
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				item->m_favicon_hires_loader = NULL;
#endif // PIXEL_SCALE_RENDERING_SUPPORT
			}
			int result = CopyIconImage(item, item, return_value);
			if (result == ES_VALUE && notify_update)
				CALL_FAILED_IF_ERROR(item->NotifyItemUpdate(origining_runtime, item));

			return result;
		}
		else if (status == LoadInlineStatus::LOADING_CANCELLED || status == LoadInlineStatus::LOADING_REFUSED)
			return item->CallDOMException(NOT_SUPPORTED_ERR, return_value);
		else if (status == LoadInlineStatus::LOADING_STARTED)
		{
			img_loader->ThreadNowBlocked();
			item->m_favicon_loader = img_loader;

			DOMSetObject(return_value, item);
			return (ES_RESTART | ES_SUSPEND);
		}
		else
		{
			CALL_FAILED_IF_ERROR(status);
			DOMSetNull(return_value);
			return ES_FAILED;
		}
	}
	else
	{
		DOMSetNull(return_value);
		return ES_FAILED;
	}
}


/* Q: Acceptable to reuse AnimatedImageContent static method from within dom/ ? */
static OP_STATUS
CopyBitmap(OpBitmap *&dest, OpBitmap *source)
{
	RETURN_IF_ERROR(OpBitmap::Create(&dest, source->Width(), source->Height()));
	return AnimatedImageContent::CopyBitmap(dest, source);
}

OP_STATUS
DOM_ExtensionUIItem::NotifyItemUpdate(ES_Runtime *origining_runtime, DOM_ExtensionContext *context)
{
	if (!m_is_attached)
		return OpStatus::OK;

	OP_ASSERT(m_parent_id > 0); // don't have the power to change the UIItem attributes of the toplevel platform UI containers.
	OP_ASSERT(context);

	if (FramesDocument *frames_doc = GetEnvironment()->GetFramesDocument())
	{
		ES_Thread *blocking_thread = origining_runtime->GetESScheduler()->GetCurrentThread();

		if (blocking_thread)
		{
			/* Record the parent connection for the duration of an async call. */
			if (this != context)
				m_parent = context;
			blocking_thread->AddListener(context->GetThreadListener());
			blocking_thread->Block();
		}

		context->SetBlockedThread(blocking_thread, m_id);

		OpExtensionUIListener::ExtensionUIItem ui_item;
		OpExtensionUIListener::ExtensionBadge badge;
		OpExtensionUIListener::ExtensionPopup popup;

		popup.href = m_popup ? m_popup->m_href : DEFAULT_POPUP_HREF;
		popup.width = m_popup ? m_popup->m_width : DEFAULT_POPUP_WIDTH;
		popup.height = m_popup ? m_popup->m_height : DEFAULT_POPUP_HEIGHT;
		if (VisualDevice *vd = frames_doc->GetVisualDevice())
		{
			/* Max 75% for both dimensions */
			int max_width = (3 * vd->GetScreenWidth()) / 4;
			int max_height = (3	 * vd->GetScreenHeight()) / 4;

			if (max_height > 0 && static_cast<int>(popup.height) > max_height)
				popup.height = static_cast<unsigned int>(max_height);
			if (max_width > 0 && static_cast<int>(popup.width) > max_width)
				popup.width = static_cast<unsigned int>(max_width);
		}

		popup.active_window_id = m_popup ? m_popup->m_window_id : DEFAULT_POPUP_WINDOW;

		if (m_badge && m_badge->m_background_color)
		{
			badge.background_color.alpha = OP_GET_A_VALUE(m_badge->m_background_color);
			badge.background_color.red = OP_GET_R_VALUE(m_badge->m_background_color);
			badge.background_color.green = OP_GET_G_VALUE(m_badge->m_background_color);
			badge.background_color.blue = OP_GET_B_VALUE(m_badge->m_background_color);
		}
		else
		{
			badge.background_color.alpha = DEFAULT_BADGE_BACKGROUND_COLOR_ALPHA;
			badge.background_color.red = DEFAULT_BADGE_BACKGROUND_COLOR_RED;
			badge.background_color.green = DEFAULT_BADGE_BACKGROUND_COLOR_GREEN;
			badge.background_color.blue = DEFAULT_BADGE_BACKGROUND_COLOR_BLUE;
		}

		if (m_badge && m_badge->m_color)
		{
			badge.color.alpha = OP_GET_A_VALUE(m_badge->m_color);
			badge.color.red = OP_GET_R_VALUE(m_badge->m_color);
			badge.color.green = OP_GET_G_VALUE(m_badge->m_color);
			badge.color.blue = OP_GET_B_VALUE(m_badge->m_color);
		}
		else
		{
			badge.color.alpha = DEFAULT_BADGE_COLOR_ALPHA;
			badge.color.red = DEFAULT_BADGE_COLOR_RED;
			badge.color.green = DEFAULT_BADGE_COLOR_GREEN;
			badge.color.blue = DEFAULT_BADGE_COLOR_BLUE;
		}

		badge.displayed = m_badge && m_badge->m_displayed;
		badge.text_content = m_badge ? m_badge->m_text_content : DEFAULT_BADGE_TEXTCONTENT;

		ui_item.id = m_id;
		ui_item.parent_id = m_parent_id;
		ui_item.owner_window = frames_doc->GetWindow()->GetOpWindow();

		/* Put the UIItem in the 'global' toolbar, but if the UIItem has a tab/page-level parent, adjust it here. */
		BOOL page_local_uiitem = FALSE;
		if (page_local_uiitem)
			ui_item.target_window = frames_doc->GetWindow()->Id();
		else
			ui_item.target_window = 0;
		ui_item.title = m_title;
		ui_item.disabled = m_disabled;
		if (!m_favicon_in_sync && m_favicon_bitmap)
		{
			/* Keep a copy of the favicon bitmap in case it has to be refreshed/retransmitted (but not re-fetched)
			   if user removes the UIItem followed by a re-add. */
			OP_STATUS status = CopyBitmap(ui_item.favicon, m_favicon_bitmap);
			if (OpStatus::IsError(status))
			{
				m_parent = NULL;
				OpStatus::Ignore(context->GetBlockedThread()->Unblock());
				context->GetThreadListener()->Remove();
				context->SetBlockedThread(NULL);
				return status;
			}
			m_favicon_in_sync = TRUE;
		}
		else
			ui_item.favicon = NULL;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		if (!m_favicon_hires_in_sync && m_favicon_hires_bitmap)
		{
			OP_STATUS status = CopyBitmap(ui_item.favicon_hires, m_favicon_hires_bitmap);
			if (OpStatus::IsError(status))
			{
				m_parent = NULL;
				OpStatus::Ignore(context->GetBlockedThread()->Unblock());
				context->GetThreadListener()->Remove();
				context->SetBlockedThread(NULL);
				return status;
			}
			m_favicon_hires_in_sync = TRUE;
		} else
			ui_item.favicon_hires = NULL;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		ui_item.popup = &popup;
		ui_item.badge = &badge;
		ui_item.callbacks = &m_notifications;

		WindowCommander *wc = frames_doc->GetWindow()->GetWindowCommander();
		wc->GetExtensionUIListener()->OnExtensionUIItemAddRequest(wc, &ui_item);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
DOM_ExtensionUIItem::NotifyItemRemove(BOOL want_notifications /*=TRUE*/)
{
	if (!m_is_attached)
		return OpStatus::OK;

	OP_ASSERT(m_parent_id > 0); /* Don't have the power to change the UIItem attributes of the toplevel platform UI containers. */

	if (!want_notifications)
		m_is_attached = FALSE;

	if (FramesDocument *frames_doc = GetEnvironment()->GetFramesDocument())
	{
		OpExtensionUIListener::ExtensionUIItem ui_item;
		ui_item.id = m_id;
		ui_item.parent_id = m_parent_id;
		ui_item.target_window = 0;
		ui_item.owner_window = frames_doc ? frames_doc->GetWindow()->GetOpWindow() : NULL;

		/* Upon shutdown, fire-and-forget the remove request; no notifications. */
		if (want_notifications)
			ui_item.callbacks = &m_notifications;
		else
			ui_item.callbacks = NULL;

		m_favicon_in_sync = FALSE;

		WindowCommander *wc = frames_doc->GetWindow()->GetWindowCommander();
		wc->GetExtensionUIListener()->OnExtensionUIItemRemoveRequest(wc, &ui_item);
		return OpStatus::OK;
	}
	else
		return OpStatus::OK;
}

/* static */ int
DOM_ExtensionUIItem::CopyIconImage(DOM_Object *this_object, DOM_ExtensionUIItem *item, ES_Value *return_value)
{
	int result = ES_VALUE;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	// Hires icons do not have to be available nor load correctly.
	if (item->m_favicon_hires_loader)
	{
		result = CopyIconImage(this_object, item->m_favicon_hires_loader, item->m_favicon_hires_bitmap, item->m_favicon_hires_in_sync, return_value);
		if (result == ES_NO_MEMORY)
			return result;
	}
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	if (item->m_favicon_loader)
	{
		result = CopyIconImage(this_object, item->m_favicon_loader, item->m_favicon_bitmap, item->m_favicon_in_sync, return_value);
		if (result != ES_VALUE)
			return result;
	}

	DOMSetObject(return_value, item);
	return result;
}

/* static */
int DOM_ExtensionUIItem::CopyIconImage(DOM_Object *this_object, DOM_ExtensionImageLoader *&loader, OpBitmap *&bitmap, BOOL &in_sync, ES_Value *return_value)
{
	URL favicon_url = loader->GetImageURL();

	/* Load in the image, copying its contents. */
	loader = NULL;
	UrlImageContentProvider	*provider = UrlImageContentProvider::FindImageContentProvider(favicon_url);
	if (provider == NULL && (provider = OP_NEW(UrlImageContentProvider, (favicon_url))) == NULL)
		return ES_NO_MEMORY;

	Image img = UrlImageContentProvider::GetImageFromUrl(favicon_url);
	img.IncVisible(null_image_listener);
	OP_ASSERT(!img.IsFailed());
	if (provider)
		img.OnLoadAll(provider);

	if (img.IsFailed() || img.Height() == 0 || img.Width() == 0)
	{
		img.DecVisible(null_image_listener);
		return DOM_CALL_DOMEXCEPTION(NOT_FOUND_ERR);
	}

	if (bitmap)
	{
		OP_DELETE(bitmap);
		bitmap = NULL;
	}

	OpBitmap *img_bitmap = img.GetBitmap(null_image_listener);

	if (!img_bitmap)
		return ES_NO_MEMORY;

	OpAutoPtr<OpBitmap> icon(img_bitmap);

	if (img_bitmap->Width() > MAX_FAVICON_WIDTH || img_bitmap->Height() > MAX_FAVICON_HEIGHT)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	CALL_FAILED_IF_ERROR(CopyBitmap(bitmap, img_bitmap));
	in_sync = FALSE;

	img.DecVisible(null_image_listener);
	icon.release();
	img.ReleaseBitmap();

	return ES_VALUE;
}

/* static */ int
DOM_ExtensionUIItem::update(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(item, DOM_TYPE_EXTENSION_UIITEM, DOM_ExtensionUIItem);
	ES_Value value;

	if (argc < 0)
	{
		if (item->m_is_loading_image)
		{
			item->m_is_loading_image = FALSE;
			int result = DOM_ExtensionUIItem::CopyIconImage(this_object, item, return_value);
			if (result != ES_VALUE)
				return result;

			CALL_INVALID_IF_ERROR(item->NotifyItemUpdate(origining_runtime, item));
			return (ES_SUSPEND | ES_RESTART);
		}

		item->m_blocked_thread = NULL;
		OpExtensionUIListener::ItemAddedStatus call_status = item->m_add_status;
		item->m_add_status = OpExtensionUIListener::ItemAddedUnknown;

		return HandleItemAddedStatus(this_object, call_status, return_value);
	}

	DOM_CHECK_ARGUMENTS("o");

	ES_Object *properties = argv[0].value.object;
	DOM_Runtime *runtime = item->GetRuntime();
	OP_BOOLEAN result;

	BOOL has_changed = FALSE;

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("disabled"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_BOOLEAN && item->m_disabled != value.value.boolean)
	{
		has_changed = TRUE;
		item->m_disabled = value.value.boolean;
	}

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("title"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING && (!item->m_title || !uni_str_eq(item->m_title, value.value.string)))
	{
		has_changed = TRUE;
		if (item->m_title)
			OP_DELETEA(item->m_title);

		RETURN_OOM_IF_NULL(item->m_title = UniSetNewStr(value.value.string));
	}

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("icon"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING && (!item->m_favicon || !uni_str_eq(item->m_favicon, value.value.string)))
	{
		has_changed = TRUE;
		if (item->m_favicon)
			OP_DELETEA(item->m_favicon);

		if ((item->m_favicon = UniSetNewStr(value.value.string)) == NULL)
			return ES_NO_MEMORY;
	}

	CALL_FAILED_IF_ERROR(DOM_ExtensionSupport::AddEventHandler(item, item->m_onclick_handler, properties, NULL, origining_runtime, UNI_L("onclick"), ONCLICK, &has_changed));
	CALL_FAILED_IF_ERROR(DOM_ExtensionSupport::AddEventHandler(item, item->m_onremove_handler, properties, NULL, origining_runtime, UNI_L("onremove"), ONREMOVE, &has_changed));

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("popup"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
	{
		has_changed = TRUE;
		CALL_FAILED_IF_ERROR(DOM_ExtensionUIPopup::Make(item->m_popup, value.value.object, item, return_value, origining_runtime));
	}

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("badge"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
	{
		has_changed = TRUE;
		CALL_FAILED_IF_ERROR(DOM_ExtensionUIBadge::Make(this_object, item->m_badge, value.value.object, item, return_value, origining_runtime));
	}

	if (item->m_favicon)
	{
		int result = DOM_ExtensionUIItem::LoadImage(item->m_extension_support->GetGadget(), item, FALSE, return_value, origining_runtime);
		if (result != ES_EXCEPTION)
		{
			if ((result & ES_SUSPEND) == ES_SUSPEND)
				item->m_is_loading_image = TRUE;
			return result;
		}
		else
		{
			OP_ASSERT(return_value->type == VALUE_OBJECT);
			CALL_FAILED_IF_ERROR(DOM_ExtensionUIItem::ReportImageLoadFailure(item->m_favicon, item->GetEnvironment()->GetFramesDocument()->GetWindow()->Id(), return_value, origining_runtime));
		}
	}

	if (has_changed)
	{
		CALL_INVALID_IF_ERROR(item->NotifyItemUpdate(origining_runtime, item));
		return (ES_SUSPEND | ES_RESTART);
	}
	else
		return ES_FAILED;
}

/* virtual */ ES_GetState
DOM_ExtensionUIItem::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_GetState result = GetEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
	if (result != GET_FAILED)
		return result;

	if (uni_str_eq(property_name, "onclick"))
	{
		DOMSetObject(value, m_onclick_handler);

		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_ExtensionUIItem::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_id:
		DOMSetNumber(value, m_id);
		break;
	case OP_ATOM_disabled:
		DOMSetBoolean(value, m_disabled);
		break;
	case OP_ATOM_title:
		DOMSetString(value, m_title);
		break;
	case OP_ATOM_icon:
		DOMSetString(value, m_favicon);
		break;
	case OP_ATOM_popup:
		DOMSetObject(value, m_popup);
		break;
	case OP_ATOM_badge:
		DOMSetObject(value, m_badge);
		break;
	case OP_ATOM_onremove:
		DOMSetObject(value, m_onremove_handler);
		break;
	default:
		return DOM_ExtensionContext::GetName(property_name, value, origining_runtime);
	}
	return GET_SUCCESS;
}

/* virtual */ ES_PutState
DOM_ExtensionUIItem::PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (uni_str_eq(property_name, "onclick") && value->type == VALUE_OBJECT && op_strcmp(ES_Runtime::GetClass(value->value.object), "Function") == 0)
	{
		PUT_FAILED_IF_ERROR(DOM_ExtensionSupport::AddEventHandler(this, m_onclick_handler, NULL, value, origining_runtime, UNI_L("onclick"), ONCLICK));
		return PUT_SUCCESS;
	}
	else
		return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_ExtensionUIItem::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_id:
		return PUT_READ_ONLY;

	case OP_ATOM_popup:
		if (m_popup)
			return m_popup->PutName(OP_ATOM_href, value, origining_runtime);
		else
			return PUT_SUCCESS;

	case OP_ATOM_badge:
		if (m_badge)
			return m_badge->PutName(OP_ATOM_textContent, value, origining_runtime);
		else
			return PUT_SUCCESS;

	case OP_ATOM_disabled:
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;
		else if (value->value.boolean != m_disabled)
		{
			m_disabled = value->value.boolean;
			PUT_FAILED_IF_ERROR_EXCEPTION(NotifyItemUpdate(origining_runtime, this), INVALID_STATE_ERR);
			DOMSetNull(value);
			return PUT_SUSPEND;
		}
		else
			return PUT_SUCCESS;

	case OP_ATOM_onremove:
		PUT_FAILED_IF_ERROR(DOM_ExtensionSupport::AddEventHandler(this, m_onremove_handler, NULL, value, origining_runtime, UNI_L("onremove"), ONREMOVE));
		return PUT_SUCCESS;

	case OP_ATOM_title:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (!m_title || !uni_str_eq(value->value.string, m_title))
		{
			PUT_FAILED_IF_ERROR(UniSetConstStr(m_title, value->value.string));
			PUT_FAILED_IF_ERROR_EXCEPTION(NotifyItemUpdate(origining_runtime, this), INVALID_STATE_ERR);
			DOMSetNull(value);
			return PUT_SUSPEND;
		}
		else
			return PUT_SUCCESS;

	case OP_ATOM_icon:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (!m_favicon || !uni_str_eq(value->value.string, m_favicon))
		{
			PUT_FAILED_IF_ERROR(UniSetConstStr(m_favicon, value->value.string));
			if (m_favicon)
			{
				int result = DOM_ExtensionUIItem::LoadImage(m_extension_support->GetGadget(), this, TRUE, value, static_cast<DOM_Runtime*>(origining_runtime));
				if ((result & ES_SUSPEND) == ES_SUSPEND)
				{
					DOMSetNull(value);
					m_is_loading_image = TRUE;
				}
				return ConvertCallToPutName(result, value);
			}
		}
		else
			return PUT_SUCCESS;

	default:
		return DOM_ExtensionContext::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ExtensionUIItem::PutNameRestart(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object)
{
	switch (property_name)
	{
	case OP_ATOM_icon:
		if (m_is_loading_image)
		{
			m_is_loading_image = FALSE;
			int result = DOM_ExtensionUIItem::CopyIconImage(this, this, value);
			if (result == ES_VALUE)
			{
				PUT_FAILED_IF_ERROR_EXCEPTION(NotifyItemUpdate(origining_runtime, this), INVALID_STATE_ERR);
				DOMSetNull(value);
				return PUT_SUSPEND;
			}
			else
				return ConvertCallToPutName(result, value);
		}
		/* fall through */
	case OP_ATOM_title:
	case OP_ATOM_disabled:
		{
			m_blocked_thread = NULL;
			OpExtensionUIListener::ItemAddedStatus call_status = m_add_status;
			m_add_status = OpExtensionUIListener::ItemAddedUnknown;

			return ConvertCallToPutName(HandleItemAddedStatus(this, call_status, value), value);
		}
	default:
		return DOM_ExtensionContext::PutNameRestart(property_name, value, origining_runtime, restart_object);
	}
}

/* virtual */ ES_GetState
DOM_ExtensionUIItem::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_ExtensionContext::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	enumerator->AddPropertyL("onclick");

	return GET_SUCCESS;
}

/* virtual */ void
DOM_ExtensionUIItem::GCTrace()
{
	GCMark(FetchEventTarget());
	GCMark(m_popup);
	GCMark(m_badge);
	GCMark(m_favicon_loader);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	GCMark(m_favicon_hires_loader);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	DOM_ExtensionContext::GCTrace();
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_ExtensionUIItem)
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionUIItem, DOM_ExtensionUIItem::update, "update", "o-")
DOM_FUNCTIONS_END(DOM_ExtensionUIItem)

DOM_FUNCTIONS_WITH_DATA_START(DOM_ExtensionUIItem)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionUIItem, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionUIItem, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_ExtensionUIItem)

#endif // EXTENSION_SUPPORT
