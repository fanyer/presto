/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef TOOLKIT_MAINLOOP_RUNNER_H
#define TOOLKIT_MAINLOOP_RUNNER_H

/** @brief Run the Opera main loop
  */
class ToolkitMainloopRunner
{
public:
	virtual ~ToolkitMainloopRunner() {}

	/** Run a slice of the Opera main loop. This function should return.
	  * @return ms to wait until RunSlice should be scheduled next, or UINT_MAX for no upper bound
	  */
	virtual unsigned RunSlice() = 0;
};

#endif // TOOLKIT_MAINLOOP_RUNNER_H
