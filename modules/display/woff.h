#ifndef _WOFF_H_
#define _WOFF_H_

#ifdef USE_ZLIB

/**
   converts a wOFF font to an sfnt font
   @param woffData the wOFF font data
   @param woffSize the size of the wOFF font data, in bytes
   @param sfntData (out) on success will point to allocated data
   containing the corresponding sfnt font - caller should OP_DELETEA
   @param sfntSize (out) the size of the sfnt font data, in bytes
   @return OpStatus::OK on success (caller should OP_DELETEA
   sfntData), OpStatus::ERR if woffData doesn't contain a valid wOFF
   font, OpStatus::ERR_NO_MEMORY on OOM
 */
OP_STATUS wOFF2sfnt(const UINT8* woffData,  const size_t  woffSize,
                    const UINT8*& sfntData, size_t& sfntSize);

/**
   @param fontData the font data
   @param fontSize the size of the font data, in bytes
   @return TRUE if woffData contains a wOFF file (first four bytes are
   'wOFF'), FALSE otherwise
 */
BOOL IswOFF(const UINT8* fontData, const size_t fontSize);

/**
   converts a wOFF font to an sfnt font
   @param woffFile full absolute path to a wOFF font file
   @param sfntFile full absolute path to the resulting sfnt font file
   @return OpStatus::OK on success, OpStatus::ERR if woffData doesn't
   contain a valid wOFF font, OpStatus::ERR_NO_MEMORY on OOM
 */
OP_STATUS wOFF2sfnt(const uni_char* woffFile, const uni_char* sfntFile);

/**
   @param fontFile full absolute path to a font file
   @return
   OpStatus::ERR on failure (file not found etc)
   OpStatus::ERR_NO_MEMORY on OOM
   OpBoolean::IS_TRUE if fontFile is a wOFF file (first four bytes are 'wOFF')
   OpBoolean::IS_FALSE otherwise
 */
OP_BOOLEAN IswOFF(const uni_char* fontFile);

#endif // USE_ZLIB

#endif // !_WOFF_H_

