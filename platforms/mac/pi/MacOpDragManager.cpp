/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/MacOpDragManager.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/util/UTempRegion.h"
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/model/MacDragConstants.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "platforms/mac/CocoaVegaDefines.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/url/url_man.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/logdoc/urlimgcontprov.h"

#include "modules/libvega/src/oppainter/vegaopbitmap.h"

# ifdef DESKTOP_BITMAP
#  include "adjunct/desktop_util/bitmap/desktop_opbitmap.h"
# endif

struct SaveDragFileInfo
{
	OpString	suggestedName;
	OpString	savePath;
	Boolean		dragOK;
	OpBitmap*	image;
};

MacOpDragManager::MacOpDragManager()
{
	mIsDragging = false;
	mDragRef = 0;
	m_drag_object = 0;
}

MacOpDragManager::~MacOpDragManager()
{
}

static const void * GetBytePointer(void *info)
{
	return info;
}

static void FreeBitmap(void *info)
{
}

static CGDataProviderDirectAccessCallbacks providerCallbacks = {
	0,
	GetBytePointer,
	NULL,
	NULL,
	FreeBitmap
};

OP_STATUS MacOpDragManager::StartDrag(DesktopDragObject* drag_object)
{

	if(!drag_object)
	{
		return OpStatus::ERR;
	}

	DragReference dragRef = NULL;
	char * buf = (char*) g_memory_manager->GetTempBuf2k();
	int len;
	int url_len = 0;
	int title_len = 0;
	int imageurl_len = 0;
	Boolean dataSet = false;
	mIsDragging = true;
	SaveDragFileInfo info;
	info.dragOK = false;
	URL theImageURL;
	FSRef fs_ref;
	CFURLRef url_ref = NULL;
	HFSFlavor hfs;
	FSCatalogInfo cat_info;
	FileInfo* finder_info = (FileInfo*)&cat_info.finderInfo;

	mBitmap = NULL;

	const OpVector<OpString>& urls = drag_object->GetURLs();
	if (drag_object->GetURL())
		url_len = uni_strlen(drag_object->GetURL());
	if (drag_object->GetTitle())
		title_len = uni_strlen(drag_object->GetTitle());
	if(drag_object->GetImageURL()) {
		imageurl_len = uni_strlen(drag_object->GetImageURL());
		theImageURL = urlManager->GetURL(drag_object->GetImageURL());
		theImageURL.PrepareForViewing(TRUE);

		if(theImageURL.Status(TRUE) != URL_LOADED)
			return -1;

		UrlImageContentProvider* m_provider = UrlImageContentProvider::FindImageContentProvider(theImageURL);
		if(!m_provider)
		{
			m_provider = new UrlImageContentProvider(theImageURL, NULL);
			if(!m_provider)
			{
				return OpStatus::ERR_NO_MEMORY;
			}
		}

		m_provider->IncRef();
		Image img = m_provider->GetImage();

		// to force loading
		img.IncVisible(null_image_listener);

		RETURN_IF_ERROR(img.OnLoadAll(m_provider));

		if(img.ImageDecoded())
		{
			OpBitmap* bitmap = img.GetBitmap(null_image_listener);
			if (bitmap)
			{
				AddDragImage(bitmap);
			}
		}
	}
	if (!mBitmap) {
		OpBitmap* bitmap = drag_object->GetBitmap();
		if (bitmap)
		{
			AddDragImage(bitmap);
		}
	}

	if(!dragRef)
	{
		if(noErr != NewDrag(&dragRef))
		{
			dragRef = NULL;
		}
	}
	if (dragRef)
	{
		mDragRef = dragRef;
		m_drag_object = drag_object;
		mImageSet = false;
		if(imageurl_len && (drag_object->GetType() == OpTypedObject::DRAG_TYPE_LINK || drag_object->GetType() == OpTypedObject::DRAG_TYPE_IMAGE))
		{
			PromiseHFSFlavor phfs;
			DragSendDataUPP saveDragFileProc = NewDragSendDataUPP(SaveDragFile);

			OpString tmp;
			RETURN_IF_LEAVE(theImageURL.GetAttributeL(URL::KSuggestedFileName_L, tmp, TRUE));

			if(tmp.HasContent())
			{
				info.suggestedName.Set(tmp);
			}
			else
			{
				info.suggestedName.Set(UNI_L("index.html"));
			}

			if (saveDragFileProc)
			{
				if (noErr == SetDragSendProc(dragRef, saveDragFileProc, &info))
				{
					OSType	fileType = 0;
					OSType	fileCreator = 0;

					// Should we use filetype/creator nowadays?
//					gInternetPreferences.GetTypeAndCreator(info.suggestedName, fileType, fileCreator);

					phfs.fileType = fileType;
					phfs.fileCreator = fileCreator;
					phfs.fdFlags = 0;
					phfs.promisedFlavor = kDragPromisedFlavor;
/* add the promise flavor first */
					if (noErr == AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, kDragFlavorTypePromiseHFS, &phfs, sizeof(phfs), flavorNotSaved))
					{
/* add the HFS flavor immediately after the promise */
						if (noErr == AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, kDragPromisedFlavor, NULL, 0, flavorNotSaved))
						{
							dataSet = true;
							// THIS IS WRONG!

							if(mBitmap)
							{
								info.image = mBitmap;

								VEGAOpBitmap* vob = (VEGAOpBitmap*)mBitmap;
								CGImageRef image = NULL;
								if (vob) {
									CFMutableDataRef dataRef = NULL;
									CGImageDestinationRef imgDest = NULL;
									CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
									CGBitmapInfo alpha = kCGBitmapByteOrderVegaInternal;
									CGDataProviderRef provider = CGDataProviderCreateDirect(vob->GetPointer(OpBitmap::ACCESS_READONLY), vob->Height()*vob->GetBytesPerLine(), &providerCallbacks);
									image = CGImageCreate(vob->Width(), vob->Height(), 8, vob->GetBpp(), vob->GetBytesPerLine(), colorSpace, alpha, provider, NULL, 0, kCGRenderingIntentAbsoluteColorimetric);
									CGDataProviderRelease(provider);
									CGColorSpaceRelease(colorSpace);

									if (image && (GetOSVersion() >= 0x1040))
									{
/*										dataRef = CFDataCreateMutable(kCFAllocatorDefault, 0);
										imgDest = CGImageDestinationCreateWithData(dataRef, kUTTypePICT, 1, NULL);
										CGImageDestinationAddImage(imgDest, image, NULL);
										CGImageDestinationFinalize(imgDest);
										CFRelease(imgDest);
										AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, 'PICT', CFDataGetMutableBytePtr(dataRef), CFDataGetLength(dataRef), 0);
										CFRelease(dataRef);*/

										dataRef = CFDataCreateMutable(kCFAllocatorDefault, 0);
										imgDest = CGImageDestinationCreateWithData(dataRef, kUTTypePNG, 1, NULL);
										CGImageDestinationAddImage(imgDest, image, NULL);
										CGImageDestinationFinalize(imgDest);
										CFRelease(imgDest);
										AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, 'PNGf', CFDataGetMutableBytePtr(dataRef), CFDataGetLength(dataRef), 0);
										CFRelease(dataRef);

										dataRef = CFDataCreateMutable(kCFAllocatorDefault, 0);
										imgDest = CGImageDestinationCreateWithData(dataRef, kUTTypeTIFF, 1, NULL);
										CGImageDestinationAddImage(imgDest, image, NULL);
										CGImageDestinationFinalize(imgDest);
										CFRelease(imgDest);
										AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, 'TIFF', CFDataGetMutableBytePtr(dataRef), CFDataGetLength(dataRef), 0);
										CFRelease(dataRef);
									}
								}
							}
						}
					}
				}
			}
		}
		if (url_len && title_len)
		{
			len = gTextConverter.ConvertBufferToMac(drag_object->GetURL(), url_len, buf, g_memory_manager->GetTempBuf2Len());
			buf[len++] = 0x0D;
			len += gTextConverter.ConvertBufferToMac(drag_object->GetTitle(), title_len, buf+len, g_memory_manager->GetTempBuf2Len()-len);
			::AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, 'URLD', buf, len, 0);
			dataSet = true;
		}
		if (url_len)
		{
			len = gTextConverter.ConvertBufferToMac(drag_object->GetURL(), url_len, buf, g_memory_manager->GetTempBuf2Len());
			::AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, 'url ', buf, len, 0);
			::AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, kScrapFlavorTypeText, buf, len, 0);
			int ulen = uni_strlen(drag_object->GetURL());
			url_ref = CFURLCreateWithBytes(NULL, (const UInt8*)drag_object->GetURL(), 2*ulen, kCFStringEncodingUnicode, NULL);
			if (url_ref)
			{
				if(CFURLGetFSRef(url_ref,&fs_ref))
				{
					if (noErr == FSGetCatalogInfo(&fs_ref, kFSCatInfoFinderInfo, &cat_info, NULL, &hfs.fileSpec, NULL))
					{
						hfs.fileType = finder_info->fileType;
						hfs.fileCreator = finder_info->fileCreator;
						hfs.fdFlags = finder_info->finderFlags;
						::AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, kDragFlavorTypeHFS, &hfs, sizeof(hfs), 0);
					}
				}
				CFRelease(url_ref);
			}
			dataSet = true;
		}
		if (title_len)
		{
			if (url_len) {
				len = gTextConverter.ConvertBufferToMac(drag_object->GetTitle(), title_len, buf, g_memory_manager->GetTempBuf2Len());
				::AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, 'urln', buf, len, 0);
				dataSet = true;
			} else {
				::AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, kScrapFlavorTypeUnicode, drag_object->GetTitle(), title_len*sizeof(uni_char), 0);
				len = gTextConverter.ConvertBufferToMac(drag_object->GetTitle(), title_len, buf, g_memory_manager->GetTempBuf2Len());
				::AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, kScrapFlavorTypeText, buf, len, 0);
				dataSet = true;
			}
		}
		else if (!url_len)
		{
			const uni_char *dummy_title = UNI_L("Title");
			::AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, kScrapFlavorTypeUnicode, dummy_title, uni_strlen(dummy_title)*sizeof(uni_char), flavorSenderOnly);
			int len = gTextConverter.ConvertBufferToMac(dummy_title, uni_strlen(dummy_title), buf, g_memory_manager->GetTempBuf2Len());
			::AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, kScrapFlavorTypeText, buf, len, flavorSenderOnly);
			dataSet = true;
		}
		if (urls.GetCount() && !url_len && (drag_object->GetType() == OpTypedObject::DRAG_TYPE_TRANSFER))
		{
//			typeFileURL?
			for (UINT32 u = 0; u < urls.GetCount(); u++)
			{
				OpString* url = urls.Get(u);
				int ulen = url->Length();
				len = gTextConverter.ConvertBufferToMac(url->CStr(), ulen, buf, g_memory_manager->GetTempBuf2Len());
				::AddDragItemFlavor(dragRef, kOperaDragFirstItemRef + u, 'url ', buf, len, 0);
				url_ref = CFURLCreateWithBytes(NULL, (const UInt8*)url->CStr(), 2*ulen, kCFStringEncodingUnicode, NULL);
				if (url_ref)
				{
					if(CFURLGetFSRef(url_ref,&fs_ref))
					{
						if (noErr == FSGetCatalogInfo(&fs_ref, kFSCatInfoFinderInfo, &cat_info, NULL, &hfs.fileSpec, NULL))
						{
							hfs.fileType = finder_info->fileType;
							hfs.fileCreator = finder_info->fileCreator;
							hfs.fdFlags = finder_info->finderFlags;
							::AddDragItemFlavor(dragRef, kOperaDragFirstItemRef + u, kDragFlavorTypeHFS, &hfs, sizeof(hfs), 0);
						}
					}
					CFRelease(url_ref);
				}
			}
		}
		if (noErr == ::AddDragItemFlavor(dragRef, kOperaDragFirstItemRef, kOperaDragFlavourObject, &drag_object, sizeof(drag_object), flavorSenderOnly))
		{
			dataSet = true;
		}

		if (dataSet)
		{
			EventRecord evt;
			UTempRegion tempRegion;

			evt.what = mouseDown;
			if (mBitmap)
			{
				OpPoint where = CocoaOpWindow::GetLastMouseDown();
				if (
					GetOSVersion() >= 0x1050)
				{
					// This code just doesn't work for a lot of images on 10.4...
					HIPoint pt = {0,0};
					OpPoint oppt = m_drag_object->GetBitmapPos();
					pt.x = oppt.x - where.x;
					pt.y = oppt.y - where.y;
					CGImageRef img = CreateCGImageFromOpBitmap(mBitmap);
					SetDragImageWithCGImage(mDragRef, img, &pt, m_drag_object->GetType() == OpTypedObject::DRAG_TYPE_TEXT ? kDragDarkerTranslucency : kDragStandardTranslucency);
					CGImageRelease(img);
					mImageSet = true;
				}
			}
			if (!mImageSet)
			{
				//FIXME: Get real rect, I don't care what it takes.
				UTempRegion outlineRgn;
				SetRectRgn(tempRegion, evt.where.h - 20, evt.where.v - 20, evt.where.h + 20, evt.where.v + 20);
				if (outlineRgn)
				{
					::CopyRgn(tempRegion, outlineRgn);
					::InsetRgn(outlineRgn, 1, 1);
					::DiffRgn(tempRegion, outlineRgn, tempRegion);
				}
			}
			g_input_manager->ResetInput();

			::TrackDrag(dragRef, &evt, tempRegion);

			if (info.dragOK)	//Ok, we promised to give the reciever a file, so here goes.
			{
				theImageURL.SaveAsFile(info.savePath);
			}
		}
		::DisposeDrag(dragRef);

		mDragRef = NULL;
		m_drag_object = NULL;
	}

	mIsDragging = false;
	return OpStatus::OK;
}

void MacOpDragManager::AddDragImage(OpBitmap* bitmap)
{
	mBitmap = bitmap;
}

void MacOpDragManager::StopDrag()
{
	mIsDragging = false;
}

BOOL MacOpDragManager::IsDragging()
{
	return mIsDragging;
}

OpDragManager * OpDragManager::Create()
{
	return new MacOpDragManager;
}

pascal OSErr MacOpDragManager::SaveDragFile(FlavorType flavorType, void *refcon, ItemReference itemRef, DragReference dragRef)
{
	OSErr myErr = noErr;
	SaveDragFileInfo *reply = (SaveDragFileInfo *) refcon;

	// Didn't want to convert the image to PICT unless necessary, so here it is. If the receiver asks for it then do the conversion.
	if(flavorType == kDragPromisedFlavor)
	{
		AEDesc dropLocDesc = {typeNull, NULL}, targetDirDesc = {typeNull, NULL};
		FSSpec theTarget;
		FSRef parentfsref;
		PromiseHFSFlavor thePromise;
		long promiseSize = sizeof(thePromise);

		if (reply->dragOK)
			return noErr;

			/* get the drop location where */
		if (noErr != (myErr = GetDropLocation(dragRef, &dropLocDesc)))
		{
			return myErr;
		}

			/* attempt to convert the location record to a FSRef.  By doing it this way
			instead of looking at the descriptorType field,  we don't need to know what
			type the location originally was or what coercion handlers are installed.  */
		if (noErr != (myErr = AECoerceDesc(&dropLocDesc, typeFSRef, &targetDirDesc)))
		{
			AEDisposeDesc(&dropLocDesc);
			return myErr;
		}
		::AEGetDescData(&targetDirDesc, (Ptr)&parentfsref, sizeof(parentfsref));

		AEDisposeDesc(&dropLocDesc);
		AEDisposeDesc(&targetDirDesc);

		OpFileUtils::ConvertFSRefToUniPath(&parentfsref, &reply->savePath);
		reply->savePath.Append(UNI_L("/"));

		if (FindUniqueFileName(reply->savePath, reply->suggestedName))
		{
			int index = reply->savePath.FindLastOf(UNI_L('/'));
			if (index < 0)
				return myErr;
			++index;

	// Figure out WHAT excactly we promised
			GetFlavorData(dragRef, itemRef, flavorTypePromiseHFS, &thePromise, &promiseSize, 0);

	//Then keep our promise... The actual data is put into the file later.
			FSCatalogInfo catInfo;

			((FInfo *)catInfo.finderInfo)->fdType = thePromise.fileType;
			((FInfo *)catInfo.finderInfo)->fdCreator = thePromise.fileCreator;
			((FInfo *)catInfo.finderInfo)->fdFlags = thePromise.fdFlags;
			((FInfo *)catInfo.finderInfo)->fdLocation.h = 0;
			((FInfo *)catInfo.finderInfo)->fdLocation.v = 0;

			UniChar* target_name = (UniChar*)(reply->savePath.CStr()+index);
			size_t target_len = uni_strlen(reply->savePath.CStr()+index);
			myErr = FSCreateFileUnicode(&parentfsref, target_len, target_name, kFSCatInfoFinderInfo, &catInfo, NULL, &theTarget);
			if (noErr == myErr || myErr == dupFNErr)
			{
				SetDragItemFlavorData(dragRef, itemRef, thePromise.promisedFlavor, &theTarget, sizeof(theTarget), 0);
				reply->dragOK = true;
				myErr = noErr;
			}
		}
	}
	else
	{
		myErr = noErr;
	}
	return myErr;
}


