/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef UNIX_OPPLUGINTRANSLATOR_H_
#define UNIX_OPPLUGINTRANSLATOR_H_

#include "modules/pi/OpPluginWindow.h"
#include "modules/hardcore/component/OpMessenger.h"
#include "modules/ns4plugins/src/plugincommon.h"
#include "modules/pi/OpPluginTranslator.h"
#include <gtk/gtk.h>

class OpPluginParentChangedMessage;

/// Implements OpPluginTranslator for Unix.
class UnixOpPluginTranslator : public OpPluginTranslator, public OpMessageListener
{
public:
	UnixOpPluginTranslator(X11Types::Display* display, X11Types::Visual* visual, const int screen_number, const X11Types::Window window, OpChannel* channel);

	~UnixOpPluginTranslator();

	void PluginGtkPlugAdded();

	virtual OP_STATUS FinalizeOpPluginImage(OpPluginImageID image, const NPWindow&);

	/// Calculate the original 32-bit colour value based on two rendering, one
	/// on black background and the other on white background. Note that the
	/// RGBA order shouldn't matter. More information on the algorithm is
	/// attached to the implementation.
	/// \param color_on_black  Colour (32-bit) rendered on black background
	/// \param color_on_white  Colour (32-bit) rendered on white background
	/// \return Calculated original 32-bit colour value
	unsigned long ReverseAlphaBlending(const unsigned long on_black, const unsigned long on_white);

	virtual OP_STATUS UpdateNPWindow(NPWindow& out_npwin, const OpRect& rect, const OpRect& clip_rect, NPWindowType type);

	virtual OP_STATUS CreateFocusEvent(OpPlatformEvent** event, bool focus_in);

	/**
	 *  Creates a sequence of a single UnixOpPlatformEvent which wraps one XKeyEvent.
	 *
	 * The XKeyEvent is restored from the specified arguments data1, data2, which were
	 * extracted from the original XKeyEvent in X11PlatformKeyEventData.
	 * @see X11PlatformKeyEventData::GetPluginPlatformData()
	 *
	 * @param data1 is the \c XKeyEvent::state of the original XKeyEvent.
	 * @param data2 is the \c XKeyEvent::keycode of the original XKeyEvent.
	 */
	virtual OP_STATUS CreateKeyEventSequence(OtlList<OpPlatformEvent*>& events, OpKey::Code key, uni_char value, OpPluginKeyState key_state, ShiftKeyState modifiers, UINT64 data1, UINT64 data2);

	virtual OP_STATUS CreateMouseEvent(OpPlatformEvent** event,
	                                   OpPluginEventType event_type,
	                                   const OpPoint& point,
	                                   int button,
	                                   ShiftKeyState modifiers);

	virtual OP_STATUS CreatePaintEvent(OpPlatformEvent** event,
	                                   OpPluginImageID image,
	                                   OpPluginImageID background,
	                                   const OpRect& paint_rect,
	                                   NPWindow* npwin,
	                                   bool* npwindow_was_modified);

	/// Not used on Unix.
	virtual OP_STATUS CreateWindowPosChangedEvent(OpPlatformEvent** event);

	virtual bool GetValue(NPNVariable variable, void* value, NPError* result);
	virtual bool SetValue(NPPVariable variable, void* value, NPError* result);

	virtual bool Invalidate(NPRect* rect) { return false; }

	virtual bool ConvertPlatformRegionToRect(NPRegion invalidRegion, NPRect &invalidRect);

	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);

protected:
	OP_STATUS CreateMouseButtonEvent(OpPlatformEvent** e,
	                                 const OpPoint& pt,
	                                 const int button,
	                                 const ShiftKeyState mod,
	                                 const int type);

	OP_STATUS CreateMouseCrossingEvent(OpPlatformEvent** e,
	                                   const OpPoint& pt,
	                                   const int type);

	/// Fill out event structure fields shared between key, button,
	/// motion and crossing events.
	/// \param[out] event       For storing platform-specific event object
	/// \param      pt          The position of the pointer relative to the document body
	OP_STATUS SetPointEventFields(XEvent* event, const OpPoint& pt);

	OP_STATUS OnPluginParentChangedMessage(OpPluginParentChangedMessage*);

	const int m_screen_number;               ///< The screen number the plugin window is connected to.
	X11Types::Window m_browser_window;       ///< The browser's window ID.
	GtkWidget* m_gtksocket;                  ///< A GtkSocket for the plugin to attach a window to.
	GtkWidget* m_gtkplug;                    ///< A window to contain our GtkSocket (and its child).
	OpChannel* m_channel;                    ///< Communication channel for exchanging messages with the browser process.
	NPSetWindowCallbackStruct m_window_info; ///< Structure storing information about the plug-in's Unix window environment.
};

#endif  // UNIX_OPPLUGINTRANSLATOR_H_
