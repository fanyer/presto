/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef DRAG_SUPPORT
# include "modules/dragdrop/dragdrop_manager.h"
# include "modules/dragdrop/src/private_data.h"
# include "modules/dragdrop/src/events_manager.h"
# include "modules/dom/domeventtypes.h"
# include "modules/doc/frm_doc.h"
# include "modules/doc/html_doc.h"
# include "modules/dochand/win.h"
# include "modules/pi/OpDragObject.h"

OP_STATUS
DragDropEventsManager::RecordAction(HTML_Document* doc, DOM_EventType event, int x, int y, int offset_x, int offset_y, int visual_viewport_x, int visual_viewport_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed)
{
	OP_ASSERT(doc);
	if (RecordedDragAction *recorded_drag_action = OP_NEW(RecordedDragAction, (doc, event, x, y, visual_viewport_x, visual_viewport_y, offset_x, offset_y, shift_pressed, control_pressed, alt_pressed)))
		recorded_drag_action->Into(&m_recorded_dnd_actions);
	else
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

void
DragDropEventsManager::ReplayRecordedDragActions()
{
	OP_ASSERT(m_replay_next_action);
	m_replay_next_action = FALSE;
	if ((m_recording_drag_actions == 0 || --m_recording_drag_actions == 0) && !m_replaying_recorded_drag_actions)
	{
		m_replaying_recorded_drag_actions = TRUE;

		while (RecordedDragAction *drag_action = m_recorded_dnd_actions.First())
		{
			drag_action->Out();

			OP_ASSERT(drag_action->GetActionDocument());

			HTML_Document* action_doc = drag_action->GetActionDocument();

			if (action_doc)
			{
				DOM_EventType event = drag_action->GetActionType();

				if (!action_doc->GetFramesDocument()->IsUndisplaying())
					drag_action->Action();
				else
				{
					// If we're going to lose ONDROP/ONDRAGEND we need to make sure the drag gets ended.
					if (event == ONDROP || event == ONDRAGEND)
					{
						DOM_EventType ending_event = (event == ONDROP) ? ONDRAGEND : DOM_EVENT_NONE;
						EnsureDragEnds(action_doc, drag_action->GetX(), drag_action->GetY(), drag_action->GetVisualViewportX(), drag_action->GetVisualViewportY(), drag_action->GetOffsetX(), drag_action->GetOffsetY(), drag_action->ShiftPressed(), drag_action->CtrlPressed(), drag_action->AltPressed(), ending_event);
					}
				}

				/* ONDRAGLEAVE action may cause the next one is replayed and the next one might be ONDRAGEND
				 This is why we need to check if we are still dragging before doing anything more. */
				if (event == ONDRAGLEAVE && g_drag_manager->IsDragging())
				{
					OpDragObject* drag_object = g_drag_manager->GetDragObject();
					OP_ASSERT(drag_object);
					PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
					OP_ASSERT(priv_data);
					if (action_doc != priv_data->GetTargetDocument())
					{
						// The target we'll enter should change this.
						action_doc->SetCurrentTargetElement(NULL);
						action_doc->SetImmediateSelectionElement(NULL);
						action_doc->SetPreviousImmediateSelectionElement(NULL);
						action_doc->GetWindow()->UseDefaultCursor();
						drag_object->SetVisualDropType(DROP_NONE);
						drag_object->SetDropType(DROP_NONE);
					}
				}
			}

			OP_DELETE(drag_action);

			if (m_recording_drag_actions != 0)
				break;
		}

		m_replaying_recorded_drag_actions = FALSE;
	}
}

void
DragDropEventsManager::Reset()
{
	m_recording_drag_actions = 0;
	m_replaying_recorded_drag_actions = FALSE;
	m_replay_next_action = FALSE;
	m_recorded_dnd_actions.Clear();
}

void
DragDropEventsManager::CancelPending()
{
	m_recorded_dnd_actions.Clear();
}

void
DragDropEventsManager::OnDocumentUnload(HTML_Document* doc)
{
	BOOL done;
	BOOL check_doc = TRUE;
	BOOL ensured_end = FALSE;
	BOOL drag_enter_over = FALSE;
	do
	{
		done = TRUE;
		RecordedDragAction* iter = m_recorded_dnd_actions.First();
		while (iter)
		{
			DOM_EventType event = iter->GetActionType();
			HTML_Document* action_document = iter->GetActionDocument();
			RecordedDragAction* next = iter->Suc();
			if (action_document == doc || !check_doc)
			{
				iter->Out();
				// If we're going to lose ONDROP/ONDRAGEND we need to make sure the drag gets ended.
				if (event == ONDROP || event == ONDRAGEND)
				{
					DOM_EventType ending_event;
					if (action_document == doc)
						ending_event = (event == ONDROP) ? ONDRAGEND : DOM_EVENT_NONE;
					else
						ending_event = event;
					EnsureDragEnds(action_document, iter->GetX(), iter->GetY(), iter->GetVisualViewportX(), iter->GetVisualViewportY(), iter->GetOffsetX(), iter->GetOffsetY(), iter->ShiftPressed(), iter->CtrlPressed(), iter->AltPressed(), ending_event);
					ensured_end = TRUE;
					next = NULL;
				}

				OP_DELETE(iter);
			}
			else
			{
				if (event == ONDRAG || event == ONDRAGENTER || event == ONDRAGOVER)
					drag_enter_over = TRUE;
			}
			iter = next;
		}

		if (!ensured_end && !drag_enter_over && m_recorded_dnd_actions.First())
		{
			// There are no ONDRAG/ONDRAGENTER/ONDRAGOVER events which drive
			// the whole d'n'd but there are some other ones.  Remove them as
			// well as there's no point in keeping them and this may even lead
			// to the queue never being cleaned up.
			done = FALSE;
			check_doc = FALSE;
		}

	} while (!done);
}

void
DragDropEventsManager::OnOOM()
{
	RecordedDragAction* iter = m_recorded_dnd_actions.First();
	while (iter)
	{
		DOM_EventType event = iter->GetActionType();
		RecordedDragAction* next = iter->Suc();
		iter->Out();
		// If we're going to lose ONDROP/ONDRAGEND we need to make sure the drag gets ended.
		if (event == ONDROP || event == ONDRAGEND)
		{
			// This will clean the rest of the queue so no need to continue iterating.
			g_drag_manager->StopDrag();
			return;
		}
		OP_DELETE(iter);
		iter = next;
	}

	// The queue is empty now. Reset all the flags.
	m_recording_drag_actions = 0;
	m_replaying_recorded_drag_actions = FALSE;
	m_replay_next_action = FALSE;
}

void
DragDropEventsManager::EnsureDragEnds(HTML_Document* doc, int x, int y, int visual_viewport_x, int visual_viewport_y, int offset_x, int offset_y, BOOL shift, BOOL ctrl, BOOL alt, DOM_EventType ending_event)
{
	OP_ASSERT(ending_event == ONDRAGEND || ending_event == ONDROP || ending_event == DOM_EVENT_NONE);
	CancelPending();
	if (ending_event == ONDRAGEND)
	{
		OpDragObject* drag_object = g_drag_manager->GetDragObject();
		OP_ASSERT(drag_object);
		drag_object->SetDropType(DROP_NONE);
		PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
		OP_ASSERT(priv_data);
		HTML_Element* dnd_source_element = priv_data->GetSourceHtmlElement();
		HTML_Document* src_doc = priv_data->GetElementsDocument(dnd_source_element);
		if (src_doc)
			g_drag_manager->DragAction(src_doc, ONDRAGEND, x, y, visual_viewport_x, visual_viewport_y, offset_x, offset_y, shift, ctrl, alt);
		else // We couldn't send ONDRAGEND event which would end and clean up d'n'd so we have to do it here.
			g_drag_manager->StopDrag();
	}
	else if (ending_event == ONDROP)
		g_drag_manager->DragAction(doc, ONDROP, x, y, visual_viewport_x, visual_viewport_y, offset_x, offset_y, shift, ctrl, alt);
	else // We couldn't send any d'n'd event which would end and clean up d'n'd so we have to do it here.
		g_drag_manager->StopDrag();

	doc->GetWindow()->UseDefaultCursor();
}

BOOL
DragDropEventsManager::RecordedDragAction::Action()
{
	return g_drag_manager->DragAction(document, event, x, y, visual_viewport_x, visual_viewport_y, offset_x, offset_y, shift_pressed, control_pressed, alt_pressed);
}
#endif // DRAG_SUPPORT
