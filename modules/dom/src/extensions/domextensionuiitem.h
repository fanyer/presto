/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_UIITEM_H
#define DOM_EXTENSIONS_UIITEM_H

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/extensions/domextensioncontext.h"
#include "modules/dom/src/domevents/domeventtarget.h"

class DOM_ExtensionImageLoader;
class DOM_ExtensionUIPopup;
class DOM_ExtensionUIBadge;

#define DEFAULT_POPUP_WIDTH 0
#define DEFAULT_POPUP_HEIGHT 0
#define DEFAULT_POPUP_HREF NULL
#define DEFAULT_POPUP_WINDOW 0

/* Opera red #fd0000 is the default background. */
#define DEFAULT_BADGE_BACKGROUND_COLOR_VALUE OP_RGB(0xfd, 0x00, 0x00)
#define DEFAULT_BADGE_BACKGROUND_COLOR_ALPHA 0xff
#define DEFAULT_BADGE_BACKGROUND_COLOR_RED 0xfd
#define DEFAULT_BADGE_BACKGROUND_COLOR_GREEN 0x00
#define DEFAULT_BADGE_BACKGROUND_COLOR_BLUE 0x00

#define DEFAULT_BADGE_COLOR_VALUE OP_RGB(0xff, 0xff, 0xff)
#define DEFAULT_BADGE_COLOR_ALPHA 0xff
#define DEFAULT_BADGE_COLOR_RED 0xff
#define DEFAULT_BADGE_COLOR_GREEN 0xff
#define DEFAULT_BADGE_COLOR_BLUE 0xff

#define DEFAULT_BADGE_DISPLAYED TRUE
#define DEFAULT_BADGE_TEXTCONTENT NULL

#define MAX_FAVICON_HEIGHT 512
#define MAX_FAVICON_WIDTH 512

/** This class represents ui(chrome) element created by extension as visible
 *  in background process javascript thread.
 */
class DOM_ExtensionUIItem
	: public DOM_ExtensionContext
	, public DOM_EventTargetOwner
	, public ListElement<DOM_ExtensionUIItem>
{
public:
	static OP_STATUS Make(DOM_ExtensionUIItem *&new_obj, DOM_ExtensionSupport *opera, DOM_Runtime *origining_runtime);

	virtual ~DOM_ExtensionUIItem();

	/* from DOM_Object */
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object *restart_object);
	virtual ES_PutState PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_UIITEM || DOM_ExtensionContext::IsA(type); }

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	OP_STATUS NotifyItemUpdate(ES_Runtime *origining_runtime, DOM_ExtensionContext *context = NULL);
	OP_STATUS NotifyItemRemove(BOOL want_notifications = TRUE);

	/* from DOM_EventTargetOwner */
	virtual DOM_Object *GetOwnerObject() { return this; }

	BOOL GetDisabled() { return m_disabled; }
	const uni_char *GetTitle() { return m_title; }
	const uni_char *GetFavicon() { return m_favicon; }

	static int CopyIconImage(DOM_Object *this_object, DOM_ExtensionUIItem *item, ES_Value *return_value);
	static int LoadImage(OpGadget *extension_gadget, DOM_ExtensionUIItem *item, BOOL notify_update, ES_Value *return_value, DOM_Runtime *origining_runtime);

	DOM_DECLARE_FUNCTION(update);
	enum
	{
		FUNCTIONS_update = 1,
		FUNCTIONS_ARRAY_SIZE
	};

	enum
	{
		FUNCTIONS_WITH_DATA_addEventListener = 1,
		FUNCTIONS_WITH_DATA_removeEventListener,
#ifdef DOM3_EVENTS
		FUNCTIONS_WITH_DATA_addEventListenerNS,
		FUNCTIONS_WITH_DATA_removeEventListenerNS,
		FUNCTIONS_WITH_DATA_willTriggerNS,
		FUNCTIONS_WITH_DATA_hasEventListenerNS,
#endif
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

	// Helpers
	static int HandleItemRemovedStatus(DOM_Object *this_object, OpExtensionUIListener::ItemRemovedStatus call_status, ES_Value *return_value);
	static int HandleItemAddedStatus(DOM_Object *this_object, OpExtensionUIListener::ItemAddedStatus call_status, ES_Value *return_value);
	static OP_STATUS ReportImageLoadFailure(const uni_char *resource, unsigned int id, ES_Value *return_value, DOM_Runtime *origining_runtime);

private:
	friend class DOM_ExtensionSupport;
	friend class DOM_ExtensionContext;
	friend class DOM_ExtensionContext::ExtensionUIListenerCallbacks;

	DOM_ExtensionUIItem(DOM_ExtensionSupport *extension_support)
		: DOM_ExtensionContext(extension_support)
		, m_disabled(FALSE)
		, m_title(NULL)
		, m_favicon(NULL)
		, m_favicon_loader(NULL)
		, m_favicon_bitmap(NULL)
		, m_favicon_in_sync(FALSE)
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		, m_favicon_hires_loader(NULL)
		, m_favicon_hires_bitmap(NULL)
		, m_favicon_hires_in_sync(FALSE)
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		, m_popup(NULL)
		, m_badge(NULL)
		, m_is_loading_image(FALSE)
		, m_onclick_handler(NULL)
		, m_onremove_handler(NULL)
		, m_is_attached(FALSE)
	{
	}

	static int CopyIconImage(DOM_Object *this_object, DOM_ExtensionImageLoader *&loader, OpBitmap *&bitmap, BOOL &in_sync, ES_Value *return_value);

	bool m_disabled;
	const uni_char *m_title;
	const uni_char *m_favicon;

	DOM_ExtensionImageLoader *m_favicon_loader;
	/**< Non-NULL the attached inline loader for the favicon images. */

	OpBitmap *m_favicon_bitmap;
	BOOL m_favicon_in_sync;
	/**< TRUE if last update to the favicon has been transmitted to the platform;
	     the implementation will not repeatedly set it when/if other properties
	     of the UIItem are updated. */

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	DOM_ExtensionImageLoader *m_favicon_hires_loader;
	OpBitmap *m_favicon_hires_bitmap;
	BOOL m_favicon_hires_in_sync;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	DOM_ExtensionUIPopup *m_popup;

	DOM_ExtensionUIBadge *m_badge;

	BOOL m_is_loading_image;

	/* .onXXX handlers for UIItems */

	DOM_EventListener *m_onclick_handler;
	DOM_EventListener *m_onremove_handler;

	BOOL m_is_attached;
	/**< TRUE if this UIItem is attached to a context; updates and removals/deletion should not be transmitted
	     to the UI. */
};

#define UniSetConstStr(str, val) UniSetStr(const_cast<uni_char*&>(str), val)

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_UIITEM_H
