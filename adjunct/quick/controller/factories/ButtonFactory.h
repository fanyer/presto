/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef BUTTON_FACTORY_H
#define BUTTON_FACTORY_H

class OpStateButton;


/***********************************************************************************
**  @class ButtonFactory
**	@brief Allows you to create buttons of different types.
**
**  This is most likely used by OpToolbar or similar classes that read UI
**  specification files to create buttons.
**
************************************************************************************/
namespace ButtonFactory
{
	/*
	**  Creates an OpStateButton and, based on the type specified, retrieves the right
	**  WidgetStateModifier and hands it over to the button. If there's no concrete
	**  StateButtonFactory implementation for the specified type, OpStatus::ERR will be
	**  returned. The button is allocated within this method, but ownership is handed
	**  over to the caller.
	**
	**  @see OpStateButton
	**  @see WidgetStateModifier
	*/
	OP_STATUS ConstructStateButtonByType(const char* type, OpStateButton** button);
}

/***********************************************************************************
**  @class StateButtonFactory
**	@brief Interface, allows you to create a state button.
**
************************************************************************************/
class StateButtonFactory
{
public:
	virtual ~StateButtonFactory() {}
	/*
	** Allows you to create a state button that is hooked up with its state button modifier.
	*/
	virtual OP_STATUS ConstructStateButton(OpStateButton** button) = 0;
};

#endif // BUTTON_FACTORY_H
