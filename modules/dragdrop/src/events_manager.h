/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DRAGDROPEVENTSMANAGER_H
# define DRAGDROPEVENTSMANAGER_H

# include "modules/dom/domeventtypes.h"

class HTML_Document;

# ifdef DRAG_SUPPORT
/**
 * A class managing drag'n'drop events recording (queuing) and replaying.
 */
class DragDropEventsManager
{
private:
	/** A flag indicating if the recorder is replaying some action at the moment. */
	BOOL			m_replaying_recorded_drag_actions;
	/** A counter which is increased when handling any action to know that the next ones must be recorded. */
	unsigned		m_recording_drag_actions;
	/** A flag indicating if the next action should be replayed. */
	BOOL			m_replay_next_action;

	/** Class representing single recorded action. */
	class RecordedDragAction : public ListElement<RecordedDragAction>
	{
	private:
		HTML_Document* document;
		DOM_EventType event;
		int x, y;
		int visual_viewport_x, visual_viewport_y;
		int offset_x, offset_y;
		BOOL shift_pressed, control_pressed, alt_pressed;

	public:
		RecordedDragAction(HTML_Document* doc, DOM_EventType event, int x, int y, int visual_viewport_x, int visual_viewport_y, int offset_x, int offset_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed)
			: document(doc)
			, event(event)
			, x(x)
			, y(y)
			, visual_viewport_x(visual_viewport_x)
			, visual_viewport_y(visual_viewport_y)
			, offset_x(offset_x)
			, offset_y(offset_y)
			, shift_pressed(shift_pressed)
			, control_pressed(control_pressed)
			, alt_pressed(alt_pressed)
			{}

		DOM_EventType	GetActionType() { return event; }
		BOOL			Action();
		HTML_Document*	GetActionDocument() { return document; }
		int				GetX() { return x; }
		int				GetY() { return y; }
		int				GetVisualViewportX() { return visual_viewport_x; }
		int				GetVisualViewportY() { return visual_viewport_y; }
		int				GetOffsetX() { return offset_x; }
		int				GetOffsetY() { return offset_y; }
		BOOL			ShiftPressed() { return shift_pressed; }
		BOOL			CtrlPressed() { return control_pressed; }
		BOOL			AltPressed() { return alt_pressed; }
	};

	/** A list of recorded actions. */
	AutoDeleteList<RecordedDragAction>
						m_recorded_dnd_actions;

	/**
	 * Makes sure the drop ends correctly.
	 *
	 * @param doc - the document the drag ends in.
	 * @param x - the x coordinate where the drag ended (in document coordinates).
	 * @param y - the y coordinate where the drag ended (in document coordinates).
	 * @param[in] visual_viewport_x The x offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's x offset and not the rendering view port's one.
	 * @see FramesDocument::GetVisualViewport()
	 * @see FramesDocument::GetVisualViewportX()
	 * @param[in] visual_viewport_y The y offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's y offset and not the rendering view port's one.
	 * @see FramesDocument::GetVisualViewport()
	 * @see FramesDocument::GetVisualViewportY()
	 * @param offset_x - the x offset within the target element (relative to element's top, left corner).
	 * @param offset_y - the y offset within the target element (relative to element's top, left corner).
	 * @param shift - TRUE if shift was pressed during the drag end.
	 * @param ctrl - TRUE if control was pressed during the drag end.
	 * @param alt - TRUE if alt was pressed during the drag end.
	 * @param ending_event - may be ONDROP, ONDRAGEND or DOM_EVENT_NONE.
	 * If it's ONDROP, the drag will be ended by sending ONDROP event.
	 * If it's ONDRAGEND the drag will be ended by sending ONDRAGEND event.
	 * If it's DOM_EVENT_NONE the drag will be ended without sending any event.
	 */
	void				EnsureDragEnds(HTML_Document* doc, int x, int y, int visual_viewport_x, int visual_viewport_y, int offset_x, int offset_y, BOOL shift, BOOL ctrl, BOOL alt, DOM_EventType ending_event);

public:
	DragDropEventsManager()
		: m_replaying_recorded_drag_actions(FALSE)
		, m_recording_drag_actions(0)
		, m_replay_next_action(FALSE)
	{}

	/**
	 * Records a drag'n'drop action to be replayed later.
	 *
	 * @param doc - a document an action is recorded for.
	 * @param event - a type of an event to record.
	 * @param x - a x coordinate an event should be replayed for (in document coordinates).
	 * @param y - a y coordinate an event should be replayed for (in document coordinates).
	 * @param[in] visual_viewport_x The x offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's x offset and not the rendering view port's one.
	 * @see FramesDocument::GetVisualViewport()
	 * @see FramesDocument::GetVisualViewportX()
	 * @param[in] visual_viewport_y The y offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's y offset and not the rendering view port's one.
	 * @see FramesDocument::GetVisualViewport()
	 * @see FramesDocument::GetVisualViewportY()
	 * @param offset_x - a x offset within the target element (relative to element's top, left corner).
	 * @param offset_y - a y offset within the target element (relative to element's top, left corner).
	 * @param shift_pressed - TRUE if the shift key is pressed at time the action is recorded.
	 * @param control_pressed - TRUE if the ctrl key is pressed at time the action is recorded.
	 * @param alt_pressed - TRUE if the alt key is pressed at time the action is recorded.
	 *
	 * @return OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 */
	OP_STATUS			RecordAction(HTML_Document* doc, DOM_EventType event, int x, int y, int visual_viewport_x, int visual_viewport_y, int offset_x, int offset_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed);

	/**
	 * While handling a d'n'd event through scripts we mustn't let
	 * other d'n'd events pass since the order between them is
	 * important. While processing a d'n'd event we store (record)
	 * other d'n'd events here and when the current event is processed
	 * we replay recorded ones into the event system.
	 *
	 * <p> This is the method that initiates the replay and is
	 * triggered by HTML_Element::HandleEvent when it has processed a
	 * d'n'd event.
	 *
	 * @return TRUE if anything was replayed.
	 */
	void				ReplayRecordedDragActions();

	/**
	 * Resets the recorder.
	 */
	void				Reset();

	/**
	 * Cancels pending actions.
	 */
	void				CancelPending();

	/**
	 * Returns BOOL value indicating whether the recorder replays some recorded actions or not.
	 */
	BOOL				IsReplaying() { return m_replaying_recorded_drag_actions; }

	/**
	 * Returns BOOL value indicating whether any drag action is being handled.
	 */
	BOOL				IsDragActionBeingProcessed() { return m_recording_drag_actions != 0; }

	/**
	 * Returns BOOL value indicating whether any drag action had been recorded and hasn't been replayed yet.
	 */
	BOOL				HasRecordedDragActions() { return m_recorded_dnd_actions.First() != NULL; }

	/**
	 * Must be called when an HTML_Document is handling a drag action.
	 */
	void				OnDragActionHandle() { ++m_recording_drag_actions; }

	/**
	 * Sets the flag indicating whether the next action must be replayed.
	 */
	void				SetMustReplayNextAction(BOOL must_replay) { m_replay_next_action = must_replay; }

	/**
	 * Returns BOOL value indicating whether the next drag action must be replayed.
	 * If it returns TRUE ReplayRecordedDragActions() must be called. If it returns FALSE
	 * ReplayRecordedDragActions() must not be called.
	 */
	BOOL				GetMustReplayNextAction() { return m_replay_next_action; }

	/**
	 * Must be called before a document is unloaded.
	 *
	 * This function removes all recorded actions applying to the document
	 * which is about to be removed. Moreover if no more 'driving' actions (the
	 * one which causes the next actions are replayed) are left the rest of the
	 * actions are removed as well but with special care taken to no lose d'n'd
	 * ending ones.
	 *
	 * @param doc - the document which is being unloaded.
	 */
	void				OnDocumentUnload(HTML_Document* doc);

	/**
	 * Allows to replay the next action even if the current action is being replayed too.
	 */
	void				AllowNestedReplay() { m_replaying_recorded_drag_actions = FALSE; }

	/**
	 * Cleans the queue taking special care of some important events though.
	 * Should be called on OOM situation while handling d'n'd event.
	 */
	void				OnOOM();
};
# endif // DRAG_SUPPORT

#endif // DRAGDROPEVENTSMANAGER_H
