/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPDRAGMANAGER_H
#define OPDRAGMANAGER_H

class OpDragListener;
class OpDragObject;

/**
 * The drag'n'drop operation manager class. It's a singleton stored in core's drag manager.
 * Core creates and destroys it.
 *
 * It's responsible for driving the drag'n'drop operation also when the dragged data comes from
 * other applications or the drag goes outside Opera so it has to handle platform specific drag'n'drop
 * events. When platform specific drag'n'drop operations are detected OpDragManager is responsible for
 * reaching the OpView in which the operation was performed and calling its OpDragListener's methods accordingly.
 *
 * @note When the drag goes outside Opera, OpDragManager - before exposing any data to an external application - must check
 * if this is allowed by calling OpDragObject::IsInProtectedMode(). If TRUE is returned any external application must not
 * by given the data.
 *
 * @see OpDragObject::IsInProtectedMode()
 * @see OpView::GetDragListener()
 * @see OpDragListener
 */
class OpDragManager
{
public:
	virtual ~OpDragManager() {};

	/** Called by core to create the OpDragManager.
	 *
	 * @param manager - a placeholder for a newly created OpDragManager.
	 *
	 * @return OpStatus::OK on successful OpDragManager creation.
	 * In case of OOM OpStatus::ERR_NO_MEMORY is returned.
	 */
	static OP_STATUS Create(OpDragManager*& manager);

	/** Core's response to OpDragListener::OnDragStart().
	 *
	 * If the drag'n'drop operation is valid, core supplies data about the drag via SetDragObject()
	 * and then calls this method to let the drag manager know that the operation has really started.
	 * The drag manager must subsequently report progress of the drag'n'drop via methods of OpDragListener.
	 * Aside from OnDragStart(), the drag manager should call no methods of OpDragListener until after
	 * core has called this method.
	 * If a drag'n'drop operation gets cancelled or dropped between the drag manager's call to OnDragStart()
	 * and core's call to StartDrag(), the drag manager must remember this and call OpDragListener's OnDragDrop()
	 * or OnDragCancel() after core calls StartDrag(). The drag move position needs to be buffered till StartDrag()
	 * is called.
	 *
	 * @see OpDragListener::OnDragStart()
	 * @see SetDragObject()
	 */
	virtual void StartDrag() = 0;

	/** Signals the end of a drag'n'drop operation.
	 *
	 * When core calls this, the drag manager can delete its current OpDragObject; subsequently, it should stop
	 * calling OpDragListener's methods. This also might be called as a response to OpDragListener::OnDragStart().
	 * Before this is called for the current drag'n'drop operation a platform should not accept nor start a new
	 * drag'n'drop operation.
	 *
	 * @param cancelled - TRUE if the drag was cancelled.
	 *
	 * @see OpDragListener::OnDragStart()
	 */
	virtual void StopDrag(BOOL cancelled) = 0;

	/** Indicates whether a drag'n'drop operation is in progress.
	 *
	 * Returns BOOL value indicating whether the drag'n'drop operation is in progress:
	 * must return FALSE before SetDragObject(object) is called and after StopDrag()/SetDragObject(NULL) was called.
	 * In other cases it must return TRUE.
	 *
	 * @see StopDrag()
	 * @see SetDragObject()
	 */
	virtual BOOL IsDragging() = 0;

	/** Report data describing the current drag.
	 *
	 * Returns the OpDragObject currently owned by this OpDragManager (set by SetDragObject()
	 * or constructed to represent an external drag). It must return NULL before SetDragObject(),
	 * or after StopDrag().
	 *
	 * @see SetDragObject()
	 */
	virtual OpDragObject* GetDragObject() = 0;

	/** Supplies data describing the current drag data.
	 *
	 * Called before StartDrag() to specify what is being dragged and possibly during a drag to update that data.
	 * The drag manager takes ownership of the OpDragObject passed in. An update may pass the same object.
	 * It may also pass a new object or NULL, in which case the drag manager
	 * is responsible for deleting the earlier object.
	 *
	 * @note When an external drag'n'drop operation moves over Opera, the drag manager is responsible for creating and setting its own OpDragObject.
	 *
	 * @see OpDragObject
	 */
	virtual void SetDragObject(OpDragObject* drag_object) = 0;
};

#endif // OPDRAGMANAGER_H
