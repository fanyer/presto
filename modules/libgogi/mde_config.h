#ifndef MDE_CONFIG_H
#define MDE_CONFIG_H

// == Make your choice ==========================

/** Requirements of the backend: Call TrigMouseDown, TrigMouseUp, TrigMouseMove and TrigMouseWheel on MDE_Screen. */
#ifndef MOUSELESS
#define MDE_SUPPORT_MOUSE
#endif // MOUSELESS

/** Requirements of the backend: Call TrigTouchDown, TrigTouchUp, TrigTouchMove on MDE_Screen. */
#ifdef TOUCH_EVENTS_SUPPORT
#define MDE_SUPPORT_TOUCH
#endif // TOUCH_EVENTS_SUPPORT

#ifdef DRAG_SUPPORT
#define MDE_SUPPORT_DND
#endif // DRAG_SUPPORT

#ifdef SELFTEST
// Define all formats so they are tested.
#define MDE_SUPPORT_RGBA32
#define MDE_SUPPORT_BGR24
#define MDE_SUPPORT_ARGB32
#define MDE_SUPPORT_RGB16
#define MDE_SUPPORT_MBGR16
#define MDE_SUPPORT_RGBA16
#define MDE_SUPPORT_SRGB16
#define MDE_SUPPORT_RGBA24
#endif

#define MDE_GfxMove(dst, src, size) op_memmove(dst, src, size)

// Use memcpy instead of memmove when scrolling vertically.
#define MDE_USE_MEMCPY_SCROLL

//#define MDE_SUPPORT_ROTATE

/** Enables support for drawing translucent, reading the alphavalue of the destination buffercolor to specify
	how much the colors should be visible (255 solid, 0 invisible). */
//#define MDE_SUPPORT_OPACITY

/** Enables support for RGBA32 format. */
//#define MDE_SUPPORT_RGBA32

/** Enables support for ARGB32 format. */
//#define MDE_SUPPORT_ARGB32

/** Enables support for BGR24 format. */
//#define MDE_SUPPORT_BGR24

/** Enables support for RGBA16 format. (4444) */
//#define MDE_SUPPORT_RGBA16

#ifdef GOGI_DRIVER_X11
/** Enables support for RGB16 format. (565) */
# define MDE_SUPPORT_RGB16
#endif

/** Enables support for MBGR16 format. (M = mask, 1555) */
//#define MDE_SUPPORT_MBGR16

/** Enables support for RGBA24 format. (5658) */
//#define MDE_SUPPORT_RGBA24


/** <Not supported yet> */
//#define MDE_SUPPORT_SCROLLPIXELS

/** Support for MDE_View and MDE_Screen. If you only need painting and buffer functions you can turn it off. */
#define MDE_SUPPORT_VIEWSYSTEM

/** Enables support for transparent views. */
#define MDE_SUPPORT_TRANSPARENT_VIEWS

/** Enables support for sprites. */
#ifdef MDE_CURSOR_SUPPORT
#define MDE_SUPPORT_SPRITES
#endif

/** Enables hardware painting. The software buffer and drawingfunctions will not be used and should be
	implemented elsewhere. If hardwaresupport is used, there is no direct buffer-data access.
	Defined by a tweak on core-2! */
//#define MDE_SUPPORT_HW_PAINTING

/** Enable the software drawingfunctions when MDE_SUPPORT_HW_PAINTING is used so they can be used as fallback.
	All softwarefunctions will be renamed to foo_soft. F.ex MDE_CreateBuffer_soft for MDE_CreateBuffer. */
//#define MDE_HW_SOFTWARE_FALLBACK

/** Enables asserts for things that is of no danger for MDE itself, but may be bugs in
	the application. F.ex. drawing a bitmap with the sourcerectangle outside the bitmap itself. */
//#define MDE_PEDANTIC_ASSERTS

#ifndef MDE_ASSERT
#define MDE_ASSERT OP_ASSERT
#endif

/** The default debug flag (which will be set during initialization of a MDE_Screen). */
#define MDE_DEBUG_DEFAULT           0x00

/** Flag for MDE_Screen::SetDebugInfo.
	Enables debugging of updateregion.
	Note: Lower performance than usual.
	The green rectangles is the rectangles in the updateregion of the visible views. */
#define MDE_DEBUG_UPDATE_REGION     0x01

/** Flag for MDE_Screen::SetDebugInfo.
	Enables debugging of invalidation-rectangle.
	Note: Lower performance than usual. When enabled, bigger parts of the screen
	      than usual will be flushed. The red rectangles is what should have
		  been flushed in nondebugmode. */
#define MDE_DEBUG_INVALIDATE_RECT   0x10

// == Set automatically =========================

// FIX: use theese to get all endianness out from the sourcefiles.
#ifdef OPERA_BIG_ENDIAN
#define MDE_COL_OFS_R 1
#define MDE_COL_OFS_G 2
#define MDE_COL_OFS_B 3
#define MDE_COL_OFS_A 0
#else
#define MDE_COL_OFS_R 2
#define MDE_COL_OFS_G 1
#define MDE_COL_OFS_B 0
#define MDE_COL_OFS_A 3
#endif

#define MDE_RGB(r,g,b) ((unsigned int)((255U<<24) + ((r)<<16) + ((g)<<8) + (b)))	///< Converts r,g,b to a unsigned int color
#define MDE_RGBA(r,g,b,a) ((unsigned int)(((a)<<24) + ((r)<<16) + ((g)<<8) + (b)))	///< Converts r,g,b,a to a unsigned int color
#define MDE_COL_R(color) (((unsigned char*)&(color))[MDE_COL_OFS_R])				///< Extracts Red from a unsigned int color
#define MDE_COL_G(color) (((unsigned char*)&(color))[MDE_COL_OFS_G])				///< Extracts Green from a unsigned int color
#define MDE_COL_B(color) (((unsigned char*)&(color))[MDE_COL_OFS_B])				///< Extracts Blue from a unsigned int color
#define MDE_COL_A(color) (((unsigned char*)&(color))[MDE_COL_OFS_A])				///< Extracts Alpha from a unsigned int color
#define MDE_RGB16(r,g,b) ( (((r)&0xf8)<<8) | (((g)&0xfc)<<3) | (((b)&0xf8)>>3) )

#ifdef MDE_SUPPORT_SRGB16
inline unsigned short swap_endian_rgb16(int r, int g, int b) {
	unsigned short col = (((r)&0xf8)<<8) | (((g)&0xfc)<<3) | (((b)&0xf8)>>3);
	return (col<<8)|(col>>8);
}
# define MDE_SRGB16(r,g,b) ( swap_endian_rgb16(r,g,b) )

# define MDE_COL_SR16(color) ((color)&0xf8)
# define MDE_COL_SG16(color) ((((color)>>11)&0x1c) | (((color)<<5)&0xe0))
# define MDE_COL_SB16(color) (((color)>>5)&0xf8)
#endif // MDE_SUPPORT_SRGB16

#define MDE_MBGR16(r,g,b) ( (1<<15) | (((b)&0xf8)<<7) | (((g)&0xf8)<<2) | (((r)&0xf8)>>3) )
#define MDE_RGBA16(r,g,b,a) ( (((r)&0xf0)<<8) | (((g)&0xf0)<<4) | ((b)&0xf0) | (((a)&0xf0)>>4) )
#define MDE_COL_R16(color) (((color)>>8)&0xf8)
#define MDE_COL_G16(color) (((color)>>3)&0xfc)
#define MDE_COL_B16(color) (((color)<<3)&0xf8)

#define MDE_COL_R16M(color) (((color)<<3)&0xf8)
#define MDE_COL_G16M(color) (((color)>>2)&0xf8)
#define MDE_COL_B16M(color) (((color)>>7)&0xf8)
#define MDE_COL_A16M(color) (((color)>>15)&1)
#define MDE_COL_R16A(color) (((color)>>8)&0xf0)
#define MDE_COL_G16A(color) (((color)>>4)&0xf0)
#define MDE_COL_B16A(color) ((color)&0xf0)
#define MDE_COL_A16A(color) (((color)<<4)&0xf0)

#define MDE_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MDE_MIN(a, b) ((a) < (b) ? (a) : (b))
#define MDE_ABS(a) ((a) < 0 ? -(a) : (a))

#define MDE_SWAP(tmp, a, b) do { tmp = a; a = b; b = tmp; } while(0)

typedef unsigned long MDE_F1616; ///< 16.16 Fixed point.

#endif // MDE_CONFIG_H
