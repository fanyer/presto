// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Author: Petter Nilsen <pettern@opera.com>
//

#ifndef __TOOLBAR_MANAGER_H__
#define __TOOLBAR_MANAGER_H__

#include "modules/util/adt/opvector.h"
#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/quick_toolkit/widgets/OpBar.h"

class DesktopWindow;
class OpToolbar;
class HotlistPanel;

#define g_toolbar_manager (ToolbarManager::GetInstance())

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
* This class is handling common tasks for toolbars that have the possibility of being either in the top level window
* or inside tabs.  It will route action handling accordingly, and ensure the visibility is correct based on the 
* preferences. 
*/
class ToolbarManager : public DesktopManager<ToolbarManager>
{
public:
	enum ToolbarType
	{
		Personalbar = 1,
		PersonalbarInline,
		InstallPersonabar,	// The persona installation toolbar
		DownloadExtensionbar
	};

	ToolbarManager();
	virtual ~ToolbarManager();

	virtual OP_STATUS Init() { return OpStatus::OK; }

	/**
	* Layout a given toolbar - if the toolbar is available for that DesktopWindow type. 
	* What this means is that if the toolbar given is configured to be available for the type
	* of DesktopWindow given, it will laid out to the given rect. 
	* This this toolbar is not supposed to be available, the OpRect given will not be modified.
	* 
	* @param toolbar_type The type of toolbar to register
	* @param root_widget The root widget for this toolbar
	* @param rect The rect available - see OpBar.h for more information
	* @param compute_rect_only See OpBar.h for more information
	*/
	OpRect LayoutToAvailableRect(ToolbarType toolbar_type, OpWidget *root_widget, const OpRect& rect, BOOL compute_rect_only = FALSE);

	/**
	* Set alignment on a given toolbar - if the toolbar is available for that DesktopWindow type. 
	* What this means is that if the toolbar given is configured to be available for the type
	* of DesktopWindow given, it will be set to the given alignment
	* 
	* @param toolbar_type The type of toolbar to register
	* @param root_widget The root widget for this toolbar
	* @param alignment Alignment to set the toolbar to. See OpBar.h for definitions
	*/
	BOOL SetAlignment(ToolbarType toolbar_type, OpWidget *root_widget, OpBar::Alignment alignment);

	/**
	* Check if this is an action any of the toolbars might be interested in. Note that this does not replace
	* the general action handled in Opera and will not call any base classes. This method is just to ensure
	* that a top-level toolbar doesn't handle the action for a inline toolbar, and vice versa
	* 
	* @param action The action that might be of interest
	* @param window The DesktopWindow that received the action
	* @return TRUE if the action was handled, otherwise FALSE
	*/
	BOOL HandleAction(OpWidget *root_widget, OpInputAction *action);

	/**
	* Find a toolbar of the given type for the given DesktopWindow.  If create_if_needed
	* is set and the toolbar is of a type that should be created for this type and DesktopWindow,
	* it will be created.
	* 
	* @param type The type of toolbar to find
	* @param root_widget The root widget for this toolbar
	* @param create_if_needed If this is a supported toolbar type for the given DesktopWindow, it will be created if needed.
	* @return OK, ERR or ERR_NO_MEMORY
	*/
	OpToolbar* FindToolbar(ToolbarType type, OpWidget *root_widget, BOOL create_if_needed = FALSE);

	/**
	* Unregister all toolbars from a DesktopWindow. This must be called by the DesktopWindow during its destruction to ensure
	* no toolbar is associated with the DesktopWindow anymore
	* 
	* @param root_widget The root widget for this toolbar
	*/
	void UnregisterToolbars(OpWidget *root_widget);

private:
	/**
	* Register a toolbar to be controlled by the toolbar manager. 
	* 
	* @param toolbar_type The type of toolbar to register
	* @param root_widget OpWidget containing this toolbar
	* @param toolbar The OpToolbar to let the manager control
	* @return OK, ERR or ERR_NO_MEMORY
	*/
	OP_STATUS	RegisterToolbar(ToolbarType toolbar_type, OpWidget *root_widget, OpToolbar *toolbar);

	/**
	 * Constructs a toolbar of given toolbar class.
	 */
	template<typename ToolbarClass>
	ToolbarClass* ConstructToolbar();

	OpToolbar*	CreateToolbar(ToolbarType type, OpWidget *root_widget);

	typedef struct _ToolbarManagerItem
	{
		_ToolbarManagerItem(ToolbarType toolbar_type, OpWidget *root_widget, OpToolbar *toolbar) : m_toolbar(toolbar), m_type(toolbar_type), m_root_widget(root_widget) {}
		
		OpToolbar		*m_toolbar;
		ToolbarType		m_type;
		OpWidget		*m_root_widget;

	} ToolbarManagerItem;

private:
	OpVector<ToolbarManagerItem> m_toolbars;	// contains a list of the registered toolbars.  
};

#endif // __FEATURE_MANAGER_H__
