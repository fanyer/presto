/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifndef COLLECTIONVIEW_SETTINGS_CONTROLLER_H
#define COLLECTIONVIEW_SETTINGS_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/PopupDialogContext.h"
#include "modules/widgets/OpWidget.h"

class DocumentDesktopWindow;
class CollectionViewPane;
/**
 * @brief Provides various collection-related settings.
 *
 * This class provides UI interface via which various types of
 * settings related to collection can be applied.
 */
class CollectionViewSettingsController
		: public PopupDialogContext
		, private OpWidgetListener
{
	typedef PopupDialogContext Base;

public:
	/**
	 * @param anchor_widget identifies reference widget which this controller
	 * points to.
	 * @param collection_view_pane is a widget which shows bookmark items in various
	 * format.
	 */
	 CollectionViewSettingsController(OpWidget* anchor_widget, CollectionViewPane* collection_view_pane);

private:
	// OpWidgetListener
	virtual void			OnBeforePaint(OpWidget *widget);

	// PopupDialogContext
	virtual void			InitL();
	virtual BOOL			CanHandleAction(OpInputAction* action);
	virtual BOOL			DisablesAction(OpInputAction* action);
	virtual BOOL			SelectsAction(OpInputAction* action);
	virtual OP_STATUS		HandleAction(OpInputAction* action);
	virtual void			OnUiClosing();

	DocumentDesktopWindow*	m_active_desktop_window;
	CollectionViewPane*		m_collection_view_pane;
};

#endif //COLLECTIONVIEW_SETTINGS_CONTROLLER_H
