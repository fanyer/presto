/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef CARET_PAINTER_H
#define CARET_PAINTER_H

#if defined(KEYBOARD_SELECTION_SUPPORT) || defined(DOCUMENT_EDIT_SUPPORT)

#include "modules/hardcore/timer/optimer.h"
#include "modules/display/vis_dev_transform.h"
#ifdef DRAG_SUPPORT
# include "modules/logdoc/selectionpoint.h"
#endif // DRAG_SUPPORT

class CaretManager;

/**
 * This class is responsible for rendering a caret at an appropriate
 * position in the document and then blink it if needed.
 */
class CaretPainter : public OpTimerListener
{
public:
	CaretPainter(FramesDocument *frames_doc);
	~CaretPainter() { OP_DELETE(timer); }

	/**
	 * Puts the caret in the "not visible" state and puts it at (0, 0). Will not stop the timer. 
	 */
	void Reset() { on = FALSE; x = paint_x = wanted_x = y = height = line_y = line_height = 0; }

	/** Makes the caret "blink" (toggle from visible->not visible or
		the other way around...) */
	void BlinkNow();

	/** Restart the caret "blinking" and starts with the caret
		visible, looks nice when changing the caret position... */
	void RestartBlink();

	/** Makes the caret invisible, stops blinking and entering the
		invisible state */
	void StopBlink();

	/** Callback for managing the "blinking" */
	virtual void OnTimeOut(OpTimer* timer);

	/** Calls VisualDevice::Update with the rectangle where the caret
		should be drawn */
	void Invalidate();

	/**
	 * Paints the caret on the supplied vis_dev if there is a caret to paint.
	 */
	void Paint(VisualDevice* vis_dev);

	/**
	 * Lock or unlock the possibility for the caret to update its
	 * drawing position. Can be used to optimize, by avoiding to
	 * update it several times during a operation.
	 * @parm lock TRUE to lock and FALSE to unlock (that is, update
	 * caret-pos if we're not inside "nestled" LockUpdatePos calls).
	 * @parm process_update_pos If FALSE will the caret NOT be updated
	 * even though the "nestling-count" reaches zero when lock ==
	 * FALSE.
	 */
	void LockUpdatePos(BOOL lock, BOOL process_update_pos = TRUE);

	/**
	 * Update the drawing position. Returns TRUE if the update is
	 * done. Returns FALSE if it is postponed. F.ex if it was locked
	 * or if the document was dirty.  This is done automatically from
	 * the Place functions.
	 * @returns TRUE if the caret's position was successfully updated.
	 */
	BOOL UpdatePos();

	/**
	 * Get position of caret in document coordinates.
	 */
	OpPoint GetCaretPosInDocument() const;

	/**
	 * Get position and size of the caret in document coordinates.
	 */
	OpRect GetCaretRectInDocument() const;

	int GetY() { return y; }
	int GetX() { return x; }
	unsigned GetHeight() { return height; }
	unsigned GetWidth() { return width; }
	int GetLineY() { return line_y; }
	unsigned GetLineHeight() { return line_height; }
	void SetY(int new_y) { y = new_y; }
	void SetX(int new_x) { x = new_x; }
	void SetHeight(unsigned new_height) { height = new_height; }
	void SetWidth(unsigned new_width) { width = new_width; }
	void SetLineY(int new_line_y) { line_y = new_line_y; }
	void SetLineHeight(unsigned new_line_height) { line_height = new_line_height; }
	void SetPaintX(int new_paint_x) { paint_x = new_paint_x; }

	int GetWantedX() { return wanted_x; }
	void SetWantedX(int new_wanted_x) { wanted_x = new_wanted_x; }

	/** Remember current x-position as the wanted x-position when
		moving caret from line to line. */
	void UpdateWantedX();

	/** When set to TRUE, caret will behave like it's not between
		character, but as it selects the entire character */
	void SetOverstrike(BOOL val);
	BOOL GetOverstrike() { return overstrike; }

#ifdef DRAG_SUPPORT
	/** Turns on/off the caret shown during d'n'd. */
	void DragCaret(BOOL val);
	/** Returns BOOL indicating if the d'n'd caret is turned on or off. */
	BOOL IsDragCaret() { return drag_caret; }
	/** Sets the point the d'n'd caret should be showned at. */
	void SetDragCaretPoint(SelectionBoundaryPoint& point) { drag_caret_point = point; }
	/** Gets the current location of the d'n'd caret. */
	SelectionBoundaryPoint& GetDragCaretPoint() { return drag_caret_point; }
#endif // DRAG_SUPPORT

	/** Scroll to ensure caret is visible. */
	void			ScrollIfNeeded();

private:
	 /**
	 * Returns helm and ofs "snapped" to values so that traversal with SelectionUpdateObject hopefully will find.
	 * @return TRUE if such position was found.
	 * @parm helm The element to start the scan from.
	 * @parm ofs The offset to start the scan from.
	 * @parm new_helm The element that was found, if it was found (might be helm itself).
	 * @parm new_ofs The offset that was found, if it was found.
	 */
	BOOL GetTraversalPos(HTML_Element *helm, int ofs, HTML_Element *&new_helm, int &new_ofs);

	/** Timer */
	OpTimer *timer;

	/** Whether the caret should be painted visible or not visible while blinking. */
	BOOL on;

	/** A pointer to the FramesDocument with the CaretPainter in it. */
	FramesDocument *frames_doc;

	// Various indicators for where the caret should be painted.
	int x, y, paint_x, wanted_x, line_y;
	unsigned width, height, line_height;
	AffinePos transform_root;

	/**
	 * Counter on the number of current locks on the "UpdatePos" function.
	 * If UpdatePos is called while this is != 0 then it will be called
	 *when the lock counter reaches zero.
	 */
	unsigned update_pos_lock;

	/** Whether there is an UpdatePos pending. */
	BOOL update_pos_needed;

	/** Pending UpdateWantedX() call */
	BOOL update_wanted_x_needed;

	/** TRUE if the caret should be painted in a "overwrite" (as opposed to the default "insert" mode). */
	BOOL overstrike;

#ifdef DRAG_SUPPORT
	/** Indicates whether a drag caret (a caret shown during d'n'd) should be shown. */
	BOOL drag_caret;
	/** A point the drag caret should be shown at */
	SelectionBoundaryPoint drag_caret_point;
#endif // DRAG_SUPPORT
};

#endif // KEYBOARD_SELECTION_SUPPORT || DOCUMENT_EDIT_SUPPORT
#endif // CARET_PAINTER_H
