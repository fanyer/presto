/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SPEED_DIAL_CONFIG_CONTROLLER_H
#define SPEED_DIAL_CONFIG_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/desktop_util/handlers/SimpleDownloadItem.h"
#include "adjunct/desktop_util/image_utils/fileimagecontentprovider.h"
#include "adjunct/quick/extensions/ExtensionInstaller.h"
#include "adjunct/quick/models/SpeedDialSuggestions.h"
#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/thumbnails/thumbnailmanager.h"
#include "modules/webfeeds/webfeeds_api.h"
#include "modules/widgets/OpWidget.h"

class DesktopSpeedDial;
class OpGadget;
class QuickAddressDropDown;
class QuickButton;
class QuickEdit;
class QuickLabel;
class QuickOverlayDialog;
class SpeedDialThumbnail;


/**
 * Controller for the single Speed Dial configuration dialog.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski@opera.com)
 */
class SpeedDialConfigController
		: public OkCancelDialogContext
		, public OpFeedListener
		, public OpWidgetListener
		, public ExtensionInstaller::Listener
{
public:

	enum Mode
	{
		ADD,
		EDIT
	};

	SpeedDialConfigController(Mode mode);
	virtual ~SpeedDialConfigController();

	/**
	 * (Re)associates the controller with a Speed Dial thumbnail.
	 *
	 * Can be called while the dialog is open to move it to another thumbnail.
	 *
	 * @param thumbnail the Speed Dial thumbnail
	 * @return status
	 */
	OP_STATUS SetThumbnail(SpeedDialThumbnail& thumbnail);

	/**
	 * @return the thumbnail currently associated with the controller.
	 */
	SpeedDialThumbnail* GetThumbnail() const { return m_thumbnail; }

	// UiContext
	virtual BOOL CanHandleAction(OpInputAction* action);
	virtual BOOL DisablesAction(OpInputAction* action);
	virtual OP_STATUS HandleAction(OpInputAction* action);
	virtual void OnUiClosing();

	// OpWidgetListener
	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, 
			INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual BOOL OnContextMenu(OpWidget* widget, INT32 child_index,
			const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked);

	// OpFeedListener
	virtual void OnUpdateFinished() {}
	virtual void OnFeedLoaded(OpFeed *feed, OpFeed::FeedStatus);
	virtual void OnEntryLoaded(OpFeedEntry *entry, OpFeedEntry::EntryStatus) {}
	virtual void OnNewEntryLoaded(OpFeedEntry *entry, OpFeedEntry::EntryStatus) {}

	//ExtensionInstaller::Listener
	virtual void OnExtensionDownloadStarted(ExtensionInstaller&) {}
	virtual void OnExtensionDownloadProgress(OpFileLength, OpFileLength) {}
	virtual void OnExtensionDownloadFinished() {}
	virtual void OnExtensionInstalled(const OpGadget& extension,const OpStringC& source);
	virtual void OnExtensionInstallAborted() {}
	virtual void OnExtensionInstallFailed();

protected:
	// OkCancelDialogContext
	virtual void OnOk();
	virtual void OnCancel();

private:

	friend class EntryEditor;
	class EntryEditor;

	/**
	 * Represents one page thumbnail in the Suggestions group.
	 */
	struct PageView
		: public ThumbnailManagerListener
		, public ::MessageObject
	{
		PageView() : m_button(NULL), m_name(NULL) { g_thumbnail_manager->AddListener(this); }
		virtual ~PageView() { g_thumbnail_manager->RemoveListener(this); StopResolvingURL(); }
		
		// ThumbnailManagerListener
		virtual void OnThumbnailRequestStarted(const URL& document_url, BOOL reload) {}
		virtual void OnThumbnailReady(const URL& document_url, const Image& thumbnail,
				const uni_char* title, long preview_refresh);
		virtual void OnThumbnailFailed(const URL& document_url,
				OpLoadingListener::LoadingFinishStatus status);
		virtual void OnInvalidateThumbnails() {}

		OP_STATUS StartResolvingURL();
		void StopResolvingURL();

		// MessageObject
		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

		QuickButton* m_button;
		QuickButton* m_name;
		URL m_url;

		OpString m_original_url;
	};

	/**
	 * Represents one extension in the Suggestions group.
	 *
	 * Handles screenshot downloading and decoding.
	 */
	struct ExtensionView : public SimpleDownloadItem::Listener
	{
		ExtensionView()
				: m_button(NULL), m_name(NULL),
				  m_image_downloader(*g_main_message_handler, *this)
		{}

		virtual void DownloadSucceeded(const OpStringC& path);
		virtual void DownloadFailed();
		virtual void DownloadInProgress(const URL&) {}

		QuickButton* m_button;
		QuickButton* m_name;
		SimpleDownloadItem m_image_downloader;
		SimpleImageReader m_image_reader;
	};

	// DialogContext
	virtual void InitL();

	void InitAddressL();
	void InitTitleL();
	void InitPageViewsL();
	void InitExtensionViewsL();

	/** Address property change handler. */
	void OnAddressChanged(const OpStringC& address);
	/** Causes the thumbnail to show a preview of the current choice. */
	OP_STATUS ShowThumbnailPreview();
	OP_STATUS GetUrlAndTitleFromHistory(const OpStringC& entered_url,
			OpString& history_url, OpString& history_title);
	void ResolveUrlName(const OpStringC& url, OpString& resolved_url);

	OP_STATUS RequestExtensionsFeed(unsigned limit);
	/** Updates an ExtensionView with feed data. */
	OP_STATUS UpdateExtensionView(unsigned pos, const OpStringC& name,
			const URL& download_url, const URL& screenshot_url);

	/** Cancels download and removes temporary extension if needed */
	void CleanUpTemporaryExtension();

	static const unsigned SUGGESTION_COUNT = 3;

	QuickOverlayDialog* m_dialog;

	SpeedDialThumbnail* m_thumbnail;
	/** The DesktopSpeedDial origianlly set in the thumbnail.  It is restored
	 * in the thumbnail eventually, unless changes are approved in the dialog
	 * with OK.
	 */
	const DesktopSpeedDial* m_original_entry;
	/** The "working" DesktopSpeedDial.  Changes entered in the dialog are
	 * applied to this instance to generate a preview.  On OK, it is passed to
	 * SpeedDialManager::ReplaceSpeedDial().
	 */
	DesktopSpeedDial* m_new_entry;

	PageView m_page_views[SUGGESTION_COUNT];

	QuickAddressDropDown* m_address_drop_down;
	QuickEdit* m_title_edit;
	ExtensionView m_extension_views[SUGGESTION_COUNT];

	/** The Speed Dial address, always up-to-date. */
	OpProperty<OpString> m_address;
	/** The Speed Dial title, always up-to-date. */
	OpProperty<OpString> m_title;
	/** If not empty this is the real address to visit, m_address is only display url */
	OpString m_original_address;

	URL m_requested_feed;

	SpeedDialSuggestionsModel m_suggestions_model;

	BOOL m_action_from_pageview;

	Mode m_mode;
	
	BOOL m_installing_extension;
	
	/** Extension source url */
	OpString m_requested_extension_url; 
	
	/** Extension which has been recently installed by this dialog*/
	OpGadget* m_extension;
};

#endif // SPEED_DIAL_CONFIG_CONTROLLER_H
