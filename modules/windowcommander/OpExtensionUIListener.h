/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OP_EXTENSION_UI_LISTENER_H
#define OP_EXTENSION_UI_LISTENER_H

#include "modules/hardcore/base/opstatus.h"

#ifdef EXTENSION_SUPPORT

// needed for OpWindowCommander::OpColor
#include "modules/windowcommander/OpWindowCommander.h"

/** The Opera Extensions (FEATURE_EXTENSIONS) platform API.
 *
 * Opera extensions are web applications, derivates of Opera Widgets, which
 * may embed UI elements (which are called UIItems) into the main Opera UI to
 * interact with the user. This API extends core's access and control to the
 * platform's UI.
 *
 * The scripting code of an Opera extension may request the platform's UI to
 * display an #ExtensionUIItem via this listener interface
 * (see OnExtensionUIItemAddRequest()). And the platform's UI implementation
 * shall inform the scripting code, which may register various event handlers,
 * via the #ExtensionUIItemNotifications about UI events.
 *
 * The platform wishing to support extensions ability to integrate and tailor
 * its UI must implement the OpExtensionUIListener interface, including the
 * notification interfaces going in the other direction to handle event
 * propagation.
 *
 */
class OpExtensionUIListener
{
public:
	/** Top-level context of an ExtensionUIItem.
	  *
	  * The list of pre-defined toplevel contexts that #ExtensionUIItem
	  * can be added to. Tracks the set defined by the Opera Extensions
	  * UIItem specification; see it for details of what the IDs can be
	  * expected to correspond to in a platform's UI.
	  *
	  * A platform can assume that ExtensionIds with
	  * values < CONTEXT_LAST_BUILTIN will never appear in ExtensionUIItem.id,
	  * but can naturally in ExtensionUIItem.parent_id if a UIItem is being
	  * added as a child to one of these predefined containers.
	  */
	enum ExtensionToplevel
	{
		CONTEXT_NONE = 0,
		CONTEXT_TOOLBAR,
		CONTEXT_PAGE,
		CONTEXT_SELECTION,
		CONTEXT_LINK,
		CONTEXT_EDITABLE,
		CONTEXT_IMAGE,
		CONTEXT_VIDEO,
		CONTEXT_AUDIO,
		CONTEXT_LAST_BUILTIN
	};

	/** The allocation of unique IDs for UI items is the responsibility of
	  * Core code calling out to the platform listener. ExtensionToplevel
	  * enumeration values can be coerced into ExtensionIds and used as
	  * parent_id's in ExtensionUIItems.
	  */
	typedef INT32 ExtensionId;

	/** An ExtensionUIItem may have a popup associated with it,
	  * which is activated upon clicking its button.
	  */
	struct ExtensionPopup
	{
		/** The URL holding the content to display in the popup window. */
		const uni_char *href;

		/** Id of a Window that must be current for the popup to be activated. */
		unsigned long active_window_id;

		/** Requested width of popup window in pixels; if 0, no width
		  * explicitly specified. The platform can assume (but should verify)
		  * that the width doesn't exceed 75% of screen's width.
		  */
		unsigned int width;

		/** Requested height of popup window in pixels; if 0, no height
		  * explicitly specified. The platform can assume (but should verify)
		  * that the height doesn't exceed 75% of screen's height.
		  */
		unsigned int height;

	};

	/** The ExtensionUIItem's badge encodes the textual representation of
	  * the UIItem. All components are updateable.
	  */
	struct ExtensionBadge
	{
		/** The text of the UI badge; platform may assume that Core will cap
		  * length at the maximum imposed by the UIItem specification. If NULL,
		  * the badge does not have a text label.
		  */
		const uni_char* text_content;

		/** The text color of the badge text.
		  */
		OpWindowCommander::OpColor color;

		/** The badge's background color.
		  */
		OpWindowCommander::OpColor background_color;

		/** TRUE => badge now visible; FALSE => hide.
		  */
		BOOL displayed;
	};

	/** The status a platform should report in response to a call
	  * OnExtensionUIItemAdded(). Processing is assumed to be in-order,
	  * hence OnExtensionUIItemAdded()'s caller is responsible for
	  * matching up status codes with the extension UIItems they belong
	  * to (and in what sequence.)
	  */
	enum ItemAddedStatus
	{
		/** Unused empty/unknown status value.
		  */
		ItemAddedUnknown = 0,
		/** Addition or update of corresponding 'id' successful.
		  */
		ItemAddedSuccess,
		/** Operation failed (generic.)
		  */
		ItemAddedFailed,
		/** Platform does not support the addition with
		  * the given ExtensionUIItem's parent.
		  */
		ItemAddedContextNotSupported,
		/** Platform cannot fulfil given request due to resource
		  * constraints, caller should remove items before attempting
		  * to create new items.
		  */
		ItemAddedResourceExceeded,
		/** Platform not capable of honouring the request.
		  */
		ItemAddedNotSupported
	};

	/** The status a platform should report in response to a
	  * call OnExtensionUIItemRemoved(). Processing is assumed to
	  * be in-order, hence OnExtensionUIItemRemoved()'s caller is
	  * responsible for matching up status codes with the extension
	  * UIItems they belong to (and in what sequence.)
	  */
	enum ItemRemovedStatus
	{
		/** Unused empty/unknown status value. */
		ItemRemovedUnknown = 0,
		/** Extension-initiated remove operation was successful.
		  * Once notified, extension may assume that the platform will
		  * no longer report events with respect to this item's ID.
		  */
		ItemRemovedSuccess,
		/** Extension-initiated remove operation failed. A reason
		  * for this might be that the user has already removed the
		  * item.
		  */
		ItemRemovedFailed
	};

	/** The UIItem notification layer is used by the platform to
	  * to inform core about results of the OpExtensionUIListener requests.
	  */
	class ExtensionUIItemNotifications
		: public Link
	{
	public:
		virtual ~ExtensionUIItemNotifications() {}

		/** The platform UI notifying the Core extension of how a request to
		  * add/update an UIItem was handled. The OpExtensionUIListener must
		  * send out a notification for every add operation received, and
		  * in the same order.
		  */
		virtual void ItemAdded(ExtensionId id, ItemAddedStatus status) = 0;

		/** The platform UI notifying the Core extension of the outcome of a
		  * UIItem remove request. The OpExtensionUIListener must send a
		  * notification for every remove operation received.
		  */
		virtual void ItemRemoved(ExtensionId id, ItemRemovedStatus status) = 0;

		/** The platform UI notifying of user interaction for a UIItem. */
		virtual void OnClick(ExtensionId id) = 0;

		/** The platform UI notifying of a user-initiated operation to
		  * remove the UIItem. Once received, the Core extension should not
		  * use nor expect further events to reported for this item.
		  */
		virtual void OnRemove(ExtensionId id) = 0;
	};

	/** ExtensionUIItem represents a UI element that shall be embedded into the main Opera UI for an Opera extension.
	 *
	 *  This struct is created by core when an Opera extension wants to request to add, update or remove an #ExtensionUIItem by
	 *  calling OpExtensionUIListener::OnExtensionUIItemAddRequest() resp.
	 *  OpExtensionUIListener::OnExtensionUIItemRemoveRequest(). If the platform
	 *  implementation needs these values, it shall copy the struct.
	 *  A unique ExtensionId is created by the platform implementation of
	 *  OpExtensionUIListener::OnExtensionUIItemAddRequest() when a new item
	 *  is requested.
	 */
	struct ExtensionUIItem
	{
		/** Unique id of the UIItem.

		 *  When a new UIItem is requested by core, this attribute shall be
		 *  #CONTEXT_NONE. In that case the platform is responsible for creating
		 *  a new unique id >= #CONTEXT_LAST_BUILTIN and report it back to
		 *  ExtensionUIItemNotifications::ItemAdded().
		 *  On requesting to update or remove a UIItem, core sets this id to the
		 *  UIItem to update or remove.
		 */
		ExtensionId id;

		/** The parent of this item; either one of the builtin contexts
		  * (see the ExtensionToplevel enumeration), or a previously
		  * added item.
		  */
		ExtensionId parent_id;

		/** Window Id context/target where the UIItem will appear.
		  * 0 if the parent context is global (e.g., toolbar)
		  */
		unsigned long target_window;

		/** The window of the owning extension.
		  */
		OpWindow* owner_window;

		/** TRUE if interaction is disabled. Platform providing appropriate
		  * queues back to the user to indicate this state.
		  */
		BOOL disabled;

		/** Title/tooltip of this new item; optional. Length limit as imposed
		  * by the UIItem specification.
		  */
		const uni_char* title;

		/** Favicon / bitmap to associate with this UIItem; optional. */
		OpBitmap* favicon;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		/** Favicon / bitmap in hi-resolution to use with high density screen like Apple Retina; optional. */
		OpBitmap* favicon_hires;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		/** If the UI item should display a popup upon activation; optional. */
		ExtensionPopup* popup;

		/** The badge specifies the actual UI content and properties of the item. */
		ExtensionBadge* badge;

		/** Notifications of the document listener's handling of UI item
		  * operations. Also includes methods for notifying UI item owner of
		  * events (currently only button clicks.)
		  * May be NULL if core doesn't expect any response from the UI.
		  */
		ExtensionUIItemNotifications* callbacks;
	};

	/** A Core extension requests that a UIItem is added/updated to the
	  * platform's extension UI. If one with same ID already exists,
	  * the UIItem is updated with new content instead. The platform
	  * will notify of outcome by calling the ItemAdded() notification
	  * method on the ExtensionUIItem's callback object.
	  * 
	  */
	virtual void OnExtensionUIItemAddRequest(OpWindowCommander* commander, ExtensionUIItem* item) = 0;

	/** A Core extension requests that an existing UIItem be removed from
	  * the platform's extension UI. The platform will notify of outcome
	  * by calling the ItemRemoved() notification method on the
	  * ExtensionUIItem's callback object. Regardless of outcome,
	  * the platform can assume that Core will not issue requests for
	  * this UIItem afterwards.
	  */
	virtual void OnExtensionUIItemRemoveRequest(OpWindowCommander* commander, ExtensionUIItem* item) = 0;

	/** A Core extension's speed dial info (e.g. url or title) have been
	  * updated.
	  * The new url or title can be accessed through the gadget associated
	  * to the window commander.
	  *
	  * e.g.
	  * Getting the fallback url as was initially specified by the extension in the config.xml:
	  * commander->GetGadget()->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_FALLBACK_URL);
	  *
	  * Getting the url (if the url was changed by the extension through the dom api, the
	  * new value will be stored here, it's initially the same as the fallback url):
	  * commander->GetGadget()->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_URL);
	  *
	  * Getting the title (initially the extension's name, can be changed through the dom api):
	  * commander->GetGadget()->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_TITLE);
	  */
	virtual void OnExtensionSpeedDialInfoUpdated(OpWindowCommander* commander) = 0;
};
#endif // EXTENSION_SUPPORT

#endif // OP_EXTENSION_UI_LISTENER_H
