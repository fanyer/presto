/**
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef X11_INFORMATIONBROKER_H_
#define X11_INFORMATIONBROKER_H_

#include "platforms/x11api/pi/x11_client.h"

class X11InformationBroker : public X11Client
{
public:
	virtual X11Types::Display* GetDisplay();
	virtual int GetScreenNumber(X11Types::Window window);
	virtual X11Types::Window GetTopLevelWindow(OpWindow* window);
	virtual void GetKeyEventPlatformData(uni_char key, bool pressed, ShiftKeyState modifiers, UINT64& data1, UINT64& data2);
};

#endif  // X11_INFORMATIONBROKER_H_
