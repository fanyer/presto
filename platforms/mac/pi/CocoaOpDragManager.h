#ifndef COCOAOPDRAGMANAGER_H
#define COCOAOPDRAGMANAGER_h

#include "adjunct/desktop_pi/DesktopDragObject.h"
#include "adjunct/desktop_util/actions/delayed_action.h"
#include "modules/pi/OpDragManager.h"

#define BOOL NSBOOL
#import <Foundation/NSFileManager.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSEvent.h>
#undef BOOL

#define DRAG_PREVIEW_SIZE_LIMIT	300000L

class DelayedStartDrag;

class CocoaOpDragManager : public OpDragManager
{
public:
	CocoaOpDragManager();
	virtual ~CocoaOpDragManager();
	virtual void StartDrag();
	virtual void StopDrag(BOOL cancelled);
	virtual BOOL IsDragging();
	virtual OpDragObject* GetDragObject() { return m_drag_object; }
	virtual void SetDragObject(OpDragObject* drag_object);

	OP_STATUS InternalStartDrag();
	void DoBlockingDrag();

	void AddDragImage(const OpBitmap* bitmap);

	void SetDragView(void *drag_view) { m_drag_view = drag_view; }
	bool SavePromisedFileInFolder(const OpString &folder, OpString &full_path);

	void *GetNSDragImage();

	OP_STATUS PlaceItemsOnPasteboard(void *pasteboard);

private:
	Boolean mMakePreview;
	Boolean mIsDragging;
	Boolean mImageSet;
	const OpBitmap* mBitmap;

	DesktopDragObject* m_drag_object;
	void* m_drag_view;
	Boolean m_promise_drag;
	NSPoint m_pt;
	NSPoint m_at_point;
	NSImage* m_image;
	NSPasteboard* m_pb;
	OpString m_file_extension;
};

class DelayedStartDrag : public OpDelayedAction
{
public:
	DelayedStartDrag(CocoaOpDragManager* dm) : m_dragMan(dm) {}
	void DoAction()
	{
		m_dragMan->DoBlockingDrag();
	}
private:
	CocoaOpDragManager* m_dragMan;
};

extern CocoaOpDragManager* g_cocoa_drag_mgr;

#endif

