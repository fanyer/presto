/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef GADGET_SELFTEST_UTILS_H
#define GADGET_SELFTEST_UTILS_H
#if defined(GADGET_SUPPORT) && defined(SELFTEST)

#include "modules/gadgets/gadget_utils.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"

class OpGadget;
class SelftestOpenGadgetListener;

/** A utility for loading gadgets in selftests.
 *
 * The common usage pattern is as follows:
 *
 * <code>
 * global {
 *     GadgetSelftestUtils g_utils;
 * }
 *
 * exit {
 *     g_utils.UnloadGadget();
 * }
 *
 * test "Load gadget"
 *     file widget_path "my_selftest_widget/config.xml";
 *     async;
 * {
 *     OP_GADGET_STATUS status = LoadGadgetForSelftest(widget_path, URL_GADGET_INSTALL_CONTENT);
 *     if (OpStatus::IsError(status))
 *         ST_failed("Unable to install test widget, OpStatus: %d", status);
 *     else if (status != OpGadgetStatus::OK_SIGNATURE_VERIFICATION_IN_PROGRESS)
 *         ST_passed();
 * }
 *
 * test "First test"
 *     require success "Load gadget"
 * {
 *     // selftest code
 * }
 * </code>
 *
 * If more than one gadget is required for testing in a single selftest
 * group, each new gadget loaded only after the previous one is unloaded
 * with UnloadGadget.
 */
class GadgetSelftestUtils
{
	friend class SelftestOpenGadgetListener;
public:

	class GadgetOpenCallback
	{
	public:
		virtual void OnGadgetOpen(OP_GADGET_STATUS status) = 0;
	};

	GadgetSelftestUtils() : m_gadget(NULL), m_close_window_on_unload(FALSE) { }

	/** Loads and opens gadget the regular way.
	 *
	 * Because gadget installation is asynchronous, the test must also
	 * be an async one.
	 * When the gadget is installed it is being opened in the current
	 * selftest window and ST_passed is called automatically thus
	 * finishing the selftest.
	 *
	 * See the GadgetSelftestUtils documentation for a usage example
	 * (use the same way as LoadGadgetForSelftest).
	 */
	OP_GADGET_STATUS LoadAndOpenGadget(const uni_char* path, URLContentType type, OpGadget** new_gadget = NULL, GadgetOpenCallback* callback = NULL);
	OP_GADGET_STATUS LoadAndOpenGadget(const char* path, URLContentType type, OpGadget** new_gadget = NULL, GadgetOpenCallback* callback = NULL);

	/** Loads and opens a gadget in a selftest.
	 *
	 * Because gadget installation is asynchronous, the test must also
	 * be an async one.
	 * When the gadget is installed it is being opened in the current
	 * selftest window and ST_passed is called automatically thus
	 * finishing the selftest.
	 *
	 * The gadget is not opened the normal way (OpenGadget is not used)
	 * so use this only to ensure that the following ecmascript tests
	 * are run in the context of the widget.
	 *
	 * See the GadgetSelftestUtils documentation for a usage example.
	 *
	 * @param path Path to the gadget to be loaded (zip/wgt or config.xml file).
	 * @param type Type of the gadget content.
	 * @param new_gadget Pointer to be set to point to the newly created gadget.
	 * May be NULL. The returned value is only valid if the function returns a
	 * non-error value.
	 * This value will be also available as <code>state.win->GetGadget()</code>
	 * in C++ selftests.
	 *
	 * @return
	 *  - OK - the gadget has been installed and opened synchronously
	 *  - OK_SIGNATURE_VERIFICATION_IN_PROGRESS - the gadget installation is in
	 *    progress and will finish asynchronously.
	 *  - error values upon failure.
	 */
	OP_GADGET_STATUS LoadGadgetForSelftest(const uni_char* path, URLContentType type, OpGadget** new_gadget = NULL,  GadgetOpenCallback* callback = NULL);
	OP_GADGET_STATUS LoadGadgetForSelftest(const char* path, URLContentType type, OpGadget** new_gadget = NULL, GadgetOpenCallback* callback = NULL);

	/** Removes the previously loaded gadget.
	 *
	 * Warning: unload gadgets in a separate testcase in
	 * selftests to allow messages to be processed before
	 * the next test.
	 */
	void UnloadGadget();

	/** Notifies that the gadget has already been unloaded.
	 *
	 * Call this method if a gadget opened with LoadAndOpenGadget
	 * or LoadGadgetForSelftests has been closed by the selftests
	 * and shouldn't be closed by UnloadGadget any more.
	 */
	void MarkGadgetUnloaded() { m_gadget = NULL; }

private:
	/** Opens an installed widget in a particular window.
	 */
	static OP_STATUS OpenGadgetInWindow(OpGadget* gadget, Window* window);


	class SelftestGadgetOpenCallback : public GadgetSelftestUtils::GadgetOpenCallback
	{
		virtual void OnGadgetOpen(OP_GADGET_STATUS status);
	};

	OpGadget* m_gadget;
	BOOL m_close_window_on_unload;
	SelftestGadgetOpenCallback m_default_open_callback;
};

#endif // defined(GADGET_SUPPORT) && defined(SELFTEST)
#endif // !GADGET_SELFTEST_UTILS_H
