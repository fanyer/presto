/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_EXEC_H
#define SCOPE_EXEC_H

#ifdef SCOPE_EXEC_SUPPORT

#include "modules/scope/src/scope_service.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/hardcore/keys/opkeys.h"

#include "modules/scope/src/generated/g_scope_exec_interface.h"

class XMLFragment;

// Maximum number of colorspecs for a single screenwatch
#define EXEC_MAX_COLOR_SPECS	16

// Maximum number of simultaneously pressed keys
#define SCOPE_MAX_PRESSED_KEYS	16

class OpScopeExec
	: public OpScopeExec_SI
	, private MessageObject
	, private OpTimerListener
{
public:
	OpScopeExec();
	virtual ~OpScopeExec();

	void WindowPainted(Window* win, OpRect rect);

private:
	// State info for pressed keys
	ShiftKeyState shiftkeys_state;
	int pressed_keys_count;
	uni_char pressed_keys[SCOPE_MAX_PRESSED_KEYS]; // ARRAY OK 2009-05-05 jhoff

	// Watch window info
	BOOL is_grabbing_bitmap;
	BOOL watch_window_enabled;
	unsigned int watch_window_id;
	OpRect watch_window_rect;
	BOOL watch_document_relative;
	OpVector<OpString> watch_hashes;
	unsigned int watch_window_last_painted_id;
	OpTimer watch_window_timer;
	BOOL watch_window_timer_running;
	BOOL include_image;
	unsigned int watch_tag; // For responses, 0 means there is response to send

	struct OpScopeExecColorSpecs
	{
		int id;

		unsigned int low_red;
		unsigned int high_red;
		unsigned int low_green;
		unsigned int high_green;
		unsigned int low_blue;
		unsigned int high_blue;

		unsigned int count;
		BOOL reported;
	}
	colspec[EXEC_MAX_COLOR_SPECS]; // ARRAY OK 2009-06-08 mortenro

	struct OpScopeExecColspecReport
	{
		int id;
		unsigned int count;
	}
	colspecreport[EXEC_MAX_COLOR_SPECS]; // ARRAY OK 2009-06-09 mortenro

	unsigned int colspec_count;
	unsigned int colspecreport_count;
	unsigned int colspec_total_matches;

#ifndef MOUSELESS
	unsigned int mouse_last_window_id;
	int mouse_last_x;
	int mouse_last_y;
#endif // !MOUSELESS

	virtual void OnTimeOut(OpTimer* timer);

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);


	void HandleScreenWatcher(XMLFragment* xml);

#ifndef MOUSELESS
	void HandleMouseAction(XMLFragment* xml);
#endif // !MOUSELESS

	void ScreenWatcherAddHash(const uni_char* hash);
	void ScreenWatcherEnable(int windowid, const OpRect &rect, UINT32 timeout, BOOL document_relative);
	void ScreenWatcherCancel(void);

	void DoKey(const uni_char* key, BOOL keydown);

	void SetKeyStatus(uni_char key, BOOL pressed);
	BOOL GetKeyStatus(uni_char key);

	OP_STATUS CalculateColors(OpBitmap* bitmap);
	BOOL CountColor(UINT32 color);
	OP_STATUS ScreenWatcherAddColspec(int id,
									  unsigned int low_red,
									  unsigned int high_red,
									  unsigned int low_green,
									  unsigned int high_green,
									  unsigned int low_blue,
									  unsigned int high_blue);

	OP_STATUS CheckColorSpec(unsigned int color, int index, const uni_char *field_name);
	OP_STATUS ScreenWatcherAddColspec(int index, const ScreenWatcher::ColorSpec &color);

public:
	// Request/Response functions
	virtual OP_STATUS DoExec(const ActionList &in);
	virtual OP_STATUS DoGetActionInfoList(ActionInfoList &out);
	virtual OP_STATUS DoSendMouseAction(const MouseAction &in);
	virtual OP_STATUS DoSetupScreenWatcher(const ScreenWatcher &in, unsigned int async_tag);

private:
	OP_STATUS HandleKey(const uni_char *key, BOOL keydown);
	OP_STATUS HandleType(const uni_char *phrase);
	OP_STATUS HandleAction(const uni_char *action, INT32 data, const uni_char *data_string, const uni_char *data_string_param, unsigned window_id);

	OP_STATUS AddImage(ByteBuffer &buffer, OpBitmap* bitmap);
	OP_STATUS CalculateHash(OpString &hash, OpBitmap* bitmap);
	OP_STATUS HandleWindowPainted(unsigned int window_id, BOOL send_image);
	OP_STATUS TransmitScreenImage(unsigned int window_id, OpString &hash, OpBitmap* bitmap, BOOL send_image);
	OP_STATUS CaptureScreenDump(OpBitmap* &bitmap, Window* win, const OpRect &rect, BOOL document_relative);

#ifndef MOUSELESS
	OP_STATUS DoMouseAction(unsigned int window_id, OpPoint point,
							unsigned int action);
#endif // !MOUSELESS

	BOOL ParseInt(XMLFragment* xml, const uni_char* name, int &value);
	BOOL ParseArea(XMLFragment* xml, OpRect &rect, BOOL *document_relative);
};

#endif // SCOPE_EXEC_SUPPORT

#endif // SCOPE_EXEC_H
