/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */


#ifndef SPEED_DIAL_PAGE_CONTENT_H
#define SPEED_DIAL_PAGE_CONTENT_H

#include "adjunct/quick/speeddial/SpeedDialDragHandler.h"
#include "adjunct/quick/speeddial/SpeedDialGenericHandler.h"
#include "adjunct/quick/thumbnails/GenericThumbnailContent.h"
#include "modules/img/image.h"
#include "modules/thumbnails/thumbnailmanager.h"

class DesktopSpeedDial;

/**
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class SpeedDialPageContent
		: public GenericThumbnailContent
		, public ThumbnailManagerListener
		, public ImageListener
{
public:
	explicit SpeedDialPageContent(const DesktopSpeedDial& entry);
	virtual ~SpeedDialPageContent();

	OP_STATUS Init();

	virtual OpButton* GetButton() const;
	virtual OP_STATUS GetButtonInfo(ButtonInfo& info) const;

	virtual const uni_char* GetTitle() const;
	virtual int GetNumber() const;
	virtual OP_STATUS GetCloseButtonInfo(ButtonInfo& info) const;
	virtual BOOL IsBusy() const;

	virtual MenuInfo GetContextMenuInfo() const;

	virtual OP_STATUS HandleMidClick();

	virtual bool HandleDragStart(DesktopDragObject& drag_object) const;
	virtual bool AcceptsDragDrop(DesktopDragObject& drag_object) const;
	virtual void HandleDragDrop(DesktopDragObject& drag_object);

	virtual OP_STATUS Refresh();
	virtual OP_STATUS Zoom();

	void Reload() {}

	// ThumbnailManagerListener
	virtual void OnThumbnailRequestStarted(const URL& url, BOOL reload);
	virtual void OnThumbnailReady(const URL& url, const Image& thumbnail, const uni_char *title, long preview_refresh);
	virtual void OnThumbnailFailed(const URL& url, OpLoadingListener::LoadingFinishStatus status);
	virtual void OnInvalidateThumbnails();

	// ImageListener
	virtual void OnPortionDecoded();
	virtual void OnError(OP_STATUS status) {}

private:
	class ContentButton;

	bool IsEntryManaged() const;
	bool IsMyUrl(const URL& url) const;

	const DesktopSpeedDial* m_entry;
	ContentButton* m_button;
	Image m_image;
	SpeedDialDragHandler m_drag_handler;
	SpeedDialGenericHandler m_generic_handler;
};

#endif // SPEED_DIAL_PAGE_CONTENT_H
