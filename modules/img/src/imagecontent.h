/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef IMAGECONTENT_H
#define IMAGECONTENT_H

#include "modules/pi/OpBitmap.h"

#include "modules/util/simset.h"

#include "modules/img/image.h"

#ifdef EMBEDDED_ICC_SUPPORT
class ImageColorTransform;
#endif // EMBEDDED_ICC_SUPPORT

class ImageListener;
class AnimationListenerElm;
class FrameElm;
class AnimatedImageContent;

enum ImageContentType
{
	NULL_IMAGE_CONTENT,
	EMPTY_IMAGE_CONTENT,
	BITMAP_IMAGE_CONTENT,
	STATIC_IMAGE_CONTENT,
	ANIMATED_IMAGE_CONTENT
};

struct StaticImageInfo
{
	OpBitmap* bitmap;
	OpRect rect;
	INT32 total_width;
	INT32 total_height;
	BOOL is_transparent;
};

class ImageFrameBitmap
{
public:
	ImageFrameBitmap(OpBitmap* bmp, BOOL del);
	ImageFrameBitmap(UINT32* data, unsigned int width, unsigned int height, BOOL transparent, BOOL alpha, UINT32 transpcolor);
	ImageFrameBitmap(UINT8* data, unsigned int width, unsigned int height, BOOL transparent, BOOL alpha, UINT32 transpcolor);
	~ImageFrameBitmap();

	OP_STATUS AddLine(void* data, unsigned int line);
#ifdef USE_PREMULTIPLIED_ALPHA
	OP_STATUS AddLineAndPremultiply(void* data, unsigned int line);
#endif // USE_PREMULTIPLIED_ALPHA
#ifdef EMBEDDED_ICC_SUPPORT
	OP_STATUS AddLineWithColorTransform(void* data, unsigned int line, ImageColorTransform* color_transform);
#endif // EMBEDDED_ICC_SUPPORT
#ifdef SUPPORT_INDEXED_OPBITMAP
	OP_STATUS AddIndexedLine(void* data, unsigned int line);
	OP_STATUS SetPalette(UINT8* pal, unsigned int size);
#endif // SUPPORT_INDEXED_OPBITMAP

	// Stuff from OpBitmap
	unsigned int Width();
	unsigned int Height();
	BOOL IsTransparent();
	BOOL HasAlpha();
#ifdef SUPPORT_INDEXED_OPBITMAP
	int GetTransparentColorIndex();
#endif // SUPPORT_INDEXED_OPBITMAP

	/** @returns the OpBitmap if any, null otherwise. */
	OpBitmap* GetOpBitmap(){return m_bitmap;}
	void ClearBitmapPointer(){m_bitmap = NULL;}

	OP_STATUS CopyTo(OpBitmap* dst);
	OP_STATUS CopyTo(OpBitmap* dst, const OpRect& rect);
	OP_STATUS CopyToTransparent(OpBitmap* dst, const OpPoint& pnt, BOOL dont_blend_prev = FALSE);
private:
	BOOL m_delete_bitmap;
	OpBitmap* m_bitmap;
	UINT32* m_data32;
#ifdef SUPPORT_INDEXED_OPBITMAP
	UINT8* m_data8;
#endif // SUPPORT_INDEXED_OPBITMAP
	unsigned int m_width;
	unsigned int m_height;
	BOOL m_transparent;
	BOOL m_alpha;
	UINT32 m_transpcolor;
#ifdef SUPPORT_INDEXED_OPBITMAP
	UINT8* m_palette;
	unsigned int m_palette_size;
#endif // SUPPORT_INDEXED_OPBITMAP
};

class AnimationListenerElm
{
public:
	ImageListener* listener;
	INT32 frame_nr;
	INT32 last_frame_painted_nr;
	INT32 loop_nr;
	FrameElm* last_frame;
	INT32 ref_count;
};

class FrameElm : public Link
{
public:
	~FrameElm();

	ImageFrameBitmap* bitmap;
	OpBitmap* bitmap_buffer;
	INT32 ref_count;
	OpRect rect;
	INT32 duration;
	BOOL dont_blend_prev;
	DisposalMethod disposal_method;
	INT32 mem_used;
	INT32 flags;
	
	/** AnimatedImageContent::AnimateInternal uses pred_combine to find the frames to comine the current frame
	  * with in order to compose the result */
	FrameElm *pred_combine;
	
	enum 
	{ 
		/** If set, when this frame is disposed should all of the frame canvas be discarded 
		  * (not only the area occupied by this frame)
		  */
		FRAME_FLAGS_CLEAR_ALL = 1,
		
		/** There is no need of composing the frame with previous frames */
		FRAME_FLAGS_USE_ORG_BITMAP = 2,
		
		/* The composition of this frame and possible previous frames (if AnimatedImageContent::AnimateInternal 
		 * is necessary) frames uses transparency, bitmap returned from AnimatedImageContent::GetBitmap should
		 * have IsTransparent() == TRUE 
		 */
		FRAME_FLAGS_COMBINED_TRANSPARENT = 4,

		/* The composition of this frame and possible previous frames (if AnimatedImageContent::AnimateInternal 
		 * is necessary) frames uses alpha, bitmap returned from AnimatedImageContent::GetBitmap should
		 * have UseAlpha() == TRUE 
		 */
		FRAME_FLAGS_COMBINED_ALPHA = 8
	};

	void IncMemUsed(AnimatedImageContent* animated_image_content, INT32 size);
	void DecRefCount(AnimatedImageContent* animated_image_content);
	void ClearBuffer();
};

class ImgContent
{
public:
	virtual ~ImgContent() {}

	virtual ImageContentType Type() = 0;

	virtual OpPoint GetBitmapOffset() { return OpPoint(); }

	virtual OpBitmap* GetBitmap(ImageListener* image_listener) = 0;

	virtual INT32 Width() = 0;

	virtual INT32 Height() = 0;

	virtual BOOL IsTransparent() = 0;

	virtual BOOL IsInterlaced() = 0;

	virtual INT32 GetFrameCount() = 0;

	virtual INT32 GetBitsPerPixel() = 0;

	virtual INT32 GetLastDecodedLine() = 0;

	virtual void ReplaceBitmap(OpBitmap* new_bitmap) { OP_ASSERT(0); }

	virtual void SyncronizeAnimation(ImageListener* dest_listener, ImageListener* source_listener) {}

	virtual OpBitmap* GetTileBitmap(ImageListener* image_listener, int desired_width, int desired_height) { return NULL; }
	virtual void	  ReleaseTileBitmap() {}

	virtual OpBitmap* GetEffectBitmap(OpBitmap* bitmap, INT32 effect, INT32 effect_value, ImageListener* image_listener = NULL) { return NULL; }

	virtual void	  ReleaseEffectBitmap() {}

	virtual OpBitmap* GetTileEffectBitmap(INT32 effect, INT32 effect_value, int horizontal_count, int vertical_count) { return NULL; }
	virtual void	  ReleaseTileEffectBitmap() {}

	virtual BOOL IsBottomToTop() { return FALSE; }
	virtual UINT32 GetLowestDecodedLine() { return 0; }
};

class NullImageContent : public ImgContent
{
public:
	virtual ImageContentType Type();
	OpBitmap* GetBitmap(ImageListener* image_listener);
	virtual INT32 Width();
	virtual INT32 Height();
	BOOL IsTransparent();
	BOOL IsInterlaced();
	INT32 GetFrameCount();
	INT32 GetBitsPerPixel();
	INT32 GetLastDecodedLine();
};

class EmptyImageContent : public NullImageContent
{
public:
	EmptyImageContent(INT32 width, INT32 height);

	ImageContentType Type();
	INT32 Width();
	INT32 Height();

private:
	INT32 width;
	INT32 height;
};

class BitmapImageContent : public ImgContent
{
public:
	BitmapImageContent(OpBitmap* bitmap);
	~BitmapImageContent();

	ImageContentType Type();
	OpBitmap* GetBitmap(ImageListener* image_listener);
	INT32 Width();
	INT32 Height();
	BOOL IsTransparent();
	BOOL IsInterlaced();
	INT32 GetFrameCount();
	INT32 GetBitsPerPixel();
	INT32 GetLastDecodedLine();

	virtual OpBitmap* GetTileBitmap(ImageListener* image_listener, int desired_width, int desired_height) { return bitmap; }
	virtual void ReleaseTileBitmap() {}
	virtual void ReplaceBitmap(OpBitmap* new_bitmap) { OP_DELETE(bitmap); bitmap = new_bitmap; }

	OpBitmap* GetEffectBitmap(OpBitmap* bitmap, INT32 effect, INT32 effect_value, ImageListener* image_listener = NULL);

	void ReleaseEffectBitmap();

private:
	OpBitmap* bitmap;
	OpBitmap* bitmap_effect;	
};

class StaticImageContent : public ImgContent
{
public:
	StaticImageContent(OpBitmap* bitmap, INT32 width, INT32 height, const OpRect& rect, BOOL transparent, BOOL alpha, BOOL interlaced, INT32 bits_per_pixel
									   , BOOL bottom_to_top
		);

	~StaticImageContent();

	ImageContentType Type();
	OpPoint GetBitmapOffset();
	OpBitmap* GetBitmap(ImageListener* image_listener);
	INT32 Width();
	INT32 Height();
	BOOL IsTransparent();
	BOOL IsInterlaced();
	INT32 GetFrameCount();
	INT32 GetBitsPerPixel();
	INT32 GetLastDecodedLine();

	void ReplaceBitmap(OpBitmap* new_bitmap);

	/** Update bitmap_tile to point to a tilebitmap optimal for the desired width and height.
		bitmap_tile may be reallocated, or still NULL after this call.
		Returns the optimal bitmap which may be the original (bitmap) if no tilebitmap was needed. */
	static OpBitmap* UpdateTileBitmapIfNeeded(OpBitmap* bitmap, OpBitmap*& bitmap_tile, int desired_width, int desired_height);

	OpBitmap* GetTileBitmap(ImageListener* image_listener, int desired_width, int desired_height);
	void	  ReleaseTileBitmap();

	OpBitmap* GetEffectBitmap(OpBitmap* bitmap, INT32 effect, INT32 effect_value, ImageListener* image_listener = NULL);
	void	  ReleaseEffectBitmap();

	OpBitmap* GetTileEffectBitmap(INT32 effect, INT32 effect_value, int horizontal_count, int vertical_count);
	void	  ReleaseTileEffectBitmap();

	// INLINE-CALLED-ONCE
	void GetStaticImageInfo(StaticImageInfo& static_image_info)
	{
		static_image_info.bitmap = bitmap;
		static_image_info.rect = rect;
		static_image_info.total_width = total_width;
		static_image_info.total_height = total_width;
		static_image_info.is_transparent = is_transparent;
	}

	// INLINE-CALLED-ONCE
	void LineAdded(INT32 line)
	{
		if (bottom_to_top)
		{
			if (line < (INT32)lowest_decoded_line)
			{
				lowest_decoded_line = line; // FIXME: OFF-BY-ONE?
			}
		}
		if (line > last_decoded_line)
		{
			last_decoded_line = line;
		}
	}
	
	void ClearBitmapPointer() { bitmap = NULL; }

	BOOL IsBottomToTop();
	UINT32 GetLowestDecodedLine();

private:
	OpBitmap* bitmap;
	OpBitmap* bitmap_tile;
	OpBitmap* bitmap_effect;
	OpRect rect;
	INT32 total_width;
	INT32 total_height;
	INT32 bits_per_pixel;
	INT32 last_decoded_line;
	BOOL is_transparent;
	BOOL is_alpha;
	BOOL is_interlaced;
	BOOL bottom_to_top;
	UINT32 lowest_decoded_line;
};

class AnimationListenerElmHash : public OpHashTable
{
public:
	virtual void Delete(void* data) { AnimationListenerElm* d = (AnimationListenerElm*)data; OP_DELETE(d); }
};

class AnimatedImageContent : public ImgContent, public Link
{
public:
	AnimatedImageContent(INT32 width, INT32 height, INT32 nr_of_repeats, BOOL pal_is_shared);
	~AnimatedImageContent();

	OP_STATUS AddFrame(ImageFrameBitmap* bitmap, const OpRect& rect, INT32 duration, DisposalMethod disposal_method, BOOL dont_blend_prev);

	/** Remove the reference to the specified bitmap. This should only be used
	* if creation of the animatedImageContent failed after it assumed ownership of
	* the bitmap and that ownership must be revoked. */
	void ClearBitmapPointer(ImageFrameBitmap* bmp)
	{
		for (FrameElm* e = (FrameElm*)frame_list.First(); e; e = (FrameElm*)e->Suc())
		{
			if (e->bitmap == bmp)
				e->bitmap = NULL;
		}
	}

	ImageContentType Type();
	OpBitmap* GetBitmap(ImageListener* image_listener);
	INT32 Width();
	INT32 Height();
	BOOL IsTransparent();
	BOOL IsInterlaced();
	INT32 GetFrameCount();
	INT32 GetBitsPerPixel();
	INT32 GetLastDecodedLine();

	void SyncronizeAnimation(ImageListener* dest_listener, ImageListener* source_listener);

	OpBitmap* GetEffectBitmap(OpBitmap* bitmap, INT32 effect, INT32 effect_value, ImageListener* image_listener = NULL);
	void	  ReleaseEffectBitmap();

	OpBitmap* GetTileBitmap(ImageListener* image_listener, int desired_width, int desired_height);
	void	  ReleaseTileBitmap();

	OpRect GetCurrentFrameRect(ImageListener* image_listener);
	INT32 GetCurrentFrameDuration(ImageListener* image_listener);

	BOOL Animate(ImageListener* image_listener);
	void ResetAnimation(ImageListener* image_listener);

	OP_STATUS IncVisible(ImageListener* image_listener);
	void DecVisible(ImageListener* image_listener);

	void SetDecoded();

	void ClearBuffers();

#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	BOOL IsLarge() { return is_large; }
	INT32 NrOfRepeats() { return nr_of_repeats; }
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

	static OP_STATUS CopyBitmap(OpBitmap* dst, OpBitmap* src);
private:
	static OP_STATUS MakeBitmapTransparent(OpBitmap* bitmap);
	static OP_STATUS MakeBitmapTransparent(OpBitmap* bitmap, const OpRect& rect);

	AnimationListenerElm* GetAnimationListenerElm(ImageListener* listener);
	FrameElm* GetFrameElm(INT32 frame_nr);

	// INLINE-CALLED-ONCE
	OP_STATUS AnimateInternal(AnimationListenerElm* elm, FrameElm* frame_elm)
	{
		BOOL use_alpha = !!(frame_elm->flags & FrameElm::FRAME_FLAGS_COMBINED_ALPHA);
		BOOL is_transparent = !!(frame_elm->flags & FrameElm::FRAME_FLAGS_COMBINED_TRANSPARENT);
		FrameElm* comb_elm = frame_elm->pred_combine;
		int comb_count = 0; // Number of previous frames to combine with - 1
		OP_ASSERT(comb_elm != NULL);
		RETURN_IF_ERROR(CreateBitmapBuffer(frame_elm,is_transparent,use_alpha));

		for(;;)
		{
			if(comb_elm->flags & FrameElm::FRAME_FLAGS_USE_ORG_BITMAP || comb_elm->bitmap_buffer ||
				comb_elm->pred_combine == NULL)
			{
				break;
			}
			tmp_combine_array[comb_count++] = comb_elm;
			comb_elm = comb_elm->pred_combine;
		}

		if(comb_elm->bitmap_buffer)
		{
			CopyBitmap(frame_elm->bitmap_buffer,comb_elm->bitmap_buffer);
		}	
		else if(comb_elm->flags & FrameElm::FRAME_FLAGS_USE_ORG_BITMAP)
		{
			comb_elm->bitmap->CopyTo(frame_elm->bitmap_buffer);
		}
		else
		{
			RETURN_IF_ERROR(MakeBitmapTransparent(frame_elm->bitmap_buffer));
			RETURN_IF_ERROR(comb_elm->bitmap->CopyToTransparent(frame_elm->bitmap_buffer, 
				OpPoint(comb_elm->rect.x, comb_elm->rect.y),TRUE));
		}

		if(comb_elm->disposal_method == DISPOSAL_METHOD_RESTORE_BACKGROUND)
		{
			RETURN_IF_ERROR(MakeBitmapTransparent(frame_elm->bitmap_buffer, comb_elm->rect));			
		}

		while(comb_count)
		{
			comb_elm = tmp_combine_array[--comb_count];
			OP_ASSERT(comb_elm->bitmap_buffer == NULL);
			if(comb_elm->disposal_method == DISPOSAL_METHOD_RESTORE_BACKGROUND)
			{
				if(!(comb_elm->pred_combine->disposal_method == DISPOSAL_METHOD_RESTORE_BACKGROUND && 
					comb_elm->rect.Equals(comb_elm->pred_combine->rect)))
				{
					RETURN_IF_ERROR(MakeBitmapTransparent(frame_elm->bitmap_buffer, comb_elm->rect));
				}
			}
			else
			{
				RETURN_IF_ERROR(comb_elm->bitmap->CopyToTransparent(frame_elm->bitmap_buffer,
					OpPoint(comb_elm->rect.x, comb_elm->rect.y),comb_elm->dont_blend_prev));
			}
		}

		return frame_elm->bitmap->CopyToTransparent(frame_elm->bitmap_buffer,
			OpPoint(frame_elm->rect.x, frame_elm->rect.y),frame_elm->dont_blend_prev);
	}

	OP_STATUS CreateFirstBitmap(FrameElm* frame_elm);

	// INLINE-CALLED-ONCE
	OP_STATUS CreateFirstBitmapInternal(FrameElm* frame_elm)
	{
		RETURN_IF_ERROR(CreateBitmapBuffer(frame_elm,frame_elm->bitmap->IsTransparent(),frame_elm->bitmap->HasAlpha()));
		RETURN_IF_ERROR(MakeBitmapTransparent(frame_elm->bitmap_buffer));
		return frame_elm->bitmap->CopyToTransparent(frame_elm->bitmap_buffer, OpPoint(frame_elm->rect.x, frame_elm->rect.y), TRUE);
	}

	BOOL CanLoop(AnimationListenerElm* elm);
	OP_STATUS CreateBitmapBuffer(FrameElm *frame_elm, BOOL is_transparent, BOOL use_alpha);

	OpBitmap* bitmap_tile;
	OpBitmap* bitmap_effect;
	AnimationListenerElmHash listener_hash;
	Head frame_list;
	INT32 total_width;
	INT32 total_height;
	INT32 nr_of_repeats;
	INT32 nr_of_frames;
	BOOL is_decoded;
	BOOL is_large;
	BOOL pal_is_shared;
	
	/** Array for temporary usage in AnimateInternal, purpose for having it here is 
	 * to avoid too many allocations... */
	FrameElm **tmp_combine_array;
};

#endif // !IMAGECONTENT_H
