#ifndef LIBGOGI_CHECKFONT_H
#define LIBGOGI_CHECKFONT_H

/**
   attempts to verify the sanity of an sfnt font or ttc collection.
   @param font buffer containing sfnt or ttc font data
   @param size the size of the data, in bytes
   @param url the url the font was loaded from (for console error output only)
   @param doc the document requesting the font (for console error output only)
   @return
    TRUE if the font is (likely to be) sane
    FALSE if the font is corrupt or otherwise unusable (e.g. tagged as not embeddable)
 */
BOOL CheckSfnt(const UINT8* font, const UINT32 size, const URL* url = 0, FramesDocument* doc = 0);

/**
   attempts to verify the sanity of an sfnt font or ttc collection, by
   allocating a temporary buffer and filling it with the font data.
   @param fn the font file
   @param url the url the font was loaded from (for console error output only)
   @param doc the document requesting the font (for console error output only)
   @return
    OpStatus::OK if the font is (likely to be) sane
    OpStatus::ERR if the font is corrupt or otherwise unusable (e.g. tagged as not embeddable)
    OpStatus::ERR_NO_MEMORY if allocation of temporary memory failed
 */
OP_STATUS CheckSfnt(const uni_char* fn, const URL* url = 0, FramesDocument* doc = 0);

/**
   IMPORTANT: fonts passed to this function should have passed
   CheckSfnt with no errors detected, since the renaming code assumes
   the input font is sane.

   attempts to rename font by replacing the 'name' table. the old
   names will not be maintaned, the new name table will contain the
   following entries (where Subfamily is fetched from the input font):

   * Windows unicode BMP US English Font Family name
   * Windows unicode BMP US English Font Subamily name
   * Windows unicode BMP US English Unique font identifier (in the form "<name>-<subname>:<random 10-digit number>")
   * Windows unicode BMP US English Full font name
   * Windows unicode BMP US English Postscript name
   * Windows unicode BMP US English Preferred Family
   * Windows unicode BMP US English Preferred Subamily

   new checksums will be calculated for all tables, as will the
   checksum adjustment in the 'head' table.

   @param font the font to be renamed, in memory
   @param size the size of the font, in bytes
   @param name the new name of the font
   @param out_font (out) the renamed font - caller should OP_DELETEA on success
   @param out_size (out) the size, in bytes, of the renamed font
   @param replaced_name (out) if non-NULL, will contained the previous
   name (Windows unicode BMP US English Full font name, converted to
   NULL-terminated little endian uni_char string - caller should
   OP_DELETEA on success)

   @return OpStatus::OK on success, OpStatus::ERR on error,
   OpStatus::ERR_NO_MEMORY on OOM
 */

OP_STATUS RenameFont(const UINT8* font, UINT32 size, const uni_char* name, const UINT8*& out_font, size_t& out_size, uni_char** replaced_name = 0);

/**
   like version taking font and length, but will read fn to memory (or use MDE_MMAP if available)

   @param fn path to the font to rename
   @param name the new name of the font
   @param out_font (out) the renamed font - caller should OP_DELETEA on success
   @param out_size (out) the size, in bytes, of the renamed font
   @param replaced_name (out) if non-NULL, will contained the previous
   name (Windows unicode BMP US English Full font name, converted to
   NULL-terminated little endian uni_char string - caller should
   OP_DELETEA on success)

   @return OpStatus::OK on success, OpStatus::ERR on error,
   OpStatus::ERR_NO_MEMORY on OOM
 */
OP_STATUS RenameFont(const uni_char* fn, const uni_char* name,const UINT8*& out_font, size_t& out_size, uni_char** replaced_name = 0);

#endif // LIBGOGI_CHECKFONT_H
