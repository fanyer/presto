/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/controller/SpeedDialConfigController.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/models/HistoryAutocompleteModel.h"
#include "adjunct/quick/speeddial/DesktopSpeedDial.h"
#include "adjunct/quick/speeddial/SpeedDialThumbnail.h"
#include "adjunct/quick/widgets/OpSpeedDialView.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick_toolkit/widgets/QuickAddressDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickComposite.h"
#include "adjunct/quick_toolkit/widgets/QuickEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickGrid.h"
#include "adjunct/desktop_util/resources/ResourceDefines.h"

#include "modules/display/pixelscaler.h"
#include "modules/gadgets/OpGadgetManager.h"

namespace
{
	const char MAIN_FEED_URL[] = "https://addons.opera.com/addons/extensions/feed/featured/";
	const uni_char OPERA_REDIRECT_URL[] = UNI_L("http://redir.opera.com/");
}


/**
 * A proxy for modifying SpeedDialConfigController::m_new_entry.
 *
 * Makes sure the entry is reset in SpeedDialConfigController::m_thumbnail when
 * all the desired modifications have been made.
 */
class SpeedDialConfigController::EntryEditor
{
public:
	explicit EntryEditor(SpeedDialConfigController& controller) : m_controller(&controller), m_committed(false) {}
	~EntryEditor() { OpStatus::Ignore(Commit()); }

	OP_STATUS SetUrl(const OpStringC& url) 
	{
		return m_controller->m_new_entry->SetURL(url); 
	}

	OP_STATUS SetDisplayUrl(const OpStringC& display_url)
	{
		return m_controller->m_new_entry->SetDisplayURL(display_url); 
	}

	OP_STATUS SetTitle(const OpStringC& title) 
	{
		return SetTitle(title, m_controller->m_new_entry->IsCustomTitle()); 
	}

	OP_STATUS SetTitle(const OpStringC& title, BOOL custom)
	{
		return m_controller->m_new_entry->SetTitle(title, custom); 
	}

	OP_STATUS SetExtensionId(const OpStringC& id)
	{
		return m_controller->m_new_entry->SetExtensionID(id); 
	}

	OP_STATUS Commit()
	{
		if (!m_committed)
		{
			if (m_controller->m_thumbnail)
				RETURN_IF_ERROR(m_controller->m_thumbnail->SetEntry(m_controller->m_new_entry));
			m_committed = true;
		}
		return OpStatus::OK;
	}

private:
	SpeedDialConfigController* m_controller;
	bool m_committed;
};

SpeedDialConfigController::SpeedDialConfigController(Mode mode)
	: m_dialog(NULL)
	, m_thumbnail(NULL)
	, m_original_entry(NULL)
	, m_new_entry(NULL)
	, m_address_drop_down(NULL)
	, m_title_edit(NULL)
	, m_action_from_pageview(FALSE)
	, m_mode(mode)
	, m_installing_extension(FALSE)
	, m_extension(NULL)
{
}

SpeedDialConfigController::~SpeedDialConfigController()
{
	g_webfeeds_api->RemoveListener(this);
	if (m_mode == ADD)
		g_desktop_extensions_manager->RemoveInstallListener(this);
	OP_DELETE(m_new_entry);
}

OP_STATUS SpeedDialConfigController::SetThumbnail(SpeedDialThumbnail& thumbnail)
{
	if (m_thumbnail != &thumbnail)
	{
		if (m_thumbnail != NULL && m_original_entry != NULL)
			// Restore previous entry since we are moving!
			m_thumbnail->SetEntry(m_original_entry);

		m_thumbnail = &thumbnail;
		m_original_entry = thumbnail.GetEntry();

		// Clone the original entry into m_new_entry.
		OP_DELETE(m_new_entry);
		m_new_entry = OP_NEW(DesktopSpeedDial, ());
		RETURN_OOM_IF_NULL(m_new_entry);
		RETURN_IF_ERROR(m_new_entry->Set(*m_original_entry));

		// Copy values from m_new_entry around.
		RETURN_IF_ERROR(thumbnail.SetEntry(m_new_entry));
		m_address.Set(m_new_entry->GetDisplayURL());
		m_title.Set(m_new_entry->GetTitle());
		if (m_new_entry->HasDisplayURL())
			m_original_address.Set(m_new_entry->GetURL());
	}

	if (m_dialog != NULL)
	{
		CalloutDialogPlacer* placer = OP_NEW(CalloutDialogPlacer, (thumbnail));
		RETURN_OOM_IF_NULL(placer);
		m_dialog->SetDialogPlacer(placer);

		// Address drop-down must be hidden initially/when moving to another
		// thumbnail.
		m_address_drop_down->GetOpWidget()->ShowDropdown(FALSE);
	}

	return OpStatus::OK;
}

void SpeedDialConfigController::InitL()
{
	OP_ASSERT(m_thumbnail != NULL);
	if (!m_thumbnail)
		LEAVE(OpStatus::ERR_NULL_POINTER);

	m_dialog = OP_NEW_L(QuickOverlayDialog, ());
	OP_ASSERT(m_thumbnail->GetParentOpSpeedDial());
	m_dialog->SetBoundingWidget(*m_thumbnail->GetParentOpSpeedDial());
	m_dialog->SetAnimationType(QuickOverlayDialog::DIALOG_ANIMATION_FADE);

	m_dialog->SetResizeOnContentChange(FALSE);

	if (m_mode == ADD)
	{
		LEAVE_IF_ERROR(SetDialog("Speed Dial Configuration Dialog", m_dialog));
	}
	else
	{
		LEAVE_IF_ERROR(SetDialog("Speed Dial Edit Dialog", m_dialog));
	}

	InitAddressL();

	if (m_mode == ADD)
	{
		InitPageViewsL();
		InitExtensionViewsL();

		// we want to request enough suggestions to fill all slots;
		// it's not a problem if we request too many ;)
		unsigned num = SUGGESTION_COUNT +
			g_desktop_extensions_manager->GetExtensionsCount();
		LEAVE_IF_ERROR(RequestExtensionsFeed(num));
	}
	else
	{
		InitTitleL();
	}

	LEAVE_IF_ERROR(SetThumbnail(*m_thumbnail));

	if (m_mode == ADD)
		g_desktop_extensions_manager->AddInstallListener(this);
}

void SpeedDialConfigController::InitAddressL()
{
	m_address_drop_down = m_dialog->GetWidgetCollection()->GetL<QuickAddressDropDown>("address_inputbox");

	LEAVE_IF_ERROR(GetBinder()->Connect(*m_address_drop_down, m_address));
	LEAVE_IF_ERROR(m_address_drop_down->AddOpWidgetListener(*this));
	LEAVE_IF_ERROR(m_address.Subscribe(
				MAKE_DELEGATE(*this, &SpeedDialConfigController::OnAddressChanged)));

	OpAddressDropDown::InitInfo info;
	info.m_max_lines = 10;
	m_address_drop_down->GetOpWidget()->InitAddressDropDown(info);
}

void SpeedDialConfigController::InitTitleL()
{
	m_title_edit = m_dialog->GetWidgetCollection()->GetL<QuickEdit>("edit_title");

	LEAVE_IF_ERROR(GetBinder()->Connect(*m_title_edit, m_title));
	LEAVE_IF_ERROR(m_title_edit->AddOpWidgetListener(*this));
}

void SpeedDialConfigController::InitPageViewsL()
{
	m_suggestions_model.Populate();

	GenericGrid* grid = m_dialog->GetWidgetCollection()->GetL<QuickGrid>("suggestions_grid");

	for (unsigned i = 0; i < ARRAY_SIZE(m_page_views); ++i)
	{
		ANCHORD(OpString8, composite_name);
		LEAVE_IF_ERROR(composite_name.AppendFormat("Page%d", i));
		QuickComposite* composite = m_dialog->GetWidgetCollection()->GetL<QuickComposite>(composite_name);
		composite->AddOpWidgetListener(*this);

		ANCHORD(OpString8, button_name);
		LEAVE_IF_ERROR(button_name.AppendFormat("PageButton%d", i));
		QuickButton* button = m_dialog->GetWidgetCollection()->GetL<QuickButton>(button_name);
		m_page_views[i].m_button = button;
		button->GetOpWidget()->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
		button->GetOpWidget()->GetBorderSkin()->SetImage("Speed Dial Configuration Dialog Image");
		button->GetOpWidget()->GetAction()->SetActionData(i);
		grid->RegisterToButtonGroup(button->GetOpWidget());
		composite->GetOpWidget()->SetActionButton(button->GetOpWidget());

		ANCHORD(OpString8, name_name);
		LEAVE_IF_ERROR(name_name.AppendFormat("PageName%d", i));
		QuickButton* name = m_dialog->GetWidgetCollection()->GetL<QuickButton>(name_name);
		m_page_views[i].m_name = name;
		name->GetOpWidget()->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_TEXT);
		name->GetOpWidget()->SetJustify(JUSTIFY_CENTER, TRUE);
		name->GetOpWidget()->SetEllipsis(g_pcui->GetIntegerPref(
					PrefsCollectionUI::EllipsisInCenter) == 1 ? ELLIPSIS_CENTER : ELLIPSIS_END);
		name->GetOpWidget()->SetRelativeSystemFontSize(95);
		name->GetOpWidget()->GetBorderSkin()->SetImage("Speed Dial Configuration Dialog Page Title");

		SpeedDialSuggestionsModelItem* item = m_suggestions_model.GetItemByIndex(i);
		if (!item)
			continue;

		if (!DocumentView::IsUrlRestrictedForViewFlag(item->GetURL().CStr(), DocumentView::ALLOW_ADD_TO_SPEED_DIAL))
			continue;

		LEAVE_IF_ERROR(m_page_views[i].m_name->SetText(item->GetTitle()));

		ANCHORD(URL, url);
		url = g_url_api->GetURL(item->GetURL());
		m_page_views[i].m_url = url;
		m_page_views[i].m_original_url.Set(item->HasDisplayURL() ? item->GetURL().CStr() : UNI_L(""));
		LEAVE_IF_ERROR(m_page_views[i].m_name->SetText(item->GetTitle()));
		button->GetOpWidget()->SetFixedImage(TRUE);

		if (DocumentView::GetType(item->GetURL().CStr()) != DocumentView::DOCUMENT_TYPE_BROWSER)
		{
			Image img;
			DocumentView::GetThumbnailImage(item->GetURL().CStr(), img);
			button->GetOpWidget()->GetForegroundSkin()->SetBitmapImage(img);
		}
		else
		{
			// Opera redirect urls may not point to particular partner;
			// we don't want to allow such generic redirects to land in speeddial.
			if (item->GetURL().Find(OPERA_REDIRECT_URL) == 0)
				LEAVE_IF_ERROR(m_page_views[i].StartResolvingURL());

			OpRect size(0, 0, button->GetPreferredWidth(), button->GetPreferredHeight());
			button->GetOpWidget()->AddPadding(size);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			INT32 scale = static_cast<DesktopOpSystemInfo*>(g_op_system_info)->GetMaximumHiresScaleFactor();
			PixelScaler scaler(scale);
			size = TO_DEVICE_PIXEL(scaler, size);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

			LEAVE_IF_ERROR(g_thumbnail_manager->RequestThumbnail(
				url,
				FALSE, // check_if_expired
				FALSE, // reload
				OpThumbnail::SpeedDial, // mode
				size.width, size.height // target size
				));
		}

		OpInputAction* action = button->GetOpWidget()->GetAction();
		action->SetActionData(i);
		action->SetActionDataString(item->GetDisplayURL());
		action->SetActionDataStringParameter(item->GetTitle());
		action->SetActionObject(m_page_views[i].m_button->GetOpWidget());
	}
}

void SpeedDialConfigController::InitExtensionViewsL()
{
	GenericGrid* grid = m_dialog->GetWidgetCollection()->GetL<QuickGrid>("suggestions_grid");

	for (unsigned i = 0; i < ARRAY_SIZE(m_extension_views); ++i)
	{
		ANCHORD(OpString8, composite_name);
		LEAVE_IF_ERROR(composite_name.AppendFormat("Extension%d", i));
		QuickComposite* composite = m_dialog->GetWidgetCollection()->GetL<QuickComposite>(composite_name);
		composite->AddOpWidgetListener(*this);

		ANCHORD(OpString8, button_name);
		LEAVE_IF_ERROR(button_name.AppendFormat("ExtensionButton%d", i));
		QuickButton* button = m_dialog->GetWidgetCollection()->GetL<QuickButton>(button_name);
		m_extension_views[i].m_button = button;
		button->GetOpWidget()->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
		button->GetOpWidget()->GetBorderSkin()->SetImage("Speed Dial Configuration Dialog Image");
		button->GetOpWidget()->SetFixedImage(TRUE);
		grid->RegisterToButtonGroup(button->GetOpWidget());
		composite->GetOpWidget()->SetActionButton(button->GetOpWidget());

		ANCHORD(OpString8, name_name);
		LEAVE_IF_ERROR(name_name.AppendFormat("ExtensionName%d", i));
		QuickButton* name = m_dialog->GetWidgetCollection()->GetL<QuickButton>(name_name);
		m_extension_views[i].m_name = name;
		name->GetOpWidget()->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
		name->GetOpWidget()->SetJustify(JUSTIFY_CENTER, TRUE);
		name->GetOpWidget()->SetEllipsis(g_pcui->GetIntegerPref(
					PrefsCollectionUI::EllipsisInCenter) == 1 ? ELLIPSIS_CENTER : ELLIPSIS_END);
		name->GetOpWidget()->SetRelativeSystemFontSize(95);
		name->GetOpWidget()->GetBorderSkin()->SetImage("Speed Dial Configuration Dialog Extension Title");
	}
}

void SpeedDialConfigController::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, 
		INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	// This is here to handle double-clicking a suggestion thumbnail.
	// It should invoke the OR-ed action of the button.
	if (nclicks == 2 && !down && widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
		static_cast<OpButton*>(widget)->Click(TRUE);
}

BOOL SpeedDialConfigController::OnContextMenu(OpWidget* widget, INT32 child_index,
		const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_EDIT)
	{
		g_application->GetMenuHandler()->ShowPopupMenu("Edit Popup Menu", PopupPlacement::AnchorAtCursor(), 0, keyboard_invoked);
		return TRUE;
	}

	return FALSE;
}

BOOL SpeedDialConfigController::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GO_TO_TYPED_ADDRESS:
		case OpInputAction::ACTION_OK:
		case OpInputAction::ACTION_INSTALL_SPEEDDIAL_EXTENSION:
			return TRUE;
	}

	return OkCancelDialogContext::CanHandleAction(action);
}

BOOL SpeedDialConfigController::DisablesAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
			return m_installing_extension;
	}

	return OkCancelDialogContext::DisablesAction(action);
}

OP_STATUS SpeedDialConfigController::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GO_TO_TYPED_ADDRESS:
		{
			m_action_from_pageview = action->GetActionObject() != NULL;

			// We don't want any highlight in address drop down (DSK-337804)
			m_address_drop_down->GetOpWidget()->edit->string.SetHighlightType(OpWidgetString::None);

			m_address.Set(action->GetActionDataString());
			m_title.Set(action->GetActionDataStringParameter());

			if (m_action_from_pageview)
			{
				UINT index = action->GetActionData();
				PageView* page = index < SUGGESTION_COUNT ? m_page_views + index : NULL;
				if (page)
					m_original_address.Set(page->m_original_url);
				m_action_from_pageview = false;
				m_address_drop_down->GetOpWidget()->ShowDropdown(FALSE);
				return OpStatus::OK;
			}

			// Close dropdown list or dialog
			if (m_address_drop_down->GetOpWidget()->DropdownIsOpen())
				m_address_drop_down->GetOpWidget()->ShowDropdown(FALSE);
			else
				g_input_manager->InvokeAction(OpInputAction::ACTION_OK, 0, NULL, this);

			return OpStatus::OK;
		}

		case OpInputAction::ACTION_INSTALL_SPEEDDIAL_EXTENSION:

			const INT32 pos = action->GetActionData();	
			if (pos < 0 || unsigned(pos) >= SUGGESTION_COUNT)
				return OpStatus::ERR;

			if (m_requested_extension_url == action->GetActionDataString())
				return OpStatus::OK;

			CleanUpTemporaryExtension();

			m_installing_extension = TRUE;
			g_input_manager->UpdateAllInputStates();
			m_address.Set(NULL);
			
			OpString name;
			RETURN_IF_ERROR(m_extension_views[pos].m_name->GetText(name));
			RETURN_IF_ERROR(m_requested_extension_url.Set(action->GetActionDataString()));
			RETURN_IF_ERROR(g_desktop_extensions_manager->InstallFromExternalURL(
						m_requested_extension_url,TRUE, TRUE));

			EntryEditor editor(*this);			
			RETURN_IF_ERROR(editor.SetTitle(name, FALSE));			
			RETURN_IF_ERROR(editor.SetExtensionId(UNI_L("wuid-xxxx")));
			return OpStatus::OK;
		}

	return OkCancelDialogContext::HandleAction(action);
}

void SpeedDialConfigController::CleanUpTemporaryExtension()
{
	if (m_requested_extension_url.HasContent())
				g_desktop_extensions_manager->CancelInstallationDownload(m_requested_extension_url);
	if (m_extension)
	{
		OpStatus::Ignore(g_desktop_extensions_manager->UninstallExtension(m_extension->GetIdentifier()));
	}
	m_extension = NULL;
	m_requested_extension_url.Empty();
}

void SpeedDialConfigController::OnAddressChanged(const OpStringC& address)
{
	HistoryAutocompleteModelItem* item = m_address_drop_down->GetOpWidget()->GetSelectedItem();
	if ( item != NULL || m_action_from_pageview)
		OpStatus::Ignore(ShowThumbnailPreview());

	if (item && item->HasDisplayAddress())
		RETURN_VOID_IF_ERROR(item->GetAddress(m_original_address));
	else
		m_original_address.Empty();
}

OP_STATUS SpeedDialConfigController::ShowThumbnailPreview()
{
	OpString resolved_address;
	ResolveUrlName(m_address.Get(), resolved_address);
	if (resolved_address.IsEmpty())
		return OpStatus::OK;

	OpString resolved_title;
	RETURN_IF_ERROR(resolved_title.Set(m_title.Get()));
	if (resolved_title.IsEmpty())
	{
		OpString history_url;
		RETURN_IF_ERROR(GetUrlAndTitleFromHistory(m_address.Get(), history_url, resolved_title));
	}
	CleanUpTemporaryExtension();
	// Generate thumbnail
	EntryEditor editor(*this);
	editor.SetUrl(resolved_address);
	editor.SetTitle(resolved_title, FALSE);
	editor.SetExtensionId(NULL);
	return OpStatus::OK;
}

void SpeedDialConfigController::OnOk()
{
	OpString resolved_address;
	if (m_extension != NULL)
	{
		ResolveUrlName(m_extension->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_URL), resolved_address);
		m_title.Set(m_extension->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_TITLE));	
		g_desktop_extensions_manager->SetExtensionTemporary(m_extension->GetIdentifier(),FALSE);

		EntryEditor editor(*this);
		editor.SetUrl(resolved_address);
		editor.SetTitle(m_title.Get());
		editor.SetExtensionId(m_extension->GetIdentifier());
		m_extension = NULL;	
	}
	else
	{
		OpString display_address;
		if (m_original_address.HasContent())
			display_address.Set(m_address.Get());

		ResolveUrlName(m_original_address.HasContent() ? m_original_address : m_address.Get(), resolved_address);
		if (resolved_address.IsEmpty())
			return OnCancel();

		if (!DocumentView::IsUrlRestrictedForViewFlag(resolved_address.CStr(), DocumentView::ALLOW_ADD_TO_SPEED_DIAL))
			return OnCancel(); // TODO: This is a temp solution for now!
	
		if (m_title.Get().HasContent())
		{
			EntryEditor editor(*this);
			editor.SetUrl(resolved_address);
			editor.SetDisplayUrl(display_address);
			
			if (m_title.Get() != m_original_entry->GetTitle())
				editor.SetTitle(m_title.Get(), TRUE);
			else
				editor.SetTitle(m_title.Get());
		}
		else
		{
			EntryEditor editor(*this);
			editor.SetUrl(resolved_address);
			editor.SetDisplayUrl(display_address);

			if (m_original_entry->GetURL() != resolved_address)
				editor.SetExtensionId(NULL);

			// Look up the title from the history of known urls.  If we cannot find
			// a match we have to fall back to the url itself and let the loading
			// of the speed dial update the title when loading completes.
			OpString history_address;
			OpString history_title;
			GetUrlAndTitleFromHistory(resolved_address, history_address, history_title);
			editor.SetTitle(history_title.HasContent() ? 
				history_title : (display_address.HasContent() ? display_address : resolved_address), FALSE);
		}
	}	

	g_speeddial_manager->ReplaceSpeedDial(m_original_entry, m_new_entry);
	m_thumbnail = NULL;
}

void SpeedDialConfigController::OnCancel()
{
	if (m_thumbnail != NULL && !m_thumbnail->IsDeleted())
	{
		m_thumbnail->SetEntry(m_original_entry);		
	}
	if (m_thumbnail)
		OpStatus::Ignore(g_webfeeds_api->AbortLoading(m_requested_feed));
	m_thumbnail = NULL;

	CleanUpTemporaryExtension();
}

void SpeedDialConfigController::OnUiClosing()
{
	if (m_title_edit != NULL)
		m_title_edit->RemoveOpWidgetListener(*this);
	if (m_address_drop_down != NULL)
		m_address_drop_down->RemoveOpWidgetListener(*this);

	OkCancelDialogContext::OnUiClosing();
}

OP_STATUS SpeedDialConfigController::GetUrlAndTitleFromHistory(const OpStringC& entered_url,
		OpString& history_url, OpString& history_title)
{
	// Check if it's a nickname
	OpAutoVector<OpString> urls;
	g_hotlist_manager->ResolveNickname(entered_url, urls);
	if (urls.GetCount() == 1)
	{
		RETURN_IF_ERROR(history_url.Set(*urls.Get(0)));
	}
	else
	{
		//resolve the url so it is history compatible
		ResolveUrlName(entered_url, history_url);
		if (history_url.HasContent())
		{
			HistoryModelItem* item = DesktopHistoryModel::GetInstance()->GetItemByUrl(history_url, TRUE);
			if (item != NULL)
				RETURN_IF_ERROR(history_title.Set(item->GetTitle()));
		}
	}

	return OpStatus::OK;
}

void SpeedDialConfigController::ResolveUrlName(const OpStringC& url, OpString& resolved_url)
{
	BOOL is_resolved = FALSE;
	TRAPD(status, is_resolved = g_url_api->ResolveUrlNameL(url, resolved_url, TRUE));
	if (OpStatus::IsError(status) || !is_resolved)
		resolved_url.Empty();
}

void SpeedDialConfigController::OnExtensionInstalled(const OpGadget& extension,const OpStringC& source)
{
	if (!m_requested_extension_url.HasContent() || source != m_requested_extension_url)
		return;

	m_extension = &const_cast<OpGadget&>(extension);
	m_title.Set(m_extension->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_TITLE));
	
	EntryEditor editor(*this);
	editor.SetUrl(m_address.Get());
	editor.SetTitle(m_title.Get(), FALSE);
	editor.SetExtensionId(const_cast<OpGadget&>(extension).GetIdentifier());
	m_installing_extension = FALSE;
}

void SpeedDialConfigController::OnExtensionInstallFailed()
{
	m_extension = NULL;
	m_installing_extension = FALSE;
	m_requested_extension_url.Empty();

	EntryEditor editor(*this);
	editor.SetUrl(NULL);
	editor.SetTitle(NULL);
	editor.SetExtensionId(NULL);
}

OP_STATUS SpeedDialConfigController::RequestExtensionsFeed(unsigned limit)
{
	OpStatus::Ignore(g_webfeeds_api->AbortLoading(m_requested_feed));

	OpString url;
	RETURN_IF_ERROR(url.Set(MAIN_FEED_URL));
	RETURN_IF_ERROR(url.Append(UNI_L("?tag=speeddial")));
	RETURN_IF_ERROR(url.AppendFormat(UNI_L("&language=%s"), g_languageManager->GetLanguage().CStr()));

	if (limit > 0)
		RETURN_IF_ERROR(url.AppendFormat(UNI_L("&limit=%u"), limit));

	RETURN_IF_ERROR(url.AppendFormat(UNI_L("&country_code=%s"), g_region_info->m_country.CStr()));

	URL feed_url = g_url_api->GetURL(url);
	RETURN_IF_ERROR(g_webfeeds_api->LoadFeed(feed_url, this));

	m_requested_feed = feed_url;

	return OpStatus::OK;
}

void SpeedDialConfigController::OnFeedLoaded(OpFeed *feed, OpFeed::FeedStatus status)
{
	if (!feed)
		// happens when aborting load request
		return;

	OP_ASSERT(feed->GetURL() == m_requested_feed);

	if (OpFeed::IsFailureStatus(status))
		return;

	unsigned suggestion_index = 0;
	bool first_pass = true;

	for (OpFeedEntry* entry = feed->GetFirstEntry();; entry = entry->GetNext())
	{
		if (suggestion_index >= SUGGESTION_COUNT)
			break;
		//
		// If we went through all entries and it turned out user has already
		// installed so many SD extensions, that it is impossible not to show
		// already installed ones, then we fill remaining entries anyway.
		//
		// We do this in two passes through entries list:
		// - in first pass we want to pick only not installed extensions
		// - in second pass we pick only suggestions of installed extensions
		//   (we don't want to duplicate extensions picked in first pass)
		//
		// We don't do additional check for duplicates in feed, assuming that
		// it's taken care of on server side.
		//
		if (!entry && first_pass)
		{
			first_pass = false;
			entry = feed->GetFirstEntry();
		}
		if (!entry)
			break;

		// check if extension is installed already and act accordingly

		OpString extension_id;
		RETURN_VOID_IF_ERROR(extension_id.Set(entry->GetGuid()));
		extension_id.Strip();
		if (extension_id.IsEmpty())
			continue;

		OpGadget* gadget = g_gadget_manager->
			FindGadget(GADGET_FIND_BY_GADGET_ID, extension_id.CStr(), FALSE);

		const bool is_installed = gadget &&
			ExtensionUtils::RequestsSpeedDialWindow(*(gadget->GetClass()));

		if (is_installed == first_pass)
			continue;

		// make sure, that entry is meaningful suggestion

		OpFeedContent* name_content = entry->GetTitle();

		OpString name;
		if (name_content && name_content->IsPlainText())
		{
			RETURN_VOID_IF_ERROR(name.Set(name_content->Data()));
			name.Strip();
		}
		if (name.IsEmpty())
			continue;

		URL download_url;
		URL screenshot_url;

		for (unsigned i = 0; i < entry->LinkCount(); ++i)
		{
			OpFeedLinkElement* link = entry->GetLink(i);

			switch (link->Relationship())
			{
				case OpFeedLinkElement::Related:
					download_url = link->URI();
					break;

				case OpFeedLinkElement::Enclosure:
					screenshot_url = link->URI();
					break;
			}
		}

		if (download_url.IsEmpty())
			continue;

		// finally, request widget update
		// and let's move on to updating next suggestion

		RETURN_VOID_IF_ERROR(UpdateExtensionView(
					suggestion_index, name, download_url, screenshot_url));
		++suggestion_index;
	}
}

OP_STATUS SpeedDialConfigController::UpdateExtensionView(unsigned pos,
		const OpStringC& name, const URL& download_url,
		const URL& screenshot_url)
{
	OP_NEW_DBG("SpeedDialConfigController::UpdateExtensionView", "speeddial");
	OP_DBG(("pos = ") << pos);
	OP_DBG(("download URL = ") << download_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI));
	OP_DBG(("screenshot URL = ") << screenshot_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI));

	OP_ASSERT(pos < ARRAY_SIZE(m_extension_views));

	if (OpStatus::IsError(m_extension_views[pos].m_image_downloader.Init(screenshot_url)))
		m_extension_views[pos].DownloadFailed();

	m_extension_views[pos].m_button->GetOpWidget()->GetAction()->SetActionDataString(
			download_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI));

	RETURN_IF_ERROR(m_extension_views[pos].m_name->SetText(name));
	// Invisible, but useful for Watir tests.
	RETURN_IF_ERROR(m_extension_views[pos].m_button->GetOpWidget()->SetText(name));

	return OpStatus::OK;
}

void SpeedDialConfigController::ExtensionView::DownloadSucceeded(const OpStringC& path)
{
	OP_NEW_DBG("SpeedDialConfigController::ExtensionView::DownloadSucceeded", "speeddial");
	OP_DBG(("") << m_image_downloader.DownloadUrl().GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI) << " successfully saved as " << path);

	const BOOL read_ok = OpStatus::IsSuccess(m_image_reader.Read(path));

	OpFile image_file;
	if (OpStatus::IsSuccess(image_file.Construct(path)))
		OpStatus::Ignore(image_file.Delete());

	if (read_ok)
	{
		Image image = m_image_reader.GetImage();
		OP_DBG(("image size: ") << image.Width() << "x" << image.Height());
		m_button->GetOpWidget()->GetForegroundSkin()->SetBitmapImage(image);
	}
	else
		DownloadFailed();
}

void SpeedDialConfigController::ExtensionView::DownloadFailed()
{
	OP_NEW_DBG("SpeedDialConfigController::ExtensionView::DownloadFailed", "speeddial");
	OP_DBG(("") << m_image_downloader.DownloadUrl().GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI) << " failed to download or decode");

	Image icon;
	if (OpStatus::IsSuccess(ExtensionUtils::GetGenericExtensionIcon(icon, 64)))
		m_button->GetOpWidget()->GetForegroundSkin()->SetBitmapImage(icon);
}

OP_STATUS SpeedDialConfigController::PageView::StartResolvingURL()
{
	const OpMessage messages[] = {
		MSG_HEADER_LOADED,
		MSG_URL_LOADING_FAILED
	};
	RETURN_IF_ERROR(g_main_message_handler->
			SetCallBackList(this, 0, messages, ARRAY_SIZE(messages)));
	RETURN_IF_ERROR(m_url.SetAttribute(URL::KHTTP_Method, HTTP_METHOD_GET));
	return COMM_LOADING != m_url.Load(g_main_message_handler, URL(), TRUE, FALSE, FALSE, FALSE)?
		OpStatus::ERR : OpStatus::OK; 
}

void SpeedDialConfigController::PageView::StopResolvingURL()
{
	if (m_url.Status(TRUE) == URL_LOADING)
		m_url.StopLoading(g_main_message_handler);
	g_main_message_handler->UnsetCallBacks(this);
}

void SpeedDialConfigController::PageView::HandleCallback(OpMessage msg,
		MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
		case MSG_HEADER_LOADED:
		case MSG_URL_LOADING_FAILED:
		{
			const OpStringC& resolved_url = m_url.GetAttribute(URL::KUniName, TRUE);
			OpInputAction* action = m_button->GetOpWidget()->GetAction();
			action->SetActionDataString(resolved_url.CStr());
			break;
		}

		default:
			OP_ASSERT(!"Unexpected message");
	}
}

void SpeedDialConfigController::PageView::OnThumbnailReady(const URL& document_url,
		const Image& thumbnail,	const uni_char* title, long preview_refresh)
{
	OP_NEW_DBG("SpeedDialConfigController::PageView::OnThumbnailReady", "speeddial");

	if (m_url == document_url)
	{
		Image image = thumbnail;
		OP_DBG(("image size: ") << image.Width() << "x" << image.Height());
		m_button->GetOpWidget()->GetForegroundSkin()->SetBitmapImage(image);


		OpString name;
		if (OpStatus::IsSuccess(m_name->GetOpWidget()->GetText(name)))
		{
			// Suggestions model may be missing page title if url was never
			// loaded before (e.g. when starting opera on clean profile).
			// In such case, we want to use title reported by g_thumbnail_manager.
			if (name.IsEmpty())
				if (OpStatus::IsSuccess(name.Set(title)))
					OpStatus::Ignore(m_name->GetOpWidget()->SetText(name.CStr()));

			// Invisible, but useful for Watir tests.
			OpStatus::Ignore(m_button->GetOpWidget()->SetText(name.CStr()));
		}
	}
}

void SpeedDialConfigController::PageView::OnThumbnailFailed(const URL& document_url,
		OpLoadingListener::LoadingFinishStatus status)
{
	if (m_url == document_url)
		m_button->GetOpWidget()->GetForegroundSkin()->SetImage(NULL);
}
