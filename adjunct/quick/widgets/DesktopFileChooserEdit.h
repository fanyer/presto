/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifndef QUICK_FILE_CHOOSER_EDIT_H
#define QUICK_FILE_CHOOSER_EDIT_H

#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"
#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"
#include "modules/widgets/OpFileChooserEdit.h"


class DesktopFileChooserEdit : public OpFileChooserEdit, public DesktopFileChooserListener
{
public:
	enum SelectorMode
	{
		OpenSelector,
		SaveSelector,
		FolderSelector
	};

protected:
	DesktopFileChooserEdit(SelectorMode mode = OpenSelector);

public:
	static OP_STATUS Construct(DesktopFileChooserEdit** obj, SelectorMode mode = OpenSelector);

	virtual ~DesktopFileChooserEdit();

	virtual BOOL	OnInputAction(OpInputAction* action);

	/**
	 * Sets the title string that will be used by the file selector 
	 * dialog box. This will override the default generic title
	 *
	 * @param caption The dialog title string
	 *
	 * @return OpStatus::OK on success, otherwise an error code describing what failed.
	 */
	OP_STATUS SetTitle(const uni_char* title) { return m_title.Set(title); }

	/**
	 * Sets the initial path that will be used by the file selector 
	 * This will override the default generic path.
	 *
	 * @param caption The initial path
	 *
	 * @return OpStatus::OK on success, otherwise an error code describing what failed.
	 */
	OP_STATUS SetInitialPath(const uni_char* path) { return m_initial_path.Set(path); }

	/**
	 * Sets the filter string that will be used by the file selector 
	 * dialog box.
	 *
	 * @param filter_string The filter string. Format: "description (*.xxx)|*.xxx|description (*.yyy)|*.yyy|...."
	 *
	 * @return OpStatus::OK on success, otherwise an error code describing what failed.
	 */
	OP_STATUS SetFilterString(const uni_char* filter_string) { return m_filter_string.Set(filter_string); }

	/**
	 * Sets the file selector mode
	 *
	 * @param mode The file selector mode
	 */
	void SetSelectorMode( SelectorMode mode ) { m_selector_mode = mode; }

	// Override OpWidget
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	virtual void OnLayout();

	/**
	 * Enables or disables the widget
	 *
	 * @param enabled TRUE enables the widget, FALSE disables it
	 */
	void SetEnabled(BOOL enabled) { OpWidget::SetEnabled(enabled); }

	// QuickFileChooserListener
	virtual void 		OnFileChoosingDone(DesktopFileChooser *chooser, const DesktopFileChooserResult& result);

	// Implementing the OpTreeModelItem interface
	virtual Type		GetType() {return WIDGET_TYPE_FILECHOOSER_EDIT;}

	// OpWidgetListener override only OnClick
	virtual void		OnClick(OpWidget *object, UINT32 id);
	virtual BOOL		OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);

private:
	SelectorMode					m_selector_mode; // File selector mode
	OpString						m_title;         // Title to be used in file selector dialog box.
	OpString						m_filter_string; // Filter selection string to be used in file selector dialog box.
	DesktopFileChooserRequest		m_request;
	DesktopFileChooser*				m_chooser;
	OpString                        m_initial_path;
};


/* QuickWidget wrapper for DesktopFileChooserEdit */
class QuickDesktopFileChooserEdit : public QuickEditableTextWidgetWrapper<DesktopFileChooserEdit>
{
	IMPLEMENT_TYPEDOBJECT(QuickEditableTextWidgetWrapper<DesktopFileChooserEdit>);
protected:
	// Override QuickWidget
	virtual unsigned GetDefaultPreferredWidth()  { return WidgetSizes::Infinity; }
};

#endif // !QUICK_FILE_CHOOSER_EDIT_H
