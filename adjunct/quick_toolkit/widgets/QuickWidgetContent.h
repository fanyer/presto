/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_WIDGET_CONTENT_H
#define QUICK_WIDGET_CONTENT_H

class NullWidget;
class QuickWidget;

/** @brief A helper class for widgets that have their contents defined as another widget
  * Usage: Use a QuickWidgetContent as a member object to ensure that there's always
  * valid content
  */
class QuickWidgetContent
{
public:
	QuickWidgetContent();
	~QuickWidgetContent() { CleanContents(); }

	/** Set the content
	  * @param contents Content to set, takes ownership of this pointer
	  */
	void SetContent(QuickWidget* contents);

	/** Get the content
	  * @return a valid pointer to a QuickWidget
	  */
	QuickWidget* GetContent() const { return m_contents; }

	/** Operators for assignment and dereference
	  * NB: Assignment of contents takes ownership
	  */
	QuickWidgetContent& operator=(QuickWidget* contents) { SetContent(contents); return *this; }
	QuickWidget* operator->() const { return m_contents; }

private:
	// Prevent copying
	QuickWidgetContent(const QuickWidgetContent&);
	QuickWidgetContent& operator=(const QuickWidgetContent&);

	void CleanContents();

	static NullWidget s_null_widget;
	QuickWidget* m_contents;
};

#endif // QUICK_WIDGET_CONTENT_H
