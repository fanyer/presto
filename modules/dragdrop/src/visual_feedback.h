/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DRAGDROPVISUALFEEDBACK_H
# define DRAGDROPVISUALFEEDBACK_H

# ifdef DRAG_SUPPORT

# include "modules/dragdrop/src/private_data.h"
# include "modules/util/OpHashTable.h"

class FramesDocument;

/** Class resonsible for creating the drag'n'drop visual feedback. */
class DragDropVisualFeedback
{
public:
	/**
	 * Creates the d'n'd visual feedback. Finds a region that consists of the dragged elements' visible parts
	 * on the paint device the document was painted on. Then copies that region to the given bitmap.
	 *
	 * @param this_doc - the document in which the feedback bitmap is supposed to be shown.
	 * @param elms - a list of elements (with documents the elements are in) the visual feedback is supposed to be made of.
	 * @param[in/out] in_out_point - the current cursor's position in document's coordinates should be passed in.
	 * On successfull function return it contains the bitmap's point (the point relative to bitmap's top left corner
	 * indicating a place within the bitmap the pointing device's cursor should be kept in
	 * while the bitmap is moved together with the cursor during the drag'n'drop operation.)
	 * @param[out] out_rect - on successfull function return it contains the region the bitmap was made of (in document coordinates).
	 * @param[out] drag_bitmap - on successfull function return it contains the drag visual feedback bitmap.
	 *
	 * @return OpStatus::OK if all went ok, OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR_NULL_POINTER if this_doc == NULL
	 * and OpStatus::ERR on generic error.
	 *
	 */
	static OP_STATUS	Create(HTML_Document* this_doc, const OpPointerHashTable<HTML_Element, PrivateDragData::DocElm>& elms, OpPoint& in_out_point, OpRect& out_rect, OpBitmap*& drag_bitmap);

	/**
	 * Creates the d'n'd visual feedback. It copies regions given by the list of rectangles
	 * from the paint device the document was painted on to the given bitmap.
	 * Should be used when the feedback bitmap is supposed to be made out of a page's text selection.
	 *
	 * @param[in] this_doc - the document in which the feedback bitmap is supposed to be shown.
	 * @param[in] rects - a list of rectangles the visual feedback is supposed to be made of.
	 * @param[in/out] in_out_rect - a rectangle being a union of all rectangles in the list should be passed
	 * in. On successfull function return it contains the region the bitmap was made of (in document coordinates).
	 * @param[in/out] in_out_point - The current cursor's position in document's coordinates should be passed in.
	 * On successfull function return it contains the bitmap's point (the point relative to bitmap's top lef corner
	 * indicating a place within the bitmap the pointing device's cursor should be kept in
	 * while the bitmap is moved together with the cursor during the drag'n'drop operation.)
	 * @param[out] drag_bitmap - On successfull function return it contains the drag visual feedback bitmap.
	 *
	 * @return OpStatus::OK if all went ok, OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR_NULL_POINTER this_doc or rects
	 * parameter is NULL and OpStatus::ERR on generic error.
	 *
	 */
	static OP_STATUS	Create(HTML_Document* this_doc, const OpVector<OpRect>* rects, OpRect& in_out_rect, OpPoint& in_out_point, OpBitmap*& drag_bitmap);
};

# endif //DRAG_SUPPORT

#endif // DRAGDROPVISUALFEEDBACK_H