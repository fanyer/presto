/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/CocoaOpDragManager.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/util/CocoaPasteboardUtils.h"
#include "platforms/mac/model/OperaNSView.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "platforms/mac/model/CocoaOperaListener.h"
#include "platforms/mac/util/OpenGLContextWrapper.h"

#include "platforms/posix/posix_thread_util.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/url/url_man.h"
#include "modules/logdoc/urlimgcontprov.h"

#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#include "modules/dragdrop/dragdrop_data_utils.h"

CocoaOpDragManager* g_cocoa_drag_mgr = NULL;

CocoaOpDragManager::CocoaOpDragManager() :
	mMakePreview(true),
	mIsDragging(false),
	m_drag_object(NULL),
	m_drag_view(NULL)
{
	g_cocoa_drag_mgr = this;
}

CocoaOpDragManager::~CocoaOpDragManager()
{
	g_cocoa_drag_mgr = NULL;
}

void CocoaOpDragManager::StartDrag()
{
	InternalStartDrag();

	// Start a delayed blocking drag.
	(void) OP_NEW(DelayedStartDrag,(this));
}

void CocoaOpDragManager::DoBlockingDrag()
{
	OperaNSView* view = (OperaNSView*)m_drag_view;
	NSEvent *last_mouse_down = (NSEvent*)CocoaOpWindow::GetLastMouseDownEvent();
	CocoaOperaListener listener(TRUE);

	if (m_promise_drag && m_file_extension.HasContent())
	{
		NSString *str = [NSString stringWithCharacters:(const unichar*)m_file_extension.CStr() length:m_file_extension.Length()];
		[view dragPromisedFilesOfTypes:[NSArray arrayWithObject:str] fromRect:NSMakeRect(m_pt.x, m_pt.y, 100, 100) source:view slideBack:YES event:last_mouse_down];
	}
	else
	{
		[view dragImage:m_image
					 at:m_at_point
				 offset:NSMakeSize(0, 0)
				  event:[NSEvent mouseEventWithType:[last_mouse_down type]
										   location:m_pt
									  modifierFlags:[last_mouse_down modifierFlags]
										  timestamp:[last_mouse_down timestamp]
									   windowNumber:[last_mouse_down windowNumber]
											context:[last_mouse_down context]
										eventNumber:0
										 clickCount:1
										   pressure:0.0]
			 pasteboard:m_pb
				 source:view
			  slideBack:YES];
	}

	g_input_manager->ResetInput();

	if ([view fake_right_mouse])
		[view mouseUp:last_mouse_down];
	else
		[view releaseMouseCapture];
}

OP_STATUS CocoaOpDragManager::InternalStartDrag()
{
	if (!m_drag_object)
	{
		return OpStatus::ERR;
	}

	int url_len = 0;
	int title_len = 0;
	int imageurl_len = 0;
	Boolean dataSet = false;

	m_promise_drag = false;
	mBitmap = NULL;
	mMakePreview = true;

	if (m_drag_object->GetURL())
		url_len = uni_strlen(m_drag_object->GetURL());

	if (m_drag_object->GetTitle())
		title_len = uni_strlen(m_drag_object->GetTitle());

	if (m_drag_object->GetType() == OpTypedObject::DRAG_TYPE_IMAGE && DragDrop_Data_Utils::HasURL(m_drag_object))
	{
		TempBuffer tempBuf;
		if (OpStatus::IsSuccess(DragDrop_Data_Utils::GetURL(m_drag_object, &tempBuf)))
		{
			const uni_char* theUrl = tempBuf.GetStorage();
			URL theImageURL = urlManager->GetURL(theUrl);
			theImageURL.PrepareForViewing(TRUE);
			imageurl_len = uni_strlen(theUrl);

			if (theImageURL.Status(TRUE) == URL_LOADED && theImageURL.GetAttribute(URL::KIsImage))
			{
				OpString tmp;
				RETURN_IF_LEAVE(theImageURL.GetAttributeL(URL::KSuggestedFileName_L, tmp, TRUE));
				if (tmp.HasContent())
				{
					m_promise_drag = true;
					dataSet = true;

					int index = tmp.FindLastOf('.');
					if (index > 0)
					{
						m_file_extension.Set(tmp.CStr() + index);
						m_drag_object->SetImageURL(theUrl);
					}
				}
			}
		}
	}

	if (!mBitmap)
	{
		const OpBitmap* bitmap = m_drag_object->GetBitmap();
		if (bitmap)
		{
			AddDragImage(bitmap);
		}
	}

	mImageSet = false;

	OpPoint where = CocoaOpWindow::GetLastMouseDown();
	NSEvent *last_mouse_down = (NSEvent*)CocoaOpWindow::GetLastMouseDownEvent();

	m_pt = [last_mouse_down locationInWindow];
	m_image = NULL;

	if (mBitmap)
	{
		m_image = (NSImage *)GetNSImageFromOpBitmap((OpBitmap*)mBitmap);
		if (m_image)
		{
			[m_image lockFocus];
			[m_image drawAtPoint:NSZeroPoint fromRect:NSZeroRect operation:NSCompositeCopy fraction:0.75];
			[m_image unlockFocus];
		}
	}

	if (m_promise_drag && m_file_extension.HasContent())
	{
		m_pb = NULL;
		if (mBitmap)
		{
			OpPoint oppt = m_drag_object->GetBitmapPoint();
			m_pt.x -= oppt.x;
			m_pt.y -= mBitmap->Height() - oppt.y;
		}
	}
	else
	{
		m_pb = [NSPasteboard pasteboardWithUniqueName];
		PlaceItemsOnPasteboard(m_pb);
		OpPoint oppt = m_drag_object->GetBitmapPoint();
		m_at_point.x = m_pt.x - oppt.x;
		m_at_point.y = mBitmap ? m_pt.y - (mBitmap->Height() - oppt.y) : m_pt.y;
	}

	return OpStatus::OK;
}

void CocoaOpDragManager::AddDragImage(const OpBitmap* bitmap)
{
	if (m_drag_object->GetType() == OpTypedObject::DRAG_TYPE_IMAGE && ((UInt64)bitmap->Width() * (UInt64)bitmap->Height() > DRAG_PREVIEW_SIZE_LIMIT))
		mMakePreview = false;
	else
		mBitmap = bitmap;
}

void CocoaOpDragManager::StopDrag(BOOL cancelled)
{
	mIsDragging = false;
	mBitmap = NULL;
	OP_DELETE(m_drag_object);
	m_drag_object = NULL;
}

BOOL CocoaOpDragManager::IsDragging()
{
	return mIsDragging;
}

/* virtual */
void CocoaOpDragManager::SetDragObject(OpDragObject* drag_object)
{
	if (drag_object != m_drag_object)
	{
		OP_DELETE(m_drag_object);
		m_drag_object = (DesktopDragObject*)drag_object;
	}

	mIsDragging = (m_drag_object != 0);

	if (m_drag_object)
		m_drag_object->SynchronizeCoreContent();
}

/*static*/
OP_STATUS OpDragManager::Create(OpDragManager*& manager)
{
	manager = OP_NEW(CocoaOpDragManager, ());
	return manager ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

bool CocoaOpDragManager::SavePromisedFileInFolder(const OpString &folder, OpString &full_path)
{
	URL theImageURL = urlManager->GetURL(m_drag_object->GetImageURL());

	OpString filename, path, file_ext;
	OpString16 test_path;

	TRAPD(err, theImageURL.GetAttributeL(URL::KSuggestedFileName_L, filename, TRUE));
	if(OpStatus::IsError(err))
	{
		return false;
	}

	if(!filename.HasContent())
	{
		RETURN_VALUE_IF_ERROR(filename.Set(UNI_L("index.html")), false);
	}

	RETURN_VALUE_IF_ERROR(path.Set(folder), false);
	RETURN_VALUE_IF_ERROR(path.Append(PATHSEP), false);
	RETURN_VALUE_IF_ERROR(path.Append(filename), false);
	RETURN_VALUE_IF_ERROR(test_path.Set(path), false);

	int seq_number = 1;

	while ([[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithCharacters:(const unichar*)test_path.CStr() length:test_path.Length()]])
	{
		RETURN_VALUE_IF_ERROR(test_path.Set(path), false);
		int ext = test_path.FindLastOf('.');
		if (ext != KNotFound)
		{
			RETURN_VALUE_IF_ERROR(file_ext.Set(test_path.SubString(ext)), false);
			test_path.Delete(ext);
		}
		RETURN_VALUE_IF_ERROR(test_path.Append(UNI_L("-")), false);
		RETURN_VALUE_IF_ERROR(test_path.AppendFormat(UNI_L("%d"), seq_number++), false);
		if (ext != KNotFound)
		{
			RETURN_VALUE_IF_ERROR(test_path.Append(file_ext), false);
		}
	}
	
	RETURN_VALUE_IF_ERROR(full_path.Set(test_path), false);
	
	theImageURL.SaveAsFile(full_path);

	return true;
}

void *CocoaOpDragManager::GetNSDragImage()
{
	NSImage *image = NULL;

	if (mMakePreview)
	{
		if(mBitmap)
		{
			image = (NSImage *)GetNSImageFromOpBitmap((OpBitmap*)mBitmap);

			if(image)
			{
				[image lockFocus];
				[image drawAtPoint:NSZeroPoint fromRect:NSZeroRect operation:NSCompositeCopy fraction:0.75];
				[image unlockFocus];
			}
		}
		else
		{
			image = [[NSImage alloc] initWithSize:NSMakeSize(10,10)];
		}
	}
	return image;
}

OP_STATUS CocoaOpDragManager::PlaceItemsOnPasteboard(void *pasteboard)
{
	NSPasteboard *pb = (NSPasteboard*)pasteboard;

	int url_len = 0;
	int title_len = 0;
	int text_len = 0;
	int html_len = 0;
	bool pasteboardHasData = false;

	const uni_char* dragged_text = DragDrop_Data_Utils::GetText(m_drag_object);
	if (dragged_text)
		text_len = uni_strlen(dragged_text);

	const uni_char*	html_string = DragDrop_Data_Utils::GetStringData(m_drag_object, UNI_L("text/html"));
	if (html_string)
		html_len = uni_strlen(html_string);

	const OpVector<OpString>& urls = m_drag_object->GetURLs();
	if (m_drag_object->GetURL())
		url_len = uni_strlen(m_drag_object->GetURL());
	if (m_drag_object->GetTitle())
		title_len = uni_strlen(m_drag_object->GetTitle());

	if (mBitmap && m_drag_object->GetImageURL() && (m_drag_object->GetType() == OpTypedObject::DRAG_TYPE_IMAGE || m_drag_object->GetType() == OpTypedObject::DRAG_TYPE_LINK))
	{
		[CocoaPasteboardUtils PlaceBitmap:mBitmap pasteboard:pb];
		pasteboardHasData = true;
	}

	if (url_len)
	{
		[CocoaPasteboardUtils PlaceURL:m_drag_object->GetURL() pasteboard:pb];
		[CocoaPasteboardUtils PlaceText:m_drag_object->GetURL() pasteboard:pb];
		OpFile f;
		if(OpStatus::IsSuccess(f.Construct(m_drag_object->GetURL())))
		{
			BOOL exists = FALSE;
			if(OpStatus::IsSuccess(f.Exists(exists)) && exists)
			{
				[CocoaPasteboardUtils PlaceFilename:f.GetFullPath() pasteboard:pb];
			}
		}
		pasteboardHasData = true;
	}

	if (html_string)
	{
		[CocoaPasteboardUtils PlaceHTML:html_string pasteboard:pb];
		pasteboardHasData = true;
	}

	if (text_len)
	{
		[CocoaPasteboardUtils PlaceText:dragged_text pasteboard:pb];
		pasteboardHasData = true;
	}
	else if (title_len)
	{
		[CocoaPasteboardUtils PlaceText:m_drag_object->GetTitle() pasteboard:pb];
		pasteboardHasData = true;
	}

	if (urls.GetCount() && !url_len && (m_drag_object->GetType() == OpTypedObject::DRAG_TYPE_TRANSFER))
	{
		for (UINT32 u = 0; u < urls.GetCount(); u++)
		{
			OpString* url = urls.Get(u);
			[CocoaPasteboardUtils PlaceURL:url->CStr() pasteboard:pb];
			pasteboardHasData = true;

			OpFile f;
			if(OpStatus::IsSuccess(f.Construct(url->CStr())))
			{
				BOOL exists = FALSE;
				if(OpStatus::IsSuccess(f.Exists(exists)) && exists)
				{
					[CocoaPasteboardUtils PlaceFilename:f.GetFullPath() pasteboard:pb];
				}
			}
		}
	}

	if (!pasteboardHasData)
	{
		// We have to place SOMETHING on the pasteboard, even if it's only an internal Opera item
		// that's dragged, or it won't call any drag selectors.
		[CocoaPasteboardUtils PlaceDummyDataOnPasteboard:pb];
	}

	return OpStatus::OK;
}
