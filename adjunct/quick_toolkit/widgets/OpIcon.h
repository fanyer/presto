/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * Author: Adam Minchinton
 */

#ifndef OP_ICON_H
#define OP_ICON_H

#include "modules/widgets/OpWidget.h"

/***********************************************************************************
**
**	OpIcon
**
**	Used to display icons on Dialogs and not have the distortions of scaling
**	effects 
**
**  Note: Normally it has distortions of scaling effects if you don't give it the
**  exact same size as the skin.
**  But if you set the foreground skin, it will be centered in the OpIcon nonscaled.
**
***********************************************************************************/

class OpIcon : public OpWidget
{
public:
	static OP_STATUS	Construct(OpIcon** obj);

protected:
	OpIcon();
	~OpIcon();

public:
	virtual Type		GetType() {return WIDGET_TYPE_ICON;}

	void				SetImage(const char* image_name, BOOL foreground = FALSE);
	void				SetBitmapImage(Image& image, BOOL foreground = FALSE);
	void				SetAllowScaling() { m_allow_scaling = true; }

	virtual void		GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	virtual void		OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
private:

	bool m_allow_scaling;
};

#endif // OP_ICON_H
