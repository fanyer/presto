/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef X11_OP_INPUT_METHOD_H
#define X11_OP_INPUT_METHOD_H

#include "platforms/utilix/x11_all.h"

class X11InputContext;
class X11Widget;

/** @brief Connection to an X input method
  */
class X11InputMethod
{
private:
	struct LC_Modifier_Data
	{
		LC_Modifier_Data():valid(FALSE){}

		BOOL valid;
		OpString8 value;
	};

public:
	/** Create input method connection
	  * @param display X11 display connection to use
	  */
	X11InputMethod(X11Types::Display* display);

	~X11InputMethod();

	/** Connect to input method.
	 */
	void AttachToXIM(X11Types::Display * display);

	/** Create an input context for this input method
	  * @param window X11 window for which to create connection
	  * @return Fully initialized input context, caller assumes ownership
	  */
	X11InputContext* CreateIC(X11Widget * window);

	/** Obtain the XIM managed by this object.
	 */
	XIM GetXIM() { return m_xim; };

	/** Obtain the XIM style this object expects to be used.
	 */
	XIMStyle GetXIMStyle() { return m_xim_style; };

	/** Informs this object that 'ic' should not be accessed again, as
	 * it has become invalid.
	 */
	void InputContextIsDeleted(X11InputContext * ic);

private:
	/** Converts a locale into a list of alternatives suitable for setlocale()
	  *
	  * @param locale The locale to derive from
	  * @param list List of alternative encodings
	  *
	  * @return OpStatus::OK on sucess, otherwise an error code
	  */
	OP_STATUS GetAlternativeLocaleFormat(const OpStringC8& locale, OpVector<OpString8>& list);

	/**
	 * Calls XSetLocaleModifiers() and logs the result. 
	 *
	 * @param modifier String used by XSetLocaleModifiers(). Can be 0
	 * @param lc_ctype. If not NULL, LC_CTYPE is set to this string 
	 *        before XSetLocaleModifiers() is called
	 * @param silent Do not log any error messages if TRUE
	 * 
	 * @return TRUE if XSetLocaleModifiers() succeeded, otherwise FALSE
	 */
	BOOL SetLocaleModifier(const char* modifier, const char* lc_ctype, BOOL silent);

	/** The XIM destroy callback (XNDestroyCallback)
	 */
	static void XIMDestroyCallback(XIM xim, XPointer client_data, XPointer call_data);

	/** The callback for XRegisterIMInstantiateCallback().
	 */
	static void XIMInstantiateCallback(X11Types::Display * dpy, XPointer client_data, XPointer call_data);

private:
	X11Types::Display * m_display;
	XIM m_xim;
	XIMStyle m_xim_style;
	LC_Modifier_Data m_lc_modifier;
	struct ICList
	{
		X11InputContext * ic;
		ICList * next;
	};
	ICList * m_ic_list;
};

#endif // X11_OP_INPUT_METHOD_H
