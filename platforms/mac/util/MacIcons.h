#ifndef _MAC_UI_ICON_H_
#define _MAC_UI_ICON_H_

class OpBitmap;

OpBitmap* 	GetAttachmentIconOpBitmap(const uni_char* filename, BOOL big_attachment_icon, int iSize = 0);
OpBitmap* 	CreateOpBitmapFromIcon(IconRef icon, int cx, int cy);
OpBitmap* 	CreateOpBitmapFromIconWithBadge(IconRef backgroundIcon, int cx, int cy, OpBitmap* badgeBitmap, OpRect &badgeDestRect);
IconRef   	GetOperaApplicationIcon();
IconRef		GetOperaDocumentIcon();

#endif // _MAC_UI_ICON_H_
