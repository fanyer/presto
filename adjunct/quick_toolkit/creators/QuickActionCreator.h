/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef QUICK_ACTION_CREATOR_H
#define QUICK_ACTION_CREATOR_H

#include "adjunct/quick_toolkit/creators/QuickUICreator.h"

class OpInputAction;

/*
 * @brief Creates an OpInputAction object based on a ParserNode definition, for actions specified as a map.
 * @see QuickUICreator
 * 
 * To be used like this:
 * 
 * ParserNode node = // get it from somewhere, e.g. DialogReader
 * QuickActionCreator creator;
 * RETURN_IF_ERROR(creator.Init(node));
 * OpInputAction * action;
 * RETURN_IF_ERROR(creator.CreateInputAction(action));
 */
class QuickActionCreator : public QuickUICreator
{
public:
	QuickActionCreator(ParserLogger& log) : QuickUICreator(log) {}

	/*
	 * Traverses the attributes of the ParserNode and creates the action and all its sub-actions.
	 * Object owned by the caller.
	 * @returns Error if OOM or ill-defined parser nodes.
	 */
	OP_STATUS	CreateInputAction(OpAutoPtr<OpInputAction>& action);
};


#endif // QUICK_ACTION_CREATOR_H
