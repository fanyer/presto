/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 *
 */

#ifndef GADGETSPANEL_H
#define GADGETSPANEL_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
//#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "modules/util/adt/opvector.h"

#include "adjunct/quick/panels/HotlistPanel.h"

class OpToolbar;
class OpLabel;
#ifdef WIDGET_RUNTIME_SUPPORT
class GadgetsHotlistView;
#else
class OpHotlistView;
#endif
class OpMultilineEdit;
class OpSplitter;

/***********************************************************************************
 **
 **	GadgetsPanel
 **
 ***********************************************************************************/

class GadgetsPanel : public HotlistPanel
{
	public:

		GadgetsPanel();
#ifndef WIDGET_RUNTIME_SUPPORT
		OpHotlistView*			GetHotlistView() {return m_hotlist_view;}
#endif // WIDGET_RUNTIME_SUPPORT
			
		virtual OP_STATUS		Init();
		virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
		virtual const char*		GetPanelImage() {return "Panel Widgets";}

		virtual void			OnLayoutPanel(OpRect& rect);
		virtual void			OnFullModeChanged(BOOL full_mode);
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);

		// Implementing the OpTreeModelItem interface

		virtual Type			GetType() {return PANEL_TYPE_GADGETS;}

         // == OpInputContext ======================

		virtual const char*		GetInputContextName() {return "Gadgets Panel";}
		virtual BOOL			OnInputAction(OpInputAction* action);

	private:
#ifdef WIDGET_RUNTIME_SUPPORT
		GadgetsHotlistView* m_hotlist_view;
#else
		OpHotlistView*			m_hotlist_view;
#endif // WIDGET_RUNTIME_SUPPORT
};

#endif // GADGETSPANEL_H
