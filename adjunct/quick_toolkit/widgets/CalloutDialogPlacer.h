/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
 
 
#ifndef CALLOUT_DIALOG_PLACER
#define CALLOUT_DIALOG_PLACER
 
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"

/**
 * Positions a QuickOverlayDialog so that it looks like a callout from another
 * widget.
 */
class CalloutDialogPlacer : public QuickOverlayDialogPlacer
{
public:
	/** 
	 * Values of this enum define horizontal position of the dialog in relation to the anchor widget.
	 */
	enum Alignment
	{
		LEFT, ///< dialog will be left align to the anchor widget
		CENTER ///< dialog will be centered in relation to the anchor widget
	};

	/**
	 * @param anchor_widget the widget you want to show a callout from
	 */
	explicit CalloutDialogPlacer(OpWidget& anchor_widget, Alignment alignment = CENTER, const char* dialog_skin = NULL);

	virtual OpRect CalculatePlacement(const OpRect& bounds, const OpRect& dialog_size);

	virtual void PointArrow(OpWidgetImage& arrow_skin, const OpRect& placement);

	virtual BOOL UsesRootCoordinates() { return TRUE; }

private:
	OpWidget* m_anchor_widget;
	Alignment m_alignment;
	const char* m_dialog_skin;
};

#endif //CALLOUT_DIALOG_PLACER
