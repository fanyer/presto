/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
*/

#ifndef COLLECTION_VIEW_PANE_H
#define COLLECTION_VIEW_PANE_H

#include "modules/widgets/OpWidget.h"
#include "modules/util/OpHashTable.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick/models/CollectionModel.h"
#include "adjunct/quick/widgets/OpCollectionView.h"
#include "adjunct/quick/thumbnails/CollectionThumbnailGenerator.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickScrollContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickDynamicGrid.h"

class QuickButton;
class QuickFlowLayout;
class QuickStackLayout;
class QuickSkinElement;
class QuickCentered;
class QuickComposite;
class CollectionModelItem;
class CollectionThumbnailGenerator;

/**
 * @brief Collection's view-pane shows models' items in thumbnail
 * and listview style.
 *
 * View-pane provides mechanism to show items in two formats:
 * a. Thumbnail, which is a default view
 * b. Listview
 * c. Notification; usually for showing various informative messages.
 *
 * This view is shown on right hand side of splitter, which
 * separates navigation-pane from view-pane.
 */
class CollectionViewPane
	: public OpWidget
	, public QuickWidgetContainer
	, public QuickScrollContainerListener
{
public:
	CollectionViewPane();
	~CollectionViewPane();

	/**
	 * Initializes CollectionViewPane framework.
	 * In this method, various preferences are processed, widget-template is loaded
	 * and member variables get initialized.
	 *
	 * @param parent_container is a pointer to container widget; the one which host
	 * CollectionViewPane.
	 */
	void InitL(OpCollectionView* parent_container);

	/**
	 * Prepares viewpane to show various CollectionModelItem in one of the below format
	 * a. Listview, b. Thumbnail (default view)
	 *
	 * @param model_iter is an iterator to get item of type CollectionModelItem.
	 * @return OpStatus::OK on success, otherwise approriate error code.
	 */
	OP_STATUS ConstructView(CollectionModelIterator* model_iter);

	/**
	 * Builds viewpane to show search results in one of the preselected display format
	 * (Listview, Thumbnail).
	 *
	 * @param vector list of matched items
	 * @return OpStatus::OK on success, otherwise approriate error code.
	 */
	OP_STATUS ConstructViewInSearchMode(const OpINT32Vector& vector);

	/**
	 * Adds item to viewpane.
	 *
	 * @param item to be added into viewpane.
	 * @return OpStatus::OK on success, otherwise approriate error code.
	 */
	OP_STATUS AddItem(CollectionModelItem* item);

	/**
	 * Moves item from viewpane
	 *
	 * @param item to be moved from active viewpane.
	 * @return OpStatus::OK on success, otherwise approriate error code.
	 */
	void MoveItem(CollectionModelItem* item);

	/**
	 * Remove item from viewpane
	 *
	 * @param item to be removed from active viewpane.
	 * @return OpStatus::OK on success, otherwise approriate error code
	 */
	void RemoveItem(CollectionModelItem* item);

	/**
	 * @param item_id identifies the id of an item.
	 * @param[out] url_inuse contains URL if one found in internal cache of
	 * in-use urls for a given ID.
	 */
	OP_STATUS GetURL(INT32 item_id, URL_InUse& url_inuse) const;

	/**
	 * CollectionThumbnailGenerator uses this interface to notify that thumbnail is ready.
	 *
	 * Each ViewPane item which is displayed either in thumbnail format or listview
	 * format uses unique identifier to distinguish from others.
	 *
	 * Thumbnail images can be of two types
	 * a. One which is in skin file. For an example, there is a predefined image for
	 * URI of type "URI About Thumbnail Icon" in Opera skin package.
	 * b. Another which requires internet connection to fetch page and make thumbnail image.
	 *
	 * @param id indicates for which ViewPane item thumbnail is ready
	 * @param thumbnail holds thumbnail image.
	 * @param has_fixed_image is TRUE for images which is fetched from Opera's skin file.
	 * and FALSE means that 'thumbnail' hold webpage page.
	 */
	void ThumbnailReady(INT32 id, Image& thumbnail, bool has_fixed_image) const;

	/**
	 * CollectionThumbnailGenerator uses this interface to notify that thumbnail request
	 * is failed.
	 *
	 * @param id indicates for which ViewPane item thumbnail is failed
	 */
	void ThumbnailFailed(INT32 id) const;

	/**
	 * Makes request to regenerate thumbnail
	 *
	 * @param id of ViewPane item
	 */
	OP_STATUS ReloadThumbnail(INT32 item_id) const;

	/**
	 * Zooms all items to a given value.
	 *
	 * @param zoom_value
	 */
	void ZoomThumbnails(unsigned int zoom_value);

	/**
	 * Flips views between thumbnail and listview
	 *
	 * @param is_thumbnail_view, true means thumbnail-view and false is for listview
	 */
	void SwitchViews(bool is_thumbnail_view);

	/**
	 * Sort items to one of the given types
	 */
	void SortItems(CollectionModelIterator::SortType type) { m_parent_container->SortItems(type); }

	/**
	 * This interface can be used to change the item properties.
	 * Each ViewPane item has address and title, and can be changed by user.
	 */
	void UpdateItemUrl(CollectionModelItem* item);

	/**
	 * Update Items title
	 */
	void UpdateItemTitle(CollectionModelItem* item);

	/**
	 * Update Item Icon. Only applicable to listview presentation
	 */
	void UpdateItemIcon(CollectionModelItem* item);

	/**
	 * Return current zoom factor
	 */
	double GetWindowScale() const { return m_is_thumbnail_view ? m_zoom_factor : 1.0; }

	/**
	 * Return whether sort option is enabled or not.
	 */
	bool IsSortOptionEnabled() const;

	// OpInputContext
	virtual BOOL OnInputAction(OpInputAction* action);

	// OpWidget
	virtual BOOL						OnContextMenu(const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked);
	virtual void						OnLayout();
	virtual BOOL						OnMouseWheel(INT32 delta, BOOL vertical);
	virtual void						OnDeleted();
	virtual BOOL						IsScrollable(BOOL vertical) { return TRUE; }
	virtual BOOL						OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth = TRUE);

	// MessageObject
	virtual void						HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// OpWidgetListener
	virtual void						OnMouseEvent(OpWidget* widget, INT32 pos, INT32 x,
													INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual BOOL						OnContextMenu(OpWidget* widget, INT32 child_index,
													const OpPoint &menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);
	virtual void						OnMouseMove(OpWidget* widget, const OpPoint &point);
	virtual void						OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
	virtual void						OnPaint(OpWidget *widget, const OpRect &paint_rect);
	virtual void						OnBeforePaint(OpWidget* widget);

	// QuickWidgetContainer
	virtual void						OnContentsChanged() { Relayout(); }

	// QuickScrollContainerListener
	virtual void						OnContentScrolled(int scroll_current_value, int min, int max);

private:
	OP_STATUS							ConstructView();
	unsigned							PopulateView(unsigned num_of_items, bool from_start = true);
	OP_STATUS							CreateViewWidget(CollectionModelItem* item);
	OP_STATUS							AddWidgetInView(OpAutoPtr<QuickWidget>& widget, OpAutoPtr<TypedObjectCollection>& collection, INT32 item_id, INT32 position = -1);
	OP_STATUS							PrepareItemInfoL(INT32 item_id, TypedObjectCollection& collection);
	OP_STATUS							PrepareWidget(const TypedObjectCollection& collection, const CollectionModelItem* item);
	OP_STATUS							PrepareCompositeWidget(const CollectionModelItem* item);
	OP_STATUS							PrepareTitle(const CollectionModelItem* item);
	OP_STATUS							PrepareAddress(const CollectionModelItem* item);
	OP_STATUS							PrepareFavIcon(const CollectionModelItem* item);
	OP_STATUS							PrepareThumbnail(const CollectionModelItem* item, bool generate = false);
	OP_STATUS							PrepareDeleteButtonL(const TypedObjectCollection& collection, INT32 item_id);
	OP_STATUS							PrepareInfoButtonL(const TypedObjectCollection& collection, INT32 item_id);
	OP_STATUS							IsValidItem(CollectionModelItem* item) const;

	OP_STATUS							GetURL(const OpStringC& str_url, URL &url) const;
	unsigned							GetListViewItemCount() const;
	int									FindWidgetInListView(QuickWidget* widget) const;

	QuickButton*						GetThumbnailWidget(INT32 item_id) const;
	unsigned							GetNumOfItemsPerPage() const;

	void								UpdateVisibility() const;
	void								ShowContextMenu(INT32 item_id) const;
	void								UpdateLayout();
	void								UpdateThumbnailSize(INT32 item_id);
	void								UpdateThumbnailViewLayout();

	OP_STATUS							RemoveWidgetFromView(INT32 item_id);

	BOOL								CanHandleAction(OpInputAction* action) const;
	BOOL								DisablesAction(OpInputAction* action) const;
	BOOL								SelectsAction(OpInputAction* action) const;
	BOOL								GetActionGetState(OpInputAction* action) const;

	OP_STATUS							HandleAction(OpInputAction* action);
	OP_STATUS							ProcessOpenAction(INT32 data, BOOL background) const;

private:
	struct ViewPaneItemInfo
	{
		struct ViewContents
		{
			// Thumbnailview-widget's components
			QuickComposite* m_thumbnail_composite;
			QuickButton* m_thumbnail_button;
			QuickButton* m_thumbnail_view_title;

			// Listview-widget's components
			QuickComposite* m_listview_composite;
			QuickButton* m_fav_icon;
			QuickLabel*  m_listview_title;
			QuickLabel*  m_listview_address;

			ViewContents():
				m_thumbnail_composite(NULL),
				m_thumbnail_button(NULL),
				m_thumbnail_view_title(NULL),
				m_listview_composite(NULL),
				m_fav_icon(NULL),
				m_listview_title(NULL),
				m_listview_address(NULL)
			{ }
		};

		struct ThumbnailInfo
		{
			bool m_is_thumbnail_req_made;
			bool m_is_thumbnail_generated;		//true is success and false is failed
			double m_zoom_factor;

			ThumbnailInfo():
			m_is_thumbnail_req_made(false),
				m_is_thumbnail_generated(false),
				m_zoom_factor(1.0)
			{ }
		};

		ViewContents	m_viewitem_contents;
		ThumbnailInfo	m_thumbnail_info;
		URL_InUse		m_url_in_use;
	};

	TypedObjectCollection									m_view_pane_collection_list;
	OpAutoINT32HashTable<ViewPaneItemInfo>					m_item_id_to_viewpane_item_info;
	bool													m_is_thumbnail_view;
	bool													m_is_in_search_mode;
	OpWidget*												m_hovered_button;
	double													m_zoom_factor;
	CollectionThumbnailGenerator*							m_collection_generator;
	QuickWidget*											m_view_pane_widget;
	QuickScrollContainer*									m_scroll_container;
	QuickFlowLayout*										m_thumbnail_item_container;
	QuickDynamicGrid*										m_listview_item_container;
	QuickStackLayout*										m_view_pane_toolbar_layout;
	OpCollectionView*										m_parent_container;
	CollectionModelIterator*								m_current_item_iterator;
	OpINT32Vector											m_search_results_item_id_list;
	UINT32													m_last_search_iteration_index;
	INT32													m_scroll_current_value;
};

#endif //COLLECTION_VIEW_PANE_H
