#ifndef DESKTOP_PAINTER_H
#define DESKTOP_PAINTER_H

/** DesktopPainter is a platform painter with a few extra functions needed by the threaded painter.
  * First off, the DesktopPainter is ref-counted.
  */

class DesktopPainter : public OpPainter
{
public:
	enum PainterFunction{
		PAINTER_FUNCTION_GET_DPI,
		PAINTER_FUNCTION_IS_USING_OFFSCREEN_BITMAP,
		PAINTER_FUNCTION_GET_OFFSCREEN_BITMAP,
		PAINTER_FUNCTION_GET_OFFSCREEN_BITMAP_RECT,
		PAINTER_FUNCTION_SUPPORTS
	};
	enum AsyncClass{
		ASYNC_CLASS_REENTRANT,
		ASYNC_CLASS_OUT_OF_ORDER,
		ASYNC_CLASS_IN_ORDER
	};

	/** Decrement the reference count of the painter. If it hits 0, delete it.
	  */
	virtual void DecrementReference() = 0;

	/** Increment the reference count of the painter.
	  */
	virtual void IncrementReference() = 0;

	/** Get the async class of a function. If the operation is either totally independent of clas state,
	  * or only obtains values that never change during the lifespan of the painter, you should be able to
	  * return ASYNC_CLASS_REENTRANT. For instance, the default implementations of IsUsingOffscreenbitmap
	  * GetOffscreenBitmap and GetOffscreenBitmapRect are all reentrant.
	  *
	  * If the function might modify something in your class, that isn't properly mutex-protected, but otherwise isn't influenced
	  * by other painter calls, you should return ASYNC_CLASS_OUT_OF_ORDER. Normally it should be simple to convert this to be
	  * fully reentrant.
	  *
	  * If your function DOES depend on previous calls, you will have to return ASYNC_CLASS_IN_ORDER. For instance,
	  * if you are using an offscreen bitmap, GetOffscreenBitmap cannot return the right bitmap until all drawing call have
	  * completed.
	  *
	  * It is worth noting that this function itself will have to be reentrant, and can be called from any thread.
	  * DO NOT TOUCH ANY MEMBER VARIABLES. Just a simple switch will do. If in doubt, err on the side of caution.
	  *
	  * @param function the function we are interestted in.
	  * @return how the implementation of the function handles reentrancy.
	  */
	virtual DesktopPainter::AsyncClass GetAsyncClassOfFunction(PainterFunction function) = 0;
};

#endif // DESKTOP_PAINTER_H
