// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
/** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file			 RichTextEditorListener.h
 * @author Owner:    Alexander Remen (alexr)
 */

#ifndef RICH_TEXT_EDITOR_LISTENER_H
#define RICH_TEXT_EDITOR_LISTENER_H


class RichTextEditorListener
{
	public:

							/** Destructor
							 */
		virtual				~RichTextEditorListener() {}

							/** Will basically propagate the call from OnTextChanged from documentedit,
							 *  but also call it when other things change the text
							 */
		virtual void		OnTextChanged() {}
							
							/** The listener should decide what to do when the user wants to insert an image,
							 *  eg. open a file chooser.
							 *  To insert the image call RichTextEditor::InsertImage(OpString image_url);
							 */
		virtual void		OnInsertImage() {}

							/** Will basically propagate the call from OnTextDirectionChanged from documentedit
							 */
		virtual void		OnTextDirectionChanged (BOOL to_rtl) {}

};

#endif // RICH_TEXT_EDITOR_LISTENER_H
