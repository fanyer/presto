/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef WC_PROXY_H
#define WC_PROXY_H

#include "modules/dochand/winman_constants.h" // For Window_Type
#include "modules/inputmanager/inputmanager.h" // For FOCUS_REASON
#include "modules/windowcommander/OpWindowCommander.h" // For SecurityMode

#include "adjunct/quick/hotlist/HotlistManager.h" // For HotlistManager::ItemData
#include "adjunct/quick/windows/DocumentDesktopWindow.h" // For DocumentDesktopWindow::History
#include "adjunct/quick_toolkit/widgets/OpBar.h" // For OpBar::Alignment
#include "adjunct/quick/models/BookmarkModel.h" // BookmarkItemData

class OpBrowserView;
class OpWindowCommander;
class OpInputAction;
class OpAccessibilityExtension;
class WandLogin;
class WandLoginCallback;
struct DocumentHistoryInformation;

namespace WindowCommanderProxy
{
	void SetType(OpBrowserView* browser_view, Window_Type new_type);
	void SetType(OpWindowCommander* win_comm, Window_Type new_type);
	Window_Type	GetType(OpWindowCommander* win_comm);

	OP_STATUS SetFramesPrintType(OpWindowCommander* win_comm, DM_PrintType fptype, BOOL update);
	BOOL HasFramesPrintType(OpWindowCommander* win_comm, DM_PrintType fptype);

#ifdef ACCESS_KEYS_SUPPORT
	void UpdateAccessKeys(OpWindowCommander* win_comm, CycleAccessKeysPopupWindow*& cycles_accesskeys_popupwindow);
#endif // ACCESS_KEYS_SUPPORT

	BOOL HasWindow(OpBrowserView* browser_view);
	BOOL HasVisualDevice(OpWindowCommander* win_comm);
	BOOL HasCurrentDoc(OpBrowserView* browser_view);
	BOOL HasCurrentDoc(OpWindowCommander* win_comm);
	BOOL HasActiveFrameDocument(OpWindowCommander* win_comm);
	BOOL HasDownloadError(OpWindowCommander* win_comm);
	BOOL HasCSSModeNONE(OpWindowCommander* win_comm);
	BOOL HasCSSModeFULL(OpWindowCommander* win_comm);

	BOOL IsMailOrNewsfeedWindow(OpWindowCommander* win_comm);
	BOOL GetPreviewMode(OpWindowCommander* win_comm);

	BOOL GetScrollIsPan(OpWindowCommander* win_comm);
	void OverrideScrollIsPan(OpWindowCommander* win_comm);

	URLType	Type(OpWindowCommander* win_comm);
	URL GetCurrentURL(OpWindowCommander* win_comm);
	URL GetCurrentDocURL(OpBrowserView* browser_view);
	URL GetCurrentDocURL(OpWindowCommander* win_comm);
	URL GetCurrentDocCurrentURL(OpWindowCommander* win_comm);
	URL GetActiveFrameDocumentURL(OpWindowCommander* win_comm);
	const char* GetForceEncoding(OpWindowCommander* win_comm);
	ServerName* GetServerName(OpWindowCommander* win_comm);
	VisualDevice* GetVisualDevice(OpWindowCommander* win_comm);
	BOOL GetPageInfo(OpBrowserView* browser_view, URL& out_url);
	FramesDocument* GetTopDocument(OpWindowCommander* win_comm);

	// Progress
	long GetDocProgressCount(OpBrowserView* browser_view);
	long GetDocProgressCount(OpWindowCommander* win_comm);
	long GetProgressCount(OpWindowCommander* win_comm);
	const uni_char*	GetProgressMessage(OpWindowCommander* win_comm);
	time_t GetProgressStartTime(OpWindowCommander* win_comm);

	OpFileLength GetContentSize(OpBrowserView* browser_view);

	BOOL ShowScrollbars(OpWindowCommander* win_comm);

	void SetParentInputContext(OpWindowCommander* win_comm, OpInputContext* parent_context);
	void SetNextCSSMode(OpWindowCommander* win_comm);

#ifdef SUPPORT_VISUAL_ADBLOCK
	void SetContentBlockEditMode(OpWindowCommander* win_comm, BOOL edit_mode);
	void SetContentBlockEnabled(OpWindowCommander* win_comm, BOOL enable);
	BOOL IsContentBlockerEnabled(OpWindowCommander* win_comm);
#endif // SUPPORT_VISUAL_ADBLOCK
	void RemoveAllElementsFromHistory(OpWindowCommander* win_comm, BOOL unlink);
	void RemoveAllElementsExceptCurrent(OpWindowCommander* win_comm);
	void LoadAllImages(OpWindowCommander* win_comm);

	void TogglePrintMode(OpWindowCommander* win_comm);
	BOOL GetPrintMode(OpWindowCommander* win_comm);
	void UpdateCurrentDoc(OpWindowCommander* win_comm);
	void UpdateWindow(OpWindowCommander* win_comm);

	void ClearSelection(OpBrowserView* browser_view, BOOL clear_focus_and_highlight, BOOL clear_search = FALSE);

	BOOL HighlightNextMatch(OpBrowserView* browser_view,
							const uni_char* txt,
							BOOL match_case,
							BOOL match_words,
							BOOL links_only,
							BOOL forward,
							BOOL wrap,
							BOOL &wrapped);

	OP_STATUS OpenURL(OpBrowserView* browser_view,
					  URL& url,
					  URL& ref_url,
					  BOOL check_if_expired,
					  BOOL reload,
					  short sub_win_id,
					  BOOL user_initated = FALSE,
					  BOOL open_in_background = FALSE,
					  EnteredByUser entered_by_user = NotEnteredByUser,
					  BOOL called_from_dde = FALSE);

	OP_STATUS OpenURL(OpWindowCommander* win_comm,
					  URL& url,
					  URL& ref_url,
					  BOOL check_if_expired,
					  BOOL reload,
					  short sub_win_id,
					  BOOL user_initated = FALSE,
					  BOOL open_in_background = FALSE,
					  EnteredByUser entered_by_user = NotEnteredByUser,
					  BOOL called_from_dde = FALSE);
	
	OP_STATUS OpenURL(OpBrowserView* browser_view,
					  const uni_char* in_url_str,
					  BOOL check_if_expired = TRUE,
					  BOOL user_initated = FALSE,
					  BOOL reload = FALSE,
					  BOOL is_hotlist_url = FALSE,
					  BOOL open_in_background = FALSE,
					  EnteredByUser entered_by_user = NotEnteredByUser,
					  BOOL called_from_dde = FALSE);

	OP_STATUS OpenURL(OpWindowCommander* win_comm,
					  const uni_char* in_url_str,
					  BOOL check_if_expired = TRUE,
					  BOOL user_initated = FALSE,
					  BOOL reload = FALSE,
					  BOOL is_hotlist_url = FALSE,
					  BOOL open_in_background = FALSE,
					  EnteredByUser entered_by_user = NotEnteredByUser,
					  BOOL called_from_dde = FALSE);

#ifdef GADGET_SUPPORT
	OP_STATUS OpenURL(OpBrowserView* browser_view,
					  const uni_char* in_url_str,
					  URL_CONTEXT_ID context_id);

	OP_STATUS OpenURL(OpWindowCommander* win_comm,
					  const uni_char* in_url_str,
					  URL_CONTEXT_ID context_id);
#endif // GADGET_SUPPORT

	/**
	 * Returns TRUE if document displayed in win_comm contains form that
	 * can be used to create search engine.
	 */
	BOOL HasSearchForm(OpWindowCommander* win_comm);

	/**
	 * Initializes the 'Add/Edit Search' Dialog
	 */
	void CreateSearch(OpWindowCommander* win_comm, DesktopWindow* desktop_window, const OpStringC& window_title);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	OpAccessibleItem* GetAccessibleObject(OpWindowCommander* win_comm);
#endif

	BOOL FocusDocument(OpWindowCommander* win_comm, FOCUS_REASON reason);
	void FocusPage(OpWindowCommander* win_comm);

	BOOL IsVisualDeviceFocused(OpWindowCommander* win_comm, BOOL HasFocusedChildren = FALSE);
	void SetFocusToVisualDevice(OpWindowCommander* win_comm, FOCUS_REASON reason);
	OpRect GetVisualDeviceVisibleRect(OpWindowCommander* win_comm);

	const uni_char * GetSecurityStateText(OpWindowCommander* win_comm);

	BOOL CanViewSource(OpWindowCommander* win_comm, OpInputAction* child_action);
	BOOL ViewSource(OpWindowCommander* win_comm, BOOL frame_source);
	BOOL CanEditSitePrefs(OpWindowCommander* win_comm);

	void ReloadFrame(OpWindowCommander* win_comm);

	void GoToParentDirectory(OpWindowCommander* win_comm);

	void AddToBookmarks(OpWindowCommander* win_comm, BookmarkItemData& item_data, INT32 action_id, INT32& id);

	OP_STATUS GetDescription(OpWindowCommander* win_comm, OpString& description);

	BOOL SavePageInBookmark(OpWindowCommander* win_comm, BookmarkItemData& item_data);

	BOOL OnInputAction(OpWindowCommander* win_comm, OpInputAction* action);

	BOOL EnableImages(OpWindowCommander* win_comm, OpDocumentListener::ImageDisplayMode image_mode);

	BOOL ShouldDisplaySecurityDialog(OpWindowCommander* win_comm);

	void InitDragObject(OpWindowCommander* win_comm, DesktopDragObject* drag_object, OpRect rect);

	OP_STATUS SetEncoding(OpWindowCommander* win_comm, const uni_char* charset);

	const URL GetMovedToURL(OpWindowCommander* win_comm);

	BOOL HasConsistentRating(OpWindowCommander* win_comm);
	BOOL IsPhishing(OpWindowCommander* win_comm);
	BOOL FraudListIsEmpty(OpWindowCommander* win_comm);

	void ShowImageProperties(OpWindowCommander* win_comm, OpDocumentContext * ctx, BOOL is_background);

	void ShowBackgroundImage(OpWindowCommander* win_comm);

	void CreateLinkedWindow(OpWindowCommander* win_comm, BOOL is_keyboard_invoked);

	void ToggleUserCSS(OpWindowCommander* win_comm, OpInputAction* child_action);

	void SelectAlternateCSSFile(OpWindowCommander* win_comm, OpInputAction* child_action);

	BOOL HandlePlatformAction(OpWindowCommander* win_comm, OpInputAction* action, BOOL is_document_desktop_window = FALSE);

	BOOL IsInStrictMode(OpWindowCommander* win_comm);

	void SetVisualDevSize(OpWindowCommander* win_comm, INT32 width, INT32 height);

	void CopySettingsToWindow(OpWindowCommander* win_comm, const OpenURLSetting& setting);
	void InitOpenURLSetting(OpenURLSetting& setting, const OpStringC& url, BOOL force_new_page, BOOL force_background, BOOL url_came_from_address_field, DesktopWindow* target_window, URL_CONTEXT_ID context_id, BOOL ignore_modifier_keys);
	void SetURLCameFromAddressField(OpWindowCommander* win_comm, BOOL from_address_field);
	BOOL GetURLCameFromAddressField(OpWindowCommander* win_comm);

	OP_STATUS GetWandLink(OpWindowCommander* commander, OpString& url);
	OP_STATUS ProcessWandLogin(OpWindowCommander* commander);
	OP_STATUS GetForwardLinks(OpWindowCommander* commander, OpVector<DocumentHistoryInformation>& info_list);
	OP_STATUS AddForwardLink(OpVector<DocumentHistoryInformation>& ff_info_list, const uni_char* link_word, const uni_char* url_str);
	OP_STATUS AddForwardFileLink(OpVector<DocumentHistoryInformation>& ff_info_list, const uni_char* img_title,  const uni_char* url_str);
	int GetFastForwardValue(OpStringC which);



	void ToggleAdblock(OpWindowCommander* win_comm,
					   OpInfobar*& adblock_bar,
					   BOOL& adblock_mode,
					   ContentBlockTreeModel& content_to_block);

	void AdblockEditDone(OpWindowCommander* win_comm,
						 OpInfobar*& adblock_bar,
						 BOOL& adblock_mode,
						 BOOL accept,
						 ContentBlockTreeModel& content_to_block,
						 ContentBlockTreeModel& content_to_unblock);

	void AdBlockPattern(OpWindowCommander* win_comm,
						OpString& url_pattern,
						BOOL block,
						ContentBlockTreeModel& content_to_block);

	void OnSessionReadL(OpWindowCommander* win_comm,
						DocumentDesktopWindow* document_window,
						const OpSessionWindow* session_window,
						BOOL& focus_document);

	void OnSessionWriteL(OpWindowCommander* win_comm,
						 DocumentDesktopWindow* document_window,
						 OpSession* session,
						 OpSessionWindow* session_window,
						 BOOL shutdown);

	AutoWinReloadParam*	GetUserAutoReload(OpWindowCommander* win_comm);

	void GotoHistoryPos(OpWindowCommander* win_comm);

	void UnloadCurrentDoc(OpWindowCommander* win_comm);

#ifdef _VALIDATION_SUPPORT_
	void UploadFileForValidation(OpWindowCommander* win_comm);
#endif // _VALIDATION_SUPPORT_

	const uni_char*	GetWindowTitle(OpWindowCommander* win_comm);

#ifdef SUPPORT_VISUAL_ADBLOCK
	OP_STATUS CheckURL(const uni_char* url, BOOL& load, OpWindowCommander* win_comm);
#endif // SUPPORT_VISUAL_ADBLOCK

	OP_STATUS GetThumbnailForWindow(OpWindowCommander* win_comm, Image &thumbnail, BOOL force_rebuild);

	BOOL CloneWindow(OpWindowCommander* win_comm, Window**return_window);

	// Wand
	OP_STATUS GetLoginPassword(OpWindowCommander* win_comm, WandLogin* wand_login, WandLoginCallback* callback);
	OP_STATUS StoreLogin(OpWindowCommander* win_comm, const uni_char* id, const uni_char* username, const uni_char* password);

	OpWindowCommander* GetActiveWindowCommander();

	// Application
	void StopLoadingActiveWindow();

	OP_STATUS SavePictureToFolder(OpWindowCommander* win_comm,  OpDocumentContext * ctx, OpString& destination_filename, OpFileFolder folder);

	OP_STATUS GetSVGImage(OpWindowCommander* win_comm, Image& image, UINT32 width, UINT32 height);

	URL GetImageURL(OpWindowCommander* win_comm);
	URL GetBGImageURL(OpWindowCommander* win_comm);
};
#endif //WC_PROXY_H
