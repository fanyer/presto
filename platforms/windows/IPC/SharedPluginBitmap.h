/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_SHARED_PLUGIN_BITMAP_H
#define WINDOWS_SHARED_PLUGIN_BITMAP_H

#include "modules/pi/OpPluginImage.h"

#include "platforms/windows/windows_ui/hash_tables.h"

class WindowsSharedBitmap;

class WindowsSharedBitmapManager
{
public:
	~WindowsSharedBitmapManager()
	{
		m_shared_bitmaps.DeleteAll();
	}

	static WindowsSharedBitmapManager*
							GetInstance();

	/** Creates and returns an instance of shared memory. */
	WindowsSharedBitmap*	CreateMemory(OpPluginImageID identifier, int width, int height);

	/** Returns an instance of shared memory.. */
	WindowsSharedBitmap*	OpenMemory(OpPluginImageID identifier);

	/** Closes and destroys an instance of shared memory. */
	void					CloseMemory(OpPluginImageID identifier);

private:
	OpWindowsPointerHashTable<OpPluginImageID, WindowsSharedBitmap*>
							m_shared_bitmaps;
	static OpAutoPtr<WindowsSharedBitmapManager>
							s_shared_bitmap_manager;
};

class WindowsSharedBitmap
{
public:
	~WindowsSharedBitmap();

	/** Returns pointer to the bitmap data. */
	UINT32*					GetDataPtr() const { return &m_shared_mem->body_start; }

	/** Returns offset of the bitmap data.
	 *
	 * When creating bitmap using CreateDIBSection, it gets passed a file
	 * mapping handle of the shared memory. For it to not overwrite header
	 * data in the shared memory, it requires an offset where bitmap data
	 * should be stored.
	 */
	DWORD					GetDataOffset() const { return offsetof(SharedBitmap, body_start); }

	/** Returns file-mapping handle of the shared memory. */
	HANDLE					GetFileMappingHandle() const { return m_mapping_handle; }

	/** Returns dimensions of the bitmap that shared memory was allocated for. */
	void					GetDimensions(int& width, int& height) const { width = m_shared_mem->width; height = m_shared_mem->height; }

	/** Resize shared memory to accommodate bitmap of given width and height.
	 *
	 * Will release currently mapped memory and handles to it and wait for
	 * other users of this shared memory to close the memory. When they are
	 * done, the memory with new size will be mapped into memory. Remote
	 * users will have to re-open memory using same identifier if they want
	 * to access it later.
	 */
	OP_STATUS				ResizeMemory(int width, int height);

private:
	friend class WindowsSharedBitmapManager;

	WindowsSharedBitmap();

	/** Callback invoked when m_close_wait_handle is signaled.
	 *
	 * The purpose of the callback is to close this instance of the shared memory
	 * when the owner of the memory is attempting to re-create the memory.
	 */
	static VOID CALLBACK	CloseCallback(PVOID identifier, BOOLEAN timeout);

	/** Initialize instance. */
	OP_STATUS				Init(OpPluginImageID identifier, bool owner);

	/** Called by the owner to create shared memory. */
	OP_STATUS				ConstructMemory(int width, int height);

	/** Used by remote users to open already created shared memory. */
	OP_STATUS				OpenMemory();

	struct SharedBitmap
	{
		volatile LONG		opened;
		UINT32				width;
		UINT32				height;

		union
		{
			HANDLE			close_wait_event;		///< set to signaled to notify that remote user should close memory
			UINT64			padding;
		};
		union
		{
			HANDLE			opened_remotely_event;	///< set to signaled after remote user closes memory
			UINT64			padding;
		};
		DWORD				owner_pid;

		UINT32				body_start;				///< first color in the shared memory, memory extends past that
	};

	OpPluginImageID			m_identifier;			///< unique identifier of this instance
	OpAutoArray<uni_char>	m_memory_name;			///< name of the named shared memory

	HANDLE					m_close_wait_handle;	///< when signaled, the CloseCallback will fire and close this memory
	HANDLE					m_close_callback_handle;

	HANDLE					m_opened_remotely_handle;///< used by owner to wait for remote users to close the memory

	void*					m_mapped_memory;		///< raw pointer to the shared memory
	HANDLE					m_mapping_handle;		///< handle to the file mapping object
	SharedBitmap*			m_shared_mem;			///< parsed representation of the mapped memory

	bool					m_owner;
	bool					m_opened;
};

#endif WINDOWS_SHARED_PLUGIN_BITMAP_H
