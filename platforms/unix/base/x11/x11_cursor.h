/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef __X11_CURSOR_H__
#define __X11_CURSOR_H__

#include "platforms/utilix/x11types.h"
#include "modules/display/cursor.h"
#include "platforms/unix/base/x11/x11_settingsclient.h"

/*
 * We have X11Cursor, X11CursorItem and XcursorLoader in this file
 * X11Cursor is the interface to rest of Opera
 * X11CursorItem is an internal class that loads and maintains the shapes
 * XcursorLoader is an internal class and interface to the Xcursor library
 */

class XcursorLoader
{
private:
	typedef X11Types::Cursor (*XcursorLibraryLoadCursor_t)(X11Types::Display* dpy, const char* name);
	typedef X11Types::Bool (*XcursorSetTheme_t)(X11Types::Display* dpy, const char* theme);
	typedef X11Types::Bool (*XcursorSetDefaultSize_t)(X11Types::Display* dpy, int size);

public:
	/**
	 * Initializes the object
	 */
	XcursorLoader();

	/**
	 * Loads the Xcursor library
	 *
	 * @return OpStatus:OK on success, otherwise an error code
	 */
	OP_STATUS Init();

	/**
	 * Loads a cursor shape via Xcursor library
	 *
	 * @param shape The cursor shape
	 *
	 * @return The cursor handle or None if shape was not supported 
	 *         or library not loaded
	 */
	X11Types::Cursor Get( int shape );

	/**
	 * Update cursor set with new name and default size
	 *
	 * @param name The new theme name. Argument ignored if empty
	 * @parem size The new theme size. Argument ignored if negative or zero
	 */
	void SetTheme(const OpString8& name, INT32 size);

private:
	class OpDLL* m_dll;
	XcursorLibraryLoadCursor_t XcursorLibraryLoadCursor;
	XcursorSetTheme_t XcursorSetTheme;
	XcursorSetDefaultSize_t XcursorSetDefaultSize;
};


class X11CursorItem
{
public:
	X11CursorItem();
	~X11CursorItem();

	/**
	 * Creates a cursor handle using the shape and mask. The data layout of the shape and mask
	 * is assued to be as follows (See XCreateBitmapFromData, XLib Ref Manual Vol 2):
	 * Format:      XYPixmap
	 * bit_order:   LSBFirst
	 * byte_order:  LSBFirst
	 * bitmap_unit: 8
	 * bitmap_pad:  8
	 * xoffset:     0
	 * no extra bytes per line
	 *
	 * @param shape Bitmap describing the shape
	 * @param mask Bitmap describing the mask. Extends typically the 
	 *        shape with one pixel in all directions
	 * @param width Width of mask and shape in pixels
	 * @param height Height of mask and shape in pixels
	 * @param value Rotate shape and mask Only 90, 180 and 270 are supported
	 *
	 * @return OpStatus:OK on success, otherwise an error code
	 */
	OP_STATUS Set( const unsigned char* shape, const unsigned char* mask, unsigned int width, unsigned int height, int rotate);

	/**
	 * Rotate the data buffer with value. See @ref Set() for data format. NOTE: Data is assumed 
	 * to be located in an array of  width*height bytes for simplicity.
	 *
	 * @param buffer Data to be rotated
	 * @param width Buffer width in pixels. Will be changed on exit if totate value is 90 or 270
	 * @param height Buffer height in pixesl. Will be changed on exit if totate value is 90 or 270
	 * @param value Rotate value. Only 90, 180 and 270 are supported
	 *
	 * @return OpStatus:OK on success, otherwise an error code
	 */
	OP_STATUS Rotate(unsigned char* data, unsigned int& width, unsigned int& height, int value);

	/**
	 * Flips the buffer vertically. See @ref Set() for data format. 
	 *
	 * @param buffer Data to be rotated
	 * @param width Buffer width in pixels
	 * @param height Buffer height in pixels
	 *
	 * @return OpStatus:OK on success, otherwise an error code
	 */
	OP_STATUS Flip(unsigned char* data, unsigned int width, unsigned int height);

public:
	X11Types::Cursor m_cursor;

private:
	X11Types::Pixmap m_shape;
	X11Types::Pixmap m_mask;
};


class X11Cursor : public X11SettingsClient::Listener
{
public:

	// This enum "appends" itself on the core CursorType enum
	enum X11CursorShape
	{
		PANNING_IDLE = CURSOR_NUM_CURSORS+1,
		PANNING_UP,
		PANNING_DOWN,
		PANNING_LEFT,
		PANNING_RIGHT,
		PANNING_UP_RIGHT,
		PANNING_DOWN_RIGHT,
		PANNING_DOWN_LEFT,
		PANNING_UP_LEFT,
		X11_CURSOR_MAX // Always the last
	};

public:
	/**
	 * Return a pointer to the X11Cursor instance (only one can exists)
	 */
	static X11Cursor* Self();

	/**
	 * Create the X11Cursor instance (only one can exists)
	 *
	 * @return OpStatus::OK on sucess, otherwise an error code
	 */
	static OP_STATUS Create();

	/**
	 * Destroy the existing X11Cursor instance
	 */
	static void Destroy();

	/**
	 * Return the cursor handle for the given shape
	 *
	 * @param shape The cursor shape can be any of CursorType (core) or X11CursorShape (x11 specific)
	 *
	 * @return The cursor handle or None if the shape could not be loaded
	 */
	static X11Types::Cursor Get(unsigned int shape);

private:
	X11Cursor();
	~X11Cursor();

	/**
	 * Initialize the X11Cursor instance
	 *
	 * @return OpStatus::OK on success, otherwise an error code describing the problem 
	 */
	OP_STATUS Init();

	/**
	 * Return the cursor handle for the given shape
	 *
	 * @param shape The cursor shape can be any of CursorType (core) or X11CursorShape (x11 specific)
	 *
	 * @return The cursor handle or None if the shape could not be loaded
	 */
	X11Types::Cursor GetHandle(unsigned int shape);

	/**
	 * Return the fallback cursor handle for the given shape
	 *
	 * @param shape The cursor shape
	 *
	 * @return The cursor handle or None if the shape could not be loaded
	 */
	X11Types::Cursor GetFallbackHandle(unsigned int shape);

	/* X11SettingsClient::Listener */
	virtual void OnResourceUpdated(INT32 id, INT32 cmd, UINT32 value);
	virtual void OnResourceUpdated(INT32 id, INT32 cmd, const OpString8& value);
	virtual void OnResourceUpdated(INT32 id, INT32 cmd, const X11SettingsClient::Color& value) {}

private:
	X11CursorItem* m_items;
	XcursorLoader* m_xcursor_loader;
private:
	static X11Cursor* m_cursor;
};

#endif
