/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_PROGRESS_FIELD_H
#define OP_PROGRESS_FIELD_H

#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick/managers/PluginInstallManager.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"

/***********************************************************************************
**
**	OpProgressField
**
***********************************************************************************/

class OpProgressField:
	public OpLabel,
	public DocumentDesktopWindowSpy,
	public PIM_PluginInstallManagerListener
{
	public:

		enum ProgressType
		{
			PROGRESS_TYPE_DOCUMENT,
			PROGRESS_TYPE_IMAGES,
			PROGRESS_TYPE_TOTAL,
			PROGRESS_TYPE_SPEED,
			PROGRESS_TYPE_ELAPSED,
			PROGRESS_TYPE_STATUS,
			PROGRESS_TYPE_GENERAL,
			PROGRESS_TYPE_CLOCK,
			PROGRESS_TYPE_PLUGIN_DOWNLOAD,
			PROGRESS_TYPE_MAX
		};

	protected:
		OpProgressField(ProgressType type = PROGRESS_TYPE_GENERAL);

	public:

		static OP_STATUS Construct(OpProgressField** obj, ProgressType type = PROGRESS_TYPE_GENERAL);

		ProgressType			GetProgressType() {return m_progress_type;}

		virtual void			GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

		void					UpdateProgress(BOOL force);
		void					UpdateWhenLayout() {m_update_when_layout = TRUE;}
		BOOL					IsUpdateNeeded();

		virtual OP_STATUS		SetText(const uni_char* text);
		virtual OP_STATUS		SetLabel(const uni_char* newlabel, BOOL center = FALSE);

		/**
		 * Set the mime-type of the plugin which download progress will be tracked with this OpProgressFiled.
		 * This is only allowed for OpProgressFields with the type PROGRESS_TYPE_PLUGIN_DOWNLOAD.
		 *
		 * @param mime_type - the mime-type of the plugin which installer download progress is to be tracked, can't be empty.
		 * @return - OpStatus::OK if everything went OK, ERR if the mime_type is empty, the type of the OpProgressField is not 
		 *           PROGRESS_TYPE_PLUGIN_DOWNLOAD or if any other problem occurs.
		 */
		OP_STATUS				SetPluginMimeType(const OpStringC& mime_type);

		/**
		 * Implementation of PIM_PluginInstallManagerListener, used to receive callbacks about plugin installer download progress.
		 */
		virtual void OnFileDownloadProgress(const OpStringC& a_mime_type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long estimate);

		// Hooks

		virtual void			OnDeleted();
		virtual void			OnAdded();
		virtual void			OnDragStart(const OpPoint& point);
		virtual void			OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
		virtual void			OnLayout();
		virtual void			OnSettingsChanged(DesktopSettings* settings);
		virtual void			OnFontChanged() { m_widget_string.NeedUpdate(); OpLabel::OnFontChanged(); }
		virtual void			OnDirectionChanged() { m_widget_string.NeedUpdate(); OpLabel::OnDirectionChanged(); }

		// DocumentDesktopWindowSpy hooks

		virtual void			OnTargetDocumentDesktopWindowChanged(DocumentDesktopWindow* target_window) {if (m_progress_type != PROGRESS_TYPE_CLOCK) UpdateProgress(TRUE);}
		virtual void			OnStartLoading(DocumentDesktopWindow* document_window) {if (m_progress_type != PROGRESS_TYPE_CLOCK) {UpdateProgress(TRUE); StartTicker();}}
		virtual void			OnLoadingProgress(DocumentDesktopWindow* document_window, const LoadingInformation* info) {m_images_total = info->totalImageCount; m_images_loaded = info->loadedImageCount; m_update = TRUE; }
		virtual void			OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user) {if (m_progress_type != PROGRESS_TYPE_CLOCK) {UpdateProgress(TRUE); StopTicker();}}

		// Implementing the OpTreeModelItem interface

		virtual Type			GetType() {return WIDGET_TYPE_PROGRESS_FIELD;}

	private:

		BOOL UseLabel() { return m_progress_type != PROGRESS_TYPE_DOCUMENT && m_progress_type != PROGRESS_TYPE_GENERAL && m_progress_type != PROGRESS_TYPE_TOTAL && m_progress_type != PROGRESS_TYPE_PLUGIN_DOWNLOAD; }

		class Ticker : public OpTimerListener
		{
			public:

				Ticker()
				{
					m_timer = OP_NEW(OpTimer, ());
					if (m_timer)
					{
						m_timer->SetTimerListener(this);
						m_timer->Start(300);
					}
				}

				~Ticker()
				{
					OP_DELETE(m_timer);
				}

				virtual void OnTimeOut(OpTimer* timer)
				{
					int count = m_progress_fields.GetCount();

					for (int i = 0; i < count; i++)
					{
						if (m_progress_fields.Get(i)->IsUpdateNeeded())
						{
							m_progress_fields.Get(i)->UpdateWhenLayout();
							m_progress_fields.Get(i)->Relayout(FALSE, FALSE);
						}
					}

					m_timer->Start(300);

				}

				OpTimer*					m_timer;
				OpVector<OpProgressField>	m_progress_fields;
		};

		void					StartTicker();
		void					StopTicker();

		static Ticker*			s_ticker;

		ProgressType			m_progress_type;
		INT32					m_doc_percent;
		INT32					m_doc_total;
		INT32					m_total_size;
		INT32					m_images_total;
		INT32					m_images_loaded;
		BOOL					m_update;
		BOOL					m_update_when_layout;
		BOOL					m_is_customizing;

		// Local holder so that the string can be draw in different colours
		OpWidgetString			m_widget_string;

		// various cached strings
		OpString				m_string_progress_document;
		OpString				m_string_progress_images;
		OpString				m_string_progress_total;
		OpString				m_string_progress_persec;
		OpString				m_string_progress_speed;
		OpString				m_string_progress_time;
		OpString				m_string_progress_types[PROGRESS_TYPE_MAX];

		OpString				m_plugin_mime_type;
};

#endif // OP_PROGRESS_FIELD_H
