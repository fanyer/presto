/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_WINDOW_MOVER
#define OP_WINDOW_MOVER

#include "modules/widgets/OpButton.h"

/**
 * Widget for moving a window with the mouse
 *
 * @author Emil Segeras (emil@opera.com)
 * @author Wojciech Dzierzanowski (wdzierzanowski@opera.com)
 */
class OpWindowMover : public OpButton
{
protected:
	OpWindowMover();

public:
	static OP_STATUS Construct(OpWindowMover** obj);

	void SetTargetWindow(DesktopWindow& window) { m_window = &window; }

	// OpWidget
	virtual void OnMouseMove(const OpPoint& point);
	virtual void OnMouseDown(const OpPoint& point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseUp(const OpPoint& point, MouseButton button, UINT8 nclicks);

private:
	DesktopWindow* m_window;
	OpPoint m_down_point;
	bool m_moving;
};
#endif // OP_WINDOW_MOVER
