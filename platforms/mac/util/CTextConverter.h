#ifndef C_TEXT_CONVERTER_H
#define C_TEXT_CONVERTER_H

enum StringEncoding {
	kStringEncodingUniChar,
	kStringEncodingUTF16,
	kStringEncodingUTF8,
	kStringEncodingLatin1
};

typedef UInt16 TextCoverterOptions;
enum {
	kTextCoverterDefaultOptions				= 0x0000,
	kTextCoverterOptionLinebreaksToSpace	= 0x0001,
	kTextCoverterOptionNBSPToSpace			= 0x0002,
	kTextCoverterOptionTabToSpace			= 0x0004,
	kTextCoverterOptionCollapseWhiteSpace	= 0x0008,
	kTextCoverterOptionRemoveControlChars	= 0x0010,
	kTextCoverterTextOutDefaultOptions	= kTextCoverterOptionLinebreaksToSpace | kTextCoverterOptionNBSPToSpace | kTextCoverterOptionTabToSpace
};

class CTextConverter
{
public:
			CTextConverter();
			~CTextConverter();

	UInt32	ConvertStringToMacC(const void* inSrcStr, char* ioDestString, UInt32 inDestLen,
				TextCoverterOptions inOptions = kTextCoverterDefaultOptions,
				StringEncoding inSrcType = kStringEncodingUniChar);

	UInt32	ConvertStringToMacP(const void* inSrcStr, Str255 ioDestString,
				TextCoverterOptions inOptions = kTextCoverterDefaultOptions,
				StringEncoding inSrcType = kStringEncodingUniChar);

	UInt32	ConvertBufferToMac(const void* inSrcStr, UInt32 inSrcLen, char* ioDestString, UInt32 inDestLen,
				TextCoverterOptions inOptions = kTextCoverterDefaultOptions,
				StringEncoding inSrcType = kStringEncodingUniChar);


	UInt32	ConvertStringFromMacC(const char* inSrcStr, void* ioDestString, UInt32 inDestLen,
				TextCoverterOptions inOptions = kTextCoverterDefaultOptions,
				StringEncoding inDestType = kStringEncodingUniChar);

	UInt32	ConvertStringFromMacP(ConstStr255Param inSrcStr, void* ioDestString,  UInt32 inDestLen,
				TextCoverterOptions inOptions = kTextCoverterDefaultOptions,
				StringEncoding inDestType = kStringEncodingUniChar);

	UInt32	ConvertBufferFromMac(const char* inSrcStr, UInt32 inSrcLen, void* ioDestString, UInt32 inDestLen,
				TextCoverterOptions inOptions = kTextCoverterDefaultOptions,
				StringEncoding inDestType = kStringEncodingUniChar);


	UInt32	CleanupMacText(char* ioStr, UInt32 inLen, TextCoverterOptions inOptions);

	Boolean	GotConversionError();

private:
	TextEncoding mEncodingUTF16;
	TextEncoding mEncodingUTF8;
	TextEncoding mEncodingLatin1;
	TextEncoding mEncodingMac;

	TECObjectRef mMacToUTF16;
	TECObjectRef mMacToUTF8;
	TECObjectRef mMacToLatin1;
	TECObjectRef mUTF16ToMac;
	TECObjectRef mUTF8ToMac;
	TECObjectRef mLatin1ToMac;

	Boolean mConversionError;
};

extern CTextConverter *gTextConverter;

#endif //!C_TEXT_CONVERTER_H
