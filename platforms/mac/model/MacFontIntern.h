#ifndef _MAC_FONT_UNTERN_H
#define _MAC_FONT_UNTERN_H

#include "modules/display/fontdb.h"
#include "modules/util/adt/opvector.h"

#include "platforms/mac/util/FontInfo.h"
#include "platforms/mac/pi/MacOpSystemInfo.h"

struct InternalFontItem
{
	InternalFontItem(ATSFontRef fontRef, CTFontRef aCTFontRef)
	{
		name = NULL;
#ifdef _GLYPHTESTING_SUPPORT_
		characterMap = NULL;
#endif
		monospace = FALSE;
		serifs = OpFontInfo::UnknownSerifs;
		supportNormal = TRUE;
		supportOblique = FALSE;
		supportItalic = TRUE;
		supportSmallCaps = FALSE;
		//signature = 
		atsFontRef = fontRef;
		ctFontRef = aCTFontRef;
		internalID = kInternalFontIDUnknown;
#ifdef LOAD_SERIFS_ON_DEMAND
		serifsCalculated = FALSE;
#endif
	};

	~InternalFontItem()
	{
		if (ctFontRef)
			CFRelease(ctFontRef);
		OP_DELETEA(name);
#ifdef _GLYPHTESTING_SUPPORT_
		OP_DELETEA(characterMap);
#endif
	};

	uni_char*	    name;
#ifdef _GLYPHTESTING_SUPPORT_
	uint32*			characterMap;
#endif
	BOOL			monospace;
	OpFontInfo::FontSerifs serifs;
	BOOL			supportNormal;
	BOOL			supportOblique;
	BOOL			supportItalic;
	BOOL			supportSmallCaps;
	FONTSIGNATURE	signature;
	ATSFontRef		atsFontRef;
	CTFontRef		ctFontRef;
	InternalFontID	internalID;
	BOOL			serifsCalculated;
};

class MacFontIntern
{
	friend class WebFontContainer;
public:
	MacFontIntern();
	~MacFontIntern();

	UINT32			Count() { return fontList.GetCount(); }

	uni_char	  * GetFontName(UINT32 index) { OP_ASSERT(index >= 0 && index < Count()); return fontList.Get(index)->name; }
#ifdef _GLYPHTESTING_SUPPORT_
	uint32		  * GetCharacterMap(UINT32 index); /*{ OP_ASSERT(index >= 0 && index < GetCount()); return fontList.Get(index)->characterMap; }*/
#endif
	//TextEncoding	GetEncoding(int index) { OP_ASSERT(index >= 0 && index < Count()); return fontList.Get(index)->encoding; }
	BOOL			GetIsMonospace(UINT32 index) { OP_ASSERT(index >= 0 && index < Count()); return fontList.Get(index)->monospace; }
	OpFontInfo::FontSerifs GetHasSerifs(UINT32 index);// { OP_ASSERT(index >= 0 && index < Count()); return fontList.Get(index)->serifs; }
	BOOL			GetSupportNormal(UINT32 index) { OP_ASSERT(index >= 0 && index < Count()); return fontList.Get(index)->supportNormal; }
	BOOL			GetSupportOblique(UINT32 index) { OP_ASSERT(index >= 0 && index < Count()); return fontList.Get(index)->supportOblique; }
	BOOL			GetSupportItalic(UINT32 index) { OP_ASSERT(index >= 0 && index < Count()); return fontList.Get(index)->supportItalic; }
	BOOL			GetSupportSmallCaps(UINT32 index) { OP_ASSERT(index >= 0 && index < Count()); return fontList.Get(index)->supportSmallCaps; }

	PFONTSIGNATURE	GetFontSignatures(UINT32 index) { OP_ASSERT(index >= 0 && index < Count()); return &(fontList.Get(index)->signature); }
	ATSFontRef		GetFontIdentifier(UINT32 index) { OP_ASSERT(index >= 0 && index < Count()); return fontList.Get(index)->atsFontRef; }
	CTFontRef		GetFontRef(UINT32 index) { OP_ASSERT(index >= 0 && index < Count()); return fontList.Get(index)->ctFontRef; }
	InternalFontID	GetInternalFontID(UINT32 index) { OP_ASSERT(index >= 0 && index < Count()); return fontList.Get(index)->internalID; }

	void			SortFonts();
	
protected:
	static BOOL		ParseOS2Tag(const ATSFontRef font, BOOL &isSerif);
	static void		ParseCMAPTables(const ATSFontRef font, uint32 **characterMap);
	static BOOL		ParseOS2Tag(const CTFontRef font, BOOL &isSerif);
	static void		ParseCMAPTables(const CTFontRef font, uint32 **characterMap);
	static void		ProcessCMAP(uint32 **characterMap, unsigned char *buffer, int length);
	static void		GenerateUsb(FONTSIGNATURE &fontSig, const uint32 *characterMap);
	
private:
	BOOL			GetSerifs(InternalFontID font_id, OpFontInfo::FontSerifs &serif);
	void			GetFontRefs(CFStringRef name, ATSFontRef& atsFont, CTFontRef& ctFont);

	OpVector<InternalFontItem> fontList;
	SystemLanguage mSystemLanguage;
};

#endif //_MAC_FONT_UNTERN_H
