/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
*/

#include "core/pch.h"

#include "adjunct/quick/Application.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick/controller/CollectionViewSettingsController.h"
#include "adjunct/quick/dialogs/NewBookmarkFolderDialog.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/widgets/CollectionViewPane.h"
#include "adjunct/quick/widgets/OpZoomDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickIcon.h"
#include "adjunct/quick_toolkit/contexts/WidgetContext.h"
#include "adjunct/quick_toolkit/widgets/QuickComposite.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickFlowLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickCentered.h"
#include "adjunct/quick_toolkit/widgets/QuickScrollContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickSkinElement.h"

#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/pi/OpDragManager.h"

#define KEYNAME_OF_THUMBNAIL_STYLE_COMPOSITE_WIDGET ("Collection Manager ViewPane Thumbnail Item")
#define KEYNAME_OF_LISTVIEW_STYLE_COMPOSITE_WIDGET	("Collection Manager ViewPane ListView Item")
#define KEYNAME_OF_INFO_WIDGET						("info_button")
#define KEYNAME_OF_DELETE_WIDGET					("delete_button")
#define KEYNAME_OF_THUMBNAIL_WIDGET					("thumbnail_button")
#define KEYNAME_OF_TITLE_WIDGET						("item_title")
#define KEYNAME_OF_ICON_BUTTON						("fav_icon_button")

#define MAX_NUMBER_ITEMS_TO_SHOW							100
#define MAX_NUMBER_ITEMS_IN_RECENTLY_ADDED_FOLDER			10


CollectionViewPane::CollectionViewPane()
	: m_is_thumbnail_view(true)
	, m_is_in_search_mode(false)
	, m_hovered_button(NULL)
	, m_zoom_factor(1.0)
	, m_collection_generator(NULL)
	, m_view_pane_widget(NULL)
	, m_scroll_container(NULL)
	, m_thumbnail_item_container(NULL)
	, m_listview_item_container(NULL)
	, m_view_pane_toolbar_layout(NULL)
	, m_parent_container(NULL)
	, m_current_item_iterator(NULL)
	, m_last_search_iteration_index(0)
	, m_scroll_current_value(0)
{
}

CollectionViewPane::~CollectionViewPane()
{
	g_main_message_handler->UnsetCallBack(this, MSG_COLLECTION_ITEM_DELETE);
	g_main_message_handler->RemoveDelayedMessage(MSG_COLLECTION_ITEM_DELETE, 0, 0);
}

void CollectionViewPane::OnDeleted()
{
	OP_DELETE(m_collection_generator);
	OP_DELETE(m_view_pane_widget);
}

void CollectionViewPane::InitL(OpCollectionView* parent_container)
{
	m_parent_container = parent_container;

	LEAVE_IF_ERROR(WidgetContext::CreateQuickWidget("Collection Manager ViewPane", m_view_pane_widget, m_view_pane_collection_list));
	m_view_pane_widget->SetContainer(this);
	m_view_pane_widget->SetParentOpWidget(this);

	m_view_pane_toolbar_layout = m_view_pane_collection_list.GetL<QuickStackLayout>("view_pane_toolbar_layout");

	m_scroll_container = m_view_pane_collection_list.GetL<QuickScrollContainer>("view_pane_scroll_container");
	m_scroll_container->SetRespectScrollbarMargin(false);
	m_scroll_container->SetListener(this);

	m_thumbnail_item_container = m_view_pane_collection_list.GetL<QuickFlowLayout>("view_pane_flowlayout");
	m_listview_item_container = m_view_pane_collection_list.GetL<QuickDynamicGrid>("list_view_page");

	m_view_pane_collection_list.GetL<QuickCentered>("thumbnail_view_container")->Hide();
	m_view_pane_collection_list.GetL<QuickSkinElement>("listview_container")->Hide();
	m_view_pane_collection_list.GetL<QuickCentered>("notification_view_container")->Hide();

	m_collection_generator = OP_NEW_L(CollectionThumbnailGenerator, (*this));
	OpStatus::Ignore(m_collection_generator->Init());

	m_zoom_factor = g_pcui->GetIntegerPref(PrefsCollectionUI::CollectionZoomLevel) * 0.01;
	m_is_thumbnail_view = g_pcui->GetIntegerPref(PrefsCollectionUI::CollectionStyle) ? true : false;

	m_collection_generator->UpdateThumbnailSize(m_zoom_factor);
	OpStatus::Ignore(g_main_message_handler->SetCallBack(this, MSG_COLLECTION_ITEM_DELETE, 0));

	SetTabStop(TRUE);
}

OP_STATUS CollectionViewPane::ConstructView(CollectionModelIterator* model_iterator)
{
	RETURN_VALUE_IF_NULL(model_iterator, OpStatus::ERR);

	m_current_item_iterator = model_iterator;

	m_is_in_search_mode = false;
	m_search_results_item_id_list.Clear();

	return ConstructView();
}

OP_STATUS CollectionViewPane::ConstructViewInSearchMode(const OpINT32Vector& vector)
{
	m_is_in_search_mode = true;
	m_search_results_item_id_list.Clear();
	RETURN_IF_ERROR(m_search_results_item_id_list.DuplicateOf(vector));
	return ConstructView();
}

OP_STATUS CollectionViewPane::ConstructView()
{
	m_last_search_iteration_index = 0;
	m_scroll_current_value = 0;
	m_hovered_button = NULL;

	// Cancel any previous thumbnail request
	m_collection_generator->CancelThumbnails();

	// Clean all containers and various caches
	m_thumbnail_item_container->RemoveAllWidgets();
	m_listview_item_container->Clear();
	m_item_id_to_viewpane_item_info.DeleteAll();

	unsigned max_limit_to_show = m_current_item_iterator->IsRecent() ? MAX_NUMBER_ITEMS_IN_RECENTLY_ADDED_FOLDER : MAX_NUMBER_ITEMS_TO_SHOW;
	PopulateView(max_limit_to_show);

	UpdateVisibility();
	UpdateLayout();

	return OpStatus::OK;
}

unsigned CollectionViewPane::PopulateView(unsigned num_of_items, bool from_start)
{
	OP_ASSERT(num_of_items);

	OP_STATUS status = OpStatus::OK;

	// Adds max_limit_to_show items into view
	unsigned int max_limit_to_show, tmp_max_limit;
	max_limit_to_show = tmp_max_limit = num_of_items;

	if (m_is_in_search_mode)
	{
		unsigned count = m_search_results_item_id_list.GetCount();
		CollectionModel* model = m_current_item_iterator->GetModel();

		unsigned i = from_start ? 0 : m_last_search_iteration_index;
		for (;status != OpStatus::ERR_NO_MEMORY && i < count && max_limit_to_show; i++)
		{
			CollectionModelItem* item = model->GetModelItemByID(m_search_results_item_id_list.Get(i));
			status = CreateViewWidget(item);
			if (OpStatus::IsError(status))
				continue;

			max_limit_to_show--;
		}

		if (!from_start)
			m_last_search_iteration_index += tmp_max_limit - max_limit_to_show;
	}
	else
	{
		if (from_start)
			m_current_item_iterator->Reset();

		for (CollectionModelItem* item = m_current_item_iterator->Next();
			item && status != OpStatus::ERR_NO_MEMORY; item = m_current_item_iterator->Next())
		{
			status = CreateViewWidget(item);
			if (OpStatus::IsSuccess(status))
			{
				max_limit_to_show--;
				if (!max_limit_to_show)
					break;
			}
		}
	}

	return tmp_max_limit - max_limit_to_show;
}

OP_STATUS CollectionViewPane::AddWidgetInView(OpAutoPtr<QuickWidget>& widget,  OpAutoPtr<TypedObjectCollection>& collection, INT32 item_id, INT32 position)
{
	OpAutoPtr<TypedObjectCollection> tmp_collection(OP_NEW(TypedObjectCollection, ()));
	RETURN_OOM_IF_NULL(tmp_collection.get());

	QuickWidget *template_widget;
	const char* template_name = m_is_thumbnail_view ? KEYNAME_OF_THUMBNAIL_STYLE_COMPOSITE_WIDGET: KEYNAME_OF_LISTVIEW_STYLE_COMPOSITE_WIDGET;
	RETURN_IF_ERROR(WidgetContext::CreateQuickWidget(template_name, template_widget, *tmp_collection));

	if (m_is_thumbnail_view)
	{
		if (unsigned (position) > m_thumbnail_item_container->GetWidgetCount())
			position = -1;

		RETURN_IF_ERROR(m_thumbnail_item_container->InsertWidget(template_widget, position));
	}
	else
	{
		RETURN_IF_ERROR(m_listview_item_container->AddRow());
		RETURN_IF_ERROR(m_listview_item_container->InsertWidget(template_widget));

		// Move widget from 'widget_count_in_layout - 1' to 'position'
		unsigned int pos = position;
		unsigned int widget_count_in_layout = GetListViewItemCount();
		if (pos < widget_count_in_layout && pos != widget_count_in_layout - 1)
			m_listview_item_container->MoveRow(widget_count_in_layout - 1, position);
	}

	collection.reset(tmp_collection.release());
	widget.reset(template_widget);

	return OpStatus::OK;
}

OP_STATUS CollectionViewPane::PrepareItemInfoL(INT32 item_id, TypedObjectCollection& collection)
{
	OpAutoPtr<ViewPaneItemInfo> viewpane_item_info;
	if (!m_item_id_to_viewpane_item_info.Contains(item_id))
	{
		viewpane_item_info.reset(OP_NEW(ViewPaneItemInfo, ()));
		RETURN_OOM_IF_NULL(viewpane_item_info.get());
	}
	else
	{
		ViewPaneItemInfo* view_item_tmp;
		RETURN_IF_ERROR(m_item_id_to_viewpane_item_info.Remove(item_id, &view_item_tmp));
		viewpane_item_info.reset(view_item_tmp);
	}

	ViewPaneItemInfo::ViewContents& view_contents = viewpane_item_info->m_viewitem_contents;

	if (m_is_thumbnail_view)
	{
		view_contents.m_thumbnail_button = collection.GetL<QuickButton>(KEYNAME_OF_THUMBNAIL_WIDGET);
		view_contents.m_thumbnail_composite = collection.GetL<QuickComposite>(KEYNAME_OF_THUMBNAIL_STYLE_COMPOSITE_WIDGET);
		view_contents.m_thumbnail_view_title = collection.GetL<QuickButton>(KEYNAME_OF_TITLE_WIDGET);
	}
	else
	{
		view_contents.m_fav_icon = collection.GetL<QuickButton>(KEYNAME_OF_ICON_BUTTON);
		view_contents.m_listview_title = collection.GetL<QuickLabel>(KEYNAME_OF_TITLE_WIDGET);
		view_contents.m_listview_composite = collection.GetL<QuickComposite>(KEYNAME_OF_LISTVIEW_STYLE_COMPOSITE_WIDGET);
		view_contents.m_listview_address = collection.GetL<QuickLabel>("item_url");
	}

	return m_item_id_to_viewpane_item_info.Add(item_id, viewpane_item_info.release());
}

OP_STATUS CollectionViewPane::PrepareWidget(const TypedObjectCollection& collection, const CollectionModelItem* item)
{
	RETURN_IF_ERROR(PrepareCompositeWidget(item));
	RETURN_IF_ERROR(PrepareTitle(item));
	RETURN_IF_ERROR(PrepareThumbnail(item));
	RETURN_IF_ERROR(PrepareAddress(item));
	OpStatus::Ignore(PrepareFavIcon(item));
	OpStatus::Ignore(PrepareDeleteButtonL(collection, item->GetItemID()));
	OpStatus::Ignore(PrepareInfoButtonL(collection, item->GetItemID()));

	return OpStatus::OK;
}

OP_STATUS CollectionViewPane::PrepareCompositeWidget(const CollectionModelItem* item)
{
	ViewPaneItemInfo* viewpane_item_info;
	RETURN_IF_ERROR(m_item_id_to_viewpane_item_info.GetData(item->GetItemID(), &viewpane_item_info));

	ViewPaneItemInfo::ViewContents& view_contents = viewpane_item_info->m_viewitem_contents;
	QuickComposite *composite = m_is_thumbnail_view ? view_contents.m_thumbnail_composite : view_contents.m_listview_composite;
	RETURN_IF_ERROR(composite->AddOpWidgetListener(*this));
	composite->GetOpWidget()->SetID(item->GetItemID());

	return OpStatus::OK;
}

OP_STATUS CollectionViewPane::PrepareTitle(const CollectionModelItem* item)
{
	ViewPaneItemInfo* viewpane_item_info;
	RETURN_IF_ERROR(m_item_id_to_viewpane_item_info.GetData(item->GetItemID(), &viewpane_item_info));

	OP_STATUS status = OpStatus::ERR;
	ViewPaneItemInfo::ViewContents& view_contents = viewpane_item_info->m_viewitem_contents;
	QuickButton* thumbnail_title = view_contents.m_thumbnail_view_title;
	if (thumbnail_title)
	{
		OpWidget* op_widget = thumbnail_title->GetOpWidget();
		op_widget->SetID(item->GetItemID());
		op_widget->SetDead(TRUE);
		if (op_widget->GetAction())
			op_widget->GetAction()->SetActionData(item->GetItemID());
		status = thumbnail_title->SetText(item->GetItemTitle());
	}

	QuickLabel *listview_title = view_contents.m_listview_title;
	if (listview_title)
	{
		listview_title->GetOpWidget()->SetID(item->GetItemID());
		status = listview_title->SetText(item->GetItemTitle());
	}

	return status;
}

OP_STATUS CollectionViewPane::PrepareAddress(const CollectionModelItem* item)
{
	if (m_is_thumbnail_view)
		return OpStatus::OK;

	ViewPaneItemInfo* viewpane_item_info;
	RETURN_IF_ERROR(m_item_id_to_viewpane_item_info.GetData(item->GetItemID(), &viewpane_item_info));

	OpString str_url;
	RETURN_IF_ERROR(item->GetItemUrl(str_url));

	ViewPaneItemInfo::ViewContents& view_contents = viewpane_item_info->m_viewitem_contents;
	QuickLabel* widget = view_contents.m_listview_address;
	widget->GetOpWidget()->SetID(item->GetItemID());
	return widget->SetText(str_url);
}

OP_STATUS CollectionViewPane::PrepareFavIcon(const CollectionModelItem* item)
{
	if (m_is_thumbnail_view)
		return OpStatus::OK;

	ViewPaneItemInfo* viewpane_item_info;
	RETURN_IF_ERROR(m_item_id_to_viewpane_item_info.GetData(item->GetItemID(), &viewpane_item_info));

	OpString str_url;
	OpStatus::Ignore(item->GetItemUrl(str_url, false));

	ViewPaneItemInfo::ViewContents& view_contents = viewpane_item_info->m_viewitem_contents;
	QuickButton* button = view_contents.m_fav_icon;
	button->GetOpWidget()->SetID(item->GetItemID());
	if (button->GetAction())
		button->GetAction()->SetActionData(item->GetItemID());

	Image icon = g_favicon_manager->Get(str_url);
	if (icon.IsEmpty())
		button->GetOpWidget()->GetForegroundSkin()->SetImage("Bookmark Unvisited");
	else
		button->GetOpWidget()->GetForegroundSkin()->SetBitmapImage(icon);

	return OpStatus::OK;
}

OP_STATUS CollectionViewPane::PrepareThumbnail(const CollectionModelItem* item, bool generate)
{
	if (!m_is_thumbnail_view)
		return OpStatus::OK;

	QuickButton* thumbnail = GetThumbnailWidget(item->GetItemID());
	RETURN_VALUE_IF_NULL(thumbnail, OpStatus::ERR);
	OpWidget* thumbnail_widget = thumbnail->GetOpWidget();
	thumbnail_widget->SetID(item->GetItemID());
	if (thumbnail_widget->GetAction())
		thumbnail_widget->GetAction()->SetActionData(item->GetItemID());

	UpdateThumbnailSize(item->GetItemID());

	OpString str_url;
	RETURN_IF_ERROR(item->GetItemUrl(str_url));
	if (str_url.IsEmpty())
		return OpStatus::OK;

	ViewPaneItemInfo* viewpane_item_info;
	RETURN_IF_ERROR(m_item_id_to_viewpane_item_info.GetData(item->GetItemID(), &viewpane_item_info));

	URL_ID url_id = 0;
	URL url;
	RETURN_IF_ERROR(GetURL(str_url, url));
	url.GetAttribute(URL::K_ID, &url_id, URL::KNoRedirect);
	viewpane_item_info->m_url_in_use.SetURL(url);

	ViewPaneItemInfo::ThumbnailInfo& thumbnail_info = viewpane_item_info->m_thumbnail_info;
	if (thumbnail_info.m_is_thumbnail_req_made)
		m_collection_generator->CancelThumbnail(item->GetItemID());

	if (generate)
	{
		thumbnail_info.m_is_thumbnail_req_made = true;
		thumbnail_info.m_is_thumbnail_generated = false;
		m_collection_generator->GenerateThumbnail(item->GetItemID());
	}

	return OpStatus::OK;
}

OP_STATUS CollectionViewPane::PrepareDeleteButtonL(const TypedObjectCollection& collection, INT32 item_id)
{
	QuickButton *button = collection.GetL<QuickButton>(KEYNAME_OF_DELETE_WIDGET);
	button->GetOpWidget()->SetID(item_id);
	if (button->GetOpWidget()->GetAction())
		button->GetOpWidget()->GetAction()->SetActionData (item_id);

	return OpStatus::OK;
}

OP_STATUS CollectionViewPane::PrepareInfoButtonL(const TypedObjectCollection& collection, INT32 item_id)
{
	QuickButton *button = collection.GetL<QuickButton>(KEYNAME_OF_INFO_WIDGET);
	button->GetOpWidget()->SetID(item_id);
	if (button->GetOpWidget()->GetAction())
		button->GetOpWidget()->GetAction()->SetActionData (item_id);

	return OpStatus::OK;
}

unsigned CollectionViewPane::GetListViewItemCount() const
{
	return m_listview_item_container->GetColumnCount() * m_listview_item_container->GetRowCount();
}

int CollectionViewPane::FindWidgetInListView(QuickWidget* widget) const
{
	for (unsigned i = 0; i < GetListViewItemCount() ; i++)
		if (m_listview_item_container->GetCell(0, i)->GetContent() == widget)
			return i;

	return -1;
}

OP_STATUS CollectionViewPane::GetURL(const OpStringC& str_url, URL &url) const
{
	OpString resolved_url;
	BOOL is_resolved = g_url_api->ResolveUrlNameL(str_url, resolved_url, TRUE);
	if (!is_resolved)
		RETURN_IF_ERROR(resolved_url.Set(str_url));

	URL empty_url;
	url = g_url_api->GetURL(empty_url, resolved_url, TRUE);
	return OpStatus::OK;
}

OP_STATUS CollectionViewPane::GetURL(INT32 item_id, URL_InUse& url_inuse) const
{
	ViewPaneItemInfo* viewpane_item_info;
	RETURN_IF_ERROR(m_item_id_to_viewpane_item_info.GetData(item_id, &viewpane_item_info));
	if (!viewpane_item_info->m_url_in_use->IsEmpty())
		url_inuse.SetURL(viewpane_item_info->m_url_in_use);

	return OpStatus::OK;
}

OP_STATUS CollectionViewPane::ReloadThumbnail(INT32 item_id) const
{
	ViewPaneItemInfo* viewpane_item_info;
	RETURN_IF_ERROR(m_item_id_to_viewpane_item_info.GetData(item_id, &viewpane_item_info));

	ViewPaneItemInfo::ThumbnailInfo& thumbnail_info = viewpane_item_info->m_thumbnail_info;
	if (thumbnail_info.m_is_thumbnail_req_made)
		m_collection_generator->CancelThumbnail(item_id);

	thumbnail_info.m_is_thumbnail_req_made  = true;
	thumbnail_info.m_is_thumbnail_generated = false;
	thumbnail_info.m_zoom_factor            = m_zoom_factor;

	m_collection_generator->GenerateThumbnail(item_id, true);

	// Show spinner
	Image img;
	OpWidget* widget = GetThumbnailWidget(item_id)->GetOpWidget();
	widget->GetForegroundSkin()->SetImage(NULL, NULL);
	widget->GetBorderSkin()->SetBitmapImage(img, TRUE);
	widget->GetForegroundSkin()->SetImage("Viewpane Thumbnail Loading Started");
	return OpStatus::OK;
}

void CollectionViewPane::ThumbnailReady(INT32 id, Image& thumbnail, bool has_fixed_image) const
{
	if (!GetThumbnailWidget(id))
		return;

	OpWidget* widget = GetThumbnailWidget(id)->GetOpWidget();

	// Empty foreground and background before new image is applied
	Image img;
	widget->GetBorderSkin()->SetBitmapImage(img, TRUE);
	widget->GetForegroundSkin()->SetImage(NULL, NULL);
	if (has_fixed_image)
		widget->GetForegroundSkin()->SetBitmapImage(thumbnail);
	else
		widget->GetBorderSkin()->SetBitmapImage(thumbnail);

	ViewPaneItemInfo* viewpane_item_info;
	RETURN_VOID_IF_ERROR(m_item_id_to_viewpane_item_info.GetData(id, &viewpane_item_info));
	viewpane_item_info->m_thumbnail_info.m_is_thumbnail_generated = true;
}

void CollectionViewPane::ThumbnailFailed(INT32 id) const
{
	if (!GetThumbnailWidget(id))
		return;

	OpWidget* widget = GetThumbnailWidget(id)->GetOpWidget();

	// Empty foreground and background before new image is applied
	Image img;
	widget->GetBorderSkin()->SetBitmapImage(img, TRUE);
	widget->GetForegroundSkin()->SetImage(NULL, NULL);

	widget->GetForegroundSkin()->SetImage("Viewpane Thumbnail Loading Failed");

	ViewPaneItemInfo* viewpane_item_info;
	RETURN_VOID_IF_ERROR(m_item_id_to_viewpane_item_info.GetData(id, &viewpane_item_info));
	viewpane_item_info->m_thumbnail_info.m_is_thumbnail_generated = false;
}

OP_STATUS CollectionViewPane::RemoveWidgetFromView(INT32 item_id)
{
	m_collection_generator->CancelThumbnail(item_id);

	ViewPaneItemInfo* viewpane_item_info;
	RETURN_IF_ERROR(m_item_id_to_viewpane_item_info.Remove(item_id, &viewpane_item_info));

	ViewPaneItemInfo::ViewContents& view_contents = viewpane_item_info->m_viewitem_contents;

	if (view_contents.m_thumbnail_composite)
	{
		OpStatus::Ignore(view_contents.m_thumbnail_composite->RemoveOpWidgetListener(*this));

		int pos = m_thumbnail_item_container->FindWidget(view_contents.m_thumbnail_composite);
		if (pos != -1)
			m_thumbnail_item_container->RemoveWidget(pos);
	}

	if (view_contents.m_listview_composite)
	{
		OpStatus::Ignore(view_contents.m_listview_composite->RemoveOpWidgetListener(*this));

		int pos = FindWidgetInListView(view_contents.m_listview_composite);
		if (pos != -1)
			m_listview_item_container->RemoveRow(pos);
	}

	OP_DELETE(viewpane_item_info);

	m_hovered_button = NULL;

	return OpStatus::OK;
}

QuickButton* CollectionViewPane::GetThumbnailWidget(INT32 item_id) const
{
	ViewPaneItemInfo* viewpane_item_info;
	RETURN_VALUE_IF_ERROR(m_item_id_to_viewpane_item_info.GetData(item_id, &viewpane_item_info), NULL);
	return viewpane_item_info->m_viewitem_contents.m_thumbnail_button;
}

unsigned CollectionViewPane::GetNumOfItemsPerPage() const
{
	unsigned num_of_items = 0;
	unsigned width, height;
	if (m_is_thumbnail_view)
	{
		m_collection_generator->GetThumbnailSize(width, height);
		num_of_items = (rect.width / width) * (rect.height / height);
	}
	else
	{
		if (m_listview_item_container->GetCell(0, 0))
		{
			unsigned height = m_listview_item_container->GetCell(0, 0)->GetContent()->GetMinimumHeight();
			num_of_items = (rect.height / height) + (rect.height % height ? 1 : 0);
		}
	}
	return num_of_items;
}

void CollectionViewPane::ZoomThumbnails(unsigned int zoom_value)
{
	if (!m_is_thumbnail_view)
		return;

	const double current_zoom_factor = zoom_value * 0.01;
	if (current_zoom_factor < MIN_ZOOM || current_zoom_factor > MAX_ZOOM)
		return;

	bool is_zoomed_out = current_zoom_factor < m_zoom_factor;
	m_zoom_factor = current_zoom_factor;
	m_collection_generator->UpdateThumbnailSize(m_zoom_factor);
	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::CollectionZoomLevel, zoom_value));
	TRAP(err, g_prefsManager->CommitL());

	/*
	 * Thumbnailview or listview is created with fixed number of items. This
	 * is controlled by a define in the source file.
	 *
	 * More items are added into view (provided that there are more items to be
	 * added) as user scroll-down or zoom out(works for thumbnail view) active
	 * view. Please look into the method OnContentScrolled().
	 */
	if (is_zoomed_out)
		PopulateView(GetNumOfItemsPerPage(), false);

	UpdateThumbnailViewLayout();
	UpdateLayout();
}

void CollectionViewPane::SwitchViews(bool is_thumbnail_view)
{
	if (m_is_thumbnail_view == is_thumbnail_view)
		return;

	m_is_thumbnail_view  = is_thumbnail_view;

	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::CollectionStyle, m_is_thumbnail_view));
	TRAP(err, g_prefsManager->CommitL());

	QuickSkinElement* listview_container = m_view_pane_collection_list.GetL<QuickSkinElement>("listview_container");
	QuickCentered* thumbnailview_container = m_view_pane_collection_list.GetL<QuickCentered>("thumbnail_view_container");
	if (m_is_thumbnail_view)
	{
		listview_container->Hide();
		thumbnailview_container->Show();
	}
	else
	{
		thumbnailview_container->Hide();
		listview_container->Show();
	}

	ConstructView();
}

OP_STATUS CollectionViewPane::CreateViewWidget(CollectionModelItem* item)
{
	RETURN_IF_ERROR(IsValidItem(item));

	INT32 actual_position = m_current_item_iterator->GetPosition(item);

	OpAutoPtr<QuickWidget> widget_item;
	OpAutoPtr<TypedObjectCollection> collection;
	RETURN_IF_ERROR(AddWidgetInView(widget_item, collection, item->GetItemID(), actual_position));
	TRAPD(err, PrepareItemInfoL(item->GetItemID(), *collection));
	OP_STATUS status = PrepareWidget(*collection, item);
	if (OpStatus::IsError(status))
		RemoveWidgetFromView(item->GetItemID());

	widget_item.release();
	collection.release();

	return status;
}

OP_STATUS CollectionViewPane::IsValidItem(CollectionModelItem* item) const
{
	if (item->IsExcluded() || item->IsItemFolder())
		return OpStatus::ERR;

	HotlistModel* model = g_desktop_bookmark_manager->GetBookmarkModel();
	HotlistModelItem* hitem = static_cast<HotlistModelItem*>(item);

	// Showing all bookmarks or not descendant
	if (m_current_item_iterator->GetFolder()
		&& !model->GetIsDescendantOf(hitem, static_cast<HotlistModelItem*>(m_current_item_iterator->GetFolder())))
		return OpStatus::ERR;

	// Item is in trashbin and trashbin is not shown
	if (hitem->GetIsInsideTrashFolder())
	{
		// We're doing all or recent
		if (!m_current_item_iterator->GetFolder())
			return OpStatus::ERR;

		if (!m_current_item_iterator->GetFolder()->IsDeletedFolder()
			&& !m_current_item_iterator->GetFolder()->IsInDeletedFolder())
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS CollectionViewPane::AddItem(CollectionModelItem* item)
{
	RETURN_IF_ERROR(CreateViewWidget(item));
	UpdateVisibility();
	return OpStatus::OK;
}

void CollectionViewPane::MoveItem(CollectionModelItem* item)
{
	RemoveWidgetFromView(item->GetItemID());
	OpStatus::Ignore(CreateViewWidget(item));
	UpdateVisibility();
}

void CollectionViewPane::RemoveItem(CollectionModelItem* item)
{
	RETURN_VOID_IF_ERROR(RemoveWidgetFromView(item->GetItemID()));
	UpdateVisibility();
}

void CollectionViewPane::UpdateItemUrl(CollectionModelItem* item)
{
	OpStatus::Ignore(PrepareThumbnail(item, true));
	if (!m_is_thumbnail_view)
		OpStatus::Ignore(PrepareAddress(item));
}

void CollectionViewPane::UpdateItemTitle(CollectionModelItem* item)
{
	OpStatus::Ignore(PrepareTitle(item));
}

void CollectionViewPane::UpdateItemIcon(CollectionModelItem* item)
{
	OpStatus::Ignore(PrepareFavIcon(item));
}

bool CollectionViewPane::IsSortOptionEnabled() const
{
	return m_current_item_iterator && !m_current_item_iterator->IsRecent() && !m_is_in_search_mode ? true: false;
}

void CollectionViewPane::UpdateVisibility() const
{
	int item_count = m_is_in_search_mode ? m_search_results_item_id_list.GetCount() : m_current_item_iterator->GetCount();
	bool show_notification = item_count == 0;

	QuickCentered* thumbnailview_container = m_view_pane_collection_list.GetL<QuickCentered>("thumbnail_view_container");
	QuickSkinElement* listview_container = m_view_pane_collection_list.GetL<QuickSkinElement>("listview_container");
	QuickCentered* notification_screen = m_view_pane_collection_list.GetL<QuickCentered>("notification_view_container");
	if (show_notification)
	{
		if (thumbnailview_container->IsVisible())
			thumbnailview_container->Hide();
		if (listview_container->IsVisible())
			listview_container->Hide();
		if (notification_screen->IsVisible())
			return;

		OpString title;
		const char* skin;
		if (m_is_in_search_mode)
		{
			skin = "Collection Empty Search Results";
			OpStatus::Ignore(g_languageManager->GetString(Str::S_EMPTY_SEARCH_RESULTS, title));
		}
		else if(m_current_item_iterator->IsRecent())
		{
			skin = "Collection Empty Recently Added Folder";
			OpStatus::Ignore(g_languageManager->GetString(Str::S_NO_RECENTLY_ADDED_ITEMS_FOUND, title));
		}
		else
		{
			skin = "Collection Empty Folder";
			OpStatus::Ignore(g_languageManager->GetString(Str::S_EMPTY_FOLDER, title));
		}

		m_view_pane_collection_list.GetL<QuickIcon>("message_icon")->SetImage(skin);
		if (title.HasContent())
			m_view_pane_collection_list.GetL<QuickLabel>("information")->SetText(title);

		notification_screen->Show();
	}
	else
	{
		if (notification_screen->IsVisible())
			notification_screen->Hide();

		if (m_is_thumbnail_view)
		{
			if (thumbnailview_container->IsVisible())
				return;

			thumbnailview_container->Show();
		}
		else
		{
			if (listview_container->IsVisible())
				return;

			listview_container->Show();
		}
	}
}

void CollectionViewPane::ShowContextMenu(INT32 item_id) const
{
	OpString8 menu_name;
	if (item_id == -1)
	{
		// Context menu is opened on CollectionViewPane, not for an item
		const char* menu = NULL;
		if (!m_current_item_iterator->IsRecent() && !m_current_item_iterator->GetFolder())
			menu = "Navigationpane All Bookmarks Folder Options";
		else if (m_current_item_iterator->IsDeleted())
			menu = "Navigationpane TrashBin Options";
		else if (m_current_item_iterator->GetFolder())
		{
			if (m_current_item_iterator->GetFolder()->IsInDeletedFolder())
				menu = "Navigationpane Folders in TrashBin Options";
			else
			{
				item_id  = m_current_item_iterator->GetFolder()->GetItemID();
				menu = "Navigationpane Bookmark Folder Options";
			}
		}

		RETURN_VOID_IF_ERROR(menu_name.Set(menu));
	}
	else
		RETURN_VOID_IF_ERROR(menu_name.Set("Viewpane Bookmark Item Options"));

	g_application->GetMenuHandler()->ShowPopupMenu(menu_name.CStr(), PopupPlacement::AnchorAtCursor(), item_id);
}

void CollectionViewPane::UpdateLayout()
{
	OpRect rect = GetRect();
	int scrollbar_width = static_cast<int>(m_scroll_container->GetScrollbarSize());
	scrollbar_width = min(scrollbar_width, rect.width);
	rect.width -= scrollbar_width;
	if (rect.width >= scrollbar_width)
		m_view_pane_toolbar_layout->SetPreferredWidth(rect.width);

	int count = m_is_in_search_mode ? m_search_results_item_id_list.GetCount()
		: m_current_item_iterator ? m_current_item_iterator->GetCount() : 0;
	if (count<=0 || rect.width <= 0)
		return;

	if (m_is_thumbnail_view)
	{
		m_thumbnail_item_container->ResetBreaks();
		m_thumbnail_item_container->FitToWidth(rect.width);

		unsigned width, height;
		m_collection_generator->GetThumbnailSize(width, height);
		unsigned colbreak = rect.width/width;
		colbreak = max(colbreak, 1u);
		unsigned rowbreak = (count / colbreak) + (count % colbreak ? 1 : 0);
		m_thumbnail_item_container->SetPreferredHeight(rowbreak * height);
		m_thumbnail_item_container->SetMinimumHeight(rowbreak * height);
	}
	else
	{
		if (m_listview_item_container->GetCell(0, 0))
		{
			unsigned height = m_listview_item_container->GetCell(0, 0)->GetContent()->GetMinimumHeight();
			m_listview_item_container->SetMinimumHeight(height * count);
			m_listview_item_container->SetPreferredHeight(height * count);
		}
	}
}

void CollectionViewPane::UpdateThumbnailSize(INT32 item_id)
{
	unsigned width, height;
	m_collection_generator->GetThumbnailSize(width, height);
	QuickButton* thumbnail = GetThumbnailWidget(item_id);
	thumbnail->SetFixedHeight(height);
	thumbnail->SetFixedWidth(width);
}

void CollectionViewPane::UpdateThumbnailViewLayout()
{
	UINT32 widget_count = m_thumbnail_item_container->GetWidgetCount();
	for (unsigned i = 0 ; i < widget_count; i++)
	{
		QuickComposite* quick_widget = m_thumbnail_item_container->GetWidget(i)->GetTypedObject<QuickComposite>();
		UpdateThumbnailSize(quick_widget->GetOpWidget()->GetID());
	}
}

void CollectionViewPane::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x,
	INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (nclicks == 1
		&& !down
		&& (button == MOUSE_BUTTON_1 || button == MOUSE_BUTTON_3)
		&& (widget->GetName().Compare(KEYNAME_OF_THUMBNAIL_WIDGET) == 0
			|| widget->GetName().Compare(KEYNAME_OF_TITLE_WIDGET) == 0
			|| widget->GetName().Compare(KEYNAME_OF_ICON_BUTTON) == 0
			|| widget->GetType() == OpTypedObject::WIDGET_TYPE_DESKTOP_LABEL))
	{
		ProcessOpenAction(widget->GetID(), button == MOUSE_BUTTON_3);
	}
}

BOOL CollectionViewPane::OnContextMenu(OpWidget* widget, INT32 child_index,
	const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	INT32 id = widget->GetID();
	if (!id)
		return FALSE;

	ShowContextMenu(id);
	return TRUE;
}

BOOL CollectionViewPane::OnContextMenu(const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	ShowContextMenu(-1);
	return TRUE;
}

void CollectionViewPane::OnLayout()
{
	UpdateLayout();

	if (m_view_pane_widget)
		OpStatus::Ignore(m_view_pane_widget->Layout(GetBounds()));

	OpWidget::OnLayout();
}

BOOL CollectionViewPane::OnMouseWheel(INT32 delta, BOOL vertical)
{
	ShiftKeyState keystate = GetVisualDevice()->GetView()->GetShiftKeys();
	if (!(keystate & DISPLAY_SCALE_MODIFIER))
	{
		/* Mouse wheel events should scroll the content of the Viewpane.
		 * If the content is not scrollable, the scroll container will
		 * pass the event on to the next handler.  Which is us.  And since
		 * we don't want any other effects to happen on mouse wheel, we'll
		 * block the event here.
		 */
		return TRUE;
	}

	if (delta != 0)
	{
		UINT32 zoom_value = m_zoom_factor * 100;
		if (delta < 0)
			zoom_value += 10;
		else
			zoom_value -= 10;

		ZoomThumbnails(zoom_value);

		// Update zoom-slider
		DesktopWindow* window = GetParentDesktopWindow();
		if (!window)
			return TRUE;

		OpZoomSlider* slider = static_cast<OpZoomSlider*>(window->GetWidgetByType(WIDGET_TYPE_ZOOM_SLIDER));
		if (!slider)
			return TRUE;

		slider->UpdateZoom(zoom_value);
	}

	return TRUE;
}

void CollectionViewPane::OnMouseMove(OpWidget *widget, const OpPoint &point)
{
	OpWidget* hovered = widget->GetWidget(point, TRUE, TRUE);
	if (!hovered)
		return;

	if (m_hovered_button && m_hovered_button != hovered)
	{
		/* Reset the selected state on a previously set delete or property button */
		INT32 hover_value = m_hovered_button->GetParent() == hovered || m_hovered_button->GetParent() == hovered->GetParent() ? 100 : 0;
		m_hovered_button->GetForegroundSkin()->SetState(0, hover_value ? SKINSTATE_SELECTED : -1, hover_value);
		m_hovered_button = NULL;
	}
	else if (!m_hovered_button && (hovered->GetName() == KEYNAME_OF_DELETE_WIDGET || hovered->GetName() == KEYNAME_OF_INFO_WIDGET))
	{
		/* Set the skin to a state you can recognize, like SKINSTATE_FOCUSED */
		m_hovered_button = hovered;
		m_hovered_button->GetForegroundSkin()->SetState(SKINSTATE_SELECTED, SKINSTATE_SELECTED, 100);
	}
}

void CollectionViewPane::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	CollectionModel* model = m_current_item_iterator->GetModel();
	if (!model)
		return;

	CollectionModelItem* dragged_item = model->GetModelItemByID(widget->GetID());
	if (!dragged_item)
		return;

	DesktopDragObject* drag_object = widget->GetDragObject(OpTypedObject::DRAG_TYPE_BOOKMARK, x, y);
	if (!drag_object)
		return;

	OpString url;
	RETURN_VOID_IF_ERROR(dragged_item->GetItemUrl(url));
	RETURN_VOID_IF_ERROR(drag_object->SetURL(url.CStr()));
	RETURN_VOID_IF_ERROR(drag_object->SetTitle(dragged_item->GetItemTitle()));
	drag_object->SetID(widget->GetID());

	g_drag_manager->StartDrag(drag_object, NULL, FALSE);
}

void CollectionViewPane::OnPaint(OpWidget *widget, const OpRect &paint_rect)
{
	if (!m_is_thumbnail_view || widget->GetType() != WIDGET_TYPE_COMPOSITE)
		return;

	/*
	 * Request thumnail generation if it has not been made earlier.
	 */
	ViewPaneItemInfo* viewpane_item_info;
	RETURN_VOID_IF_ERROR(m_item_id_to_viewpane_item_info.GetData(widget->GetID(), &viewpane_item_info));
	ViewPaneItemInfo::ThumbnailInfo& thumbnail_info = viewpane_item_info->m_thumbnail_info;
	if (thumbnail_info.m_is_thumbnail_generated || thumbnail_info.m_is_thumbnail_req_made)
		return;

	thumbnail_info.m_is_thumbnail_req_made  = true;
	thumbnail_info.m_is_thumbnail_generated = false;
	thumbnail_info.m_zoom_factor            = m_zoom_factor;

	m_collection_generator->GenerateThumbnail(widget->GetID());
}

void CollectionViewPane::OnBeforePaint(OpWidget *widget)
{
	if (!m_is_thumbnail_view || widget->GetType() != WIDGET_TYPE_COMPOSITE)
		return;

	/*
	 * Cancel thumbnail requests if it has been made earlier and not in current
	 * display view (visible screen).
	 * Circumstances which make thumbnails go away from visible screen are:
	 * a. they are scrolled down/up
	 * b. Thumbnail view is zoomed-in
	 */
	ViewPaneItemInfo* viewpane_item_info;
	RETURN_VOID_IF_ERROR(m_item_id_to_viewpane_item_info.GetData(widget->GetID(), &viewpane_item_info));
	ViewPaneItemInfo::ThumbnailInfo& thumbnail_info = viewpane_item_info->m_thumbnail_info;

	/*
	 * Return if item is in visible area and thumbnail is not regenerated OR
	 * if item is not in visible area and request has been generated.
	 */
	OpRect rect_scrollcontainer = m_scroll_container->GetScreenRect();
	bool is_in_visible_area = rect_scrollcontainer.Intersecting(widget->GetScreenRect())  ? true : false;
	if (is_in_visible_area ^ thumbnail_info.m_is_thumbnail_generated)
		return;

	/*
	 * Return if thumbnail request is generated and has same zoom factor as m_zoom_factor.
	 */
	else if (thumbnail_info.m_zoom_factor == m_zoom_factor && thumbnail_info.m_is_thumbnail_generated)
		return;

	if ((thumbnail_info.m_is_thumbnail_req_made && !thumbnail_info.m_is_thumbnail_generated)
		|| thumbnail_info.m_zoom_factor != m_zoom_factor)
	{
		m_collection_generator->CancelThumbnail(widget->GetID());

		thumbnail_info.m_is_thumbnail_generated = false;
		thumbnail_info.m_is_thumbnail_req_made = false;
	}
}

BOOL CollectionViewPane::OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth)
{
	return m_scroll_container->SetScroll(delta, smooth);
}

void CollectionViewPane::OnContentScrolled(int scroll_current_value, int min, int max)
{
	/*
	 * Thumbnailview or listview is created with fixed number of items. This
	 * is controlled by a define in the source file.
	 *
	 * More items are added into view (provided that there are more items to be
	 * added) as user scroll-down or zoom out (works for thumbnail view) active
	 * view. Please look into the method ZoomThumbnails() related to adding of
	 * items when thumbnail view is zoom-out.
	 */
	int item_count = m_is_in_search_mode ? m_search_results_item_id_list.GetCount() : m_current_item_iterator->GetCount();
	if (item_count < 1)
		return;

	unsigned int max_limit_to_add;

	if (m_is_thumbnail_view)
	{
		if (unsigned(item_count) <= m_thumbnail_item_container->GetWidgetCount())
			return;
	}
	else
	{
		if (unsigned(item_count) <= GetListViewItemCount())
			return;
	}

	int prev_page    = m_scroll_current_value / rect.height;
	int current_page = scroll_current_value / rect.height;
	if (current_page <= prev_page)
		return;

	m_scroll_current_value = scroll_current_value;

	max_limit_to_add = GetNumOfItemsPerPage() * (current_page - prev_page);

	PopulateView(max_limit_to_add, false);
}

void CollectionViewPane::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
		case MSG_COLLECTION_ITEM_DELETE:
		{
			CollectionModel* model = m_current_item_iterator->GetModel();
			if (!model)
				return;

			model->DeleteItem(par1);
			return;
		}
	}
	OpWidget::HandleCallback(msg, par1, par2);
}

BOOL CollectionViewPane::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
			 return FALSE;

		case OpInputAction::ACTION_GET_ACTION_STATE:
			 return GetActionGetState(action->GetChildAction());
	}

	BOOL handled = FALSE;

	if (CanHandleAction(action) && !DisablesAction(action))
		handled =  OpStatus::IsSuccess(HandleAction(action));

	return handled;
}

BOOL CollectionViewPane::GetActionGetState(OpInputAction* action) const
{
	if (CanHandleAction(action))
	{
		action->SetEnabled(!DisablesAction(action));

		if (action->GetActionOperator() !=  OpInputAction::OPERATOR_PLUS && !action->GetNextInputAction())
			action->SetSelected(SelectsAction(action));

		return TRUE;
	}

	return FALSE;
}

BOOL CollectionViewPane::CanHandleAction(OpInputAction* action) const
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_DELETE_ITEM:
		case OpInputAction::ACTION_EDIT_PROPERTIES:
		case OpInputAction::ACTION_NEW_BOOKMARK:
		case OpInputAction::ACTION_NEW_FOLDER:
		case OpInputAction::ACTION_OPEN_ITEM:
		case OpInputAction::ACTION_ZOOM_TO:
		case OpInputAction::ACTION_RELOAD_THUMBNAIL:
		case OpInputAction::ACTION_FIND:
		case OpInputAction::ACTION_SHOW_ZOOM_POPUP_MENU:
		case OpInputAction::ACTION_STOP:
		case OpInputAction::ACTION_RELOAD:
		case OpInputAction::ACTION_OPEN_COLLECTION_VIEW_SETTINGS:
			 return TRUE;

		default:
			 return FALSE;
	}
}

BOOL CollectionViewPane::DisablesAction(OpInputAction* action) const
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_RELOAD_THUMBNAIL:
		case OpInputAction::ACTION_ZOOM_TO:
			 return !m_is_thumbnail_view;

		case OpInputAction::ACTION_STOP:
		case OpInputAction::ACTION_RELOAD:
		case OpInputAction::ACTION_SHOW_ZOOM_POPUP_MENU:
		case OpInputAction::ACTION_FIND:
			 return TRUE;

		default:
			 return FALSE;
	}
}

BOOL CollectionViewPane::SelectsAction(OpInputAction* action) const
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_RELOAD:
			 return TRUE;

		default:
			 return FALSE;
	}
}

OP_STATUS CollectionViewPane::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_DELETE_ITEM:
			 {
				 g_main_message_handler->RemoveDelayedMessage(MSG_COLLECTION_ITEM_DELETE, action->GetActionData(), 0);
				 g_main_message_handler->PostMessage(MSG_COLLECTION_ITEM_DELETE, action->GetActionData(), 0);
				 return OpStatus::OK;
			 }

		case OpInputAction::ACTION_EDIT_PROPERTIES:
			 {
				 CollectionModel* model = m_current_item_iterator->GetModel();
				 RETURN_VALUE_IF_NULL(model, OpStatus::ERR_NULL_POINTER);
				 return model->EditItem(action->GetActionData());
			 }

		case OpInputAction::ACTION_ZOOM_TO:
			 {
				 ZoomThumbnails(action->GetActionData());
				 g_application->GetActiveDocumentDesktopWindow()->BroadcastDesktopWindowContentChanged(); //Kind of hack!
				 return OpStatus::OK;
			 }

		case OpInputAction::ACTION_SWITCH_TO_THUMBNAIL_VIEW:
		case OpInputAction::ACTION_SWITCH_TO_LIST_VIEW:
			 SwitchViews(action->GetAction() == OpInputAction::ACTION_SWITCH_TO_THUMBNAIL_VIEW);
			 return OpStatus::OK;

		case OpInputAction::ACTION_OPEN_ITEM:
			 return ProcessOpenAction(action->GetActionData(), FALSE);

		case OpInputAction::ACTION_NEW_FOLDER:
			 {
				// Temp solution as we don't have New folder dialog in YAML format!
				NewBookmarkFolderDialog* dialog = OP_NEW(NewBookmarkFolderDialog, ());
				RETURN_OOM_IF_NULL(dialog);
				return dialog->Init(g_application->GetActiveDesktopWindow(), action->GetActionData());
			 }

		case OpInputAction::ACTION_NEW_BOOKMARK:
			 g_desktop_bookmark_manager->NewBookmark(action->GetActionData(), g_application->GetActiveDesktopWindow());
			 return OpStatus::OK;

		case OpInputAction::ACTION_RELOAD_THUMBNAIL:
			 return ReloadThumbnail(action->GetActionData());

		case OpInputAction::ACTION_OPEN_COLLECTION_VIEW_SETTINGS:
			 {
				 QuickButton* button = m_view_pane_collection_list.GetL<QuickButton>("settings_button");
				 DialogContext* context = OP_NEW(CollectionViewSettingsController, (button->GetOpWidget(), this));
				 return ShowDialog(context, g_global_ui_context, g_application->GetActiveDocumentDesktopWindow());
			 }

		default:
			 return OpStatus::ERR;
	}
}

OP_STATUS CollectionViewPane::ProcessOpenAction(INT32 data, BOOL background) const
{
	if (!data)
		return OpStatus::ERR;

	CollectionModel* model = m_current_item_iterator->GetModel();
	RETURN_VALUE_IF_NULL(model, OpStatus::ERR_NULL_POINTER);

	CollectionModelItem* item = model->GetModelItemByID(data);
	RETURN_VALUE_IF_NULL(item, OpStatus::ERR_NULL_POINTER);

	OpString url;
	RETURN_IF_ERROR(item->GetItemUrl(url, false));
	if (url.CompareI("javascript:", 11) == 0)
		return OpStatus::OK;

	OpenURLSetting setting;
	RETURN_IF_ERROR(setting.m_address.Set(url));
	setting.m_save_address_to_history = YES;
	setting.m_new_page = background ? YES : MAYBE;
	setting.m_in_background = background ? YES : NO;
	g_application->OpenURL(setting);

	return OpStatus::OK;
}
