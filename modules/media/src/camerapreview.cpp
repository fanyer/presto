/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_CAMERA_SUPPORT

#include "modules/media/camerapreview.h"

#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/domutils.h"
#include "modules/layout/box/box.h"

#include "modules/media/camerarecorder.h"

CameraPreview::CameraPreview(HTML_Element* elm)
	: Media(elm)
	, m_controls(NULL)
	, m_enabled(FALSE)
	, m_has_controls(FALSE)
	, m_recording_paused(FALSE)
	, m_last_width(0)
	, m_last_height(0)
{
	FramesDocument* doc = GetFramesDocument();
	if (doc)
		doc->AddMedia(this);
}

CameraPreview::~CameraPreview()
{
	FramesDocument* doc = GetFramesDocument();
	if (doc)
		doc->RemoveMedia(this);

	DisablePreview();
	m_has_controls = FALSE;
	ManageCameraControls();
}

void
CameraPreview::Suspend(BOOL removed)
{
	DisablePreview();

	OP_DELETE(m_controls);
	m_controls = NULL;
}

OP_STATUS
CameraPreview::Resume()
{
	OP_STATUS result = ManageCameraControls();
	if (OpStatus::IsSuccess(result))
		return EnablePreview();

	return result;
}

#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
void
CameraPreview::OnMultiFuncButton()
{
	CameraRecorder* camera_recorder;
	RETURN_VOID_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));
	if (camera_recorder->IsRecording())
	{
		if (!GetCameraSupportsPause())
		{
			OP_ASSERT(!"Record button should be disabled when recording");
			return;
		}

		PauseVideoCapture(!camera_recorder->IsRecordingPaused());
	}
	else
		StartVideoCapture();
}

void
CameraPreview::OnCameraStop()
{
	StopVideoCapture();
}
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT

BOOL
CameraPreview::GetCameraSupportsPause()
{
	CameraRecorder* camera_recorder;
	if (OpStatus::IsError(g_media_module.GetCameraRecorder(camera_recorder)))
		return FALSE;
	return camera_recorder->IsPauseSupported();
}

BOOL
CameraPreview::FocusNextInternalTabStop(BOOL forward)
{
	if (m_controls)
	{
		OpWidget* next = m_controls->GetNextInternalTabStop(forward);
		if (next)
		{
			next->SetFocus(FOCUS_REASON_KEYBOARD);
			return TRUE;
		}
	}

	return FALSE;
}

void
CameraPreview::FocusInternalEdge(BOOL forward)
{
	if (m_controls)
	{
		OpWidget* widget = m_controls->GetFocusableInternalEdge(forward);
		if (widget)
			widget->SetFocus(FOCUS_REASON_KEYBOARD);
	}
}

void
CameraPreview::HandleFocusGained()
{
	FramesDocument* doc = GetFramesDocument();
	if (doc && doc->GetHtmlDocument())
		doc->GetHtmlDocument()->SetFocusedElement(m_element);
}

OP_STATUS
CameraPreview::Paint(VisualDevice* vis_dev, OpRect video, OpRect content)
{
	OpBitmap* bmp = NULL;
	CameraRecorder* camera_recorder;
	RETURN_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));
	RETURN_IF_MEMORY_ERROR(camera_recorder->GetCurrentPreviewFrame(bmp));

	if (bmp && bmp->Width() && bmp->Height())
		vis_dev->BitmapOut(bmp, OpRect(0, 0, bmp->Width(), bmp->Height()), video);

	/// Paint controls
	if (m_controls)
	{
		INT32 c_height, dummy;
		m_controls->GetPreferedSize(&dummy, &c_height, 0, 0);

		const int controls_offset_y = content.Bottom() - c_height;
		vis_dev->Translate(content.x, controls_offset_y);

		AffinePos doc_ctm = vis_dev->GetCTM();

		m_controls->SetWidgetPosition(vis_dev, doc_ctm, OpRect(0, 0, content.width, c_height));
		m_controls->Paint(vis_dev, m_controls->GetBounds());
		vis_dev->Translate(-content.x, -controls_offset_y);
	}

	return OpStatus::OK;
}

void
CameraPreview::OnMouseEvent(DOM_EventType event_type, MouseButton button, int x, int y)
{
	if (m_controls)
	{
		// Controls events
		Box* layout_box = m_element->GetLayoutBox();
		if (!layout_box)
			return;

		RECT rect;
		AffinePos rect_pos;
		FramesDocument* doc = GetFramesDocument();
		if (!doc || !layout_box->GetRect(doc, CONTENT_BOX, rect_pos, rect))
			return;

		long height = rect.bottom - rect.top;
		OpPoint wpoint(x, y - height + m_controls->GetHeight()); // Mouse position translated to widget space

		if (m_controls->GetBounds().Contains(wpoint))
		{
			switch (event_type)
			{
			case ONMOUSEDOWN:
				m_controls->GenerateOnMouseDown(wpoint, button, 1);
				break;

			case ONMOUSEMOVE:
				// Hooked mousemove events is given to the widget from HTML_Document::MouseAction
				if (!OpWidget::hooked_widget)
					m_controls->GenerateOnMouseMove(wpoint);
				break;

			default:
				break;
			}
		}
	}
}

OP_CAMERA_STATUS
CameraPreview::EnablePreview()
{
	CameraRecorder* camera_recorder;
	RETURN_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));

	if (!m_enabled)
	{
		OP_CAMERA_STATUS status = camera_recorder->AttachPreviewListener(this);
		if (OpStatus::IsSuccess(status))
			m_enabled = TRUE;

		return status;
	}
	else
	{
		OP_ASSERT(!"Preview already enabled");
		return OpStatus::OK;
	}
}

void
CameraPreview::DisablePreview()
{
	if (m_enabled)
	{
		CameraRecorder* camera_recorder;
		RETURN_VOID_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));
		camera_recorder->DetachPreviewListener(this);
		m_enabled = FALSE;

	}
}

#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
OP_STATUS
CameraPreview::EnableControls(BOOL enable, const RecordingParams& recording_params)
{
	if (enable)
	{
		RETURN_IF_ERROR(m_suspended_recording.filename.Set(recording_params.filename.CStr()));
		m_suspended_recording.listener = recording_params.listener;
		m_suspended_recording.max_duration_milliseconds = recording_params.max_duration_milliseconds;
		m_suspended_recording.low_res = recording_params.low_res;
	}
	m_has_controls = enable;

	Update();

	return ManageCameraControls();
}
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT

OP_STATUS
CameraPreview::ManageCameraControls()
{
	OP_STATUS result = OpStatus::OK;

	do
	{
		if (m_has_controls)
		{
			FramesDocument* doc = GetFramesDocument();

			if (!m_controls)
				m_controls = OP_NEW(CameraControls, (this));

			if (!m_controls)
			{
				result = OpStatus::ERR_NO_MEMORY;
				break;
			}

			if (OpStatus::IsError(result = m_controls->Init()))
				break;

			VisualDevice* visdev = doc->GetVisualDevice();
			m_controls->SetVisualDevice(visdev);
			m_controls->SetParentInputContext(visdev);
		}
	}
	while(0);

	if (OpStatus::IsError(result) && m_has_controls)
		m_has_controls = FALSE;

	if (!m_has_controls)
	{
		OP_DELETE(m_controls);
		m_controls = NULL;
	}

	return result;
}

#ifdef MEDIA_CAMERA_CAPTURE_SUPPORT
OP_CAMERA_STATUS
CameraPreview::PauseVideoCapture(BOOL pause)
{
	CameraRecorder* camera_recorder;
	RETURN_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));
	if (!camera_recorder->IsPauseSupported()) // this returns FALSE for now
		return OpStatus::ERR_NOT_SUPPORTED;

	m_recording_paused = pause;

	OP_ASSERT(!"Call OpCamera::Pause(m_recording_paused);");

	if (m_controls)
	{
		if (m_recording_paused)
			m_controls->SetPaused();
		else
			m_controls->SetRecording();
	}

	return OpStatus::OK;
}

OP_CAMERA_STATUS
CameraPreview::StopVideoCapture()
{
	CameraRecorder* camera_recorder;
	RETURN_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));
	return camera_recorder->StopVideoCapture();
}

OP_CAMERA_STATUS
CameraPreview::StartVideoCapture()
{
	OP_ASSERT(m_has_controls);
	OpString filename;
	RETURN_IF_ERROR(filename.Set(m_suspended_recording.filename));
	CameraRecorder* camera_recorder;
	RETURN_IF_ERROR(g_media_module.GetCameraRecorder(camera_recorder));
	OP_STATUS result = camera_recorder->CaptureVideo(&filename,
	                                                                  m_suspended_recording.max_duration_milliseconds,
	                                                                  m_suspended_recording.low_res,
	                                                                  m_suspended_recording.listener);

	if (OpStatus::IsSuccess(result))
	{
		m_controls->SetRecording();
		m_recording_paused = FALSE;
	}
	return result;
}
#endif // MEDIA_CAMERA_CAPTURE_SUPPORT

void
CameraPreview::OnCameraUpdateFrame(OpCamera* camera)
{
	Update();
}

void
CameraPreview::OnCameraFrameResize(OpCamera* camera)
{
	OpCamera::Resolution new_size;
	if (OpStatus::IsSuccess(camera->GetPreviewDimensions(&new_size)))
	{
		OP_ASSERT(new_size.x >= 0 && new_size.y >= 0);
		if (m_last_width != static_cast<unsigned int>(new_size.x) || m_last_height != static_cast<unsigned int>(new_size.y))
		{
			m_element->MarkExtraDirty(GetFramesDocument());
			m_last_width = new_size.x;
			m_last_height = new_size.y;
		}
	}
}

FramesDocument*
CameraPreview::GetFramesDocument()
{
	OP_ASSERT(m_element);
	if (LogicalDocument* log_doc = m_element->GetLogicalDocument())
		return log_doc->GetFramesDocument();
	else if (DOM_Environment* dom_env = DOM_Utils::GetDOM_Environment(m_element->GetESElement()))
		return dom_env->GetFramesDocument();
	else
	{
		return NULL;
	}
}

void
CameraPreview::Update()
{
	VisualDevice* vis_dev;
	FramesDocument* frm_doc = GetFramesDocument();
	if (m_element->GetLayoutBox() &&
	    frm_doc && (vis_dev = frm_doc->GetVisualDevice()))
	{
		RECT box_rect;
		m_element->GetLayoutBox()->GetRect(frm_doc, CONTENT_BOX, box_rect);
		OpRect update_rect = OpRect(box_rect.left, box_rect.top, box_rect.right-box_rect.left, box_rect.bottom-box_rect.top);

		vis_dev->Update(update_rect.x, update_rect.y, update_rect.width, update_rect.height);
	}
}

#endif // MEDIA_CAMERA_SUPPORT
