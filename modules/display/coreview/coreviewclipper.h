#ifndef COREVIEW_CLIPPER_H
#define COREVIEW_CLIPPER_H

#include "modules/util/simset.h"
#include "modules/display/coreview/coreview.h"
#include "modules/display/bg_clipping.h"

class OpPluginWindow;

#ifdef _PLUGIN_SUPPORT_

/**
	ClipView is a CoreViewContainer that intersect itself to the visible rect of the CoreView so that a clipping rectangle
	can be calculated and set on the container. That will clip the painting and input of the child-OpPluginWindow to the visible area.
*/

class ClipView : public CoreViewContainer
{
public:
	ClipView();
	virtual ~ClipView();

	OP_STATUS Construct(const OpRect& rect, CoreView* parent);

	void Update();

	/** The virtual rect is the desired rect. Will be used as the source when calculating the
		real rect in Clip() */
	void SetVirtualRect(const OpRect& rect);
	OpRect GetVirtualRect() { return m_virtual_rect; }

	/** Get the current clip rect for this ClipView (relative to itself) */
	OpRect GetClipRect() { return m_clip_rect; }

	void SetPluginWindow(OpPluginWindow* window) { m_window = window; }
	OpPluginWindow* GetPluginWindow() { return m_window; }

	virtual void OnMoved();
	virtual void OnResized();

#ifndef MOUSELESS
	virtual BOOL OnMouseWheel(INT32 delta,BOOL vertical, ShiftKeyState keystate);
	virtual BOOL GetHitStatus(INT32 x, INT32 y);
	virtual bool GetHitStatus(int x, int y, OpView *view);

	void SetIgnoresMouse(BOOL ignore_mouse) { m_ignore_mouse = ignore_mouse; }
#endif // !MOUSELESS

private:
	OpPluginWindow* m_window;
	OpRect m_virtual_rect;
	OpRect m_clip_rect;
	BOOL m_updating;
#ifndef MOUSELESS
	BOOL m_ignore_mouse;
#endif // MOUSELESS

	friend class CoreViewContainer;
	BgRegion m_visible_region;	///< The current visibility region of the plugin view.

	/** The clipview and pluginwindow will be resized & positioned so that it will only 
		be visible inside the currently visible rect. */
	void Clip();
};

/**
	ClipViewEntry is a holder for a ClipView.
*/

class ClipViewEntry
{
public:
	ClipViewEntry();
	virtual ~ClipViewEntry();

	OP_STATUS Construct(const OpRect& rect, CoreView* parent);

	void Update()								{ m_view->Update(); }

	void SetVirtualRect(const OpRect& rect)		{ m_view->SetVirtualRect(rect); }
	OpRect GetVirtualRect()						{ return m_view->GetVirtualRect(); }

	void SetPluginWindow(OpPluginWindow* window){ m_view->SetPluginWindow(window); }
	OpPluginWindow* GetPluginWindow()			{ return m_view->GetPluginWindow(); }

	ClipView* GetClipView()						{ return m_view; }
	OpView* GetOpView()							{ return m_view->GetOpView(); }

private:
	ClipView* m_view;
};

/**
	CoreViewClipper handles a list of ClipViewEntrys.
*/

class CoreViewClipper
{
public:
	virtual ~CoreViewClipper();

	void Add(ClipViewEntry* clipview);

	void Remove(ClipViewEntry* clipview);

	/** Returns the ClipViewEntry for the given pluginwindow or NULL if not in the list. */
	ClipViewEntry* Get(OpPluginWindow* window);

	/** The view has been scrolled so the virtual rectangles must be updated. */
	void Scroll(int dx, int dy, CoreView *parent = NULL);
        void Update(CoreView *parent = NULL);
        void Hide();

private:
        OpVector<ClipViewEntry> clipviews;
};

#endif // _PLUGIN_SUPPORT_

#endif // COREVIEW_CLIPPER_H
