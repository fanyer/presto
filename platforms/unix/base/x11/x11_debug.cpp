/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef _DEBUG

#include "platforms/unix/base/x11/x11_debug.h"

#include "platforms/utilix/x11_all.h"

void X11Debug::DumpEvent(XEvent* event)
{
	switch(event->type)
	{
	case KeyPress:
		printf("KeyPress");
		break;
	case KeyRelease:
		printf("KeyRelease");
		break;
	case ButtonPress:
		printf("ButtonPress"); 
		break;
	case ButtonRelease:
		printf("ButtonRelease"); 
		break;
	case MotionNotify:
		printf("MotionNotify"); 
		break;
	case EnterNotify:
		printf("EnterNotify"); 
		break;
	case LeaveNotify:
		printf("LeaveNotify"); 
		break;
	case FocusIn:
		printf("FocusIn"); 
		break;
	case FocusOut:
		printf("FocusOut"); 
		break;
	case KeymapNotify:
		printf("KeymapNotify"); 
		break;
	case Expose:
		printf("Expose"); 
		break;
	case GraphicsExpose:
		printf("GraphicsExpose"); 
		break;
	case NoExpose:
		printf("NoExpose"); 
		break;
	case VisibilityNotify:
		printf("VisibilityNotify"); 
		break;
	case CreateNotify:
		printf("CreateNotify"); 
		break;
	case DestroyNotify:
		printf("DestroyNotify"); 
		break;
	case UnmapNotify:
		printf("UnmapNotify"); 
		break;
	case MapNotify:
		printf("MapNotify"); 
		break;
	case MapRequest:
		printf("MapRequest"); 
		break;
	case ReparentNotify:
		printf("ReparentNotify"); 
		break;
	case ConfigureNotify:
		printf("ConfigureNotify"); 
		break;
	case ConfigureRequest:
		printf("ConfigureRequest"); 
		break;
	case GravityNotify:
		printf("GravityNotify"); 
		break;
	case ResizeRequest:
		printf("ResizeRequest"); 
		break;
	case CirculateNotify:
		printf("CirculateNotify"); 
		break;
	case CirculateRequest:
		printf("CirculateRequest"); 
		break;
	case PropertyNotify:
		printf("PropertyNotify"); 
		break;
	case SelectionClear:
		printf("SelectionClear"); 
		break;
	case SelectionRequest:
		printf("SelectionRequest"); 
		break;
	case SelectionNotify:
		printf("SelectionNotify"); 
		break;
	case ColormapNotify:
		printf("ColormapNotify"); 
		break;
	case ClientMessage:
		printf("ClientMessage"); 
		break;
	case MappingNotify:
		printf("MappingNotify"); 
		break;
	default:
		printf("Unknown (%d)", event->type);
		break;
	}

	printf(" serial %ld, send_event %d window %d\n", 
		   event->xany.serial, event->xany.send_event, (int)event->xany.window);
}

#endif // _DEBUG
