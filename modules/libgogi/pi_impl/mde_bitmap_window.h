/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef BITMAP_MDE_SCREEN
#define BITMAP_MDE_SCREEN

#if defined(MDE_BITMAP_WINDOW_SUPPORT)

#include "modules/util/adt/oplisteners.h" 

#include "modules/hardcore/mem/mem_man.h"
#include "modules/libgogi/mde.h"
#include "modules/libgogi/pi_impl/mde_opwindow.h"
#include "modules/windowcommander/src/WindowCommander.h"

class OpBitmap;


class OpBitmapScreenListener
{
public: 
	virtual ~OpBitmapScreenListener(){};

	/**
	* Happens when core has scheduled something to be repainted later. Nothing is painted until Validate is called.
	*/
	virtual void OnInvalid() = 0;

	/**
	* Called when object is being deleted. 
	*/
	virtual void OnScreenDestroying() = 0;
};

/** 
*	Virtual screen which uses bitmap as an painting buffer
*/
class BitmapMDE_Screen : public MDE_Screen 
{
	public: 
		BitmapMDE_Screen();
		OP_STATUS Init(int bufferWidth, int bufferHeight);
		~BitmapMDE_Screen();

		MDE_FORMAT GetFormat() {return MDE_FORMAT_BGRA32;}
		MDE_BUFFER *LockBuffer();
		void UnlockBuffer(MDE_Region *update_region);
		void OnInvalid();

		VEGAOpPainter* GetVegaPainter();	
		void OutOfMemory(){g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);}
		
		OP_STATUS AddListener(OpBitmapScreenListener* listener){return m_listeners.Add(listener);}
		OP_STATUS RemoveListener(OpBitmapScreenListener* listener){return m_listeners.Remove(listener);}

		void SetSize(INT32 new_w, INT32 new_h);
		void GetSize(INT32& width, INT32& height);

		/**
		 * Any invalid content is painted before returning the bitmap.
		 */
		OpBitmap* GetBitmap();
		void  GetRenderingBufferSize(UINT32* width, UINT32* height);

	private:
		void BroadcastOnInvalid();
		void BroadcastOnDestroying();

		OpBitmap* m_bitmap;
		MDE_BUFFER m_buffer;
		OpListeners<OpBitmapScreenListener> m_listeners;
};


/**
* A virtual window which does not have platform window representation
* It uses bitmap as an screen painting buffer
*/
class BitmapOpWindow : public  MDE_OpWindow,
					   public  NullDocumentListener
					   
{
	public: 
		BitmapOpWindow();	

		/**
		 * BitmapWindow will create fixed buffer for window with max size width x height
		 * You can later resize window below initial sizes using SetSize function. 
		 * param @width max window width
		 * param @height max window height
		 */
		OP_STATUS Init(int width, int height);
		
		virtual ~BitmapOpWindow();

		void GetRenderingBufferSize(UINT32* width, UINT32* height);
		BOOL IsActiveTopmostWindow(){return FALSE;}	
		BitmapMDE_Screen* GetScreen() {return m_screen;}
		
		/**
		 * Set current virtual size of window (size shuld be always equal or below init size)
		 * Changing size does not affect bitmap buffer size.
		 * param @width current window width (painting area)
		 * param @height current window height (painitng area)
		 */
		void SetSize(INT32 new_w, INT32 new_h);

		// NullDocumentListener
		virtual void OnGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height);
		virtual void OnGetOuterSize(OpWindowCommander* commander, UINT32* width, UINT32* height);


		//not needed for now
		void SetFloating(BOOL floating){}
		void SetAlwaysBelow(BOOL below){}
		void SetShowInWindowList(BOOL in_window_list){}

	private:
		BitmapMDE_Screen*   m_screen;		
};
#endif // MDE_BITMAP_WINDOW_SUPPORT
#endif // BITMAP_MDE_SCREEN
