/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

/** @mainpage img

 The img module is responsible for the following:

 <ol>
   <li>Decode images.
   <li>Cache images.
   <li>Information about images, so that other parts of the code (like
       the display module) can use this information to draw an image,
	   or to format a page according to the size of the image.
 </ol>

 A number of image formats is available in the img module. See
 ImageDecoderFactory for more information about the image formats.

 The img module is rather independent of the rest of the modules.

 The img module <b>depends on</b>:

 <ul>
   <li>libjpeg or jaypeg
   <li>minpng
   <li>pi
   <li>util
 </ul>

 The img module <b>is used by</b>:

 <ul>
   <li>display
   <li>doc
   <li>dochand
   <li>hardcore
   <li>layout
   <li>logdoc
   <li>skin
   <li>style
   <li>url
   <li>windowcommander
   <li>adjunct/quick
 </ul>

 <h3>What about image drawing?</h3>

 The img module is not responsible for drawing images. You can ask
 for an OpBitmap for an Image from the img module, and other code
 (the display module) will be responsible for drawing the actual image.

 The img module can also create tiled bitmaps and effect bitmaps
 on request from code responsible for image drawing.

 One important thing to know is that the img module is a little
 bit more complex than it would need to be, since it supports
 both images used in a web page, and images used in the user
 interface.

 <h3>What about animation?</h3>

 The img module is not responsible for the actual animation. The img
 module knows nothing about timers. However, there are a number of
 functions for the code that does the actual animation, that the
 img module provide. Image::IsAnimated(), Image::GetCurrentFrameDuration(),
 Image::GetCurrentFrameRect(), Image::Animate().

 <h3>Which features can I use?</h3>

 The most advanced feature for the img module is the
 FEATURE_ASYNC_IMAGE_DECODERS. It will enable the use
 of asynchronous img decoders, that is, decoders that
 we will get a callback for later if they have finished decoding,
 and not directly, when the call to ImageDecoder::DecodeData()
 has returned.

 FEATURE_ASYNC_IMAGE_DECODERS is only used on the Symbian platform,
 to be able to integrate with the Symbian media server. We have
 also a define, ASYNC_IMAGE_DECODERS_EMULATION, which can be used
 to emulate asynchronous decoders with our own decoders. We need
 this to be able to do debugging on other platforms than Symbian,
 with better tools.

 FEATURE_INTERNAL_IMAGE_DECODERS is normally used, but is not used
 for the Symbian platform. If this one is defined, the decoders
 in the img module will be compiled and used.

 FEATURE_LIBPNG will use the libpng instead of minpng. I recommend
 that you do not use libpng, it will be deprecated soon.

 FEATURE_SYSTEM_JPG will use the system's libjpeg implementation
 instead of one that we compile ourselves. Will save footprint,
 but you may get unpleasent bugs. Have this in mind if you turn
 this feature on.

 FEATURE_INDEXED_OPBITMAP will use indexed 8-bit bitmaps when
 it is possible. If this feature is not used, all bitmaps
 will be 32 bits.

 Then we have all the image format features, FEATURE_JPG, FEATURE_PNG,
 FEATURE_BMP, FEATURE_WBMP, FEATURE_XBM, and FEATURE_ICO. There is
 no feature for gif, we expect gif decoding to always be available.
 Notice that you will need to turn this features on, if you want
 to support an image format, even if you are not using internal
 image decoders. Other modules depend on these features. Also
 note that the targa (tga) decoder is only used for testing purposes.
 It is not included in release builds, and is not planned to be
 a part of the release of dali_3.

 <h3>How can I adapt the img module to my needs?</h3>

 First of all, you can use all the features above. Then, you
 can initialize the img module with the right set of decoders.
 You can also, at any time, change cache size and cache policy
 with ImageManager::SetCacheSize(). When memory is low,
 ImageManager::FreeMemory() can be called, which will free up
 as much memory as possible.

 <h3>How do I initialize the img module?</h3>

 <ol>
   <li>Call ImageManager::Create(), and set the imgManager variable
       to the ImageManager returned by this function. Send in an
	   ImageProgressListener (may not be NULL). We need the
	   ImageProgessListener to be able to take a break in the decoding
	   of an image, and just say that we want to continue decoding
	   later. This is to make sure that tasks with higher priorities
	   (like ui response) will not be delayed.
   <li>Call ImageManager::AddImageDecoderFactory() for each image format
       that you want to support. The ImageDecoderFactory will be deleted
	   when the imgManager is deleted, thus the ownership of the
	   ImageDecoderFactory is passed from the caller to the ImageManager.
	   You can add an ImageDecoderFactory provided by the img module,
	   or an ImageDecoderFactory implemented elsewhere.
   <li>Call ImageManager::SetCacheSize() to set the cache size
       and the cache policy.

 </ol>

 <h3>How do I free the img module?</h3>

 <ol>
   <li>Make sure that no Image objects are visible.
   <li>Make sure that no Image objects are referenced from outside the
       img module.
   <li>delete imgManager and set imgManager=NULL.
 </ol>

 If you forget to free the img module, a lot of memory will be leaked,
 in the worst case a lot more than the image cache size, because of
 the cache policy used.

 <h3>What about stack memory usage?</h3>

 The img module does not consume a lot of stack. It is not recursive,
 and it has not very deep call chains. However, it is calling libjpeg/jaypeg
 and minpng modules, so it depends on the stack depth of those modules.

 <h3>What about heap memory usage?</h3>

 The img module uses a lot of heap memory.

 All referenced images have a little object (max 32 bytes each). There can be
 lot of images with a reference, since all documents with an image in them
 will reference an image in the img module. Thus, this is a O(n) issue. If we
 have a lot of documents with a lot of images, this may take up a lot of
 memory. However, it is not likely to be a problem, since the document will
 use much more memory than the referenced image. It is not possible to
 set an hard upper limit on the number of referenced images, so it is
 a little bit dangerous.

 Then we have representations of decoded images (bitmaps). They will use
 much more memory than the referenced images (width * height * bytes_per_pixel
 + constant_amount_of_info_bytes). The constant_amount_of_info_bytes is not
 included in the cache size right now, but the rest of the data is.

 With the right cache policy, we would never use more memory for the decoded
 images than the specified cache size. However, we always keep visible images
 decoded in memory. There can be a lot of visible documents at once, so the
 cache size has nothing to do with how much memory is used. I would like to
 implement a strict cache, but that will need changes in the document code.
 The document code is always expecting all inline urls to be loaded into
 memory as long as the document is visible/active.

 For animated images, we keep decoded images for each frame, plus sometimes
 also the combined image of all frames up to a certain point (important
 speed-up). This combined image is in 32 bit format, so it will use a lot of
 memory. Today, this implementation has not a very good and well defined
 cache strategy. Need to document better.

 Other memory used is listener objects, one for each document referencing
 to a specific image. They do not use much memory (around 16 bytes), but
 and is not bound by anything else than the current cached documents.
 The first implementation only had listeners for visible images, but it
 is possible that this has changed in the desktop optimization. Need to
 document better.

 During decoding, some more memory is used. There is no upper bound of
 the number of images decoding at the same time, but normally, it will
 not be that much. There is a special hack, and not very well documented,
 used for desktop, where both visible and invisible images is decoded at
 the same time.

 Depending on the decoder, more or less memory is used during decoding.
 See documentation for each ImageDecoder to find out how much.

 <h3>What about static memory usage?</h3>

 Two global variables exist in the img module, both are pointers
 to objects, both are created in ImageManager::Create() and deleted
 in ImageManager::~ImageManger(). They use very little memory.

 <h3>OOM handling</h3>

 The img module is using OP_STATUS internally. Mostly, it is also using
 OP_STATUS externally. However, since decoding can happen asynchronously,
 other oom strategies have been needed. ImageListener::OnError() is signalled
 when something (oom or decoding error) happens during decoding. Each
 document interested in the image will get called when the error happens.
 Then, it is also possible to call Image::IsOOM() to know if an image
 had an oom error. This one is only called from the VisualDevice in ImageOut,
 do not know if this one is needed. Need more documentation.

 OOM is not handled internally in the img module. However, the img module
 will never break internally because an oom, and thus is always oom safe.

 <h3>Is temp buffers used?</h3>

 No temp buffers are used. However, memory used during decoding could use
 some kind of temporary memory, if we get an API later for retrieving safe
 (not shared) temporary memory, for higher speed. It is in the decoders
 we may need that, the rest of the code is not using much temporary memory.

 <h3>Coverage</h3>

 Coverage information missing right now.

 <h3>Tests</h3>

 A lot of tests missing. There are tests for the gif decoder, but the rest
 of the code in the module has no selftests.

 <h3>How do I use the img module?</h3>

 First, you need to know how to initialize and free the img module. It is
 documented above.

 If you want to decode an image with the help of the img module, you will
 need to implement an ImageContentProvider, which will provide the img
 module with a stream of data. This is an abstraction so that th img
 module does not need to know anything about urls. The ImageContentProvider
 will tell the Image that there is more data available in the stream when
 that happens, and will call ImageContentProviderListener::OnMoreData()
 (reimplemented in Image). Since this is a kind of a stream interface,
 the img module will decide if it wants to read data or not when it is
 told that there are more data available through the ImageContentProvider.

 When you want to decode an Image, you should use ImageManager::GetImage()
 and send in the ImageContentProvider to your streamed data source. The img
 module will check if this object is already available, and return a reference
 to this Image, or create new Image data if this Image is already available.
 If the Image already was decoded, you will not need to decode the Image, which
 is a nice time saver.

 If you have an Image that will be visible on screen, you call
 Image::IncVisible() with an ImageListener as parameter. When
 it is not visible anymore, Image::DecVisible() should be called.
 Depending on the cache policy, Image::IncVisible() can trigger
 a decoding of the Image.

 If you want to reload an Image, first call ImageManager::ResetImage().
 with the right ImageContentProvider. That way,
 any information belonging to the Image, like the decoded bitmap and
 the size of the Image, will be cleared. Then of course you will
 need to trigger the decoding with ImageContentProviderListener::OnMoreData().

 <h3>Future plans for the img module</h3>

 There are no immediate changes in the pipe, however
 there are a few ideas for small changes.

 <ol>
   <li>Move atvef code outside the img module. It was discussed quite a bit
       before I accepted atvef code in the img module (I was against it, and
	   still am). To do this in a good way, some of the doc code will need
	   to be redesigned. Will discuss this with other module owners. Will
	   not happen in core-2, maybe in core-3.
   <li>Handle error messages differently. Now, a lot of things is happening
       through callbacks in listeners, and it seems like it is a little
	   too much of it going on, making the code a bit too hard to follow.
   <li>Create modules for all the img decoders. That way, less merging
       will be needed when bugs are fixed.
   <li>Support the mng format. We may need some redesign of the rest of
       the img module to be able to do that.
   <li>Get a better design for what is needed by the ui and what is needed
       by core in the img module.
   <li>Use better caching for tiles, animations, scaled bitmaps.
   <li>Scale bitmaps during decoding.
   <li>Be able to decode just parts of an image.
   <li>Support decoders (like the MHW decoder) that will need to scale
       the bitmap during decoding.
   <li>Implement a cache policy where visible images can be thrown out
       of the cache. We will need to redesign other code to be able to
	   do that.
   <li>Implement a cache policy where images in an active document will
       be decoded even if they are not visible. This is already happening
	   today, but it is a hack, and it isn't nice with memory usage (a
	   desktop hack only good for newer desktop machines). In the planned
	   improvement, only if we have enough room in the cache, we will
	   decode non-visible images.
   <li>Make it possible to cache encoded images in memory, instead of caching
       decoded bitmaps.
   <li>Make it possible to RLE encode bitmaps, to have a faster format than
       other formats, but use less memory than bitmaps.
   <li>Make it possible to decode smaller parts of an image.
   <li>Make it possible to reload an image in one document, and still
       use the old representation of the image in another document.
 </ol>


 */

#ifndef IMAGE_H
#define IMAGE_H

#include "modules/img/img_capabilities.h"

#if defined(IMG_CACHE_UNUSED_IMAGES) || defined(IMG_TOGGLE_CACHE_UNUSED_IMAGES)
# define CACHE_UNUSED_IMAGES
#endif // IMG_CACHE_UNUSED_IMAGES || IMG_TOGGLE_CACHE_UNUSED_IMAGES


class ImageRep;
class OpBitmap;
#ifdef CPUUSAGETRACKING
class CPUUsageTracker;
#endif // CPUUSAGETRACKING

#ifdef ASYNC_IMAGE_DECODERS
class AsyncImageDecoderFactory;
#endif // ASYNC_IMAGE_DECODERS


#ifdef CACHE_UNUSED_IMAGES
#include "modules/url/url_id.h"
#endif // CACHE_UNUSED_IMAGES

/**
 * DisposalMethod is used to describe what happens after a frame has been displayed. Before the
 * first frame is added (even when we repeat the animation) all of the area of the image should be cleared.
 */
enum DisposalMethod
{
	/**
	 * The frame should not be disposed, but left as is when we add the next frame.
	 */
	DISPOSAL_METHOD_DO_NOT_DISPOSE = 1,

	/**
	 * The rectangle that were covered with the frame should be cleared (the area will be transparent) before
	 * we add next frame.
	 */
	DISPOSAL_METHOD_RESTORE_BACKGROUND = 2,

	/**
	 * The rectangle that were covered with the frame should be restored as it was before the frame was added,
	 * before we add next frame.
	 */
	DISPOSAL_METHOD_RESTORE_PREVIOUS = 3
};

#ifdef IMAGE_METADATA_SUPPORT
/** ImageMetaData is used to identify different meta data values. Currently
 * they are only set from Jaypeg. */
enum ImageMetaData
{
	IMAGE_METADATA_ORIENTATION = 0,
	IMAGE_METADATA_DATETIME,
	IMAGE_METADATA_DESCRIPTION,
	IMAGE_METADATA_MAKE,
	IMAGE_METADATA_MODEL,
	IMAGE_METADATA_SOFTWARE,
	IMAGE_METADATA_ARTIST,
	IMAGE_METADATA_COPYRIGHT,

	IMAGE_METADATA_EXPOSURETIME,
	IMAGE_METADATA_FNUMBER,
	IMAGE_METADATA_EXPOSUREPROGRAM,
	IMAGE_METADATA_SPECTRALSENSITIVITY,
	IMAGE_METADATA_ISOSPEEDRATING,
	IMAGE_METADATA_SHUTTERSPEED,
	IMAGE_METADATA_APERTURE,
	IMAGE_METADATA_BRIGHTNESS,
	IMAGE_METADATA_EXPOSUREBIAS,
	IMAGE_METADATA_MAXAPERTURE,
	IMAGE_METADATA_SUBJECTDISTANCE,
	IMAGE_METADATA_METERINGMODE,
	IMAGE_METADATA_LIGHTSOURCE,
	IMAGE_METADATA_FLASH,
	IMAGE_METADATA_FOCALLENGTH,
	IMAGE_METADATA_SUBJECTAREA,
	IMAGE_METADATA_FLASHENERGY,
	IMAGE_METADATA_FOCALPLANEXRESOLUTION,
	IMAGE_METADATA_FOCALPLANEYRESOLUTION,
	IMAGE_METADATA_FOCALPLANERESOLUTIONUNIT,
	IMAGE_METADATA_SUBJECTLOCATION,
	IMAGE_METADATA_EXPOSUREINDEX,
	IMAGE_METADATA_SENSINGMETHOD,
	IMAGE_METADATA_CUSTOMRENDERED,
	IMAGE_METADATA_EXPOSUREMODE,
	IMAGE_METADATA_WHITEBALANCE,
	IMAGE_METADATA_DIGITALZOOMRATIO,
	IMAGE_METADATA_FOCALLENGTHIN35MMFILM,
	IMAGE_METADATA_SCENECAPTURETYPE,
	IMAGE_METADATA_GAINCONTROL,
	IMAGE_METADATA_CONTRAST,
	IMAGE_METADATA_SATURATION,
	IMAGE_METADATA_SHARPNESS,
	IMAGE_METADATA_SUBJECTDISTANCERANGE,

	IMAGE_METADATA_COUNT
};
#endif // IMAGE_METADATA_SUPPORT

/**
 * ImageFrameData is a package for attributes for a frame in an image. It is used in
 * ImageDecoderListener::OnNewFrame, to avoid using lots of similar parameters to the
 * function (error prone, and probably also generates larger footprint or is slower).
 */
struct ImageFrameData
{
	/**
	 * Default constructor, that will fill all of the attributes with 0 or FALSE, depending on the
	 * type of each attricute. disposal_method will be set to DISPOSAL_METHOD_DO_NOT_DISPOSE.
	 * @return a ImageFrameData with default values.
	 */
	ImageFrameData() : interlaced(FALSE),
					   transparent(FALSE),
					   alpha(FALSE),
					   duration(0),
					   dont_blend_prev(FALSE),
					   disposal_method(DISPOSAL_METHOD_DO_NOT_DISPOSE),
					   bits_per_pixel(0),
					   pal(NULL),
					   pal_is_shared(FALSE),
					   transparent_index(0),
					   num_colors(0),
					   bottom_to_top(FALSE)
	{
	}

	/**
	 * The rectangle of the frame. This rectangle does not need to cover all of an image's total size.
	 */
	OpRect rect;

	/**
	 * TRUE if the frame data is interlaced, that is the data is not strictly given in a (0 -> (n -1),
	 * where n is the number of lines) order, with line height 1 for each added line.
	 */
	BOOL interlaced;

	/**
	 * TRUE if this is a transparent frame, that is, an image where each pixel can be either opaque
	 * or transparent.
	 */
	BOOL transparent;

	/**
	 * TRUE if this is a frame with alpha channels, where opaque level on each pixel can be between
	 * 0 (totally transparent) to 255 (totally opaque).
	 */
	BOOL alpha;

	/**
	 * Nr of 1/100th:s of seconds this frame should be viewed, if this is an animated image (frame
	 * nr at least a total of 2). If the complete image only consists of one frame, this value should
	 * be ignored.
	 */
	INT32 duration;

	/**
	 * When combining a frame (after the first) in an animation, if TRUE, this frame will not be mixed
	 * (using alpha/transparency) with the previous content in the canvas on the area it occupies, all
	 * color values (including alpha/transparency) will simply be overwritten.
	 */
	BOOL dont_blend_prev;

	/**
	 * The disposal method for this frame. Only valid for animated images. See documentation of DisposalMethod.
	 */
	DisposalMethod disposal_method;

	/**
	 * Bits per pixel in the source data for the frame.
	 */
	INT32 bits_per_pixel;

	/**
	 * An array of palette entries(RGB, 3 bytes) if this is a index based image. If it is not index
	 * based, this value will be NULL. The palette data is not valid after ImageDecoderListener::OnNewFrame()
	 * has returned, so the listener will need to copy the data if it is interested in the data.
	 */
	UINT8* pal;

	/**
	 * Should be TRUE if it is known that the palette is shared for all frames (if there is several frames in the image).
	 * FALSE if not, or if it is not known.
	 */
	BOOL pal_is_shared;

	/**
	 * The index value (0-255), that represents the index used to mark a pixel as transparent. This value only
	 * has meaning if the transparent flag is TRUE, and we had a palette.
	 */
	INT32 transparent_index;

	/**
	 * The number of colors used in an indexed based image (1-255). If the image is not indexed based
	 * (pal == NULL), this value has no meaning.
	 */
	INT32 num_colors;

	BOOL bottom_to_top;
};

/**
 * Abstract class. The functions in this class
 * will be called from a decoder, when things
 * are happening there.
 */
class ImageDecoderListener
{
public:
	virtual ~ImageDecoderListener() {}

	/**
	 * Called when a line is decoded. Not normally called from asynchronous decoders. After the call, the
	 * data given to the listener through this function is not valid, so the listener will have to copy the
	 * data if it finds it interesting (which is the normal case).
	 * @param data pointer to the data that represents the line. If the platform is supporting index based data, and the image can be represented with index based data, the data here will be in indexed data (one byte for each pixel). Otherwise, this data will be given in 32 bit format, BGRA on little endian and ARGB on big endian machines. This is because we want to be able to read an unsigned 32-bit number directly from the line data without, doing any conversion.
	 * @param line the line in an image where we want the data to be placed. This line might be outside the specified image size, so you will have to clip all lines that does not fit into the image.
	 * @param lineHeight the number of lines that should get the data given through this function, starting with line line. Normally this will be 1, the known exception is for gif images, where we for interlaced images want to write a line on several lines, to give the image the wanted interlaced effect. Since we will eventually add data to all lines, even when we have an interlaced image, the listener can choose to ignore to fill more than one line to save processor time.
	 */
	virtual void OnLineDecoded(void* data, INT32 line, INT32 lineHeight) = 0;

	/**
	 * Called when the total size of an image is known. An image can have a frame inside this total size that
	 * does not cover everything. But that is just data, the real width and height of the image is the size
	 * given here.
	 * @return value is obsolete, you shouldn't use that. It will be removed in a later version of the img module.
	 * @param width the total width of the image.
	 * @param height the total height of the image.
	 */
	virtual BOOL OnInitMainFrame(INT32 width, INT32 height) = 0;

	/**
	 * Called when we have a new frame. When a new frame is started, all calls to OnLineDecoded belongs
	 * to that frame (the new frame is the active frame). Before OnNewFrame has been called, OnLineDecoded
	 * should not be called. For more documentation, see documentation of ImageFrameData.
	 * @param image_frame_data the data describing the frame.
	 */
	virtual void OnNewFrame(const ImageFrameData& image_frame_data) = 0;

	/**
	 * Sets the number of times the animation will be played. A value of 0 means that it
	 * will repeat forever. If this function is not called, you should assume that the
	 * animation should be played once.
	 * @param nrOfRepeats the number of times the animation will be played.
	 */
	virtual void OnAnimationInfo(INT32 nrOfRepeats) = 0;

	/**
	 * Called when all of the images has been decoded.
	 */
	virtual void OnDecodingFinished() = 0;

	/**
	   used when decoding asynchronously, to inform the listener that
	   it should propagate that some progress has been made. will
	   typically be called after a bunch of calls to OnLineDecoded, eg
	   when the decoding thread yields. cannot be done from
	   OnLineDecoded since this would cause too many invalidations.
	 */
	virtual void ReportProgress() {}
	/**
	   used when decoding asynchronously, to inform the listener that
	   it should propagate that decoding failed.
	   @param reason the reason decoding failed
	 */
	virtual void ReportFailure(OP_STATUS reason)  {}

#ifdef IMAGE_METADATA_SUPPORT
	virtual void OnMetaData(ImageMetaData id, const char* data) = 0;
#endif // IMAGE_METADATA_SUPPORT

#ifdef EMBEDDED_ICC_SUPPORT
	/**
	 * Called when data for an embedded ICC profile has been decoded.
	 */
	virtual void OnICCProfileData(const UINT8* data, unsigned datalen) = 0;
#endif // EMBEDDED_ICC_SUPPORT
#ifdef ASYNC_IMAGE_DECODERS
	/**
	 * The Symbian Image Converter outputs data directly to an
	 * Epoc Specific bitmap, not line by line in RGBA format.
	 * Hence the current bitmap need to be passed to the decoder.
	 * @return, the current bitmap that the decoder should use.
	 */
	virtual OpBitmap* GetCurrentBitmap() = 0;

    /**
	 * Called when a portion (part) of the image has been decoded.
	 */
    virtual void OnPortionDecoded() = 0;

    /**
	 * Called when the decoder fails for some reason.
	 * @param reason, OpStatus::ERR if this is a decoding error, OpStatus::ERR_NO_MEMORY if out of memory.
	 */
	virtual void OnDecodingFailed(OP_STATUS reason) = 0;

	/**
	* This makes it possible for the decoder to query the content-type
	* of the image.
	*/
	virtual INT32 GetContentType() = 0;

	/**
	* This makes it possible for the decoder to query the content-size
	* of the image.
	*/
	virtual INT32 GetContentSize() = 0;
#endif // ASYNC_IMAGE_DECODERS

	/**
	 * This makes it possible for the listener to receive data without any colorspace conversion applied.
	 */
	virtual BOOL IgnoreColorSpaceConversions() const { return FALSE; }
};

/**
 * Abstract class that we inherit to implement a decoder for a specific
 * format (gif, jpeg, png, etc).
 */
class ImageDecoder
{
public:

	/**
	 * Deletes the ImageDecoder object. The only way to restart a decoding is to delete the ImageDecoder object, and then
	 * create a new one, sending in the data from the start of the image stream.
	 */
	virtual ~ImageDecoder() {}

	/**
	 * Decodes a chunk of an image. This function can be called several times.
	 * @return OpStatus::ERR if decoding failed because of corrupt image, OpStatus::ERR_NO_MEMORY if out of memory while decoding.
	 * @param data pointer to the data to be decoded.
	 * @param numBytes number of bytes in the data to be decoded.
	 * @param more TRUE if there potentially (but not necessarily) will be more data sent to the image decoder after this chunk of data.
	 * @param resendBytes will contain the number of bytes which needs to be
	 * sent as the start of the next chunk if the function returns OpStatus::OK. This is needed because some decoders are designed in such a way that some states cannot be saved inside the decoder, but will instead restart from a known safe position.
	 * @param load_all if TRUE, someone has requested that the entire image should be loaded at once, if possible. does not necessarily mean that all data is available at this time - see Image::OnLoadAll.
	 */
	virtual OP_STATUS DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL load_all = FALSE) = 0;

	/**
	 * Sets the listener that is given information from the decoder.
	 * @param imageDecoderListener will receive the information from the decoder.
	 */
	virtual void SetImageDecoderListener(ImageDecoderListener* imageDecoderListener) = 0;
};

/**
 * ImageContentProvider is the source of the data given to images. We pull data from the
 * ImageContentProvider with GetData() when we are interested in more data.
 */
class ImageContentProvider
{
public:
	/**
	 * Deletes the content provider object.
	 */
	virtual ~ImageContentProvider() {}

	/**
	 * Is FALSE by default. Was needed to be able to compare two content providers, if they have
	 * different implementation, it will not be possible to compare them with IsEqual(). The
	 * reason that the return value is a BOOL, is that right now, in the current implementation,
	 * we are only interested in sharing URL objects, ui images are not shared by several Image
	 * objects. If that changed in the future, we will have to replace this function with one
	 * returning an enumeration.
	 * @return TRUE if this content provider got this data from a URL.
	 */
	virtual BOOL IsUrlProvider() { return FALSE; }

#ifdef CPUUSAGETRACKING
	/**
	 * To properly blame the correct tab for the time spent in image decoding
	 * an ImageContentProvider can implement this method and let it return
	 * a pointer to an appropriate CPUUsageTracker. If this method
	 * returns NULL, then the default ("Other") CPUUsageTracker will be used.
	 *
	 * This only applies for operations initiated by the image system itself,
	 * such as non-synchronous decoding (ImageManager::Progress()). In other
	 * cases the initiator of image operations have to decide what
	 * CPUUsageTracker to blame.
	 *
	 * @return NULL or a pointer to a CPUUsageTracker.
	 */
	virtual CPUUsageTracker* GetCPUUsageTracker() { return NULL; }
#endif // CPUUSAGETRACKING

	/**
	 * Gets a buffer with data from the content provider. The buffer will be valid until one of
	 * ConsumeData(), Grow() or Reset() is called. Also, you should not keep a reference to the buffer for very long time.
	 * @return OpStatus::ERR if no data was available at this time. OpStatus::ERR_NO_MEMORY if we ran out of memory while calling this function. If everthing is ok, and we got a buffer, OpStatus::OK will be returned.
	 * @param buf will be set top point to the buffer with data from the content provider.
	 * @param len will be set to the length of the buffer.
	 * @param more will be set to TRUE if there is more data already loaded in the content provider. If there is not more data loaded after this buffer, or if all of the content provider's data has been loaded, more will be set to FALSE.
	 */
	virtual OP_STATUS GetData(const char*& buf, INT32& len, BOOL& more) = 0;

	/**
	 * Returns image type if it was known to the provider. This function will be called
	 * if the regular image factories can not determine the type. It is intended for fallback
	 * handling if everything else fails.
	 *
	 * @param data raw image data
	 * @param len Length of data buffer in bytes
	 */
	virtual INT32 CheckImageType(const UCHAR* data, INT32 len) { return -1; }

	/**
	 * Grows the size of the buffer that will be returned in the next call to GetData(). This function
	 * should only be called if GetData() returned TRUE in its more parameter the last time it was
	 * called, so we know that the content provider has more data that can be used to grow the buffer
	 * size. You are going to want to use this function when you have called ImageDecoder::DecodeData()
	 * and the function returned a value equal to the size of the buffer sent to DecodeData().
	 * @return OpStatus::ERR_NO_MEMORY if there is not enough memory to create a larger buffer, otherwise it will return OpStatus::OK.
	 */
	virtual OP_STATUS Grow() = 0;

	/**
	 * Consumes a number of bytes in the content provider, that is, moves the pointer in the data stream in
	 * the content provider a number of bytes. You should call this function after a GetData() call, before
	 * any call to Reset() or Grow(). You should not call this function with a value larger than the size of
	 * the buffer returned by GetData().
	 * @param len the number of bytes to consume.
	 */
	virtual void ConsumeData(INT32 len) = 0;

	/**
	 * @return the content type of the content provider's content.
	 */
	virtual INT32 ContentType() = 0;

#ifdef ASYNC_IMAGE_DECODERS
	/**
	 * @return the content size of the content provider's content.
	 */
	virtual INT32 ContentSize() = 0;
#endif // ASYNC_IMAGE_DECODERS

	/**
	 * Sets the content type of the content provider. This function will be called when someone finds out that
	 * the content type of the content provider is wrong, and need to be updated to the correct value.
	 * @param content_type the new content type for the content provider.
	 */
	virtual void SetContentType(INT32 content_type) = 0;

	/**
	 * Resets the content provider and if possible releases all buffers and other resources.
	 */
	virtual void Reset() = 0;

	/**
	 * Rewinds the content provider so that the next time that we call GetData, we will start reading from the beginning.
	 * If possible, buffers and other resources are kept.
	 */
	virtual void Rewind() = 0;

	/**
	 * Compares two content providers, to see if they are using the same data stream.
	 * @return TRUE if the content providers use the same data stream.
	 * @param content_provider that we should compare this object with.
	 */
	virtual BOOL IsEqual(ImageContentProvider* content_provider) = 0;

	/**
	 * @return TRUE if all the content provider's content has been loaded from the network. Will always TRUE if the content provider is receiving its data from a synchronous source (disk or memory).
	 */
	virtual BOOL IsLoaded() = 0;
};

/**
 * The ImageContentProviderListener is called by the corresponding ImageContentProvider. There is no
 * visible connection between the ImageContentProviderListener and the ImageContentProvider, except
 * for the parameter given to OnMoreData() and OnLoadAll().
 */
class ImageContentProviderListener
{
public:
	/**
	 * Deletes the content provider listener.
	 */
	virtual ~ImageContentProviderListener() {}

	/**
	 * Called when more data has arrived to the content provider. The listener can do whatever it wants
	 * with this information. Error handling must be handled in a special way, since this function does
	 * not have a return value.
	 * @param content_provider the content provider that has more data.
	 */
	virtual void OnMoreData(ImageContentProvider* content_provider) = 0;

	/**
	 * Called to let the content provider listener that it should use all data at once from the content
	 * provider. For an image, that means that all available data in the content provider should be decoded.
	 * Note that animated images will only decode the first frame. After this call one
	 * of IsDecoded(), IsAnimated() or IsFailed() will return TRUE.
	 * @return OpStatus::ERR if decoding failed, OpStatus::ERR_NO_MEMORY or OpStatus::OK otherwise.
	 * @param content_provider the content provider we are listening to.
	 */
	virtual OP_STATUS OnLoadAll(ImageContentProvider* content_provider) = 0;
};

/**
 * An ImageListener listens to a specified image. An image can have several image listeners connected
 * to it. An ImageListener object is connected to an Image through the Image::IncVisible() call, and
 * disconnected through the Image::DecVisible() call.
 */
class ImageListener
{
public:
	/**
	 * Deletes the ImageListener.
	 */
	virtual ~ImageListener() {}

	/**
	 * Called when a portion of the image has been decoded.
	 */
	virtual void OnPortionDecoded() = 0;

	/**
	 * Called when there was an error while decoding the image that the ImageListener is connected to.
	 * @param status OpStatus::ERR if decoding failed, OpStatus::ERR_NO_MEMORY if decoding failed because of we run out of memory.
	 */
	virtual void OnError(OP_STATUS status) = 0;
};

/**
 * The NullImageListener has a singleton object called null_image_listener. It can be used in
 * Image::IncVisible() and Image::DecVisible() by everyone who isn't interested in error
 * reporting. The user interface is ising this when using ImageContentProviderListener::OnLoadAll(),
 * since the error messages are given as a return value then.
 */
class NullImageListener : public ImageListener
{
public:
	void OnPortionDecoded() {}

	void OnError(OP_STATUS status) { OpStatus::Ignore(status); }
};

/**
 * An Image object is an object most often used on the stack. It shares ImageRep objects with other Image
 * objects. The ImageRep object is reference counted. Image objects with the same ImageContentProvider will
 * share the same ImageRep object.
 * We get an Image object from the ImageManager.
 */
class Image : public ImageContentProviderListener
{

public:

	/**
	 * An effect is a graphical operation that can be done on an image to change its appearance in a more or less
	 * subtle way. The effect value given in Image::GetEffectBitmap() should be between 0 and 100. Effects are only
	 * used for cool ui effects.
	 * Effects can be combined, since we use a bit field.
	 */
	enum Effect
	{
		/**
		 * The image will look like it glows when this effect is applied. It becomes more yellow. The higher effect
		 * value, the more the image will glow.
		 */
		EFFECT_GLOW			= 0x01,

		/**
		 * This effect is not implemented or used. Should probably be removed.
		 */
		EFFECT_COLORLESS	= 0x02,

		/**
		 * A disabled image will lose half of its opaque level, and thus will look more transparent. The effect
		 * value has no effect.
		 */
		EFFECT_DISABLED		= 0x04,

		/**
		 * Blends the background with this image, by reducing the opaque level in this image by a factor
		 * effect_value / 100. A blend operation with effect value 50 is the same operation as the disabled effect.
		 */
		EFFECT_BLEND		= 0x08
	};

	Image();

	Image(const Image &image);

	~Image();

	Image &operator=(const Image &image);

    /**
	 * @return width of the image. Returns 0 if the width of the image is unknown or if the image contains an error before finding the width.
	 */
    UINT32 Width() const;

    /**
	 * @return height of the image. Returns 0 if the height of the image is unknown or if the image contains an error before finding the height.
	 */
    UINT32 Height() const;

    /**
	 * @return is this image not containing any data?
	 */
	BOOL IsEmpty() const;

    /**
	 * Empty() the image. (set rep to NULL)
	 */
	void Empty() {SetImageRep(NULL);}

    /**
	 * @return is all of the image has been successfully decoded?
	 */
	BOOL ImageDecoded() const;

    /**
	 * @return has this image any form of transparency? (transparent or alpha)
	 */
	BOOL IsTransparent() const;

    /**
	 * @return TRUE if the image does not get its data lines in normal order (0 -> height -1, one line at a time).
	 */
	BOOL IsInterlaced() const;

	/**
	 * Return TRUE if the content is believed relevant (interesting) to the user.
	 * Basically, images with only one color (such as images used as spacers or backgroundcolors) would return FALSE.
	 * All images return TRUE until all decoding is finished.
	 */
	BOOL IsContentRelevant() const;

	/**
	 * @return the last decoded line (line + 1). Will return Height() if ImageDecoded(). Will return 0 if no lines are decoded.
	 */
	UINT32 GetLastDecodedLine() const;

    /**
	 * When an image is made visible on screen, it should call IncVisible(). Only visible images will be decoded.
	 * @return a status value of OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 * @param image_listener is the ImageListener that is interested in this image.
	 */
	OP_STATUS IncVisible(ImageListener* image_listener);

    /**
	 * When an image is not visible on screen anymore, it should call DecVisible(). An image that is not visible at all should stop decoding. With some cache policies, the image representation might be deleteted immediately.
	 * @param image_listener the ImageListener that is not interested in this image anymore.
	 */
	void DecVisible(ImageListener* image_listener);

	/**
	 * Try decode the image if there is free space in the memorycache. Does nothing if the image is visible.
	 * PreDecoding images has lower priority than visible images.
	 * PreDecoding images will not decoded while there is visible images to decode.
	 * PreDecoding images will be removed from cache if a visible image needs the memory.
	 */
	void PreDecode(ImageListener* image_listener);

	/**
	 * DEBUG_PRIVATE: Counts the current image_listeners.
	 */
	INT32 CountListeners();

	/**
	 * DEBUG_PRIVATE: Get the reference count to this image.
	 */
	INT32 GetRefCount();

	/**
	 * MODULE_PRIVATE
	 * Sets the ImageRep that this Image should represent.
	 */
	void SetImageRep(ImageRep* imageRep);

	/**
	 * COMMENT: Will not work for animated images?
	 * @return the number of bits per pixel in the source image (that has nothing to do with the nr of pixels used in the actual image representation in memory).
	 */
	int GetBitsPerPixel();

	/**
	 * @return the number of frames in the source image. An image that is not animated has 1 frame. An image has 0 frames if not decoded or if it was something wrong with the decoding. If !ImageDecoded(), the return value from this function may not be accurate.
	 */
	int GetFrameCount();

	/**
	 * Syncronizes the animation accosiated with the dest_listener so that it will have the same loopnr and framenr as
	 * the animation accosiated with source_listener.
	 */
	void SyncronizeAnimation(ImageListener* dest_listener, ImageListener* source_listener);

	/**
	 * Replaces the current bitmap in an Image that we got with ImageManager::GetImage(OpBitmap*).
	 * @param new_bitmap the bitmap used to replace the current bitmap.
	 * @return OpStatus::ERR_NO_MEMORY or OpStatus::OK
	 */
	OP_STATUS ReplaceBitmap(OpBitmap* new_bitmap);

	/**
	 * Will decode a part of the image, unless decoding this image has failed, image already decoded, or the image is not
	 * visible. Will be called when there are more data available in the ImageContentProvider.
	 * @param content_provider the ImageContentProvider that this Image gets its data from.
	 */
	void OnMoreData(ImageContentProvider* content_provider);

	/**
	 * Will decode all of this image, unless decoding this image has failed, image already decoded, or the image is
	 * not visible. Only the first frame will be loaded in case of animated images, to avoid freeze for long animations.
	 * Normally used for ui and javascript mouse over effects.
	 * @return OpStatus::ERR if decoding error, otherwise OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 * @param content_provider the ImageContentProvider that this Image gets its data from.
	 */
	OP_STATUS OnLoadAll(ImageContentProvider* content_provider);

	/**
	 * @return the offset of the image data inside the total image frame. Only used for single-frame images.
	 */
	OpPoint GetBitmapOffset();

	/**
	 * Gets an OpBitmap representing the image, and locks it into memory.
	 * @return the bitmap with the data representation of the image. Will return NULL if there are no data in the image yet, or if there was an OOM condition.
	 * @param image_listener the ImageListener that the returned bitmap will belong to. Needed because different image listeners may have different bitmap representations (only animated bitmaps can have different representations).
	 */
	OpBitmap* GetBitmap(ImageListener* image_listener);

	/**
	 * Releases the lock from the bitmap locked with GetBitmap(). If GetBitmap() returned NULL, this
	 * function should not be called.
	 */
	void ReleaseBitmap();

	/**
	 * Gets a tiled representation of the image, and locks the bitmap into memory.
	 * If the original bitmap is tiny, the image code will create and cache a larger (but not too large) bitmap and return that bitmap.
	 * The desired_width and desired_height are used as a hint for the needed size of the tilebitmap (so it won't create a unnecessary large bitmap if you are only painting a small area anyway).
	 * @return a OpBitmap with a tiled representation of the image. Will return NULL if there are no data in the image yet, or if there was an OOM condition.
	 * @param image_listener the ImageListener that the returned bitmap will belong to. Needed because different image listeners may have different bitmap representations (only animated bitmaps can have different representations).
	 * @param desired_width The width you are going to draw the tiled bitmap on.
	 * @param desired_height The height you are going to draw the tiled bitmap on.
	 */
	OpBitmap* GetTileBitmap(ImageListener* image_listener, int desired_width = 64, int desired_height = 64);

	/**
	 * Releases the lock from the bitmap locked with GetTileBitmap(). If GetTileBitmap() returned NULL, this
	 * function should not be called.
	 */
	void ReleaseTileBitmap();

	/**
	 * Gets an effect representation of the image, and locks the bitmap into memory. Used for ui effects.
	 * @return a OpBitmap with an effect representation of the image. Will return NULL if there are no data in the image yet, or if there was an OOM condition.
	 * @param effect the effect used on the original image representation, see Effect enumeration.
	 * @param effect_value the value given to the effect, see documentation of Effect enumeration.
	 */
	OpBitmap* GetEffectBitmap(INT32 effect, INT32 effect_value, ImageListener* image_listener = NULL);
	/**
	 * Releases the lock from the bitmap locked with GetEffectBitmap(). If GetEffectBitmap() returned NULL, this
	 * function should not be called.
	 */
	void ReleaseEffectBitmap();

	/**
	 * Gets a tiled effect representation of the image, and locks the bitmap into memory. Used for ui effects.
	 * @return a OpBitmap with a tiled effect representation of the image. Will return NULL if there are no data in the image yet, or if there was an OOM condition.
	 * @param effect the effect used on the original image representation, see Effect enumeration.
	 * @param effect_value the value given to the effect, see documentation of Effect enumeration.
	 * @param horizontal_count the number of times the image representation should be duplicated horizontally.
	 * @param vertical_count the number of times the image representation should be duplicated vertically.
	 */
	OpBitmap* GetTileEffectBitmap(INT32 effect, INT32 effect_value, int horizontal_count, int vertical_count);

	/**
	 * Releases the lock from the bitmap locked with GetTileEffectBitmap(). If GetTileEffectBitmap() returned NULL, this
	 * function should not be called.
	 */
	void ReleaseTileEffectBitmap();

	/**
	 * Checks if the image is known to be animated. This will only
	 * return TRUE once we've found a second frame in the image
	 * so it's also a sign that the first frame is decoded.
	 *
	 * @return TRUE if it is known that this is an animated image.
	 */
	BOOL IsAnimated();

	/**
	 * COMMENT: Need to discuss documentation with Emil.
	 * Gets the duration of the current frame for a specific animated image.
	 * @return the duration of the current image frame given in 1/100th:s of a second. Will return -1 if this is the last frame to be displayed infinitely.
	 * @param image_listener the ImageListener that has a specific current image frame.
	 */
	INT32 GetCurrentFrameDuration(ImageListener* image_listener);

	/**
	 * COMMENT: Need to discuss documentation with Emil.
	 * Gets the area that needs to be updated in an image, when a new frame should be displayed.
	 * @return the area in the image that needs to be updated.
	 * @param image_listener the ImageListener with a specific current image frame.
	 */
	OpRect GetCurrentFrameRect(ImageListener* image_listener);

	/**
	 * COMMENT: Need to discuss documentation with Emil.
	 * Replaces the frame that the animated image points to, to the following frame.
	 * @return TRUE if the frame was changed. Will not change frame if next frame is missing, or if the animation should not loop anymore.
	 * @param image_listener the ImageListener that has its own position in the animation.
	 */
	BOOL Animate(ImageListener* image_listener);

	/**
	* Resets animation's loop of an image having finite loop
	* @param image_listener the ImageListener that has its own position in the animation.
	*/
	void  ResetAnimation(ImageListener* image_listener);

	/**
	 * Peeks the imagedimensions if they aren't known already. This doesn't load the image, it just tries to find the dimensions if there is enough data.
	 */
	void PeekImageDimension();

	BOOL IsFailed();

	/**
	 * Normally, images decode their data with the top line first, but sometimes
	 * the decoding is the opposite (bmp). Ask this function how the data is decoded.
	 * @return TRUE if this image is decoded with the bottom line first.
	 */
	BOOL IsBottomToTop();

	/**
	 * Call this function instead of GetLastDecodedLine() if IsBottomToTop() returns TRUE.
	 * @return the lowest decoded line in the image. Everything below this line, including
	 * the line, can be drawn to screen, the rest of the data is uninitialized, and should
	 * not be drawn to screen (garbage).
	 */
	UINT32 GetLowestDecodedLine();

#ifdef IMG_GET_AVERAGE_COLOR
	/**
	 * Get the average pixel color of the image.
	 * @param image_listener the ImageListener that has its own position in the animation.
	 */
	COLORREF GetAverageColor(ImageListener* image_listener);
#endif // IMG_GET_AVERAGE_COLOR

#ifdef HAS_ATVEF_SUPPORT
	/**
	 * @return TRUE if this is an atvef image. Only used as a hack to help the atvef implementation.
	 */
    BOOL IsAtvefImage();
#endif

	BOOL operator==(const Image &image) {return image_rep == image.image_rep;}
	BOOL operator!=(const Image &image) {return image_rep != image.image_rep;}

	BOOL IsOOM();

#ifdef IMAGE_METADATA_SUPPORT
	char* GetMetaData(ImageMetaData id);
	BOOL HasMetaData();
#endif // IMAGE_METADATA_SUPPORT

private:

	void IncRefCount();
	void DecRefCount();

	ImageRep* image_rep;
};

/**
 * The ImageManager has normally an ImageProgressListener. The ImageProgressListener is used to report
 * that the image code has decoded some of its data, and want to continue doing that, but do not want
 * to lock up the user interface. ImageProgressListener::OnProgress() may not, when it is called, make
 * a call back to the image code.
 */
class ImageProgressListener
{
public:
	virtual ~ImageProgressListener() {}

	/**
	 * Called when the image manager has decoded some data, and is offering the user interface to
	 * handle user input, to avoid freezing the user interface. The ImageProgressListener should
	 * make sure that ImageManager::Progress() is called later on.
	 */
	virtual void OnProgress() = 0;
#ifdef ASYNC_IMAGE_DECODERS_EMULATION
	/**
	 * Only used when ASYNC_IMAGE_DECODERS_EMULATION is defined. Called when there is more data
	 * added to an image, that will need to be decoded later (asynchronously). The implementation
	 * calls the hidden img module function ImageManagerImp::MoreBufferedData(). It is possible
	 * that I move the function to ImageManager later, but since this is no code that should be
	 * used other than when emulating asynchronous image decoding, I will let it be like this for
	 * a while.
	 */
	virtual void OnMoreBufferedData() = 0;

	/**
	 * Only used when ASYNC_IMAGE_DECODERS_EMULATION is defined. Called when we have an ImageDecoder
	 * that will need to be deleted later. The ImageProgressListener should make sure that
	 * ImageManager::FreeUnusedAsyncDecoders() will be called later.
	 */
	virtual void OnMoreDeletedDecoders() = 0;
#endif // ASYNC_IMAGE_DECODERS_EMULATION
};

/**
 * It is possible to change the policy for what the image manager does with its images. The plan is
 * to add more cache policies as soon as they are needed.
 */
enum ImageCachePolicy
{
	/**
	 * A soft image cache policy works like this. It is using a least recently used cache, trying to
	 * follow the cache limit in the ImageManager. However, a visible image cannot be thrown out of
	 * the cache, so it is possible that the cache limit will be exceeded from time to time.
	 */
	IMAGE_CACHE_POLICY_SOFT,

	/**
	 * The strict image cache policy is not implemented yet.
	 */
	IMAGE_CACHE_POLICY_STRICT
};

class ImageDecoderFactory;

/**
 * The ImageManager is the main interface between the rest of the code and the img module. One (and
 * only one) ImageManager object should be created with ImageManager::Create, and it should be assigned
 * to the global variable imgManager. The ImageManager is mainly responsible for caching image objects,
 * keeping a list of all referenced images, and making sure that images are decoded properly. While decoding,
 * the ImageManager will keep a list of loaded images that are not decoded completely yet. The ImageManager
 * will make sure that decoding of images will not use too large time slice at once. This is to ensure that
 * the user interface stays responsible during decoding.
 */
class ImageManager
{
public:

	/**
	 * Creates a ImageManager object. Should only be called once. The ImageManger should be saved in the global
	 * imgManager variable, since that variable is used internally in the img module.
	 * @param listener the ImageProgressListener that will be called when there are data to be decoded in at least one of the images belonging to the ImageManager.
	 * @return an ImageManager object, or NULL if OOM.
	 */
	static ImageManager* Create(ImageProgressListener* listener);

	/**
	 * Clears up the ImageManager and the rest of the img module.
	 * After this function is called, no functions or objects in the
	 * img module may be used.
	 */
	virtual ~ImageManager() {}

	/**
	   locks the image cache from purging. any call to FreeMemory will
	   be suppressed until UnlockImageCache is called.
	 */
	virtual void LockImageCache() = 0;
	/**
	   unlocks the image cache. calls FreeMemory if any call was made
	   while cache was locked.

	   this function will also set the current grace time, so that any
	   images in state of grace will not be purged too early (see
	   BeginGraceTime).
	 */
	virtual void UnlockImageCache() = 0;

	/**
	   Sometimes images are marked as no longer visible due to a
	   MarkExtraDirty-reflow removing its layout box. these images may
	   still be visible, in which case the next paint traversal will
	   mark them as such again. Because purging the image cache may
	   happen between the reflow and the traverse this can cause
	   images to be needlessly kicked out of the image cache only to
	   be subsequently redecoded. To remedy this a grace time can be
	   used, signaling that the image must not be purged until after a
	   specified amount of time has passed.

	   any images marked no longer visible between a call to
	   BeginGraceTime and EndGraceTime will be in state of grace, and
	   will not be purged until the time specified in
	   TWEAK_IMG_GRACE_TIME has passed.

	   should only be called when the image cache is locked from
	   purging (see LockImageCache).
	*/
	virtual void BeginGraceTime() = 0;
	/**
	   see BeginGraceTime
	 */
	virtual void EndGraceTime() = 0;

	/**
	 * Frees as much memory as possible used by the images in the system. With current cache
	 * policies, visible images cannot be freed.
	 */
	virtual void FreeMemory() = 0;

	/**
	 * Sets the cache size and the cache policy for the ImageManager.
	 * @param cache_size the size of the cache used by the img module in bytes.
	 * @param cache_policy the cache policy used in the img module, see ImageCachePolicy enumeration for documentation.
	 */
	virtual void SetCacheSize(INT32 cache_size, ImageCachePolicy cache_policy) = 0;

	/**
	 * Adds an ImageDecoderFactory to the ImageManager, to take care of decoding a specified image type.
	 * @return a OP_STATUS with OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 * @param factory the ImageDecoderFactory to add.
	 * @param type the type that this ImageDecoderFactory will handle. The image code doesn't know what the type value means, it only needs it to be unique for every decoder factory. The type may not be 0.
	 * @param check_header TRUE if we should check if a file with the wrong mime type, should be checked if it can be decoded by this ImageDecoderFactory.
	 */
	virtual OP_STATUS AddImageDecoderFactory(ImageDecoderFactory* factory, INT32 type, BOOL check_header = TRUE) = 0;

#ifdef ASYNC_IMAGE_DECODERS
	/**
	 * Sets the factory that will be used to create ImageDecoder objects for decoding images. When
	 * we are using asynchronous image decoders, the ImageDecoderFactory objects added with
	 * AddImageDecoderFactory will not be used to create ImageDecoder objects, only to check the
	 * type of an image, or the size of an image.
	 */
	virtual void SetAsyncImageDecoderFactory(AsyncImageDecoderFactory* factory) = 0;
#endif // ASYNC_IMAGE_DECODERS

	/**
	 * @return a Image correspodning to the ImageContentProvider. Will return an empty image if OOM (Image::IsEmpty() will return TRUE).
	 * @param content_provider the ImageContentProvider that represents the ImageRep that the Image will share.
	 */
	virtual Image GetImage(ImageContentProvider* content_provider) = 0;

	/**
	 * @return a Image which encapsulates a OpBitmap. Such bitmap cannot be redecoded or shared, but will stay in memory as long as there are references to the Image object.
	 * @param bitmap the bitmap to encapsulate.
	 */
	virtual Image GetImage(OpBitmap* bitmap) = 0;

	/**
	 * Resets an image, that is, all memory is freed, and all data such as Width() and Height() will
	 * disappear. The ImageContentProvider will still be there.
	 * @param content_provider the ImageContentProvider representing the ImageRep that we should reset.
	 */
	virtual void ResetImage(ImageContentProvider* content_provider) = 0;

	/**
	 * @return the type of the image with the given header. Will return 0 if image type for the header is unknown to the ImageManager.
	 * @param data_buf pointer to the data for the start of the image file.
	 * @param buf_len length of the data with the start of the image file.
	 */
	virtual int CheckImageType(const unsigned char* data_buf, UINT32 buf_len) = 0;

	/**
	 * @return TRUE if the type is a supported image type.
	 * @param type the type of image.
	 */
	virtual BOOL IsImage(int type) = 0;

	/**
	 * Decode a part of the next image in the decode queue. If we haven't finished decoding all of the
	 * images in the decode queue, ImageProgressListener::OnProgress will be called from this function.
	 */
	virtual void Progress() = 0;

#ifdef ASYNC_IMAGE_DECODERS
	/**
	 * Frees all unreferenced asynchronous ImageDecoder objects. Normally (when not using asynchronous
	 * image decoding) we delete the image decoder when loading fails. But, if we are decoding
	 * asynchronously, we are not able to destroy the image decoder immediately during decoding, since
	 * the image decoder object is referenced from outside the img module. Therefore, the img module
	 * just mark the image decoder for deletion, and deletes it when FreeUnusedAsyncDecoders() is called.
	 */
	virtual void FreeUnusedAsyncDecoders() = 0;
#endif // ASYNC_IMAGE_DECODERS

#ifdef HAS_ATVEF_SUPPORT
	/**
	 * @return a Image representing the atvef image. This image does not have any content, and will always return TRUE from Image::IsAtvefImage(). It is not possible to get an OOM condition when calling this function.
	 */
    virtual Image GetAtvefImage() = 0;
#endif

	virtual INT32 GetCacheSize() = 0;
	virtual INT32 GetUsedCacheMem() = 0;
#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	virtual INT32 GetAnimCacheSize() = 0;
	virtual INT32 GetUsedAnimCacheMem() = 0;
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

#ifdef CACHE_UNUSED_IMAGES
	/**
	 * Frees all images for a given URL context.
	 */
	virtual void FreeImagesForContext(URL_CONTEXT_ID id) = 0;
#endif // CACHE_UNUSED_IMAGES

	/**
	   helper object for placing images in state of grace - locks
	   image cache and starts grace time mode. does the opposite on
	   destruction.
	 */
	class GraceTimeLock
	{
	public:
		GraceTimeLock(ImageManager* image_manager) : m_image_manager(image_manager)
		{
			OP_ASSERT(image_manager);
			m_image_manager->LockImageCache();
			m_image_manager->BeginGraceTime();
		}
		~GraceTimeLock() { m_image_manager->EndGraceTime(); m_image_manager->UnlockImageCache(); }
	private:
		ImageManager* m_image_manager;
	};
#ifdef IMG_TOGGLE_CACHE_UNUSED_IMAGES
	virtual void CacheUnusedImages(BOOL strategy) = 0;
	virtual BOOL IsCachingUnusedImages() const = 0;
#endif // IMG_TOGGLE_CACHE_UNUSED_IMAGES
};

#endif // !IMAGE_H
