/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"

#include "x11_cursor.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/utilix/x11_all.h"

#include "modules/pi/OpDLL.h"

#include <X11/cursorfont.h>


// For CSS3
#define vertical_cursor_width 18
#define vertical_cursor_height 9
static const unsigned char vertical_cursor_bits[] = {
   0xf8, 0x7f, 0x00, 0xfa, 0x7f, 0x01, 0xfa, 0x7f, 0x01, 0x02, 0x00, 0x01,
   0xfc, 0xff, 0x00, 0x02, 0x00, 0x01, 0xfa, 0x7f, 0x01, 0xfa, 0x7f, 0x01,
   0xf8, 0x7f, 0x00 };
static const unsigned char vertical_cursor_mask_bits[] = {
   0x07, 0x80, 0x03, 0x07, 0x80, 0x03, 0x07, 0x80, 0x03, 0xff, 0xff, 0x03,
   0xff, 0xff, 0x03, 0xff, 0xff, 0x03, 0x07, 0x80, 0x03, 0x07, 0x80, 0x03,
   0x07, 0x80, 0x03 };

#define context_menu_width 18
#define context_menu_height 18
static const unsigned char context_menu_bits[] = {
   0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x06, 0x00, 0x00, 0x0e, 0x00, 0x00,
   0x1e, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x7e, 0xf8, 0x01, 0xfe, 0xf8, 0x01,
   0xfe, 0x09, 0x01, 0xfe, 0x0b, 0x01, 0x3e, 0xf8, 0x01, 0x76, 0x08, 0x01,
   0x72, 0x08, 0x01, 0xe0, 0xf8, 0x01, 0xe0, 0x08, 0x01, 0xc0, 0x09, 0x01,
   0xc0, 0xf9, 0x01, 0x00, 0x00, 0x00 };
static const unsigned char context_menu_mask_bits[] = {
   0x03, 0x00, 0x00, 0x07, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x1f, 0x00, 0x00,
   0x3f, 0x00, 0x00, 0x7f, 0xfc, 0x03, 0xff, 0xfc, 0x03, 0xff, 0xfd, 0x03,
   0xff, 0xff, 0x03, 0xff, 0xff, 0x03, 0xff, 0xff, 0x03, 0xff, 0xfc, 0x03,
   0xff, 0xfc, 0x03, 0xf7, 0xfd, 0x03, 0xf0, 0xfd, 0x03, 0xe0, 0xff, 0x03,
   0xe0, 0xff, 0x03, 0xc0, 0xfd, 0x03 };

#define bdiag_width 15
#define bdiag_height 15
static const unsigned char bdiag_bits[] = {
   0x00, 0x00, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x38, 0x00, 0x34, 0x00, 0x22,
   0x00, 0x01, 0x80, 0x00, 0x40, 0x00, 0x22, 0x00, 0x16, 0x00, 0x0e, 0x00,
   0x1e, 0x00, 0x3e, 0x00, 0x00, 0x00 };
static const unsigned char bdiag_mask_bits[] = {
   0x00, 0x7f, 0x00, 0x7f, 0x00, 0x7e, 0x00, 0x7c, 0x00, 0x7e, 0x00, 0x77,
   0x80, 0x63, 0xc0, 0x01, 0xe3, 0x00, 0x77, 0x00, 0x3f, 0x00, 0x1f, 0x00,
   0x3f, 0x00, 0x7f, 0x00, 0x7f, 0x00 };

#define fdiag_width 15
#define fdiag_height 15
static const unsigned char fdiag_bits[] = {
   0x00, 0x00, 0x3e, 0x00, 0x1e, 0x00, 0x0e, 0x00, 0x16, 0x00, 0x22, 0x00,
   0x40, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00, 0x22, 0x00, 0x34, 0x00, 0x38,
   0x00, 0x3c, 0x00, 0x3e, 0x00, 0x00 };
static const unsigned char fdiag_mask_bits[] = {
   0x7f, 0x00, 0x7f, 0x00, 0x3f, 0x00, 0x1f, 0x00, 0x3f, 0x00, 0x77, 0x00,
   0xe3, 0x00, 0xc0, 0x01, 0x80, 0x63, 0x00, 0x77, 0x00, 0x7e, 0x00, 0x7c,
   0x00, 0x7e, 0x00, 0x7f, 0x00, 0x7f };

#define magnifier_width 18
#define magnifier_height 18
static unsigned char magnifier_inc_bits[] = {
   0x00, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x18, 0x06, 0x00, 0x04, 0x08, 0x00,
   0xc4, 0x08, 0x00, 0xc2, 0x10, 0x00, 0xf2, 0x13, 0x00, 0xf2, 0x13, 0x00,
   0xc2, 0x10, 0x00, 0xc4, 0x08, 0x00, 0x04, 0x08, 0x00, 0x18, 0x1e, 0x00,
   0xe0, 0x39, 0x00, 0x00, 0x70, 0x00, 0x00, 0xe0, 0x00, 0x00, 0xc0, 0x01,
   0x00, 0x80, 0x01, 0x00, 0x00, 0x00 };
static unsigned char magnifier_inc_mask_bits[] = {
   0xe0, 0x01, 0x00, 0xf8, 0x07, 0x00, 0xfc, 0x0f, 0x00, 0xfe, 0x1f, 0x00,
   0xfe, 0x1f, 0x00, 0xff, 0x3f, 0x00, 0xff, 0x3f, 0x00, 0xff, 0x3f, 0x00,
   0xff, 0x3f, 0x00, 0xfe, 0x1f, 0x00, 0xfe, 0x1f, 0x00, 0xfc, 0x3f, 0x00,
   0xf8, 0x7f, 0x00, 0xe0, 0xf9, 0x00, 0x00, 0xf0, 0x01, 0x00, 0xe0, 0x03,
   0x00, 0xc0, 0x03, 0x00, 0x80, 0x03 };
static unsigned char magnifier_dec_bits[] = {
   0x00, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x18, 0x06, 0x00, 0x04, 0x08, 0x00,
   0x04, 0x08, 0x00, 0x02, 0x10, 0x00, 0xf2, 0x13, 0x00, 0xf2, 0x13, 0x00,
   0x02, 0x10, 0x00, 0x04, 0x08, 0x00, 0x04, 0x08, 0x00, 0x18, 0x1e, 0x00,
   0xe0, 0x39, 0x00, 0x00, 0x70, 0x00, 0x00, 0xe0, 0x00, 0x00, 0xc0, 0x01,
   0x00, 0x80, 0x01, 0x00, 0x00, 0x00 };
static unsigned char magnifier_dec_mask_bits[] = {
   0xe0, 0x01, 0x00, 0xf8, 0x07, 0x00, 0xfc, 0x0f, 0x00, 0xfe, 0x1f, 0x00,
   0xfe, 0x1f, 0x00, 0xff, 0x3f, 0x00, 0xff, 0x3f, 0x00, 0xff, 0x3f, 0x00,
   0xff, 0x3f, 0x00, 0xfe, 0x1f, 0x00, 0xfe, 0x1f, 0x00, 0xfc, 0x3f, 0x00,
   0xf8, 0x7f, 0x00, 0xe0, 0xf9, 0x00, 0x00, 0xf0, 0x01, 0x00, 0xe0, 0x03,
   0x00, 0xc0, 0x03, 0x00, 0x80, 0x03 };

#define copy_width 18
#define copy_height 18
static const unsigned char copy_bits[] = {
   0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x06, 0x00, 0x00, 0x0e, 0x00, 0x00,
   0x1e, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x7e, 0x00, 0x00, 0xfe, 0x00, 0x00,
   0xfe, 0x01, 0x00, 0xfe, 0x03, 0x00, 0x3e, 0x00, 0x00, 0x76, 0x60, 0x00,
   0x72, 0x60, 0x00, 0xe0, 0xf8, 0x01, 0xe0, 0xf8, 0x01, 0xc0, 0x61, 0x00,
   0xc0, 0x61, 0x00, 0x00, 0x00, 0x00 };
static const unsigned char copy_mask_bits[] = {
   0x03, 0x00, 0x00, 0x07, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x1f, 0x00, 0x00,
   0x3f, 0x00, 0x00, 0x7f, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x01, 0x00,
   0xff, 0x03, 0x00, 0xff, 0x07, 0x00, 0xff, 0xf7, 0x00, 0xff, 0xf0, 0x00,
   0xff, 0xfd, 0x03, 0xf3, 0xfd, 0x03, 0xf0, 0xff, 0x03, 0xf0, 0xff, 0x03,
   0xe0, 0xf3, 0x00, 0xe0, 0xf3, 0x00 };

#define alias_width 18
#define alias_height 18
static const unsigned char alias_bits[] = {
   0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x06, 0x00, 0x00, 0x0e, 0x00, 0x00,
   0x1e, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x7e, 0x00, 0x00, 0xfe, 0x00, 0x00,
   0xfe, 0x01, 0x00, 0xfe, 0x03, 0x00, 0x3e, 0xe0, 0x01, 0x76, 0xc0, 0x01,
   0x72, 0xe0, 0x01, 0xe0, 0x60, 0x01, 0xe0, 0x30, 0x00, 0xc0, 0x31, 0x00,
   0xc0, 0x11, 0x00, 0x00, 0x00, 0x00 };
static const unsigned char alias_mask_bits[] = {
   0x03, 0x00, 0x00, 0x07, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x1f, 0x00, 0x00,
   0x3f, 0x00, 0x00, 0x7f, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x01, 0x00,
   0xff, 0x03, 0x00, 0xff, 0xf7, 0x03, 0xff, 0xf7, 0x03, 0xff, 0xf0, 0x03,
   0xff, 0xf1, 0x03, 0xf3, 0xf9, 0x03, 0xf0, 0xfb, 0x03, 0xf0, 0x7b, 0x00,
   0xe0, 0x7b, 0x00, 0xe0, 0x3b, 0x00 };

#define progress_width 18
#define progress_height 18
static const unsigned char progress_bits[] = {
   0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x06, 0x00, 0x00, 0x0e, 0x00, 0x00,
   0x1e, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x7e, 0x00, 0x00, 0xfe, 0x00, 0x00,
   0xfe, 0x01, 0x00, 0xfe, 0x03, 0x00, 0x3e, 0x70, 0x00, 0x76, 0x70, 0x00,
   0x72, 0xa8, 0x00, 0xe0, 0xb8, 0x01, 0xe0, 0x88, 0x00, 0xc0, 0x71, 0x00,
   0xc0, 0x71, 0x00, 0x00, 0x00, 0x00 };
static const unsigned char progress_mask_bits[] = {
   0x03, 0x00, 0x00, 0x07, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x1f, 0x00, 0x00,
   0x3f, 0x00, 0x00, 0x7f, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x01, 0x00,
   0xff, 0x03, 0x00, 0xff, 0x77, 0x00, 0xff, 0xff, 0x00, 0xff, 0xf8, 0x00,
   0xff, 0xfd, 0x01, 0xf3, 0xfd, 0x03, 0xf0, 0xff, 0x01, 0xf0, 0xfb, 0x00,
   0xe0, 0xfb, 0x00, 0xe0, 0x73, 0x00 };

// For panning
#define idle_width 21
#define idle_height 33
static const unsigned char idle_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x1f, 0x00,
   0x80, 0x3f, 0x00, 0xc0, 0x7f, 0x00, 0xe0, 0xff, 0x00, 0xf0, 0xff, 0x01,
   0xf8, 0xff, 0x03, 0xfc, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x80, 0x3f, 0x00, 0x80, 0x3f, 0x00,
   0x80, 0x3f, 0x00, 0x80, 0x3f, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x1f, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x07,
   0xf8, 0xff, 0x03, 0xf0, 0xff, 0x01, 0xe0, 0xff, 0x00, 0xc0, 0x7f, 0x00,
   0x80, 0x3f, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x04, 0x00,
   0x00, 0x00, 0x00 };
static const unsigned char idle_mask_bits[] = {
   0x00, 0x04, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x1f, 0x00, 0x80, 0x3f, 0x00,
   0xc0, 0x7f, 0x00, 0xe0, 0xff, 0x00, 0xf0, 0xff, 0x01, 0xf8, 0xff, 0x03,
   0xfc, 0xff, 0x07, 0xfe, 0xff, 0x0f, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00,
   0x00, 0x1f, 0x00, 0x80, 0x3f, 0x00, 0xc0, 0x7f, 0x00, 0xc0, 0x7f, 0x00,
   0xc0, 0x7f, 0x00, 0xc0, 0x7f, 0x00, 0xc0, 0x7f, 0x00, 0x80, 0x3f, 0x00,
   0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x1f, 0xfe, 0xff, 0x0f,
   0xfc, 0xff, 0x07, 0xf8, 0xff, 0x03, 0xf0, 0xff, 0x01, 0xe0, 0xff, 0x00,
   0xc0, 0x7f, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x0e, 0x00,
   0x00, 0x04, 0x00 };

#define up_width 21
#define up_height 21
static const unsigned char up_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x1f, 0x00,
   0x80, 0x3f, 0x00, 0xc0, 0x7f, 0x00, 0xe0, 0xff, 0x00, 0xf0, 0xff, 0x01,
   0xf8, 0xff, 0x03, 0xfc, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x80, 0x3f, 0x00, 0x80, 0x3f, 0x00,
   0x80, 0x3f, 0x00, 0x80, 0x3f, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x1f, 0x00,
   0x00, 0x00, 0x00 };
static const unsigned char up_mask_bits[] = {
   0x00, 0x04, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x1f, 0x00, 0x80, 0x3f, 0x00,
   0xc0, 0x7f, 0x00, 0xe0, 0xff, 0x00, 0xf0, 0xff, 0x01, 0xf8, 0xff, 0x03,
   0xfc, 0xff, 0x07, 0xfe, 0xff, 0x0f, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00,
   0x00, 0x1f, 0x00, 0x80, 0x3f, 0x00, 0xc0, 0x7f, 0x00, 0xc0, 0x7f, 0x00,
   0xc0, 0x7f, 0x00, 0xc0, 0x7f, 0x00, 0xc0, 0x7f, 0x00, 0x80, 0x3f, 0x00,
   0x00, 0x1f, 0x00 };

#define upright_width 21
#define upright_height 21
static const unsigned char upright_bits[] = {
   0x00, 0x00, 0x00, 0x80, 0xff, 0x0f, 0x00, 0xff, 0x0f, 0x00, 0xfe, 0x0f,
   0x00, 0xfc, 0x0f, 0x00, 0xf8, 0x0f, 0x00, 0xf0, 0x0f, 0x00, 0xe0, 0x0f,
   0x00, 0xc0, 0x0f, 0x00, 0x80, 0x0f, 0x00, 0x00, 0x0f, 0xf0, 0x01, 0x0e,
   0xf8, 0x03, 0x0c, 0xf8, 0x03, 0x08, 0xf8, 0x03, 0x00, 0xf8, 0x03, 0x00,
   0xf8, 0x03, 0x00, 0xf0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00 };
static const unsigned char upright_mask_bits[] = {
   0xe0, 0xff, 0x1f, 0xc0, 0xff, 0x1f, 0x80, 0xff, 0x1f, 0x00, 0xff, 0x1f,
   0x00, 0xfe, 0x1f, 0x00, 0xfc, 0x1f, 0x00, 0xf8, 0x1f, 0x00, 0xf0, 0x1f,
   0x00, 0xe0, 0x1f, 0x00, 0xc0, 0x1f, 0xf0, 0x81, 0x1f, 0xf8, 0x03, 0x1f,
   0xfc, 0x07, 0x1e, 0xfc, 0x07, 0x1c, 0xfc, 0x07, 0x18, 0xfc, 0x07, 0x10,
   0xfc, 0x07, 0x00, 0xf8, 0x03, 0x00, 0xf0, 0x01, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00 };

// Blank cursor
#define blank_width 16
#define blank_height 16
static const unsigned char blank_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };


X11Cursor* X11Cursor::m_cursor = 0;

// static
X11Cursor* X11Cursor::Self()
{
	return m_cursor;
}

// static
OP_STATUS X11Cursor::Create()
{
	if (!m_cursor)
	{
		m_cursor = OP_NEW(X11Cursor,());
		if (!m_cursor)
			return OpStatus::ERR_NO_MEMORY;
		OP_STATUS rc = m_cursor->Init();
		if (OpStatus::IsError(rc))
		{
			OP_DELETE(m_cursor);
			m_cursor = 0;
			return rc;
		}
	}
	return OpStatus::OK;
}


// static
void X11Cursor::Destroy()
{
	OP_DELETE(m_cursor);
	m_cursor = 0;
}


// static 
X11Types::Cursor X11Cursor::Get(unsigned int shape)
{
	return m_cursor->GetHandle(shape);
}

OP_STATUS X11Cursor::Init()
{
	if (!m_items)
	{
		m_items = OP_NEWA(X11CursorItem, X11_CURSOR_MAX);
		if (!m_items)
			return OpStatus::ERR_NO_MEMORY;
	}

	if (!m_xcursor_loader)
	{
		// Allow failure. We can live with a missing interface for Xcursor.
		m_xcursor_loader = OP_NEW(XcursorLoader,());
		if (m_xcursor_loader)
		{
			if (OpStatus::IsError(m_xcursor_loader->Init()))
			{
				OP_DELETE(m_xcursor_loader);
				m_xcursor_loader = 0;
			}
		}
	}

	if (g_xsettings_client && m_xcursor_loader)
		g_xsettings_client->AddListener(this);

	return OpStatus::OK;
}


X11Cursor::X11Cursor()
	:m_items(0)
	,m_xcursor_loader(0)
{
}


X11Cursor::~X11Cursor()
{
	OP_DELETEA(m_items);
	OP_DELETE(m_xcursor_loader);
	if (g_xsettings_client)
		g_xsettings_client->RemoveListener(this);
}


X11Types::Cursor X11Cursor::GetHandle(unsigned int shape)
{
	// X11 specific cursors are added after common opera cursors.
	// The CURSOR_NUM_CURSORS entry is not used
	if (shape >= X11_CURSOR_MAX || shape == CURSOR_NUM_CURSORS)
		return None;

	// Use cached shape if any
	if (m_items[shape].m_cursor != None)
		return m_items[shape].m_cursor;

	// Load the cursor

	int xctype = -1;
	if (shape < CURSOR_NUM_CURSORS)
	{
		// Common Opera cursors
		switch (shape)
		{
			case CURSOR_URI:
				xctype = XC_left_ptr;
				break;
			case CURSOR_CROSSHAIR:
				xctype = XC_cross;
				break;
			case CURSOR_DEFAULT_ARROW:
				xctype = XC_left_ptr;
				break;
			case CURSOR_CUR_POINTER:
				xctype = XC_hand2;
				break;
			case CURSOR_MOVE:
				xctype = XC_fleur;
				break;
			case CURSOR_E_RESIZE:
				xctype = XC_right_side;
				break;
			case CURSOR_NE_RESIZE:
				xctype = XC_top_right_corner;
				break;
			case CURSOR_NW_RESIZE:
				xctype = XC_top_left_corner;
				break;
			case CURSOR_N_RESIZE:
				xctype = XC_top_side;
				break;
			case CURSOR_SE_RESIZE:
				xctype = XC_bottom_right_corner;
				break;
			case CURSOR_SW_RESIZE:
				xctype = XC_bottom_left_corner;
				break;
			case CURSOR_S_RESIZE:
				xctype = XC_bottom_side;
				break;
			case CURSOR_W_RESIZE:
				xctype = XC_left_side;
				break;
			case CURSOR_TEXT:
				xctype = XC_xterm;
				break;
			case CURSOR_WAIT:
				xctype = XC_watch;
				break;
			case CURSOR_HELP:
				xctype = XC_question_arrow;
				break;
			case CURSOR_HOR_SPLITTER:
				xctype = XC_sb_v_double_arrow;
				break;
			case CURSOR_VERT_SPLITTER:
				xctype = XC_sb_h_double_arrow;
				break;
			case CURSOR_ARROW_WAIT:
				xctype = XC_watch;
				break;
			case CURSOR_PROGRESS:
				GetFallbackHandle(shape);
				break;
			case CURSOR_AUTO:
				xctype = XC_xterm;
				break;
			case CURSOR_MAGNIFYING_GLASS:
				GetFallbackHandle(shape);
				break;
			case CURSOR_ZOOM_IN:
				GetFallbackHandle(shape);
				break;
			case CURSOR_ZOOM_OUT:
				GetFallbackHandle(shape);
				break;
			case CURSOR_CONTEXT_MENU:
				GetFallbackHandle(shape);
				break;
			case CURSOR_CELL:
				xctype = XC_plus;
				break;
			case CURSOR_VERTICAL_TEXT:
				GetFallbackHandle(shape);
				break;
			case CURSOR_ALIAS:
				GetFallbackHandle(shape);
				break;
			case CURSOR_COPY:
				GetFallbackHandle(shape);
				break;
			case CURSOR_NO_DROP:
				GetFallbackHandle(shape);
				break;
			case CURSOR_DROP_COPY:
				GetFallbackHandle(shape);
				break;
			case CURSOR_DROP_MOVE:
				GetFallbackHandle(shape);
				break;
			case CURSOR_DROP_LINK:
				GetFallbackHandle(shape);
				break;
			case CURSOR_NOT_ALLOWED:
				xctype = XC_circle;
				break;
			case CURSOR_EW_RESIZE:
				xctype = XC_sb_h_double_arrow;
				break;
			case CURSOR_NS_RESIZE:
				xctype = XC_sb_v_double_arrow;
				break;
			case CURSOR_NESW_RESIZE:
				GetFallbackHandle(shape);
				break;
			case CURSOR_NWSE_RESIZE:
				GetFallbackHandle(shape);
				break;
			case CURSOR_COL_RESIZE:
				xctype = XC_sb_h_double_arrow;
				break;
			case CURSOR_ROW_RESIZE:
				xctype = XC_sb_v_double_arrow;
				break;
			case CURSOR_ALL_SCROLL:
				xctype = XC_fleur;
				break;
			case CURSOR_NONE:
				GetFallbackHandle(shape);
				break;
			default:
				OP_ASSERT(FALSE);
		}
	}
	else
	{
		// Custom cursors
		switch (shape)
		{
			case PANNING_IDLE:
				m_items[shape].Set(idle_bits, idle_mask_bits, idle_width, idle_height, 0);
				break;
			case PANNING_UP:
				m_items[shape].Set(up_bits, up_mask_bits, up_width, up_height, 0);
				break;
			case PANNING_RIGHT:
				m_items[shape].Set(up_bits, up_mask_bits, up_width, up_height, 90);
				break;
			case PANNING_DOWN:
				m_items[shape].Set(up_bits, up_mask_bits, up_width, up_height, 180);
				break;
			case PANNING_LEFT:
				m_items[shape].Set(up_bits, up_mask_bits, up_width, up_height, 270);
				break;
			case PANNING_UP_RIGHT:
				m_items[shape].Set(upright_bits, upright_mask_bits, upright_width, upright_height, 0);
				break;
			case PANNING_DOWN_RIGHT:
				m_items[shape].Set(upright_bits, upright_mask_bits, upright_width, upright_height, 90);
				break;
			case PANNING_DOWN_LEFT:
				m_items[shape].Set(upright_bits, upright_mask_bits, upright_width, upright_height, 180);
				break;
			case PANNING_UP_LEFT:
				m_items[shape].Set(upright_bits, upright_mask_bits, upright_width, upright_height, 270);
				break;
		}
	}

	if (m_items[shape].m_cursor == None)
	{
		if (xctype == -1)
		{
#ifdef DEBUG
			printf("Don't know to display cursor type %d\n", shape);
			xctype = XC_gobbler;
#else
			printf("opera: unknown cursor shape, using standard shape\n"); 
			xctype = XC_left_ptr;
#endif
		}
		m_items[shape].m_cursor = XCreateFontCursor(g_x11->GetDisplay(), xctype);
	}

	return m_items[shape].m_cursor;
}



X11Types::Cursor X11Cursor::GetFallbackHandle(unsigned int shape)
{
	m_items[shape].m_cursor = m_xcursor_loader ? m_xcursor_loader->Get(shape) : None;
	if (m_items[shape].m_cursor == None)
	{
		switch (shape)
		{
		case CURSOR_PROGRESS:
			m_items[shape].Set(progress_bits, progress_mask_bits, progress_width, progress_height, 0);
			break;

		case CURSOR_MAGNIFYING_GLASS:
			m_items[shape].Set(magnifier_inc_bits, magnifier_inc_mask_bits, magnifier_width, magnifier_height, 0);
			break;

		case CURSOR_ZOOM_IN:
			m_items[shape].Set(magnifier_inc_bits, magnifier_inc_mask_bits, magnifier_width, magnifier_height, 0);
			break;

		case CURSOR_ZOOM_OUT:
			m_items[shape].Set(magnifier_dec_bits, magnifier_dec_mask_bits, magnifier_width, magnifier_height, 0);
			break;

		case CURSOR_CONTEXT_MENU:
			m_items[shape].Set(context_menu_bits, context_menu_mask_bits, context_menu_width, context_menu_height, 0);
			break;

		case CURSOR_VERTICAL_TEXT:
			m_items[shape].Set(vertical_cursor_bits, vertical_cursor_mask_bits, vertical_cursor_width, vertical_cursor_height, 0);
			break;

		case CURSOR_ALIAS:
			m_items[shape].Set(alias_bits, alias_mask_bits, alias_width, alias_height, 0);
			break;

		case CURSOR_COPY:
			m_items[shape].Set(copy_bits, copy_mask_bits, copy_width, copy_height, 0);
			break;

		case CURSOR_NO_DROP:
			// No hardcoded shape
			m_items[shape].m_cursor = XCreateFontCursor(g_x11->GetDisplay(), XC_circle);
			break;

		case CURSOR_NESW_RESIZE:
			m_items[shape].Set(bdiag_bits, bdiag_mask_bits, bdiag_width, bdiag_height, 0);
			break;

		case CURSOR_NWSE_RESIZE:
			m_items[shape].Set(fdiag_bits, fdiag_mask_bits, fdiag_width, fdiag_height, 0);
			break;

		case CURSOR_NONE:
			m_items[shape].Set(blank_bits, blank_bits, blank_width, blank_height, 0);
			break;

		case CURSOR_DROP_COPY:
		case CURSOR_DROP_LINK:
		case CURSOR_DROP_MOVE:
			// No hardcoded shape
			m_items[shape].m_cursor = XCreateFontCursor(g_x11->GetDisplay(),XC_left_ptr);
			break;

		default:
			OP_ASSERT(FALSE);
			break;
		}
	}

	return m_items[shape].m_cursor;
}




void X11Cursor::OnResourceUpdated(INT32 id, INT32 cmd, const OpString8& value)
{
	if (id == X11SettingsClient::GTK_CURSOR_THEME_NAME && m_xcursor_loader)
	{
		X11CursorItem* items = OP_NEWA(X11CursorItem, X11_CURSOR_MAX);
		if (items)
		{
			OP_DELETEA(m_items);
			m_items = items;
			m_xcursor_loader->SetTheme(value,-1);
			g_x11->GetWidgetList()->UpdateCursor();
		}
	}
}


void X11Cursor::OnResourceUpdated(INT32 id, INT32 cmd, const UINT32 value)
{
	if (id == X11SettingsClient::GTK_CURSOR_THEME_SIZE && m_xcursor_loader)
	{
		X11CursorItem* items = OP_NEWA(X11CursorItem, X11_CURSOR_MAX);
		if (items)
		{
			OP_DELETEA(m_items);
			m_items = items;
			m_xcursor_loader->SetTheme(OpString8(),value);
			g_x11->GetWidgetList()->UpdateCursor();
		}
	}
}


X11CursorItem::X11CursorItem()
	:m_cursor(None)
	,m_shape(None)
	,m_mask(None)
{
}


X11CursorItem::~X11CursorItem()
{
	X11Types::Display* display = g_x11 ? g_x11->GetDisplay() : 0;
	if (display)
	{
		if (m_shape != None)
			XFreePixmap(display, m_shape);
		if (m_mask != None)
			XFreePixmap(display, m_mask);
		if (m_cursor != None)
			XFreeCursor(display, m_cursor);
	}
}



OP_STATUS X11CursorItem::Rotate(unsigned char* data, unsigned int& width, unsigned int& height, int value)
{
	unsigned char* buf = OP_NEWA(unsigned char,width*height);
	if (!buf)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	op_memset(buf, 0, width*height);

	if (value == 90 || value == 270)
	{
		// current size
		unsigned int cur_w = (width + 7) / 8;
		unsigned int cur_h = height;
		unsigned int cur_bit_per_line = width;

		// new size
		unsigned int new_w = (height + 7) / 8;
		unsigned int new_bit_per_line = height;

		int pos = new_bit_per_line - 1;
		for (unsigned int y=0; y<cur_h; y++)
		{
			int col = (height-y-1) / 8;
			for (unsigned int x=0; x<cur_bit_per_line; x++)
			{
				unsigned char src = data[y*cur_w + x/8];
				if (src & (1 << (x%8)))
				{
					int row = x;
					int bit = pos % 8;
					buf[row*new_w + col] |= (1 << bit);
				}
			}
			pos --;
		}

		unsigned int tmp = width;
		width = height;
		height = tmp;
	}
	else if (value == 180)
	{
		unsigned int cur_w = (width + 7) / 8;
		unsigned int cur_h = height;
		unsigned int cur_bit_per_line = width;

		for (unsigned int y=0; y<cur_h; y++)
		{
			int row = cur_h-y-1;
			int pos = 0;
			for (int x=cur_bit_per_line-1; x>=0; x--)
			{
				unsigned char src = data[y*cur_w + x/8];
				if (src & (1 << (x%8)))
				{
					int col = pos/8;
					int bit = pos%8;
					buf[row*cur_w + col] |= (1 << bit);
				}
				pos ++;
			}
		}
	}

	if( value == 270 )
	{
		OP_STATUS rc = Rotate(buf, width, height, 180);
		if (OpStatus::IsError(rc))
		{
			OP_DELETEA(buf);
			return rc;
		}
	}

	op_memcpy(data, buf, width*height);
	OP_DELETEA(buf);

	return OpStatus::OK;
}


OP_STATUS X11CursorItem::Flip(unsigned char* data, unsigned int width, unsigned int height)
{
	unsigned int h = height;
	unsigned int w = (width + 8) / 8;

	unsigned char* buf = OP_NEWA(unsigned char,w*h);
	if (!buf)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	for( unsigned int y=0; y<h; y++ )
	{
		op_memcpy(&buf[(h-y-1)*w], &data[y*w], w);
	}

	op_memcpy(data, buf, w*h);
	OP_DELETEA(buf);

	return OpStatus::OK;
}


OP_STATUS X11CursorItem::Set(const unsigned char* shape, const unsigned char* mask, unsigned int width, unsigned int height, int rotate)
{
	X11Types::Display* display = g_x11->GetDisplay();
	X11Types::Window root = RootWindowOfScreen(DefaultScreenOfDisplay(display));

	if (m_shape != None)
	{
		XFreePixmap(display, m_shape);
		m_shape = 0;
	}
	if (m_mask != None)
	{
		XFreePixmap(display, m_mask);
		m_mask = 0;
	}
	if (m_cursor != None)
	{
		XFreeCursor(display, m_cursor);
		m_cursor = None;
	}

	if (rotate > 0)
	{
		// X11Cursor::Rotate() expects a buffer of width*height bytes

		unsigned char* buf = OP_NEWA(unsigned char,width*height);
		if (!buf)
			return OpStatus::ERR_NO_MEMORY;
		unsigned int w, h;

		op_memcpy(buf, shape, height*((width + 7)/8) );
		w = width;
		h = height;
		OP_STATUS rc = Rotate(buf, w, h, rotate);
		if (OpStatus::IsError(rc))
		{
			OP_DELETEA(buf);
			return rc;
		}

		m_shape = XCreateBitmapFromData(display, root, (char*)buf, w, h);

		op_memcpy(buf, mask, height*((width + 7)/8) );
		w = width;
		h = height;
		rc = Rotate(buf, w, h, rotate);
		if (OpStatus::IsError(rc))
		{
			OP_DELETEA(buf);
			return rc;
		}

		m_mask = XCreateBitmapFromData(display, root, (char*)buf, w, h);
		OP_DELETEA(buf);
	}
	else
	{
		m_shape = XCreateBitmapFromData(display, root, (char*)shape, width, height);
		m_mask  = XCreateBitmapFromData(display, root, (char*)mask, width, height);
	}

	XColor bg, fg;
	bg.red   = 255 << 8;
	bg.green = 255 << 8;
	bg.blue  = 255 << 8;
	fg.red   = 0;
	fg.green = 0;
	fg.blue  = 0;

	m_cursor = XCreatePixmapCursor(display, m_shape, m_mask, &fg, &bg, 8, 8);

	return OpStatus::OK;
}




XcursorLoader::XcursorLoader()
	: m_dll(0)
	, XcursorLibraryLoadCursor(0)
	, XcursorSetTheme(0)
	, XcursorSetDefaultSize(0)
{
}


OP_STATUS XcursorLoader::Init()
{
	if (m_dll)
		return OpStatus::OK;

	RETURN_IF_ERROR(OpDLL::Create(&m_dll));

	const uni_char* library_name = UNI_L("libXcursor.so.1");
	OP_STATUS rc = m_dll->Load(library_name);
	if (OpStatus::IsError(rc))
	{
		if (g_startup_settings.debug_libraries)
		{
			OpString8 tmp;
			tmp.Set(library_name);
			fprintf(stderr, "opera: Failed to load %s\n", tmp.CStr() ? tmp.CStr() : "");
		}
		OP_DELETE(m_dll);
		m_dll = 0;
		return rc;
	}

	OpStatus::Ignore(g_x11->UnloadOnShutdown(m_dll));

	XcursorLibraryLoadCursor = reinterpret_cast<XcursorLibraryLoadCursor_t>(m_dll->GetSymbolAddress("XcursorLibraryLoadCursor"));
	XcursorSetTheme = reinterpret_cast<XcursorSetTheme_t>(m_dll->GetSymbolAddress("XcursorSetTheme"));
	XcursorSetDefaultSize = reinterpret_cast<XcursorSetDefaultSize_t>(m_dll->GetSymbolAddress("XcursorSetDefaultSize"));

	if (!XcursorLibraryLoadCursor || !XcursorSetTheme || !XcursorSetDefaultSize)
	{
		if (g_startup_settings.debug_libraries)
		{
			OpString8 tmp;
			tmp.Set(library_name);
			fprintf(stderr, "opera: Failed to load %s (missing symbols)\n", tmp.CStr() ? tmp.CStr() : "");
		}
		m_dll = 0;
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}


X11Types::Cursor XcursorLoader::Get(int shape)
{
	if (m_dll)
	{
		switch(shape)
		{
			case CURSOR_VERTICAL_TEXT:
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "vertical-text");
			case CURSOR_ALIAS:
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "alias");
			case CURSOR_COPY:
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "copy");
			case CURSOR_PROGRESS:
			{
				// We prefer 'left_ptr_watch' but can use 'progress' as well (see DSK-322116)
				X11Types::Cursor cursor = XcursorLibraryLoadCursor(g_x11->GetDisplay(), "left_ptr_watch");
				if(cursor == None)
					cursor = XcursorLibraryLoadCursor(g_x11->GetDisplay(), "progress");
				return cursor;
			}
			case CURSOR_CONTEXT_MENU:
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "context-menu");
			case CURSOR_NESW_RESIZE:
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "size_bdiag");
			case CURSOR_NWSE_RESIZE:
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "size_fdiag");
			case CURSOR_HOR_SPLITTER:
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "split_v");
			case CURSOR_VERT_SPLITTER:
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "split_h");
			case CURSOR_MAGNIFYING_GLASS:
				// I have only seen 'zoomIn' for Obsidian
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "zoomIn");
			case CURSOR_NO_DROP:
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "dnd-no-drop");
			case CURSOR_DROP_COPY:
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "dnd-copy");
			case CURSOR_DROP_LINK:
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "dnd-link");
			case CURSOR_DROP_MOVE:
				return XcursorLibraryLoadCursor(g_x11->GetDisplay(), "dnd-move");
		}
	}
	return None;
}

void XcursorLoader::SetTheme(const OpString8& name, INT32 size)
{
	if (name.HasContent())
		XcursorSetTheme(g_x11->GetDisplay(), name.CStr());
	if (size > 0)
		XcursorSetDefaultSize(g_x11->GetDisplay(), size);
}
