/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef EXTENSIONS_PANEL_H
#define EXTENSIONS_PANEL_H

#include "adjunct/quick/panels/HotlistPanel.h"
#include "adjunct/quick/models/ExtensionsModel.h"
#include "adjunct/quick_toolkit/widgets/QuickScrollContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"

class OpWidget;
class QuickLabel;
class QuickButton;
class QuickPagingLayout;
class QuickStackLayout;
class QuickWidget;
class ExtensionsManagerView;

/**
 * @brief View for Extensions Manager.
 *
 * Actions on this view are forwarded to DesktopExtensionsManager,
 * which acts as a controller for this widget. Extensions Panel is
 * mostly responsible for managing Extensions Manager views.
 */
class ExtensionsPanel :
	public HotlistPanel,
	public QuickWidgetContainer,
	public ExtensionsModel::Listener
{
	public:

		ExtensionsPanel();

		// OpTypedObject
		virtual Type GetType() { return PANEL_TYPE_EXTENSIONS; }

		// HotlistPanel
		virtual OP_STATUS Init();
		virtual const char*	GetPanelImage() { return "Extension 16"; }
		virtual void GetPanelText(OpString& text,
				Hotlist::PanelTextType text_type);
		virtual void OnFullModeChanged(BOOL full_mode);
		virtual void OnLayoutPanel(OpRect& rect);

		// OpInputContext
		virtual const char*	GetInputContextName() { return "Extensions Panel"; }
		virtual BOOL OnInputAction(OpInputAction* action);

		// OpWidget
		virtual void OnDirectionChanged() { OnContentsChanged(); }
		virtual BOOL IsScrollable(BOOL vertical) { return TRUE; }
		virtual BOOL OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth = TRUE);

		// OpWidgetListener
		virtual void OnMouseEvent(OpWidget *widget, INT32 pos,
				INT32 x, INT32 y, MouseButton button, BOOL down,
					UINT8 nclicks);
		virtual void OnDeleted();

		// QuickWidgetContainer
		virtual void OnContentsChanged();

		// ExtensionsModel
		virtual void OnBeforeExtensionsModelRefresh(
				const ExtensionsModel::RefreshInfo& info);
		virtual void OnAfterExtensionsModelRefresh() { /* Do nothing. */ }
		virtual void OnNormalExtensionAdded(
				const ExtensionsModelItem& model_item) { /* Do nothing. */ }
		virtual void OnDeveloperExtensionAdded(
				const ExtensionsModelItem& model_item) { /* Do nothing. */ }
		virtual void OnExtensionUninstall(
				const ExtensionsModelItem& model_item) { /* Do nothing. */ }
		virtual void OnExtensionEnabled(
				const ExtensionsModelItem& model_item) { /* Do nothing. */ }
		virtual void OnExtensionDisabled(
				const ExtensionsModelItem& model_item) { /* Do nothing. */ }
		virtual void OnExtensionUpdateAvailable(
				const ExtensionsModelItem& model_item) { /* Do nothing. */ }
		virtual void OnExtensionExtendedNameUpdate(
				const ExtensionsModelItem& model_item) { /* Do nothing. */ }
														
	private:

		QuickScrollContainer* m_content;
		QuickPagingLayout* m_page_layout;

		ExtensionsManagerView* m_extensions_view;
		QuickLabel*	m_counter_label ;

		ExtensionsManagerView* m_extensions_dev_view;
		QuickLabel*	m_dev_counter_label ;

		unsigned m_dev_count;
		unsigned m_normal_count;
		BOOL m_contents_changed;
		OpRect m_prev_rect;

		/**
		 * Helper function, creates stack which is a header including
		 * label and toolbar button. It is used to create common header
		 * part for developer and normal extensions list.
		 *
		 * @param[out] list Header widget ready for further use.
		 * @param[out] count_label Counter label widget is put here.
		 * @param[out] button Toolbar button widget is put here.
		 * @param label_str Localized string for label.
		 * @param button_str Localized string for button.
		 * @param button_action Button action.
		 * @return Status.
		 */
		OP_STATUS ConstructHeader(QuickStackLayout** list, 
				QuickLabel** count_label, QuickButton** button,
					Str::LocaleString label_str, Str::LocaleString button_str, 
					OpInputAction* button_action);

		/**
		 * Each view has a counter label indicating how many
		 * extensions are installed, this function updates these
		 * labels according to data from model.
		 *
		 * @return Status.
		 */
		OP_STATUS RefreshCounterLabels();

		/**
		 * Extracts common part for creation of normal and developer
		 * extensions lists.
		 *
		 * @param[out] widget Extensions list widget is put here.
		 * @param[out] counter_label Extensions counter label widget is put here.
		 * @param action Action for list's header button.
		 * @param button_skin Skin for list's header button.
		 * @return NULL iff error.
		 */
		template <typename T>
		ExtensionsManagerView* ConstructExtensionsList(QuickWidget** widget,
				QuickLabel** counter_label, Str::LocaleString button_str,
					OpInputAction::Action action,
					const char* button_skin);

		/**
		 * Create normal extensions list.
		 *
		 * @return NULL iff error.
		 */
		QuickWidget* ConstructNormalExtensionsList();

		/**
		 * Create developer extensions list.
		 *
		 * @return NULL iff error.
		 */
		QuickWidget* ConstructDeveloperExtensionsList();

		/**
		 * Each time a OnExtensionsModelRefresh event comes, all views
		 * are recreated according to how much normal and developer
		 * extensions are in the model. This function checks that and
		 * manages creating views and registering them as listeners.
		 *
		 * @return Status.
		 */
		OP_STATUS ResetViews();

		/**
		 * Destruct all extensions views.
		 */
		void RemoveViews();

		/**
		 * Pack normal and developer lists to vertical stack.
		 *
		 * @return NULL iff error.
		 */
		QuickStackLayout* ConstructNormalAndDeveloper();

		/**
		 * Pack developer and empty page widget to vertical stack.
		 *
		 * @return NULL iff error.
		 */
		QuickStackLayout* ConstructDeveloperAndEmptyPageWidget();
};

#endif // EXTENSIONS_PANEL_H
