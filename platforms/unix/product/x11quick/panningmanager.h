/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef __PANNING_MANAGER_H__
#define __PANNING_MANAGER_H__


#include "modules/hardcore/timer/optimer.h"
#include "modules/inputmanager/inputaction.h"
#include "adjunct/quick_toolkit/widgets/OpPageListener.h"
#include "platforms/unix/base/x11/x11_widget.h"

class OpWindowCommander;

BOOL IsPanning();


class PanningManager : public OpTimerListener, public X11Widget::X11WidgetListener, public OpPageListener
{
private:
	enum Direction
	{
		MOVE_INIT,
		MOVE_NOWHERE,
		MOVE_UP,
		MOVE_DOWN,
		MOVE_LEFT,
		MOVE_RIGHT
	};


public:
	/**
	 * Creates the PanningManager instance
	 *
	 * @return OpStatus::OK on sucess, otherwise an error code
	 */
	static OP_STATUS Create();

	/**
	 * Destroys the PanningManager instance
	 */
	static void Destroy();

	/**
	 * Starts panning. The function will not block
	 *
	 * @param commander Window commander of the window that shall be panned
	 *
	 * @param page The page that called the panning code
	 *
	 * @return OpStatus::OK on sucess, otherwise an error code
	 */
	static OP_STATUS Start(OpWindowCommander* commander, OpPage* page);

	/**
	 * Scroll document
	 * 
	 * @param commander Window commander of the window that shall be panned
	 * @param action Can be ACTION_SCROLL_UP, ACTION_SCROLL_DOWN, ACTION_SCROLL_LEFT or ACTION_SCROLL_RIGHT
	 * @param lines Number of lines to scroll
	 *
	 * @return TRUE if scrolled, otherwise FALSE
	 */
	static BOOL Scroll(OpWindowCommander* commander, OpInputAction::Action action, int lines);

	/**
	 * Returns TRUE if panning is activated
	 */
	BOOL IsPanning() const { return m_active; }

	/**
	 * Inspects events and uses them if appropriate
	 *
	 * @return TRUE if event is consumed, otherwose FALSE
	 */
	BOOL HandleEvent(XEvent* event);

private:
	/**
	 * Creates object
	 */
	PanningManager();

	/**
	 * Removes object and deletes all allocated resources
	 */
	~PanningManager();

	/**
	 * Prepares panning. The function will not block. A timer will start
	 * and actual panning starts when it expires.
	 *
	 * @param doc_man Window commander of the window that shall be panned
	 * @param page The page that called the panning code
	 *
	 * @return OpStatus::OK on sucess, otherwise an error code. If opera is
	 *         already in panning mode OpStatus::OK is returned.
	 */
	OP_STATUS Setup(OpWindowCommander* commander, OpPage* page);

	/**
	 * Computes how must to scroll the next time based on the spesified position
	 * The position is tpyicallt the mouse position
	 *
	 * @param p The hotspot
	 */
	void ComputeStep(const OpPoint& p);

	/**
	 * Performs panning
	 */
	void Exec();

	/**
	 * Stops the panning
	 */
	void Stop();

	// OpTimerListener		
	void OnTimeOut(OpTimer* timer);
	
	// X11Widget::X11WidgetListener
	void OnDeleted(X11Widget* widget);

	// OpPageListener
	void OnPageClose(OpWindowCommander* commander);

private:	
	OpTimer* m_timer;
	BOOL m_active;
	OpWindowCommander* m_commander;
	OpPage* m_active_page;
	X11Widget* m_captured_widget;
	OpPoint m_anchor;
	int m_vertical_step;
	int m_horizontal_step;
	int m_num_on_edge;
	int m_num_extra_step;
};

extern PanningManager* g_panning_manager;

#endif
