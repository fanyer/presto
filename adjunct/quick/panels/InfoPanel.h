/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef INFO_PANEL_H
#define INFO_PANEL_H

#include "adjunct/quick/panels/WebPanel.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#ifdef USE_ABOUT_FRAMEWORK
#include "modules/about/opgenerateddocument.h"
#include "modules/locale/locale-enum.inc"
#endif

class OpToolbar;
class OpTreeView;
class LinksModel;

/***********************************************************************************
**
**	InfoPanel
**
***********************************************************************************/

class InfoPanel : public WebPanel, public DesktopWindowSpy
{
	public:
			
								InfoPanel();

		virtual OP_STATUS		Init();
		virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
		virtual const char*		GetPanelImage() {return "Panel Info";}

		virtual Type			GetType() {return PANEL_TYPE_INFO;}

		virtual	void			OnDeleted();
		virtual void			OnShow(BOOL show);

		// == OpInputContext ======================

		virtual const char*		GetInputContextName() {return "Info Panel";}
		virtual BOOL			OnInputAction(OpInputAction* action);

		// DesktopWindowSpy hooks

		virtual void			OnTargetDesktopWindowChanged(DesktopWindow* target_window);
		virtual void			OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL stopped_by_user) {if (GetBrowserView()->GetWindowCommander() != commander)UpdateInfo(FALSE);}

	private:

		void					UpdateInfo(BOOL clear);
		BOOL					SetPanelLocked(BOOL locked);

		BOOL					m_is_cleared;
		BOOL					m_needs_update;
		BOOL					m_is_locked;

#ifdef USE_ABOUT_FRAMEWORK
		class InfoPanelEmptyHTMLView : public OpGeneratedDocument
		{
		public:
			InfoPanelEmptyHTMLView(URL &url, Str::LocaleString text);

			OP_STATUS GenerateData();

		private:
			int m_text;
		};
#endif

};

#endif // INFO_PANEL_H
