/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen
 */

#ifndef VISUAL_DEVICE_HANDLER_H
#define VISUAL_DEVICE_HANDLER_H

class VisualDevice;
class OpWidget;

/** @brief Makes sure that a visual device handler exists for a certain widget
  *
  * Instantiate this class with a given OpWidget to make sure that a visual device
  * exists for that widget as long as the VisualDeviceHandler is in scope.
  * This can be used to execute several functions of an OpWidget that only run
  * if a VisualDevice is present.
  *
  * Example:
  * {
  *		VisualDeviceHandler handler(widget);
  *     widget->GetPreferedSize(&width, &height, 1, 1);
  * }
  * 
  */
class VisualDeviceHandler
{
public:
	VisualDeviceHandler(OpWidget* widget);	
	~VisualDeviceHandler();

private:
	OpWidget*     m_widget;
	VisualDevice* m_vd;
};

#endif // VISUAL_DEVICE_HANDLER_H
