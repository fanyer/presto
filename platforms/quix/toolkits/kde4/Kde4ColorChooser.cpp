/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patryk Obara (pobara)
 */

#include "platforms/quix/toolkits/kde4/Kde4ColorChooser.h"
#include "platforms/quix/toolkits/kde4/Kde4Utils.h"

#include <KDE/KColorDialog>
#include <QX11Info>
#include <X11/Xlib.h>


Kde4ColorChooser::Kde4ColorChooser()
	: m_color(0x0)
{
}


Kde4ColorChooser::~Kde4ColorChooser()
{
}


bool Kde4ColorChooser::Show(X11Types::Window parent, uint32_t initial_color)
{
	QColor new_color;
	QColor old_color ( 0xff &  initial_color,
					   0xff & (initial_color >> 8),
					   0xff & (initial_color >> 16) );
	KColorDialog dlg;
	dlg.setButtons( KDialog::Ok | KDialog::Cancel );
	dlg.show();
	XSetTransientForHint( QX11Info::display(), dlg.winId(), parent);
	Kde4Utils::SetResourceName(&dlg, "colorselectordialog");

	dlg.setDefaultColor( old_color );
	dlg.setColor( new_color );
	int result = dlg.exec();
	new_color = dlg.color();
	m_color = (new_color.blue()<<16) | (new_color.green()<<8) | new_color.red();
	return (result == KColorDialog::Accepted);
}
 
