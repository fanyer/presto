#ifndef MacOpView_H
#define MacOpView_H

#include "modules/pi/OpView.h"
#include "adjunct/desktop_util/actions/delayed_action.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "platforms/mac/pi/MacOpSystemInfo.h"

class MacOpWindow;

class MacOpView : public MDE_OpView
{
public:
	virtual ~MacOpView(){}
	virtual void GetMousePos(INT32 *xpos, INT32 *ypos);
	virtual OpPoint ConvertToScreen(const OpPoint &point);
	virtual OpPoint ConvertFromScreen(const OpPoint &point);
	OpPoint MakeGlobal(const OpPoint &point, BOOL stop_at_toplevel=TRUE);
	virtual OpWindow* GetRootWindow();
	virtual void SetDragListener(OpDragListener* inDragListener);
	virtual ShiftKeyState GetShiftKeys() { return g_op_system_info->GetShiftKeyState(); }

	void DelayedInvalidate(OpRect rect, UINT32 timeout);
	OpDragListener *GetDragListener() { return mDragListener; }

#ifdef WIDGETS_IME_SUPPORT
	virtual void AbortInputMethodComposing();
	virtual void SetInputMethodMode(IME_MODE mode, IME_CONTEXT context, const uni_char* istyle);
#endif // WIDGETS_IME_SUPPORT
	
private:
	class DelayedInvalidation : public OpDelayedAction
	{
	public:
		DelayedInvalidation(MacOpView* view, OpRect rect, UINT32 timeout) : OpDelayedAction(timeout), m_view(view), m_rect(rect) {}
		virtual void DoAction() { m_view->Invalidate(m_rect); m_view->RemoveDelayedInvalidate(this); }
		
	public:
		MacOpView* m_view;
		OpRect m_rect;
	};
	
	void RemoveDelayedInvalidate(DelayedInvalidation* action);
	
	OpDragListener* mDragListener;
	OpAutoVector<DelayedInvalidation> mDelayedActions;
};

#endif //MacOpView_H
