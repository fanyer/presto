/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef X11_ALL_H
#define X11_ALL_H

// Include forward-declarable types
#include "platforms/utilix/x11types.h"

// Define this to never import global X11 types
#ifndef X11_CLEAN_NAMESPACE

/***********************************************************************************
 ** Redefine global X11 types to prevent collisions
 **
 ** Add more types here when you need them, add to X11Types and Undef global X11
 ** Types as well!
 ***********************************************************************************/
#define Window	 X__Window
#define Display  X__Display
#define Region	 X__Region
#define Atom	 X__Atom
#define Picture	 X__Picture
#define Pixmap	 X__Pixmap
#define Drawable X__Drawable
#define Colormap X__Colormap
#define VisualID X__VisualID
#define Time	 X__Time
#define Visual	 X__Visual
#define Cursor	 X__Cursor
#define XSyncCounter X__XSyncCounter
#define XSyncValue   X__SyncValue
#define Bool	 X__Bool


/***********************************************************************************
 ** Actual X11 includes
 ***********************************************************************************/
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/sync.h>
#ifdef _UNIX_DESKTOP_
#include "X11/extensions/Xinerama.h"
#include <X11/extensions/Xrandr.h>
#endif
#include <X11/extensions/Xrender.h>
#include <X11/extensions/XShm.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/Xresource.h>
#include <X11/SM/SMlib.h>
#include <X11/XKBlib.h>

/***********************************************************************************
 ** Undef global X11 types
 ***********************************************************************************/
#undef Window
#undef Status
#undef Display
#undef Region
#undef Atom
#undef Picture
#undef Pixmap
#undef Drawable
#undef Colormap
#undef VisualID
#undef Time
#undef Visual
#undef Cursor
#undef XSyncCounter
#undef XSyncValue
#undef Bool


/***********************************************************************************
 ** X11Constants
 **
 ** Add any X11 constants you use here, and add them to Undef X11 constants
 ** and Global X11 constants
 ***********************************************************************************/
namespace X11Constants
{
	const long XCurrentTime		= CurrentTime;
	const int  XRectangleOut	= RectangleOut;
	const int  XRectangleIn		= RectangleIn;
	const int  XRectanglePart	= RectanglePart;
	const int  XCursorShape		= CursorShape;
	const unsigned long XNone	= None;
	const int  XKeyPress		= KeyPress;
	const int  XKeyRelease		= KeyRelease;
	const int  XFocusIn			= FocusIn;
	const int  XFocusOut		= FocusOut;
	const int  XFontChange		= FontChange;
	const int  XSuccess		= Success;
	const int  XBadRequest      = BadRequest;
};


/***********************************************************************************
 ** Undef X11 constants
 ***********************************************************************************/
#undef CurrentTime
#undef RectangleOut
#undef RectangleIn
#undef RectanglePart
#undef CursorShape
#undef None
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef Success
#undef BadRequest

/***********************************************************************************
 ** Global X11 constants we want
 ***********************************************************************************/
const long CurrentTime		= X11Constants::XCurrentTime;
const int  RectangleOut		= X11Constants::XRectangleOut;
const int  RectangleIn		= X11Constants::XRectangleIn;
const int  RectanglePart	= X11Constants::XRectanglePart;
const int  CursorShape		= X11Constants::XCursorShape;
const unsigned long None	= X11Constants::XNone;
const int  KeyPress			= X11Constants::XKeyPress;
const int  KeyRelease		= X11Constants::XKeyRelease;
const int  FocusIn			= X11Constants::XFocusIn;
const int  FocusOut			= X11Constants::XFocusOut;
const int  FontChange		= X11Constants::XFontChange;
const int  Success		= X11Constants::XSuccess;
const int  BadRequest   = X11Constants::XBadRequest;

/***********************************************************************************
 ** Additions to X11Types
 ***********************************************************************************/
namespace X11Types
{
	// This one can't be forward-declared
	typedef X__Visual Visual;

	typedef X__Atom Atom;

	/** This class exists only because "Visual" itself can not be
	 * forward-declared (at least not in a simple, sensible way).  It
	 * acts as a forward-declarable, simple wrapper for code that does
	 * not want to include the whole of this header file.
	 */
	class VisualPtr
	{
		public:
			VisualPtr(Visual* ptr)			   : m_ptr(ptr) {}
			VisualPtr(const VisualPtr& holder) : m_ptr(holder.m_ptr) {}
			VisualPtr()						   {}
			operator Visual*() const		   { return m_ptr; }

		private:
			Visual* m_ptr;
	};

	struct XShmSegmentInfoHolder
	{
		XShmSegmentInfoHolder(XShmSegmentInfo& p_info) : info(p_info) {}
		XShmSegmentInfo& info;
	};
};

/***********************************************************************************
 ** Undef X11 macros that conflict with Opera sources
 ***********************************************************************************/

#undef IsFunctionKey /* conflicts with modules/hardcore/keys/opkeys.h */


#endif // !X11_CLEAN_NAMESPACE

#endif // X11_INCLUDE_H
