#ifndef MacOpColorChooser_H
#define MacOpColorChooser_H

#include "adjunct/desktop_pi/DesktopColorChooser.h"

class MacOpColorChooser : public OpColorChooser
{
	virtual OP_STATUS Show(COLORREF initial_color, OpColorChooserListener* listener, DesktopWindow* parent=NULL);
};

#endif // MacOpColorChooser_H
