/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OT_TABLE_H
#define OT_TABLE_H

#ifdef MDEFONT_MODULE
#ifdef MDF_OPENTYPE_SUPPORT

#include "modules/mdefont/mdefont.h"
#include "modules/mdefont/mdf_ot_indic.h"
#include "modules/mdefont/processedstring.h"

struct MDE_FONT;
struct MDE_BUFFER;

class OTHandler
{
    enum LOOKUP_TYPE
    {
	TYPE_GSUB,
	// room for other tables here (i.e. gpos etc)
    };
public:
    static OP_STATUS Create(MDE_FONT* font, OTHandler*& handler);

    const UINT8* GetGSUBTable() { return m_gsub; }

    ~OTHandler();

	/**
	   processes str, applying complex script handling.
	   @return
	     (OP_STATUS)1 if processing is not necessary (in which case processed_string is left as-is)
		 OpStatus::ERR_NO_MEMORY on OOM
		 OpStatus::ERR on other error
		 OpStatus::OK on success
	 */
	static OP_STATUS ProcessString(MDE_FONT* font,
								   ProcessedString& processed_string,
								   const uni_char* str, const unsigned int in_len,
								   const int extra_space = 0, const short word_width = -1,
								   const int flags = MDF_PROCESS_FLAG_NONE);


	// messy - handle to processed text. must never be created with new,
	// since it unlocks resources on destruction. remember to check status
	// of object.
	class OTProcessedText
	{
		friend class OTHandler;
	public:
		~OTProcessedText(); // will unlock opentype buffers if locked
		// status of GetProcessedText() - i.e. status of GetHandler(), Lock() and Process()
		OP_STATUS Status() const;
		UINT32 Length() const;
		const uni_char* UStr() const;
		const uni_char* IStr() const;

	private:
		// dummy - used if no processing needed or on failure
		OTProcessedText(const OP_STATUS status = OpStatus::OK);
		OTProcessedText(OTHandler* handler, const uni_char* ustr, const uni_char* istr, const UINT32 length);

		OTHandler* const m_handler;
		const uni_char* const m_ustr;
		const uni_char* const m_istr;
		const UINT32 m_length;
		const OP_STATUS m_status;
#ifdef DEBUG
		mutable BOOL m_checked_status;
#endif // DEBUG
	};
	friend OTProcessedText::~OTProcessedText();
	// returned object should be used if it reports non-zero length
	// remember to check satus!
	static const OTProcessedText GetProcessedText(MDE_FONT* font, const uni_char* str, const UINT32 len);

protected:
    // protected - use create method instead
    OTHandler(const UINT8* gsub, const UINT32 gsub_len);

    static BOOL NeedsProcessing(const uni_char* str);

    OP_STATUS Lock(const UINT32 len);
    void Unlock();
    OP_STATUS Grow(const UINT32 len);

    OP_STATUS Process(MDE_FONT* font, const uni_char* str, const UINT32 len);
    OP_STATUS ProcessGeneric(MDE_FONT* font, const UINT32 s, UINT32& l);
    OP_STATUS ApplyGenericFeatures(const UINT8* LanguageSystem,
                                   const UINT32 s, UINT32& l);
    OP_STATUS ProcessIndic(const IndicGlyphClass::Script script,
                           MDE_FONT* font, const UINT32 s, UINT32& l);
    OP_STATUS ApplyIndicFeatures(const UINT32 s, UINT32& l);
    void GetRegion(MDE_FONT* font, const UINT32 s, UINT32& l);

    BOOL ApplyFeature(LOOKUP_TYPE lookupType, const UINT8* Feature);
    BOOL ApplyLookup(const UINT16 lookupIndex);


    // GSUB specific functionality
    BOOL ApplyGSUBFeature(const UINT32 featureTag,
		      UINT32& p, UINT32& slen);
    BOOL ApplyGSUBFeature(const UINT8* Feature,
		      UINT32& p, UINT32& slen);
    BOOL ApplyGSUBLookup(const UINT8* Lookup);
    BOOL ApplyExtensionSubstLookup(const UINT8* Lookup);
    // substitutions
	typedef BOOL (OTHandler::*SFNTSubtableFunc)(const UINT8*, const UINT16);
	SFNTSubtableFunc GetSubtableFunc(const UINT16 type);
    BOOL ApplySingleSubst(const UINT8* Subtable, const UINT16 flags);               // type 1
    BOOL ApplyMultiSubst(const UINT8* Subtable, const UINT16 flags);                // type 2
    BOOL ApplyLigSubst(const UINT8* Subtable, const UINT16 flags);                  // type 3
    BOOL ApplyAlternateSubst(const UINT8* Subtable, const UINT16 flags);            // type 4
    BOOL ApplyContextSubst(const UINT8* Subtable, const UINT16 flags);              // type 5
    BOOL ApplyChainingContextSubst(const UINT8* Subtable, const UINT16 flags);      // type 6
    BOOL ApplyReverseContextSingleSubst(const UINT8* Subtable, const UINT16 flags); // type 8
    // for contextual substitutions
    BOOL ApplyContextSubst(const UINT8* Subtable, const UINT16 flags, BOOL chaining);
    // context substitution formats
    BOOL ApplyContextSubst1(const UINT8* Subtable, const UINT16 flags, const UINT16 coverageIndex);
    BOOL ApplyContextSubst2(const UINT8* Subtable, const UINT16 flags);
    BOOL ApplyContextSubst3(const UINT8* Subtable, const UINT16 flags);
    BOOL ApplyChainContextSubst1(const UINT8* Subtable, const UINT16 flags, const UINT16 coverageIndex);
    BOOL ApplyChainContextSubst2(const UINT8* Subtable, const UINT16 flags);
    BOOL ApplyChainContextSubst3(const UINT8* Subtable, const UINT16 flags);
    BOOL ApplyContextSubstitutions(const UINT8* SubstLookupRecord, const UINT16 substCount);
    // sets character at position idx (relative to string, not syllable) to ch
    void SetChar(const UINT16 ch, const UINT32 idx);
    // applies a substitution (e.g. ligature) on the current strings
    void ApplySubst(const UINT16 ch, const UINT16 len);

    static BOOL GetOTBuffers(class TempBuffer** u, class TempBuffer** i);
    static void ReleaseOTBuffers();

    const UINT8 *m_gsub, *m_gsub_end;
    const UINT8* m_table_end;
    const UINT8* m_ScriptList;
    const UINT8* m_FeatureList;
    UINT16 m_FeatureCount;

    const UINT8* m_LookupList;
    UINT16 m_LookupCount;
    BOOL (OTHandler::*m_LookupFunc)(const UINT8* Lookup);

    UINT32 m_base, m_len, m_pos, m_slen, m_advance;
    BOOL m_has_reph;

    OP_STATUS m_status;

    // internal buffers for (output) unicode string and glyph index string
    class TempBuffer* m_ubuf;
    class TempBuffer* m_ibuf;
    uni_char* m_istr;
    uni_char* m_ustr;
};

class OTCacheEntry : public Link
{
public:
    static OTCacheEntry* Create(MDE_FONT* font, const uni_char* family_name, const uni_char* file_name);

    OTCacheEntry();
    ~OTCacheEntry();
    OTHandler* m_handler;
    uni_char* m_family_name;
	uni_char* m_file_name;
};

#if 1
inline OP_STATUS OTHandler::OTProcessedText::Status() const
{
#ifdef DEBUG
	m_checked_status = TRUE;
#endif // DEBUG
	return m_status;
}
inline UINT32 OTHandler::OTProcessedText::Length() const
{
	OP_ASSERT(m_handler || !m_length);
	return m_length;
}
inline const uni_char* OTHandler::OTProcessedText::UStr() const {	return m_ustr; }
inline const uni_char* OTHandler::OTProcessedText::IStr() const { return m_istr; }
#endif // 1

#endif // MDF_OPENTYPE_SUPPORT
#endif // MDEFONT_MODULE

#endif // OT_TABLE_H
