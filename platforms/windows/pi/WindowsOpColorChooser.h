#ifndef WINDOWS_OP_COLORCHOOSER_H
#define WINDOWS_OP_COLORCHOOSER_H

#include "adjunct/desktop_pi/DesktopColorChooser.h"

class WindowsOpColorChooser : public OpColorChooser
{
public:
	WindowsOpColorChooser();
	~WindowsOpColorChooser();

	OP_STATUS Show(COLORREF initial_color, OpColorChooserListener* listener, DesktopWindow* parent=NULL);
};

#endif // WINDOWS_OP_COLORCHOOSER_H
