/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_STATUS_FIELD_H
#define OP_STATUS_FIELD_H

#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

/***********************************************************************************
**
**	OpStatusField
**
***********************************************************************************/



class OpStatusField : public OpLabel, public DesktopWindowListener
{
	public:
								OpStatusField(DesktopWindowStatusType type = STATUS_TYPE_ALL);
		static OP_STATUS		Construct(OpStatusField** obj);

		DesktopWindowStatusType GetStatusType() { return m_status_type; }

		// Hooks

		virtual void			GetPreferedSize(INT32* w, 
												INT32* h, 
												INT32 cols, 
												INT32 rows);

		virtual void			OnAdded();
		virtual void			OnRemoving();
		virtual void			OnDragStart(const OpPoint& point);

		// DesktopWindowListener hooks
		virtual void			OnDesktopWindowStatusChanged(DesktopWindow* desktop_window, 
															 DesktopWindowStatusType type);
	
	    virtual void	        OnDesktopWindowClosing(DesktopWindow* desktop_window, 
													   BOOL user_initiated);

		// Implementing the OpTreeModelItem interface

		virtual Type			GetType() 
	    {
			return WIDGET_TYPE_STATUS_FIELD;
		}

		// == OpDelayedTriggerListener ======================

		virtual void			OnTrigger() 
	    {
			if (m_status_type != STATUS_TYPE_ALL) Relayout();
		}

		// Implementing the OpWidgetListener interface
		virtual void			OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
		virtual void			OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
		virtual void			OnMouseMove(const OpPoint &point);
	
	private:

		BOOL						m_global_widget; // TRUE: window-specific widget, FALSE: tab-specific widget
		DesktopWindowStatusType m_status_type;
     	OpString                  m_underlying_status_text;
     	DesktopWindow* m_desktop_window;
	
};

#endif // OP_STATUS_FIELD_H
