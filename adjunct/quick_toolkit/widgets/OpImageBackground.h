/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_IMGAE_BACKGROUND_H
#define OP_IMGAE_BACKGROUND_H

#include "adjunct/quick_toolkit/widgets/OpMouseEventProxy.h"

/**
 * Paints an image using one of the available layout algorithms.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class OpImageBackground : public OpMouseEventProxy
{
public:
	enum Layout
	{
		BEST_FIT,
		CENTER,
		STRETCH,
		TILE
	};

	OpImageBackground() : m_layout(BEST_FIT) {}

	void SetImage(Image image) { m_image = image; }
	void SetLayout(Layout layout) { m_layout = layout; }
	bool HasBackground() const { return !m_image.IsEmpty(); }

	// Override OpWidget
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);

private:
	Image m_image;
	Layout m_layout;
};

#endif // OP_IMGAE_BACKGROUND_H
