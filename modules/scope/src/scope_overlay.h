/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_OVERLAY_H
#define SCOPE_OVERLAY_H

#ifdef SCOPE_OVERLAY

#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/generated/g_scope_overlay_interface.h"
#include "modules/display/vis_dev.h"
#include "modules/otl/list.h"
#include "modules/util/OpHashTable.h"

class OpScopeOverlay
	: public OpScopeOverlay_SI
{
public:
	virtual ~OpScopeOverlay() { }

	// OpScopeService
	virtual OP_STATUS OnServiceDisabled();

	// OpScopeOverlay_SI
	virtual OP_STATUS DoCreateOverlay(const CreateOverlayArg &in, OverlayID &out);
	virtual OP_STATUS DoRemoveOverlay(const RemoveOverlayArg &in);

private:
	/** Convert Color struct (individual RGBA values in the 0-255 range) to a COLORREF. */
	static COLORREF ToCOLORREF(const Color &c);

	/** Convert scope insertion method to an enum value used by the visual device. */
	static VisualDevice::InsertionMethod ConvertInsertionMethodType(const InsertionMethod method);

	/**
	 * Find a Window associated with a given ID.
	 *
	 * @param id The id of the window to find.
	 * @param[out] window The window pointer set on success.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR otherwise.
	 */
	OP_STATUS FindWindow(unsigned int id, Window *&window);

	/**
	 * Register a new overlay.
	 *
	 * @param window The window with which the overlay should be associated with.
	 * @param spotlight The spotlight to register.
	 * @param[out] overlay_id The id of the spotlight on success.
	 *
	 * @return OpStatus::OK if the overlay was registered successfully. Generates
	 *         a new id and sets overlay_id. OpStatus::ERR or OpStatus::ERR_NO_MEMORY
	 *         on failure.
	 */
	OP_STATUS RegisterOverlay(Window *window, VDSpotlight *spotlight, unsigned int &overlay_id);

	/**
	 * Remove and unregister the overlay.
	 *
	 * @param window The window for which given ID was registered.
	 * @param overlay_id The overlay ID to unregister.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NULL_POINTER if given
	 *         window does not have document associated.
	 */
	OP_STATUS UnregisterOverlay(Window *window, unsigned int overlay_id);

	// Last used spotlight id. Increased for each overlay that was registered.
	unsigned int m_last_spotlight_id;

	// List of spotlights that were registered for a specific window.
	OpAutoPointerHashTable< Window, OtlList<unsigned int> > m_window_to_spotlight_list;
};

#endif // SCOPE_OVERLAY
#endif // SCOPE_OVERLAY_H
