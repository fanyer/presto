/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_inputmethod.h"
#include "platforms/unix/base/x11/x11_inputcontext.h"

#include <locale.h>
#include <stdlib.h>


X11InputMethod::X11InputMethod(X11Types::Display* display)
	: m_display(display)
	, m_xim(0)
	, m_xim_style(0)
	, m_ic_list(0)
{
	AttachToXIM(display);
};

void X11InputMethod::AttachToXIM(X11Types::Display* display)
{
	if (m_xim != 0)
		return;
	if (!SetLocaleModifier(0, 0, FALSE) )
	{
		// Recovery to avoid bug DSK-297409 which really is about a broken distribution

		OpString8 lc_ctype;
		lc_ctype.Set(setlocale(LC_CTYPE, 0));

		BOOL ok = FALSE;
		OpAutoVector<OpString8> list;
		
		OP_STATUS rc = GetAlternativeLocaleFormat(lc_ctype, list);
		if (OpStatus::IsError(rc) || list.GetCount()==0)
		{
			ok = SetLocaleModifier(0, "C", FALSE);
		}
		else
		{
			for (UINT32 i=0; !ok && i<list.GetCount(); i++)
				ok = SetLocaleModifier(0, list.Get(i)->CStr(), FALSE);
		}

		if(!ok)
		{
			fprintf(stderr, "opera: You appear to have an invalid locale set. This may prevent you from being able to type\n");
			return;
		}
	}

	m_xim = XOpenIM(display, NULL, NULL, NULL);
	if (m_xim == NULL) 
	{
		// Recovery to avoid bug DSK-304220 (broken XMODIFIERS)
		SetLocaleModifier("@im=", m_lc_modifier.value.CStr(), TRUE);

		m_xim = XOpenIM(display, NULL, NULL, NULL);
		if (m_xim == NULL) 
		{
			fprintf(stderr, "opera: XOpenIM failed. This may prevent you from being able to type\n");
			return;
		}
	}

	{ // local scope
		XIMCallback cb;
		cb.client_data = (XPointer)this;
		cb.callback = XIMDestroyCallback;
		const char * setvalueerr = XSetIMValues(m_xim, XNDestroyCallback, &cb, NULL);
		if (setvalueerr != 0)
			fprintf(stderr, "opera: Failed to set XIM destroy callback\n");
	}

	XIMStyles* xim_styles = 0;

	char* im_values = XGetIMValues(m_xim, XNQueryInputStyle, &xim_styles, NULL);
	if(im_values != NULL || xim_styles == NULL) 
	{
		fprintf(stderr, "opera: input method does not support any styles\n");
	}

	if(xim_styles) 
	{
		unsigned long callbacks = 0;
		for (int i = 0; i < xim_styles->count_styles; i++)
		{
			// We try to find the style with the most support for preedit and status callbacks
			if ((xim_styles->supported_styles[i] & XIMPreeditCallbacks) && 
				(xim_styles->supported_styles[i] & callbacks) == callbacks)
			{
				m_xim_style = xim_styles->supported_styles[i];
				callbacks |= XIMPreeditCallbacks;
			}
			if ((xim_styles->supported_styles[i] & XIMStatusCallbacks) &&
				(xim_styles->supported_styles[i] & callbacks) == callbacks)
			{
				m_xim_style = xim_styles->supported_styles[i];
				callbacks |= XIMStatusCallbacks;
			}
			if ((xim_styles->supported_styles[i] == (XIMPreeditNothing | XIMStatusNothing)) &&
				!callbacks)
			{
				m_xim_style = xim_styles->supported_styles[i];
			}
		}

		if (m_xim_style == 0) 
		{
			fprintf(stderr, "opera: input method does not support the style we support\n");
		}
		XFree(xim_styles);
	}
}


X11InputMethod::~X11InputMethod()
{
	/* I hope it is safe to call this even if we haven't registered
	 * any callbacks.  (If it is also sufficient to call this once to
	 * undo all matching XRegisterIMInstantiateCallback()s, we could
	 * remove the other call to this function.)
	 */
	XUnregisterIMInstantiateCallback(m_display, NULL, NULL, NULL, XIMInstantiateCallback, (XPointer)this);
	while (m_ic_list != 0)
	{
		ICList * ic = m_ic_list;
		ic->ic->InputMethodIsDeleted();
		m_ic_list = ic->next;
		OP_DELETE(ic);
	};
	if(m_xim)
		XCloseIM(m_xim);
}


X11InputContext* X11InputMethod::CreateIC(X11Widget * window)
{
	if( !m_xim || !m_xim_style )
		return 0;

	X11InputContext* input_context = OP_NEW(X11InputContext, ());
	ICList * listentry = OP_NEW(ICList, ());
	if (!input_context ||
		!listentry ||
		OpStatus::IsError(input_context->Init(this, window)))
	{
		OP_DELETE(input_context);
		return 0;
	}

	listentry->next = m_ic_list;
	listentry->ic = input_context;
	m_ic_list = listentry;

	return input_context;
}

void X11InputMethod::InputContextIsDeleted(X11InputContext * ic)
{
	ICList ** p = &m_ic_list;
	while (*p != 0 && (*p)->ic != ic)
		p = &(*p)->next;
	if (*p == 0)
		return;
	ICList * q = *p;
	*p = (*p)->next;
	OP_DELETE(q);
	return;
};

// Bug DSK-297409
// We convert the locale of the form *.utf8 to {*.UTF-8, *, POSIX, C}
OP_STATUS X11InputMethod::GetAlternativeLocaleFormat(const OpStringC8& locale, OpVector<OpString8>& list)
{
	int pos = locale.Find(".utf8");
	if (pos != KNotFound)
	{
		OpString8* s = OP_NEW(OpString8,());
		if(!s)
			return OpStatus::ERR_NO_MEMORY;
		s->Set(locale, pos);
		s->Append(".UTF-8");
		list.Add(s);

		s = OP_NEW(OpString8,());
		if(!s)
			return OpStatus::ERR_NO_MEMORY;
		s->Set(locale, pos);
		list.Add(s);
	}
	
	OpString8* s = OP_NEW(OpString8,());
	if(!s)
		return OpStatus::ERR_NO_MEMORY;
	s->Set("POSIX");
	list.Add(s);
	
	s = OP_NEW(OpString8,());
	if(!s)
		return OpStatus::ERR_NO_MEMORY;
	s->Set("C");
	list.Add(s);

	return OpStatus::OK;
}


BOOL X11InputMethod::SetLocaleModifier(const char* modifier, const char* lc_ctype, BOOL silent)
{
	if (lc_ctype && *lc_ctype)
	{
		setlocale(LC_CTYPE, lc_ctype);
		if (!silent)
			fprintf(stderr, "opera: LC_CTYPE changed to '%s'\n", lc_ctype);
	}

	BOOL ok = XSetLocaleModifiers(modifier ? modifier : "") != 0;
	if (!ok && !silent)
		fprintf(stderr, "opera: XSetLocaleModifiers failed with LC_CTYPE '%s'\n", lc_ctype ? lc_ctype : setlocale(LC_CTYPE, 0));
	
	if (ok && !m_lc_modifier.valid)
	{
		m_lc_modifier.valid = TRUE;
		if (lc_ctype)
			m_lc_modifier.value.Set(lc_ctype);
	}

	return ok;
}


void X11InputMethod::XIMDestroyCallback(XIM xim, XPointer client_data, XPointer call_data)
{
	X11InputMethod * self = (X11InputMethod *)client_data;
	self->m_xim = 0;
	for (ICList * p = self->m_ic_list; p != 0; p = p->next)
		p->ic->XIMHasBeenDestroyed();

	/* I don't know if it is necessary to call unregister as many
	 * times as register has been called.  But I'll do it to be on the
	 * safe side.  Conversely, I don't know whether it is really safe
	 * to call unregister without having called register first.  But I
	 * expect it is.
	 */
	XUnregisterIMInstantiateCallback(self->m_display, NULL, NULL, NULL, XIMInstantiateCallback, (XPointer)self);
	if (XRegisterIMInstantiateCallback(self->m_display, NULL, NULL, NULL, XIMInstantiateCallback, (XPointer)self) != True)
		fprintf(stderr, "opera: Failed to register callback to be notified of input methods becoming available.\n");
};

void X11InputMethod::XIMInstantiateCallback(X11Types::Display * dpy, XPointer client_data, XPointer call_data)
{
	X11InputMethod * self = (X11InputMethod *)client_data;
	if (self->m_xim != 0)
		return;
	self->AttachToXIM(dpy);
	if (self->m_xim == 0)
		return;
	for (ICList * p = self->m_ic_list; p != 0; p = p->next)
		p->ic->Init(self);
};
