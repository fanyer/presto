/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef TOOLKIT_LOADER_H
#define TOOLKIT_LOADER_H

#include "modules/hardcore/timer/optimer.h"
#include "platforms/quix/environment/DesktopEnvironment.h"
#include "platforms/quix/toolkits/ToolkitMainloopRunner.h"

class ToolkitLibrary;
class OpDLL;

/** @brief Loads a toolkit library, and takes care of ownership
  * singleton
  */
class ToolkitLoader : public OpTimerListener
{
public:
	ToolkitLoader();
	~ToolkitLoader();

	/** Initialize and load a toolkit library
	  * @param toolkit Toolkit library to load, use TOOLKIT_AUTOMATIC to get preferred library
	  */
	OP_STATUS Init(DesktopEnvironment::ToolkitType toolkit = DesktopEnvironment::TOOLKIT_AUTOMATIC);

	/** @return A new ToolkitLibrary instance as loaded by this loader.
	  * The caller takes ownership and is responsible for deleting
	  * the ToolkitLibrary.
	  */
	ToolkitLibrary* CreateToolkitLibrary();

	/** Creates and initializes singleton toolkit loader */
	static OP_STATUS Create();
	/** Destroys singleton toolkit loader */
	static void Destroy();
	/** @return singleton toolkit loader */
	static ToolkitLoader* GetInstance() { return s_toolkit_loader; }

private:
	class MainloopRunner : public ToolkitMainloopRunner
	{
		virtual unsigned RunSlice();
		static const unsigned MaxWait = 50; ///< maximum ms to wait after RunSlice calls
	};

	const uni_char* GetLibraryName(DesktopEnvironment::ToolkitType toolkit);

	// OpTimerListener
	void OnTimeOut(OpTimer* timer);

	OpDLL* m_toolkit_dll;
	MainloopRunner m_runner;

	OpTimer m_optimer;

	static ToolkitLoader* s_toolkit_loader;
};

#endif // TOOLKIT_LIBRARY_H
