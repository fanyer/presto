/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Rolland, mortenro@opera.com
*/

#include "core/pch.h"

#ifdef SCOPE_EXEC_SUPPORT

#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/scope/src/scope_exec.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/display/vis_dev.h"
#include "modules/dochand/fdelm.h"
#include "modules/doc/frm_doc.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/layout/box/box.h"
#include "modules/minpng/minpng_encoder.h"
#include "modules/zlib/zlib.h"
#include "modules/libcrypto/include/CryptoHash.h"

OpScopeExec::OpScopeExec()
	: OpScopeExec_SI()
	, shiftkeys_state(SHIFTKEY_NONE)
	, pressed_keys_count(0)
	, is_grabbing_bitmap(FALSE)
	, watch_window_enabled(FALSE)
	, watch_window_timer_running(FALSE)
	, include_image(TRUE)
	, watch_tag(0)
	, colspec_count(0)
	, colspecreport_count(0)
#ifndef MOUSELESS
	, mouse_last_window_id(0)
#endif
{
	g_main_message_handler->SetCallBack(this, MSG_SCOPE_WINDOW_PAINTED, 0);
	watch_window_timer.SetTimerListener(this);
}

OpScopeExec::~OpScopeExec(void)
{
	ScreenWatcherCancel();
	g_main_message_handler->UnsetCallBack(this, MSG_SCOPE_WINDOW_PAINTED);
}

void OpScopeExec::HandleCallback(OpMessage msg, MH_PARAM_1 par1,
								 MH_PARAM_2 par2)
{
	if ( msg == MSG_SCOPE_WINDOW_PAINTED )
	{
		if (watch_tag == 0)
			return;

		watch_window_last_painted_id = (unsigned int)par1;
		OP_STATUS status = HandleWindowPainted(watch_window_last_painted_id, FALSE);
		if (OpStatus::IsError(status))
		{
			unsigned tag = watch_tag;
			watch_tag = 0;
			RETURN_VOID_IF_ERROR(SendAsyncError(tag, Command_SetupScreenWatcher, status));
		}
	}
}

void OpScopeExec::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer == &watch_window_timer); // Only one timer at the moment

	if (watch_tag == 0)
		return;

	unsigned int id = watch_window_id;

	if ( id == 0 )
		id = watch_window_last_painted_id;

	watch_window_timer_running = FALSE; // Not any more

	OP_STATUS status = HandleWindowPainted(id, TRUE);
	if (OpStatus::IsError(status))
	{
		unsigned tag = watch_tag;
		watch_tag = 0;
		RETURN_VOID_IF_ERROR(SendAsyncError(tag, Command_SetupScreenWatcher, status));
	}
	ScreenWatcherCancel();
}

OP_STATUS OpScopeExec::HandleWindowPainted(unsigned int window_id, BOOL timeout)
{
	Window* win = g_windowManager->FirstWindow();

	// FIXME: There should be a common method for this somewhere
	while ( win != 0 )
	{
		if ( win->Id() == window_id )
			break;
		win = (Window*)win->Suc();
	}

	if ( win != 0 )
	{
		OpString hash;
		OpAutoPtr<OpBitmap> bitmap;
		OpBitmap* use_bitmap;
		BOOL do_send = timeout;
		BOOL send_image = TRUE;

		RETURN_IF_ERROR(CaptureScreenDump(use_bitmap, win, watch_window_rect, watch_document_relative));
		bitmap.reset(use_bitmap);

		if ( use_bitmap != 0 )
		{
			RETURN_IF_ERROR(CalculateHash(hash, use_bitmap));
			{
				// Check if hash is one of the interesting ones

				for ( unsigned int k = 0; k < watch_hashes.GetCount(); k++ )
				{
					if ( watch_hashes.Get(k)->CompareI(hash) == 0 )
					{
						send_image = FALSE;
						do_send = TRUE;
						break;
					}
				}

				// Check if any of the colspecs matched

				CalculateColors(use_bitmap);
				if ( colspec_total_matches > 0 )
				{
					send_image = FALSE;
					do_send = TRUE;
				}
			}

			if ( do_send )
			{
				RETURN_IF_ERROR(TransmitScreenImage(win->Id(), hash, use_bitmap, send_image));
				ScreenWatcherCancel();
			}
		}
		else
			return OpStatus::ERR;
	}
	else
		return OpStatus::ERR;
	return OpStatus::OK;
}

OP_STATUS OpScopeExec::TransmitScreenImage(unsigned int window_id,
									       OpString &hash, OpBitmap* bitmap, BOOL send_image)
{
	if ( ! IsEnabled() )
		return OpStatus::OK;
	ScreenWatcherResult watch;
	watch.SetWindowID(window_id);
	RETURN_IF_ERROR(watch.SetMd5(hash));

	if (send_image && include_image)
		RETURN_IF_ERROR(AddImage(watch.GetPngRef(), bitmap));

	//
	// Include colspec report, if there are any colspecs specified
	//
	OpProtobufMessageVector<ScreenWatcherResult::ColorMatch> &color_matches = watch.GetColorMatchListRef();
	if ( colspecreport_count > 0 )
	{
		for ( unsigned int k = 0; k < colspecreport_count; k++ )
		{
			int id = colspecreport[k].id;
			unsigned int count = colspecreport[k].count;

			ScreenWatcherResult::ColorMatch *match = color_matches.AddNew();
			if (match == NULL)
				return OpStatus::ERR_NULL_POINTER;
			match->SetId(id);
			match->SetCount(count);
		}
	}

	RETURN_IF_ERROR(SendSetupScreenWatcher(watch, watch_tag));
	watch_tag = 0;
	return OpStatus::OK;
}

void OpScopeExec::SetKeyStatus(uni_char key, BOOL pressed)
{
	for ( int k = 0; k < pressed_keys_count; k++ )
	{
		if ( pressed_keys[k] == key )
		{
			if ( pressed )
				return;
			pressed_keys[k] = pressed_keys[--pressed_keys_count];
			return;
		}
	}

	if ( pressed && pressed_keys_count < SCOPE_MAX_PRESSED_KEYS )
		pressed_keys[pressed_keys_count++] = key;
}

BOOL OpScopeExec::GetKeyStatus(uni_char key)
{
	for ( int k = 0; k < pressed_keys_count; k++ )
		if ( pressed_keys[k] == key )
			return TRUE;
	return FALSE;
}

OP_STATUS OpScopeExec::HandleType(const uni_char* phrase)
{
	uni_char code[2];  // ARRAY OK 2008-06-16 mortenro

	if (phrase == 0)
	{
		SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Value required")); // TODO: Proper error status (input validation?)
		return OpStatus::ERR;
	}

	while ( *phrase != 0 )
	{
		code[0] = *phrase++;
		code[1] = 0;
		DoKey(code, TRUE);
		DoKey(code, FALSE);
	}
	return OpStatus::OK;
}

void OpScopeExec::DoKey(const uni_char* key, BOOL keydown)
{
	OpKey::Code code;
	ShiftKeyState keymask = SHIFTKEY_NONE;
	uni_char esc_key[2] = {0}; // ARRAY OK 2012-02-01 sof

	// Check if direct (^X) or indirect escaped input (Ctrl-X):
	if ( key[1] != 0 || (shiftkeys_state & (SHIFTKEY_CTRL | SHIFTKEY_ALT)) == SHIFTKEY_CTRL )
	{
		if ( key[0] == '^' && key[2] == 0 )
		{
			esc_key[0] = key[1];
			keymask |= SHIFTKEY_CTRL;
		}
		else if ( (shiftkeys_state & SHIFTKEY_CTRL) != 0 && key[1] == 0 )
			esc_key[0] = key[0];

		if ( esc_key[0] != 0 )
		{
			// ^@ ... ^Z => ASCII 0 ... ASCII 26
			uni_char ch = uni_tolower(esc_key[0]);
			if ( ch < '@' || ch > 'z' )
				return;

			code = OpKey::FromASCIIToKeyCode(static_cast<char>(ch), keymask);
			esc_key[0] = ch == '@' ? 0 : static_cast<uni_char>(ch - 'a' + 1);
			key = esc_key;
		}
		else
		{
			// Look up the symbol-name of the key
			code = OpKey::FromString(key);

			if ( code == OP_KEY_FIRST )
				return;
			key = NULL;
		}
	}
	else
	{
		code = OP_KEY_FIRST;
		if ( *key < 0x80 )
			code = OpKey::FromASCIIToKeyCode(static_cast<char>(*key), keymask);

		// For character input that cannot be mapped to a virtual key,
		// forward the input keycode-less.
		if ( code == OP_KEY_FIRST )
			code = OP_KEY_UNICODE;
	}

	switch ( code )
	{
	case OP_KEY_SHIFT:
		keymask = SHIFTKEY_SHIFT;
		break;

	case OP_KEY_CTRL:
		keymask = SHIFTKEY_CTRL;
		break;

	case OP_KEY_ALT:
		keymask = SHIFTKEY_ALT;
		break;

#ifdef OP_KEY_META_ENABLED
	case OP_KEY_META:
		keymask = SHIFTKEY_META;
		break;
#endif // OP_KEY_META_ENABLED
	}

	if ( keydown )
	{
		shiftkeys_state |= keymask;

		if ( !GetKeyStatus(code) )
		{
			g_input_manager->InvokeKeyDown(code, key, NULL, shiftkeys_state, FALSE, OpKey::LOCATION_STANDARD, TRUE);
			SetKeyStatus(code, TRUE);
		}

		g_input_manager->InvokeKeyPressed(code, key, shiftkeys_state, FALSE, TRUE);
	}
	else
	{
		shiftkeys_state &= ~keymask;
		g_input_manager->InvokeKeyUp(code, key, NULL, shiftkeys_state, FALSE, OpKey::LOCATION_STANDARD, TRUE);
		SetKeyStatus(code, FALSE);
	}
}

OP_STATUS
OpScopeExec::HandleKey(const uni_char *key, BOOL keydown)
{
	if (key == 0)
	{
		SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Key value required")); // TODO: Proper error status (input validation?)
		return OpStatus::ERR;
	}
	DoKey(key, keydown);
	return OpStatus::OK;
}

OP_STATUS
OpScopeExec::HandleAction(const uni_char *action, INT32 data, const uni_char *data_string, const uni_char *data_string_param, unsigned window_id)
{
	OpInputAction::Action a;
	OpString8 text;
	RETURN_IF_ERROR(text.Set(action));
	RETURN_IF_ERROR(g_input_manager->GetActionFromString(text.CStr(), &a));

	OP_ASSERT(a != OpInputAction::ACTION_UNKNOWN); // FIXME: Should report error

	if (window_id)
	{
		for (Window* win = g_windowManager->FirstWindow(); win; win = (Window*)win->Suc())
		{
			if (win->Id() == window_id)
			{
				VisualDevice* visdev = win->VisualDev();
				if (visdev)
				{
					g_input_manager->InvokeAction(a, (INTPTR)data, data_string, data_string_param, visdev, NULL, TRUE, OpInputAction::METHOD_OTHER);
					return OpStatus::OK;
				}
			}
		}
		SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Window not found"));
		return OpStatus::ERR;
	}
	else
		g_input_manager->InvokeAction(a, (INTPTR)data, data_string, data_string_param, NULL, NULL, TRUE, OpInputAction::METHOD_OTHER);
	return OpStatus::OK;
}

OP_STATUS OpScopeExec::DoSetupScreenWatcher(const ScreenWatcher &in, unsigned async_tag)
{
	UINT32 timeout;
	int id;
	OpRect rect(0, 0, 200, 100);

	ScreenWatcherCancel();

	timeout = in.GetTimeOut();
	id = in.HasWindowID() ? in.GetWindowID() : 0;
	const Area &a = in.GetArea();
	rect.x = a.GetX();
	rect.y = a.GetY();
	rect.height = a.GetH();
	rect.width = a.GetW();

	for ( unsigned int k = 0; k < in.GetMd5List().GetCount(); k++ )
		ScreenWatcherAddHash(in.GetMd5List().Get(k)->CStr());

	for ( unsigned int l = 0; l < in.GetColorSpecList().GetCount(); ++l )
		RETURN_IF_ERROR(ScreenWatcherAddColspec(l, *in.GetColorSpecList().Get(l)));

	include_image = !in.HasIncludeImage() || in.GetIncludeImage();
	ScreenWatcherEnable(id, rect, timeout, TRUE);
	watch_tag = async_tag;

	return OpStatus::OK;
}

BOOL OpScopeExec::ParseInt(XMLFragment* xml, const uni_char* name, int &value)
{
	BOOL rc = FALSE;

	if ( xml->EnterElement(name) )
	{
		value = static_cast<int>(uni_strtol(xml->GetText(), NULL, 10));
		xml->LeaveElement();
		rc = TRUE;
	}

	return rc;
}

BOOL OpScopeExec::ParseArea(XMLFragment* xml, OpRect &rect, BOOL *document_relative)
{
	BOOL rc = FALSE;
	int x;
	int y;
	int w;
	int h;

	if ( ParseInt(xml, UNI_L("x"), x) &&
		 ParseInt(xml, UNI_L("y"), y) &&
		 ParseInt(xml, UNI_L("w"), w) &&
		 ParseInt(xml, UNI_L("h"), h) )
	{
		rect.x = x;
		rect.y = y;
		rect.width = w;
		rect.height = h;
		rc = TRUE;
	}

	*document_relative = TRUE;
	if (xml->EnterElement(UNI_L("viewport-relative")))
	{
		*document_relative = FALSE;
		xml->LeaveElement();
	}

	xml->LeaveElement();
	return rc;
}

OP_STATUS OpScopeExec::CheckColorSpec(unsigned int color, int index, const uni_char *field_name)
{
	if (color > 255)
	{
		const uni_char *error_format = UNI_L("field colorSpecList[%d].%s out of range [0-255]");
		uni_char error_text[43+11+30+1]; // ARRAY OK 2009-08-07 jhoff
		OpScopeTPError error;

		uni_snprintf(error_text, ARRAY_SIZE(error_text), error_format, index, field_name);
		error.SetStatus(OpScopeTPMessage::BadRequest);
		RETURN_IF_ERROR(error.SetDescription(error_text));
		return SetCommandError(error);
	}
	return OpStatus::OK;
}

OP_STATUS OpScopeExec::ScreenWatcherAddColspec(int index, const ScreenWatcher::ColorSpec &color)
{
	unsigned int low_red,   high_red,
				 low_green, high_green,
				 low_blue,  high_blue;

	low_red    = color.HasRedLow()    ? color.GetRedLow()    : 0;
	high_red   = color.HasRedHigh()   ? color.GetRedHigh()   : 255;
	low_green  = color.HasGreenLow()  ? color.GetGreenLow()  : 0;
	high_green = color.HasGreenHigh() ? color.GetGreenHigh() : 255;
	low_blue   = color.HasBlueLow()   ? color.GetBlueLow()   : 0;
	high_blue  = color.HasBlueHigh()  ? color.GetBlueHigh()  : 255;

	RETURN_IF_ERROR(CheckColorSpec(low_red,    index, UNI_L("redLow")));
	RETURN_IF_ERROR(CheckColorSpec(high_red,   index, UNI_L("redHigh")));
	RETURN_IF_ERROR(CheckColorSpec(low_green,  index, UNI_L("greenLow")));
	RETURN_IF_ERROR(CheckColorSpec(high_green, index, UNI_L("greenHigh")));
	RETURN_IF_ERROR(CheckColorSpec(low_blue,   index, UNI_L("blueLow")));
	RETURN_IF_ERROR(CheckColorSpec(high_blue,  index, UNI_L("blueHigh")));

	OP_STATUS status = ScreenWatcherAddColspec(color.GetId(),
												low_red,   high_red,
												low_green, high_green,
												low_blue,  high_blue);
	if (status == OpStatus::ERR_OUT_OF_RANGE)
		return SetCommandError(OpScopeTPMessage::BadRequest, UNI_L("The maximum colorspec count of 16 has been reached"));
	return status;
}
#undef COLOR_SPEC_CHECK

void OpScopeExec::ScreenWatcherAddHash(const uni_char* hash)
{
	OpString* str = new OpString;
	if ( str != 0 )
	{
		RETURN_VOID_IF_ERROR(str->Set(hash));
		watch_hashes.Add(str);
	}
}

OP_STATUS OpScopeExec::ScreenWatcherAddColspec(int id,
											   unsigned int low_red,
											   unsigned int high_red,
											   unsigned int low_green,
											   unsigned int high_green,
											   unsigned int low_blue,
											   unsigned int high_blue)
{
	if ( colspec_count >= EXEC_MAX_COLOR_SPECS )
		return OpStatus::ERR_OUT_OF_RANGE;

	colspec[colspec_count].id = id;

	colspec[colspec_count].low_red = low_red;
	colspec[colspec_count].high_red = high_red;
	colspec[colspec_count].low_green = low_green;
	colspec[colspec_count].high_green = high_green;
	colspec[colspec_count].low_blue = low_blue;
	colspec[colspec_count].high_blue = high_blue;

	colspec_count++;
	return OpStatus::OK;
}

void OpScopeExec::ScreenWatcherCancel(void)
{
	watch_window_enabled = FALSE;
	if ( watch_window_timer_running )
		watch_window_timer.Stop();
	watch_window_timer_running = FALSE;
	for ( unsigned int k = 0; k < watch_hashes.GetCount(); k++ )
		OP_DELETE(watch_hashes.Get(k));
	watch_hashes.Clear();
	colspec_count = 0;
	colspecreport_count = 0;
	if (watch_tag != 0)
	{
		// Send back previous response since it was cancelled
		ScreenWatcherResult watch;
		watch.SetWindowID(0);
		watch.SetMd5(UNI_L(""));
		RETURN_VOID_IF_ERROR(SendSetupScreenWatcher(watch, watch_tag));
		watch_tag = 0;
	}
}

void OpScopeExec::ScreenWatcherEnable(int windowid, const OpRect &rect,
									  UINT32 timeout, BOOL document_relative)
{
	watch_window_enabled = TRUE;
	watch_window_id = windowid;
	watch_window_rect = rect;
	watch_document_relative = document_relative;
	watch_window_last_painted_id = 0;
	if ( timeout != 0 )
	{
		watch_window_timer.Start(timeout);
		watch_window_timer_running = TRUE;
	}
	watch_tag = 0;

	// Send a 'fake' message when setting up a new screen-watcher so a
	// match with what is already on the screen will be instant.
	// This does however rely on giving the correct window-ID.
	if ( windowid )
		g_main_message_handler->PostMessage(MSG_SCOPE_WINDOW_PAINTED,
										windowid, 0, 0);
}

void OpScopeExec::WindowPainted(Window* win, OpRect rect)
{
	// Don't trigger for our own Paint events, or when not enabled
	if ( !watch_window_enabled || is_grabbing_bitmap || !IsEnabled() )
		return;

	if ( watch_window_id == 0 || watch_window_id == win->Id() )
	{
		// We are watching this window
		if ( rect.Intersecting(watch_window_rect) || watch_document_relative == FALSE )
		{
			g_main_message_handler->PostMessage(MSG_SCOPE_WINDOW_PAINTED,
											win->Id(), 0, 0);
		}
	}
}

OP_STATUS
OpScopeExec::DoExec(const ActionList &in)
{
	const OpProtobufMessageVector<ActionList::Action> &actions = in.GetActionList();
	int count = actions.GetCount();
	for (int i = 0; i < count; ++i)
	{
		const ActionList::Action *action = actions.Get(i);
		OP_ASSERT(action != NULL);
		if (action->GetName().Compare(UNI_L("_keydown")) == 0)
			RETURN_IF_ERROR(HandleKey(action->GetValue().CStr(), TRUE));
		else if (action->GetName().Compare(UNI_L("_keyup")) == 0)
			RETURN_IF_ERROR(HandleKey(action->GetValue().CStr(), FALSE));
		else if (action->GetName().Compare(UNI_L("_type")) == 0)
			RETURN_IF_ERROR(HandleType(action->GetValue().CStr()));
		else
			RETURN_IF_ERROR(HandleAction(action->GetName().CStr(),
			                             action->HasData() ? action->GetData() : 0,
			                             action->HasValue() ? action->GetValue().CStr() : NULL,
			                             action->HasStringParam() ? action->GetStringParam().CStr() : NULL,
			                             action->GetWindowID()));
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeExec::DoGetActionInfoList(ActionInfoList &out)
{
	OpProtobufMessageVector<ActionInfoList::ActionInfo> &action_info_list = out.GetActionInfoListRef();
	for (INT32 i = OpInputAction::ACTION_UNKNOWN + 1; i < OpInputAction::LAST_ACTION; i++)
	{
		const char* action_string = OpInputAction::GetStringFromAction((OpInputAction::Action) i);

		if (action_string)
		{
			OpStackAutoPtr<ActionInfoList::ActionInfo> action_info(OP_NEW(ActionInfoList::ActionInfo, ()));
			RETURN_OOM_IF_NULL(action_info.get());
			RETURN_IF_ERROR(action_info->SetName(action_string));
			RETURN_IF_ERROR(action_info_list.Add(action_info.get()));
			action_info.release();
		}
	}

	// Add build-ins
	for ( unsigned j = 0; j < 3; j++ )
	{
		OpStackAutoPtr<ActionInfoList::ActionInfo> action_info(OP_NEW(ActionInfoList::ActionInfo, ()));
		RETURN_OOM_IF_NULL(action_info.get());
		const uni_char *str = 0;
		if (j==0)      str = UNI_L("_keydown");
		else if (j==1) str = UNI_L("_keyup");
		else if (j==2) str = UNI_L("_type");
		RETURN_IF_ERROR(action_info->SetName(str));
		RETURN_IF_ERROR(action_info_list.Add(action_info.get()));
		action_info.release();
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeExec::DoSendMouseAction(const MouseAction &in)
{
	OpPoint point;

	point.x = in.GetX();
	point.y = in.GetY();
	return DoMouseAction(in.GetWindowID(), point, in.GetButtonAction());
}

OP_STATUS OpScopeExec::AddImage(ByteBuffer &buffer, OpBitmap* bitmap)
{
	OP_ASSERT(bitmap != 0);

	OP_STATUS rc = OpStatus::ERR_NO_MEMORY;
	PngEncFeeder feeder;
	PngEncRes res;

	minpng_init_encoder_result(&res);
	minpng_init_encoder_feeder(&feeder);

	feeder.has_alpha = 0;
	feeder.compressionLevel = Z_BEST_SPEED;
	feeder.xsize = bitmap->Width();
	feeder.ysize = bitmap->Height();
	feeder.scanline_data = OP_NEWA(UINT32, bitmap->Width());

	if ( feeder.scanline_data != 0 )
	{
		PngEncRes::Result status = PngEncRes::NEED_MORE;
		int y = 0;

		while ( status == PngEncRes::AGAIN || status == PngEncRes::NEED_MORE )
		{
			if ( status == PngEncRes::NEED_MORE )
			{
				feeder.scanline = y++;
				OP_ASSERT(feeder.scanline < bitmap->Height());
				bitmap->GetLineData(feeder.scanline_data, feeder.scanline);
			}

			status = minpng_encode(&feeder, &res);

			if ( res.data_size )
			{
				rc = buffer.AppendBytes((char*)res.data, res.data_size);
				if ( OpStatus::IsError(rc) )
					goto finish;
			}

			minpng_clear_encoder_result(&res);
		}

		if ( status == PngEncRes::OK )
			rc = OpStatus::OK;
		else if ( status == PngEncRes::OOM_ERROR )
			rc = OpStatus::ERR_NO_MEMORY;
		else
			rc = OpStatus::ERR;
	}

 finish:
	OP_DELETEA(feeder.scanline_data);
	minpng_clear_encoder_feeder(&feeder);
	return rc;
}

OP_STATUS OpScopeExec::CalculateHash(OpString &hash, OpBitmap* bitmap)
{
	OP_ASSERT(bitmap != 0);

	unsigned int width = bitmap->Width();
	unsigned int height = bitmap->Height();

	OP_STATUS rc = OpStatus::ERR_NO_MEMORY;

	UINT32* scanline = OP_NEWA(UINT32, width);
	UINT8* rgb = OP_NEWA(UINT8, width*3);
	CryptoHash* md5 = CryptoHash::CreateMD5();
	rc = md5->InitHash();

	if ( rc == OpStatus::OK && scanline != 0 && rgb != 0 && md5 != 0 )
	{
		for ( unsigned int y = 0; y < height; y++ )
		{
			bitmap->GetLineData(scanline, y);
			UINT8* ptr = rgb;
			for ( unsigned int x = 0; x < width; x++ )
			{
				UINT32 color = scanline[x]; // 0xAARRGGBB
				*ptr++ = (color >> 16);
				*ptr++ = (color >>  8);
				*ptr++ = (color      );
			}

			md5->CalculateHash(rgb, width * 3);
		}

		OP_ASSERT(md5->Size() == 16);	// Always the case for MD5
		UINT8 sum[16];					// ARRAY OK 2009-04-29 mortenro
		md5->ExtractHash(sum);

		uni_char buffer[40];			// ARRAY OK 2009-04-28 mortenro
		uni_char tmp[4];				// ARRAY OK 2009-04-28 mortenro
		uni_strcpy(buffer, UNI_L("0x"));
		for ( int k = 0; k < 16; k++ )
		{
			uni_sprintf(tmp, UNI_L("%02x"), sum[k]);
			uni_strcat(buffer, tmp);
		}

		rc = hash.Set(buffer);
	}

	OP_DELETEA(scanline);
	OP_DELETEA(rgb);
	OP_DELETE(md5);

	return rc;
}

OP_STATUS OpScopeExec::CalculateColors(OpBitmap* bitmap)
{
	OP_ASSERT(bitmap != 0);

	// Reset counts before reporting
	colspecreport_count = 0;
	colspec_total_matches = 0;
	for ( unsigned int k = 0; k < colspec_count; k++ )
	{
		colspec[k].count = 0;
		colspec[k].reported = FALSE;
	}

	// Don't count anything if no colspecs
	if ( colspec_count == 0 )
		return OpStatus::OK;

	unsigned int width = bitmap->Width();
	unsigned int height = bitmap->Height();

	OP_STATUS rc = OpStatus::ERR_NO_MEMORY;

	UINT32* scanline = OP_NEWA(UINT32, width);

	//
	// The cached color is for the latest color that was not covered
	// by any of the color classes, typically the background color.
	// caching it speeds up counting, probably a lot depending on the
	// color classes defined.
	//
	UINT32 cached_color = 0;
	BOOL cached_color_valid = FALSE;

	if ( scanline != 0 )
	{
		for ( unsigned int y = 0; y < height; y++ )
		{
			if ( !bitmap->GetLineData(scanline, y) )
				goto done;

			for ( unsigned int x = 0; x < width; x++ )
			{
				UINT32 color = scanline[x] & 0xffffff; // 0xAARRGGBB

				if ( cached_color_valid && color == cached_color )
					continue;

				//
				// Count the color if needed, and update the color cache
				// if it was not interesting.
				//
				if ( !CountColor(color) )
				{
					cached_color = color;
					cached_color_valid = TRUE;
				}
			}
		}

		//
		// Map the counts for all the colspecs into id's, so two
		// colspecs with same id is reported summed for the id.
		//
		for ( unsigned int k = 0; k < colspec_count; k++ )
		{
			if ( !colspec[k].reported )
			{
				int id = colspec[k].id;
				unsigned int count = 0;
				for ( unsigned int m = k; m < colspec_count; m++ )
				{
					if ( colspec[m].id == id )
					{
						count += colspec[m].count;
						colspec[m].reported = TRUE;
					}
				}

				colspecreport[colspecreport_count].id = id;
				colspecreport[colspecreport_count].count = count;
				colspec_total_matches += count;
				colspecreport_count++;
			}
		}

		rc = OpStatus::OK;
	}

 done:
	OP_DELETEA(scanline);

	return rc;
}

BOOL OpScopeExec::CountColor(UINT32 color)
{
	unsigned int red =   (color >> 16) & 0xff;
	unsigned int green = (color >>  8) & 0xff;
	unsigned int blue =  (color      ) & 0xff;

	BOOL rc = FALSE;

	for ( unsigned int k = 0; k < colspec_count; k++ )
	{
		if ( colspec[k].low_red <= red && colspec[k].high_red >= red &&
			 colspec[k].low_green <= green && colspec[k].high_green >= green &&
			 colspec[k].low_blue <= blue && colspec[k].high_blue >= blue )
		{
			// A match for this color-spec
			rc = TRUE;
			colspec[k].count++;
		}
	}

	return rc;
}

OP_STATUS OpScopeExec::CaptureScreenDump(OpBitmap* &bitmap, Window* win, const OpRect &rect, BOOL document_relative)
{
	OP_STATUS rc;
	bitmap = 0;

	rc = OpBitmap::Create(&bitmap, rect.width, rect.height,
						  FALSE, FALSE, 0, 0, TRUE);
	if ( OpStatus::IsError(rc) )
		return rc;

	OpPainter* painter = bitmap->GetPainter();
	VisualDevice *vis_dev = win->VisualDev();

	if ( painter == 0 || vis_dev == 0 )
	{
		OP_DELETE(bitmap);
		bitmap = 0;
		return OpStatus::ERR_NULL_POINTER;
	}

	UINT32 old_scale = vis_dev->SetTemporaryScale(100);

	int shift_x = rect.x;
	int shift_y = rect.y;

	if (document_relative) // alternative is viewport relative
	{
		int viewport_x = vis_dev->GetRenderingViewX();
		int viewport_y = vis_dev->GetRenderingViewY();

		is_grabbing_bitmap = TRUE;

		shift_x -= viewport_x;
		shift_y -= viewport_y;
	}

	OpRect area;
	area.x = 0;
	area.y = 0;
	area.width = rect.width;
	area.height = rect.height;

	vis_dev->TranslateView(shift_x, shift_y);
	vis_dev->GetContainerView()->Paint(area, painter, 0, 0, TRUE, FALSE);
	vis_dev->TranslateView(-shift_x, -shift_y);
	vis_dev->SetTemporaryScale(old_scale);

	bitmap->ReleasePainter();

	is_grabbing_bitmap = FALSE;

	return OpStatus::OK;
}

#ifndef MOUSELESS
OP_STATUS OpScopeExec::DoMouseAction(unsigned int window_id, const OpPoint point,
							  unsigned int actions)
{
	//
	// A mouse action
	//
	// They are controlled through a bit-field in the 'actions' variable:
	//
	// Bit 0: Button 1 down
	// Bit 1: Button 1 up
	//
	// Bit 2: Button 2 down
	// Bit 3: Button 2 up
	//
	// Bit 4: Button 3 down
	// Bit 5: Button 4 down
	//
	// There is always an implied 'mouse move' before any button clicking
	// is performed. Not clicking (or unclicking) any buttons will cause
	// just a mouse move.
	//

	Window* win = g_windowManager->FirstWindow();

	// FIXME: There should be a common method for this somewhere
	while ( win != 0 )
	{
		if ( win->Id() == window_id )
			break;
		win = (Window*)win->Suc();
	}

	if ( win != 0 )
	{
		// FIXME: Verify if asserts below are indeed invariants

		VisualDevice* vis_dev = win->VisualDev();
		OP_ASSERT(vis_dev != 0);

		CoreView* cview = vis_dev->GetContainerView();
		OP_ASSERT(cview != 0);

		OpView* vis_dev_view = vis_dev->GetOpView();
		OP_ASSERT(vis_dev_view);

		CoreViewContainer* container = cview->GetContainer();
		OP_ASSERT(container != 0);

		//
		// FIXME: Should we do a synthetic OnMouseLeave() here if we
		// move to another window?
		//

		if ( mouse_last_window_id != window_id ||
			 mouse_last_x != point.x || mouse_last_y != point.y )
		{
			// Mouse has moved, do a mouse move event first
			container->OnMouseMove(point, SHIFTKEY_NONE, vis_dev_view);
		}

		if ( actions & 1 )
			container->OnMouseDown(MOUSE_BUTTON_1, 1, SHIFTKEY_NONE, vis_dev_view);
		if ( actions & 2 )
			container->OnMouseUp(MOUSE_BUTTON_1, SHIFTKEY_NONE, vis_dev_view);

		if ( actions & 4 )
			container->OnMouseDown(MOUSE_BUTTON_2, 1, SHIFTKEY_NONE, vis_dev_view);
		if ( actions & 8 )
			container->OnMouseUp(MOUSE_BUTTON_2, SHIFTKEY_NONE, vis_dev_view);

		if ( actions & 16 )
			container->OnMouseDown(MOUSE_BUTTON_3, 1, SHIFTKEY_NONE, vis_dev_view);
		if ( actions & 32 )
			container->OnMouseUp(MOUSE_BUTTON_3, SHIFTKEY_NONE, vis_dev_view);
		return OpStatus::OK;
	}
	return OpStatus::ERR_NO_SUCH_RESOURCE;
}
#endif // !MOUSELESS

#endif // SCOPE_EXEC_SUPPORT
