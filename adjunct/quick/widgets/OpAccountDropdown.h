/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_ACCOUNT_DROPDOWN_H
#define OP_ACCOUNT_DROPDOWN_H

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/listeners.h"
#include "modules/widgets/OpDropDown.h"

/***********************************************************************************
**
**	OpAccountDropDown
**
***********************************************************************************/

class OpAccountDropDown : public OpDropDown, public EngineListener
{
	protected:
								OpAccountDropDown();

	public:

		static OP_STATUS Construct(OpAccountDropDown** obj);

		virtual void			OnDeleted();
		virtual void			OnSettingsChanged(DesktopSettings* settings);

		// OpWidgetListener

		virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) {((OpWidgetListener*)GetParent())->OnDragMove(this, drag_object, pos, x, y);}
		virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) {((OpWidgetListener*)GetParent())->OnDragDrop(this, drag_object, pos, x, y);}
		virtual void			OnDragLeave(OpWidget* widget, OpDragObject* drag_object) {((OpWidgetListener*)GetParent())->OnDragLeave(this, drag_object);}
		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
		virtual BOOL            OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
		{
			OpWidgetListener* listener = GetParent() ? GetParent() : this;
			return listener->OnContextMenu(this, child_index, menu_point, avoid_rect, keyboard_invoked);
		}

		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

		// Implementing the OpTreeModelItem interface

		virtual Type			GetType() {return WIDGET_TYPE_ACCOUNT_DROPDOWN;}

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);

		// == MessageEngine::EngineListener =================

		virtual void			OnProgressChanged(const ProgressInfo& progress, const OpStringC& progress_text) {}
		virtual void			OnImporterProgressChanged(const Importer* importer, const OpStringC& infoMsg, OpFileLength current, OpFileLength total, BOOL simple = FALSE) {}
		virtual void			OnImporterFinished(const Importer* importer, const OpStringC& infoMsg) {}
		virtual void			OnIndexChanged(UINT32 index_id) {}
		virtual void			OnActiveAccountChanged();
		virtual void			OnReindexingProgressChanged(INT32 progress, INT32 total) {}
		virtual void			OnNewMailArrived() {}

	private:

		void					PopulateAccountDropDown();
};

#endif
#endif // OP_ACCOUNT_DROPDOWN_H
