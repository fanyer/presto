/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPDRAGOBJECT_H
#define OPDRAGOBJECT_H

#if defined DRAG_SUPPORT || defined USE_OP_CLIPBOARD

#include "modules/util/OpTypedObject.h"

class OpBitmap;
class OpFile;

#ifdef DRAG_SUPPORT
/** Enumeration of the available drop operation side effects. */
enum DropType
{
	/** The drop effect was not initialized. */
	DROP_UNINITIALIZED = 0x0,
	/** Dropping will cause that the data being dragged will be copied. */
	DROP_COPY          = 0x1,
	/** Dropping will cause that the data being dragged will be moved. */
	DROP_MOVE          = 0x2,
	/** Dropping will cause that a link to the data being dragged will be dropped (used when dragging a link). */
	DROP_LINK          = 0x4,
	/** A drop operation is not available - it will not have any effect. */
	DROP_NOT_AVAILABLE = 0x8,

	DROP_NONE = DROP_NOT_AVAILABLE
};

/** Core's private drag data. */
class OpCorePrivateDragData
{
private:
	friend class PrivateDragData;
	OpCorePrivateDragData() {}
public:
	virtual ~OpCorePrivateDragData() {}
};
#endif // DRAG_SUPPORT

/**
 * The dragged data iterator.
 *
 * Iterates through representations of the data being dragged.
 * The representation of the data consists of (the data's mime type, the data) pair.
 */
class OpDragDataIterator
{
public:
	virtual ~OpDragDataIterator() {}
	/**
	 * Moves to the first data.
	 *
	 * @return TRUE if it was possible to move to the first data. FALSE otherwise.
	 */
	virtual BOOL First() = 0;
	/**
	 * Moves to the next data.
	 *
	 * @return TRUE if it was possible to move to the next data. FALSE otherwise.
	 */
	virtual BOOL Next() = 0;

	/**
	 * Checks if the current data is a file.
	 *
	 * @return TRUE if the current data is a file.
	 */
	virtual BOOL IsFileData() const = 0;

	/**
	 * Checks if the current data is a string.
	 *
	 * @return TRUE if the current data is a string.
	 */
	virtual BOOL IsStringData() const = 0;

	/**
	 * Gets the current data as a file.
	 *
	 * @return a file if the current data is a file. NULL otherwise.
	 */
	virtual const OpFile* GetFileData() = 0;

	/**
	 * Gets the current data as a string.
	 *
	 * @return a string if the current data is a string. NULL otherwise.
	 */
	virtual const uni_char* GetStringData() = 0;

	/**
	 * Gets the mime type of the current data.
	 *
	 * @return the data's mime type.
	 */
	virtual const uni_char* GetMimeType() = 0;

	/** Removes the current data. */
	virtual void Remove() = 0;
};

/**
 * Basic d'n'd class. An instance of it stores the data being dragged.
 *
 * If Opera is the drag source the data is filled by core.
 * When the drag comes from some external application the platform must
 * fill the object with the data given by the external applciation.
 */
class OpDragObject : public OpTypedObject
{
#ifdef DRAG_SUPPORT
private:
	OpCorePrivateDragData* m_private_data;
#endif // DRAG_SUPPORT
public:

	/**
	 * Used by core for creating OpDragObject.
	 *
	 * @param object On success contains newly created OpDragObject.
	 * @param type The type of this object.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM, OpStatus::OK otherwise.
	 *
	 * @see OpTypedObject::Type
	 */
	static OP_STATUS Create(OpDragObject*& object, OpTypedObject::Type type);

	/**
	 * Returns the data iterator.
	 *
	 * @see OpDragDataIterator
	 */
	virtual OpDragDataIterator& GetDataIterator() = 0;

	/**
	 * Sets the drag'n'drop data of the given type and format.
	 *
	 * The data may be a string or a file.
	 *
	 * @param path_or_data contains string data if is_file == FALSE or a file path if is_file == TRUE.
	 * @param format contains data's mime type. If is_file == TRUE, format is a format of the file.
	 * @param is_file a flag indicating whether the data is a file or a string.
	 * @param remove_if_same_exists when TRUE, if data of the same type and format already exists,
	 * delete it prior to adding the new data.
	 *
	 * @return OpStatus::OK if all went OK, OpStatus::ERR_NO_MEMORY in case of OOM.
	 */
	virtual OP_STATUS SetData(const uni_char *path_or_data, const uni_char *format, BOOL is_file, BOOL remove_if_same_exists) = 0;

	/**
	 * Removes all the drag'n'drop data.
	 *
	 * @see SetData()
	 */
	virtual void ClearData() = 0;

	/**
	 * Returns the number of data items being dragged.
	 *
	 * @see SetData()
	 */
	virtual unsigned GetDataCount() = 0;

#ifdef DRAG_SUPPORT

	OpDragObject() : m_private_data(NULL) {}

	virtual ~OpDragObject() { OP_DELETE(m_private_data); }

	/**
	 * Sets the drag'n'drop visual feedback bitmap.
	 *
	 * The visual feedback bitmap should be shown
	 * by the pointing device's cursor during the drag'n'drop operation.
	 * This object becomes bitmap's owner and it's responsible for releasing it.
	 *
	 * @param bitmap The drag visual feedback bitmap.
	 */
	virtual void SetBitmap(OpBitmap* bitmap) = 0;

	/**
	 * Returns the drag'n'drop visual feedback bitmap or NULL if the bitmap has not been set.
	 *
	 * @see SetBitmap()
	 */
	virtual const OpBitmap* GetBitmap() const = 0;

	/**
	 * Sets the drag'n'drop visual feedback bitmap's point.
	 *
	 * The point is relative to the visual feedback bitmap's top, left corner
	 * and it indicates a place the pointing device's cursor should be kept in as the bitmap moves
	 * together with the cursor during the drag'n'drop operation.
	 *
	 * @param bitmap_point The bitmap's point.
	 */
	virtual void SetBitmapPoint(const OpPoint& bitmap_point) = 0;

	/**
	 * Gets a the visual feedback bitmap's point.
	 *
	 * @see SetBitmapPoint()
	 */
	virtual const OpPoint& GetBitmapPoint() const = 0;

	/**
	 * Sets the drag source.
	 *
	 * @param source The drag source.
	 *
	 * @see OpTypedObject
	 */
	virtual void SetSource(OpTypedObject* source) = 0;

	/**
	 * Returns the drag source or NULL if no source has been set.
	 *
	 * @see SetSource()
	 */
	virtual OpTypedObject* GetSource() const = 0;

	/**
	 * Sets a mask of drop effects (ORed DropType's values) allowed by a drag source
	 * to be performed with the data on drop.
	 *
	 * This function is called by core only when Opera is the drag source.
	 * The effects allowed must be passed to a drop target.
	 * Until core calls this method, when Opera is the drag source,
	 * the implementation should behave as if it had been called with DROP_UNINITIALIZED,
	 * which allows all drop types.
	 *
	 * @param effects The allowed effects mask.
	 *
	 * @see DropType
	 * @see SetDropType().
	 * @see GetDropType().
	 */
	virtual void SetEffectsAllowed(unsigned effects) = 0;

	/**
	 * Gets the drop effects allowed by the source of a drag.
	 *
	 * It must return:
	 * - value set with SetEffectsAllowed() if Opera is the drag source and
	 *  SetEffectsAllowed() was called by core.
	 * - DROP_UNINITIALIZED if Opera is the drag source and SetEffectsAllowed()
	 *   was not called by core.
	 * - a suitable value, when the drag source is an external application,
	 *   to reflect what it's acceptable to do with the dragged resource.
	 *
	 * @see SetEffectsAllowed()
	 */
	virtual unsigned GetEffectsAllowed() const = 0;

	/**
	 * Sets the current drop type.
	 *
	 * The drop type drives what will be done with data on drop.
	 * This function is called by core if Opera is the drop target application.
	 * This may be called very frequently while negotiating the drop effect with the current drop target
	 * causing the drop type value flickering. For visual purposes (e.g. changing the cursor)
	 * or during communication with an external drag source application the visual drop effect should be used.
	 *
	 * @note The platform does not need to check values passed to this function. Core assures
	 * that only a drop effect allowed by the effects allowed or DROP_NONE will be used on drop.
	 *
	 * @param drop_type The drop type to be set.
	 *
	 * @see DropType
	 * @see SetVisualDropType()
	 */
	virtual void SetDropType(DropType drop_type) = 0;

	/**
	 * Sets the visual drop type.
	 *
	 * This function is called by core if Opera is the drop target application.
	 * The visual drop type should be reflected in the cursor type or
	 * passed to an external source application.
	 * It is set by core when the drop effect has been negotiated and is stable
	 * for the current drop target.
	 *
	 * @note The platform does not need to check values passed to this function. Core assures
	 * that only a drop effect allowed by the effects allowed or DROP_NONE will be passed to this function.
	 *
	 * @param drop_type The visual drop type.
	 *
	 * @see DropType
	 * @see GetEffectsAllowed()
	 */
	virtual void SetVisualDropType(DropType drop_type) = 0;

	/**
	 * Gets the visual drop type.
	 *
	 * It must return DROP_NONE if no visual drop type has been set.
	 *
	 * @see SetVisualDropType()
	 */
	virtual DropType GetVisualDropType() const = 0;

	/**
	 * Gets the current drop type.
	 *
	 * It must return DROP_NONE if no drop type has been set.
	 *
	 * @see SetDropType()
	 */
	virtual DropType GetDropType() const = 0;

	/**
	 * Gets the suggested drop type.
	 *
	 * The implementation may, for various reasons relating to external sources of drags
	 * and system idioms, make use of modifier keys. If a modifier key should change the drop type,
	 * and the implementation wants to recommend a new drop type to be used by core,
	 * GetSuggestedDropType() returns it.
	 *
	 * It must return DROP_UNINITIALIZED if the suggested drop type is not meant to be used.
	 */
	virtual DropType GetSuggestedDropType() const = 0;

	/**
	 * Controls whether the drag data may be given to external applications.
	 *
	 * When protected mode is enabled, the drag object must not give its data out to external applications.
	 * Otherwise, the data can be delivered wherever the user drags it; this is the default mode.
	 *
	 * @param protected_mode Restrict where data may be delivered precisely if this is TRUE.
	 */
	virtual void SetProtectedMode(BOOL protected_mode) = 0;

	/**
	 * Reports whether delivery of this object's data is restricted to Opera.
	 *
	 * @see SetProtectedMode()
	 */
	virtual BOOL IsInProtectedMode() = 0;

	/**
	 * Controls whether this object may be modified.
	 *
	 * By default, platform code can modify the drag data freely; core will call this method to mark the data
	 * as read-only if the platform should not do so.
	 *
	 * @param read_only If TRUE, the platform is forbidden to modify the drop data.
	 */
	virtual void SetReadOnly(BOOL read_only) = 0;

	/**
	 * Reports whether the platform is forbidden to modify the drag data.
	 *
	 * @see SetReadOnly()
	 */
	virtual BOOL IsReadOnly() = 0;

	/**
	 * Sets core's private data.
	 *
	 * This function may only be called by core.
	 * This object becomes the owner of the data.
	 *
	 * @param private_data The private data to be set in this object.
	 *
	 * @see OpCorePrivateDragData
	 */
	void SetPrivateData(OpCorePrivateDragData* private_data) { m_private_data = private_data; }

	/**
	 * Gets core's private drag data.
	 *
	 * @see SetPrivateData().
	 */
	OpCorePrivateDragData* GetPrivateData() { return m_private_data; }
#endif // DRAG_SUPPORT
};
#endif // DRAG_SUPPORT || USE_OP_CLIPBOARD
#endif // OPDRAGOBJECT_H

