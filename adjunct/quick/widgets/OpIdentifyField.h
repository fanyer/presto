// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2003 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#ifndef __OP_IDENTIFY_FIELD_H__
#define __OP_IDENTIFY_FIELD_H__

#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"

class OpIdentifyField : public OpLabel, public DocumentDesktopWindowSpy
{
public:
	OpIdentifyField(OpWidgetListener *listener);

	virtual void			GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	void 					OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	// Hooks

	virtual void			OnDeleted() {SetSpyInputContext(NULL, FALSE); OpLabel::OnDeleted();}
	virtual void			OnAdded();
	virtual void			OnDragStart(const OpPoint& point);
	virtual void			OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	virtual void			OnSettingsChanged(DesktopSettings* settings);

	// DocumentDesktopWindowSpy hooks
	virtual void			OnTargetDocumentDesktopWindowChanged(DocumentDesktopWindow* target_window) {UpdateIdentity();}

	// Implementing the OpTreeModelItem interface

	virtual Type			GetType() {return WIDGET_TYPE_IDENTIFY_FIELD;}

private:
	void					UpdateIdentity();

private:
	OpWidgetListener* m_listener;

};

#endif
