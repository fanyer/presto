/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMSELECTION_H
#define DOM_DOMSELECTION_H

#include "modules/dom/src/domobj.h"

#ifdef DOM_SELECTION_SUPPORT

#include "modules/dom/src/domrange/range.h"
#include "modules/util/tempbuf.h"

class DOM_Document;
class DOM_Node;

class DOM_WindowSelection
	: public DOM_BuiltInConstructor
{
protected:
	DOM_WindowSelection(DOM_Document *document);
	~DOM_WindowSelection();

	/** The document the selection belongs to. */
	DOM_Document *owner_document;
	/** The range associated with the selection. Can be NULL if nothing is
	 * selected and no script has added a range. */
	DOM_Range *range;
	/** TRUE if the selection's direction is forward, otherwise FALSE. */
	BOOL forwards;
	/** TRUE if the user has made a new selection and we need to update the
	 * internal range before doing further operations on it. */
	BOOL is_dirty;
	/** If TRUE, we are currently updating the member range from the users
	 * selection and we should not trigger any further updates as long
	 * as that is going on. */
	BOOL is_updating_range;
	/** If TRUE calls to UpdateSelectionFromRange() will be no-op. */
	BOOL lock_updates_from_range;

	/** Called to check if the selection is collapsed.
	 * @param[out] is_collapsed TRUE after the call if selection is collapsed.
	 * @returns Normal OOM values. */
	OP_STATUS IsCollapsed(BOOL &is_collapsed);

	/** Called before operations on the member range to make sure we have
	 * updated the range to the user selection if needed.
	 * @returns Normal OOM values. */
	OP_STATUS UpdateRange()
	{
		if (!range)
			return UpdateRangeFromSelection();
		return OpStatus::OK;
	}

	/** Gets both the end points of the associated range. Depending on the
	 * direction of the selection, the anchor point and the focus point can
	 * be the first or second end point of the range.
	 * @param[out] anchorBP Will contain the anchor point after the call.
	 * @param[out] focusBP Will contain the focus point after the call. */
	void GetAnchorAndFocus(DOM_Range::BoundaryPoint &anchorBP, DOM_Range::BoundaryPoint &focusBP);

	/** Gets the node of one of the end points of the associated range.
	 * @param[out] node Will contain a pointer to the requested node if the
	 *                  call is successful.
	 * @param anchor If TRUE, the anchor node will be fetched, otherwise
	 *               the focus node.
	 * @returns Normal OOM values. */
	OP_STATUS GetBoundaryNode(DOM_Node *&node, BOOL anchor);

	/** Gets the offset of one of the end points of the associated range.
	 * @param[out] offset Will contain the requested offset if the call
	 *                    is successful.
	 * @param anchor If TRUE, the anchor offset will be fetched, otherwise
	 *               the focus offset.
	 * @returns Normal OOM values. */
	OP_STATUS GetBoundaryOffset(unsigned &offset, BOOL anchor);

	/** Must be called to update the member range from a user selection.
	 * @returns Normal OOM values. */
	OP_STATUS UpdateRangeFromSelection();

	/** Must be called after operations on the member range to set the actual
	 * selection in the displayed document.
	 * @returns Normal OOM values. */
	OP_STATUS UpdateSelectionFromRange();

	friend class DOM_Range;

public:
	static OP_STATUS Make(DOM_WindowSelection *&selection, DOM_Document *document);

	DOM_Range* GetRange() { return range; }

	/** Called when the user has changed the selection in any way to mark that
	 * the member range needs update to the new user selection before any
	 * future operations on the range. */
	void OnSelectionUpdated() { if (!is_updating_range) range = NULL; }

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WINDOWSELECTION || DOM_Object::IsA(type); }
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(getRangeAt);
	DOM_DECLARE_FUNCTION(collapse);
	DOM_DECLARE_FUNCTION(extend);
	DOM_DECLARE_FUNCTION(containsNode);
	DOM_DECLARE_FUNCTION(selectAllChildren);
	DOM_DECLARE_FUNCTION(addRange);
	DOM_DECLARE_FUNCTION(deleteFromDocument);
	DOM_DECLARE_FUNCTION(toString);
	enum { FUNCTIONS_ARRAY_SIZE = 10 };

	DOM_DECLARE_FUNCTION_WITH_DATA(removeRange);
	DOM_DECLARE_FUNCTION_WITH_DATA(collapseToStartOrEnd);
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 5 };
};

#endif // DOM_SELECTION_SUPPORT
#endif // DOM_DOMSELECTION_H
