/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef X11_TYPES_H
#define X11_TYPES_H

/** This file contains forward declarations of global X11 types.
 *
 * Include x11_all.h to get full definitions.
 */

typedef struct _XDisplay X__Display;
typedef struct _XRegion* X__Region;
typedef union  _XEvent	 XEvent;
typedef struct _XSyncValue X__SyncValue;


namespace X11Types
{
	// Base types
	typedef unsigned long XID;
	typedef unsigned long Atom;
	typedef unsigned long Time;
	typedef unsigned long VisualID;
	typedef int			  Status;
	typedef int			  Bool;

	// XID types
	typedef XID			  Window;
	typedef XID			  Picture;
	typedef XID			  Pixmap;
	typedef XID			  Drawable;
	typedef XID			  Colormap;
	typedef XID			  Cursor;
	typedef XID			  XSyncCounter;

	// Complex types
	typedef X__Display	  Display;
	typedef X__Region	  Region;
	typedef X__SyncValue  XSyncValue;
	class 				  VisualPtr;
	struct				  XShmSegmentInfoHolder;
};

#endif // X11_TYPES_H
