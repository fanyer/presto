#ifndef COCOAPASTEBOARDUTILS_H
#define COCOAPASTEBOARDUTILS_H

#define BOOL NSBOOL
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#import <Foundation/NSData.h>
#import <AppKit/NSPasteboard.h>
#undef BOOL

extern NSString* kOperaDragType;

@interface CocoaPasteboardUtils : NSObject
{
}

+ (OP_STATUS)PlaceText:(const uni_char *)text pasteboard:(NSPasteboard*)pb;
+ (OP_STATUS)PlaceURL:(const uni_char *)url pasteboard:(NSPasteboard*)pb;
+ (OP_STATUS)PlaceBitmap:(const OpBitmap*)bitmap pasteboard:(NSPasteboard*)pb;
+ (OP_STATUS)PlaceFilename:(const uni_char *)filename pasteboard:(NSPasteboard*)pb;
+ (OP_STATUS)PlaceFilenamePromise:(const uni_char *)filename pasteboard:(NSPasteboard*)pb;
+ (OP_STATUS)PlaceDummyDataOnPasteboard:(NSPasteboard*)pb;
+ (OP_STATUS)PlaceHTML:(const uni_char *)html_string pasteboard:(NSPasteboard*)pb;
@end

#endif
