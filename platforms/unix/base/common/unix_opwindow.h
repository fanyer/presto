/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Espen Sand
*/

#ifndef _UNIX_OPWINDOW_H_
#define _UNIX_OPWINDOW_H_ 

#include "modules/pi/OpWindow.h"

class UnixOpWindow : public OpWindow
{
public:
     UnixOpWindow( bool is_x11 )
	  : m_is_x11(is_x11)
     {
     }

     bool isX11() const { return m_is_x11; }

private:
     bool m_is_x11;
};


#endif
