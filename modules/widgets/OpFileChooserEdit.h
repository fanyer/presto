/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_FILE_CHOOSER_EDIT_H
# define OP_FILE_CHOOSER_EDIT_H

# include "modules/util/opfile/opfile.h"
# include "modules/widgets/OpWidget.h"
# include "modules/windowcommander/OpWindowCommander.h"

# ifdef _FILE_UPLOAD_SUPPORT_
/** Listener for the platform file dialog. */
class OpFileChooserEdit;
class OpFileChooserEditCallback : public OpFileSelectionListener::UploadCallback
{
	OpFileChooserEdit*	m_fc_edit;
	OpString			m_initial_path;
	OpString			m_filter;
public:
	~OpFileChooserEditCallback() {}
	OpFileChooserEditCallback(OpFileChooserEdit* fc_edit)
		:	m_fc_edit(fc_edit) { }

	// Called by UI when setting up for choosing
	const OpFileSelectionListener::MediaType*	GetMediaType(unsigned int index);
	const uni_char*								GetInitialPath();
	unsigned int								GetMaxFileCount();

	// Called by OpFileChooserEdit when closing
	void Closing(OpFileChooserEdit* fc_edit)
	{
		if (m_fc_edit && fc_edit==m_fc_edit)
			m_fc_edit = NULL;
	}

	// OpFileSelectionListener::UploadCallback API:
	void OnFilesSelected(const OpAutoVector<OpString>* files);
};
# endif // _FILE_UPLOAD_SUPPORT_

/** OpFileChooserEdit contains a editfield for filepath, and a browse button that will bring up a platform filedialog
	so files can be choosen from UI. */

class OpFileChooserEdit : public OpWidget
# if !defined (QUICK)
			 , public OpWidgetListener
# endif // QUICK
{
	friend class OpFileChooserEditCallback;
protected:
	OpFileChooserEdit();

public:
	static OP_STATUS Construct(OpFileChooserEdit** obj);

	virtual ~OpFileChooserEdit();

#ifdef WIDGETS_CLONE_SUPPORT
	virtual OP_STATUS CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded);
	virtual BOOL IsCloneable() { return TRUE; }
#endif

	OP_STATUS SetButtonText(const uni_char* text);

	INT32 GetButtonWidth();

# ifdef _FILE_UPLOAD_SUPPORT_	
    OP_STATUS GetText(OpString &str);
	OP_STATUS SetText(const uni_char* text);

	/**
	 * Sets the widget mode. Some elements can be disabled in upload
	 * mode
	 */
	void SetIsFileUpload(BOOL is_upload);

	/**
	 * Sets a number to be the maximum number of files to let the user
	 * input. This is not strictly checked, but if max_count > 1, then 
	 * the widget behaves slightly different.
	 */
	void SetMaxNumberOfFiles(unsigned int max_count);
	unsigned int GetMaxNumberOfFiles() { return m_max_no_of_files; }

	virtual void SetEnabled(BOOL enabled);

	OpEdit* GetEdit() const { return edit; }

	OpButton* GetButton() const { return button; }

	INT32 GetTextLength();

	void GetText(uni_char *buf);

	void ResetFileChooserCallback() { m_callback = NULL; }

	void OnFilesSelected(const OpVector<OpString> *files);

# endif // _FILE_UPLOAD_SUPPORT_

	virtual Type			GetType() {return WIDGET_TYPE_FILECHOOSER_EDIT;}
	virtual BOOL			IsOfType(Type type) { return type == WIDGET_TYPE_FILECHOOSER_EDIT || OpWidget::IsOfType(type); }

	// == Hooks ======================
	void OnResize(INT32* new_w, INT32* new_h);
	void OnFocus(BOOL focus,FOCUS_REASON reason);
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	void EndChangeProperties();

# ifdef _FILE_UPLOAD_SUPPORT_

	void OnClick();
	virtual void OnAdded();

	// OpWidgetListener
	virtual void OnChangeWhenLostFocus(OpWidget *widget);
	virtual void OnGeneratedClick(OpWidget *widget, UINT32 id = 0);
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);

#ifndef QUICK
	/* These seemingly meaningless overrides are implemented to avoid warnings
	   caused, by the overrides of same-named functions from OpWidget. */
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect) {}
	virtual void OnClick(OpWidget *widget, UINT32 id) {}
#endif // QUICK

protected:
	/**
	 * Appends a file name to the edit field.
	 */
	void AppendFileName(const uni_char* new_file_name);

	OP_STATUS InitializeMediaTypes();
	const OpFileSelectionListener::MediaType* GetMediaType(unsigned int index);
# endif // _FILE_UPLOAD_SUPPORT_
private:
	OpButton*	 				button;
	OpEdit*		 				edit;
# ifdef _FILE_UPLOAD_SUPPORT_
#  ifdef WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
	/**
	   stores the absolute path(s), since edit contains the localized
	   path(s) when WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER is enabled.
	*/
	OpString					m_path;
#  endif // WIDGETS_HIDE_FILENAMES_IN_FILE_CHOOSER
	OpAutoVector<OpFileSelectionListener::MediaType> *m_media_types;
	BOOL         				m_is_upload;     // TRUE when used as a file upload widget
	unsigned int				m_max_no_of_files;
	OpFileChooserEditCallback *	m_callback;
# endif // _FILE_UPLOAD_SUPPORT_
};

#endif // OP_FILE_CHOOSER_EDIT_H
