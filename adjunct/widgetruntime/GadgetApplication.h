/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef GADGET_APPLICATION_H
#define GADGET_APPLICATION_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick/application/NonBrowserApplication.h"
#include "adjunct/quick/spellcheck/SpellCheckContext.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/widgetruntime/GadgetMenuHandler.h"
#include "modules/img/image.h"
#include "modules/wand/wandmanager.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"

#ifdef WIDGETS_UPDATE_SUPPORT
#include "adjunct/autoupdate_gadgets/GadgetUpdateController.h"
#endif //WIDGETS_UPDATE_SUPPORT

class DesktopGadget;
class OpGadget;
class OpInputContext;
class OpenURLSetting;

/** 
 * Class intended for starting Opera widget in separate process.
 */
class GadgetApplication
		: public NonBrowserApplication
	  	, public DesktopWindowListener
		, public WandListener
#ifdef WIDGETS_UPDATE_SUPPORT
		, public GadgetUpdateListener
#endif //WIDGETS_UPDATE_SUPPORT
{
public:
	GadgetApplication();
	virtual ~GadgetApplication();

	//
	// NonBrowserApplication
	//
	virtual OpUiWindowListener* CreateUiWindowListener();

	virtual DesktopOpAuthenticationListener* CreateAuthenticationListener()
		{ return NULL; }

	virtual OpGadgetListener* CreateGadgetListener()
		{ return NULL; }

	virtual OP_STATUS Start();

	virtual DesktopMenuHandler* GetMenuHandler();

	virtual DesktopWindow* GetActiveDesktopWindow(BOOL toplevel_only);

	virtual BOOL HasCrashed() const
		{ return FALSE; }

	virtual Application::RunType DetermineFirstRunType()
		{ return Application::RUNTYPE_NORMAL; }

	virtual BOOL IsExiting() const
		{ return FALSE; }

	virtual OperaVersion GetPreviousVersion()
		{ return OperaVersion(); }

	virtual UiContext* GetInputContext()
		{ return m_input_context; }

	virtual void SetInputContext(UiContext& input_context);

	virtual BOOL OpenURL(const OpStringC &address);

	//
	// DestopWindow::Listener
	//
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window,
			BOOL user_initiated);

	//
	// WandListener
	//
	virtual OP_STATUS OnSubmit(WandInfo* info);
	virtual OP_STATUS OnSelectMatch(WandMatchInfo* info);

#ifdef WIDGETS_UPDATE_SUPPORT
	virtual void OnGadgetUpdateFinish(GadgetUpdateInfo* data,
		GadgetUpdateController::GadgetUpdateResult result);

	virtual void OnGadgetUpdateStarted(GadgetUpdateInfo* data);
#endif //WIDGETS_UPDATE_SUPPORT

private:
	friend class UiWindowListener;
	class UiWindowListener : public OpUiWindowListener
	{
	public:
		explicit UiWindowListener(GadgetApplication& application);

		virtual OP_STATUS CreateUiWindow(OpWindowCommander* windowCommander,
				OpWindowCommander* opener, UINT32 width, 
				UINT32 height, UINT32 flags = CREATEFLAG_SCROLLBARS | CREATEFLAG_TOOLBARS | CREATEFLAG_USER_INITIATED);

		virtual OP_STATUS CreateUiWindow(OpWindowCommander* commander, const CreateUiWindowArgs& args);

		virtual void CloseUiWindow(OpWindowCommander* commander);

	private:
		GadgetApplication* m_application;
	};

	OP_STATUS InitGadget();
	OP_STATUS InitGadgetWindow();

	OpGadget& GetGadget();

	OP_STATUS RestoreGadgetWindowSettings();
	OP_STATUS StoreGadgetWindowSettings();

	DesktopGadget* m_gadget_desktop_window;
	UiContext* m_input_context;
	SpellCheckContext m_spell_check_context;
	GadgetMenuHandler m_menu_handler;

#ifdef WIDGETS_UPDATE_SUPPORT
	GadgetUpdateController m_update_controller;
	WidgetUpdater*		   m_updater;
	BOOL				   m_block_app_exit;
#endif //WIDGETS_UPDATE_SUPPORT
};


#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_APPLICATION_H
