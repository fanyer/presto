#ifndef MACOPDRAGMANAGER_H
#define MACOPDRAGMANAGER_h

#include "modules/pi/OpDragManager.h"

class MacOpDragManager : public OpDragManager
{
	public:
		MacOpDragManager();
		virtual ~MacOpDragManager();
		virtual OP_STATUS StartDrag(DesktopDragObject* drag_object);
		virtual void StopDrag();
		virtual BOOL IsDragging();

		static pascal OSErr SaveDragFile(FlavorType flavorType, void *refcon, ItemReference itemRef, DragReference dragRef);

		void AddDragImage(OpBitmap* bitmap);
		DesktopDragObject* GetDragObject() { return m_drag_object; }
	
	private:
		Boolean mIsDragging;
		DesktopDragObject* m_drag_object;
		DragReference mDragRef;
		Boolean mImageSet;
		OpBitmap* mBitmap;
};

#endif

