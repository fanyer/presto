/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Owner: julienp
 */

#ifdef FEATURE_SCROLL_MARKER

//The following define how the scroll marker should appear
//Currently, sortof bullets defined by the skin are used to mark it.
//The other solution (cover the part of the screen scrolled from with a transparent layer) is too slow on Unix
//but I keep it as an option for the future.

#define OP_SCROLL_MARKER_SCROLL_TIME 750
#define OP_SCROLL_MARKER_INVISIBLE_SCROLL_TIME 100
#define OP_SCROLL_MARKER_PERSIST_TIME 1250

//8080a0

#include "modules/pi/OpTimer.h"
#include "modules/display/vis_dev.h"

class OpPainter;
class OpRect;

class OpScrollMarker : public OpTimerListener, public VisualDeviceScrollListener, public OpSkinAnimationListener
{
public:

	static OP_STATUS Create(OpScrollMarker** scroll_marker, VisualDevice* vis_dev);

	~OpScrollMarker();

	//Should be called by whatever the ScrollMarker should be painted on.
	void OnPaint(OpPainter* painter, OpRect rect);

	/*-------- OpTimerListener interface ---------- */

	virtual void OnTimeOut(OpTimer* timer);


	/*---- VisualDeviceScrollListener interface --- */

	virtual void OnDocumentScroll(VisualDevice *vis_dev,INT32 dx, INT32 dy, SCROLL_REASON reason);


	/*----- OpSkinAnimationListener interface ----- */

	virtual void OnAnimatedImageChanged();

private:

	VisualDevice* m_vis_dev;

	OpTimer* m_timer;

	BOOL m_scroll_in_progress;
	BOOL m_visible;
	INT32 m_direction;

	OpRect m_rect;

	OpScrollMarker(VisualDevice* vis_dev);

	void Show(BOOL visible);
	void Invalidate();
};

#endif // FEATURE_SCROLL_MARKER

