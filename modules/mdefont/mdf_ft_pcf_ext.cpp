#include "core/pch.h"

#ifdef MDEFONT_MODULE

#ifdef MDF_FREETYPE_SUPPORT
# ifdef MDF_FREETYPE_PCF_FONT_SUPPORT

#include "modules/mdefont/mdf_ft_pcf_ext.h"
#include "modules/unicode/unicode.h"

static inline uint16 get_word(void* buf)
{
	byte* byte_buf = static_cast<byte*>(buf);
	return static_cast<uint16>( *byte_buf << 8 ) | static_cast<uint16>( *(byte_buf+1) );
}

static inline uint32 get_long(void* buf)
{
	byte* byte_buf = static_cast<byte*>(buf);
	return static_cast<uint32>( *byte_buf  << 24 ) |  
		static_cast<uint32>( *(byte_buf+1) << 16 ) |
		static_cast<uint32>( *(byte_buf+2) << 8 ) |
		static_cast<uint32>( *(byte_buf+3) );
}

static inline uint16 b2lendian(uint16 x)
{
	return ((x & 0xff) << 8) | ((x & 0xff00) >> 8);
	
}

static inline uint32 b2lendian32(uint32 x)
{
	return ((x & 0xff) << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) | ((x & 0xff000000) >> 24);
}


struct toc_entry 
{
	void init(byte* buf)
		{
			type = b2lendian32(get_long(buf));
			buf += 4;
			format = b2lendian32(get_long(buf));
			buf += 4;
			size = b2lendian32(get_long(buf));
			buf += 4;
			offset = b2lendian32(get_long(buf));
			buf += 4;
		}
	
	int32 type;		// Indicates which table
    int32 format;   // See below, indicates how the data are formatted in the table
    int32 size;		// In bytes
    int32 offset;	// from start of file
};


#define PCF_PROPERTIES		 (1<<0)
#define PCF_ACCELERATORS	 (1<<1)
#define PCF_METRICS		     (1<<2)
#define PCF_BITMAPS		     (1<<3)
#define PCF_INK_METRICS		 (1<<4)
#define	PCF_BDF_ENCODINGS	 (1<<5)
#define PCF_SWIDTHS		     (1<<6)
#define PCF_GLYPH_NAMES		 (1<<7)
#define PCF_BDF_ACCELERATORS (1<<8)

#define PCF_DEFAULT_FORMAT	   0x00000000
#define PCF_INKBOUNDS		   0x00000200
#define PCF_ACCEL_W_INKBOUNDS  0x00000100
#define PCF_COMPRESSED_METRICS 0x00000100


//returns false if no blockinfo was found
static bool GetUnicodeBlockInfo(UnicodePoint ch, /* int start_block,*/ int &block_no, UnicodePoint &block_lowest, UnicodePoint &block_highest)
{
	
	const uint32_t unicode_blocks[][2] = {
		{0x0000, 0x007F},  //  Basic Latin
		{0x0080, 0x00FF},  //  Latin-1 Supplement
		{0x0100, 0x017F},  //  Latin Extended-A
		{0x0180, 0x024F},  //  Latin Extended-B
		{0x0250, 0x02AF},  //  IPA Extensions
		{0x02B0, 0x02FF},  //  Spacing Modifier Letters
		{0x0300, 0x036F},  //  Combining Diacritical Marks
		{0x0370, 0x03FF},  //  Greek and Coptic
		{0x0400, 0x04FF},  //  Cyrillic // 8
		{0x0500, 0x052F},  //  Cyrillic Supplement
		{0x0530, 0x058F},  //  Armenian
		{0x0590, 0x05FF},  //  Hebrew
		{0x0600, 0x06FF},  //  Arabic  // 12
		{0x0700, 0x074F},  //  Syriac
		{0x0750, 0x077F},  //  Arabic Supplement
		{0x0780, 0x07BF},  //  Thaana
		{0x0900, 0x097F},  //  Devanagari
		{0x0980, 0x09FF},  //  Bengali
		{0x0A00, 0x0A7F},  //  Gurmukhi
		{0x0A80, 0x0AFF},  //  Gujarati
		{0x0B00, 0x0B7F},  //  Oriya  // 20
		{0x0B80, 0x0BFF},  //  Tamil
		{0x0C00, 0x0C7F},  //  Telugu
		{0x0C80, 0x0CFF},  //  Kannada
		{0x0D00, 0x0D7F},  //  Malayalam  //24
		{0x0D80, 0x0DFF},  //  Sinhala
		{0x0E00, 0x0E7F},  //  Thai  // 26
		{0x0E80, 0x0EFF},  //  Lao
		{0x0F00, 0x0FFF},  //  Tibetan
		{0x1000, 0x109F},  //  Myanmar
		{0x10A0, 0x10FF},  //  Georgian // 30
		{0x1100, 0x11FF},  //  Hangul Jamo
		{0x1200, 0x137F},  //  Ethiopic
		{0x1380, 0x139F},  //  Ethiopic Supplement
		{0x13A0, 0x13FF},  //  Cherokee
		{0x1400, 0x167F},  //  Unified Canadian Aboriginal Syllabics
		{0x1680, 0x169F},  //  Ogham
		{0x16A0, 0x16FF},  //  Runic
		{0x1700, 0x171F},  //  Tagalog // 38
		{0x1720, 0x173F},  //  Hanunoo
		{0x1740, 0x175F},  //  Buhid
		{0x1760, 0x177F},  //  Tagbanwa // 41
		{0x1780, 0x17FF},  //  Khmer
		{0x1800, 0x18AF},  //  Mongolian
		{0x1900, 0x194F},  //  Limbu
		{0x1950, 0x197F},  //  Tai Le  // 45
		{0x1980, 0x19DF},  //  New Tai Lue
		{0x19E0, 0x19FF},  //  Khmer Symbols
		{0x1A00, 0x1A1F},  //  Buginese
		{0x1D00, 0x1D7F},  //  Phonetic Extensions  // 49
		{0x1D80, 0x1DBF},  //  Phonetic Extensions Supplement
		{0x1DC0, 0x1DFF},  //  Combining Diacritical Marks Supplement
		{0x1E00, 0x1EFF},  //  Latin Extended Additional // 52
		{0x1F00, 0x1FFF},  //  Greek Extended
		{0x2000, 0x206F},  //  General Punctuation
		{0x2070, 0x209F},  //  Superscripts and Subscripts // 55
		{0x20A0, 0x20CF},  //  Currency Symbols
		{0x20D0, 0x20FF},  //  Combining Diacritical Marks for Symbols
		{0x2100, 0x214F},  //  Letterlike Symbols 
		{0x2150, 0x218F},  //  Number Forms  // 59 
		{0x2190, 0x21FF},  //  Arrows
		{0x2200, 0x22FF},  //  Mathematical Operators
		{0x2300, 0x23FF},  //  Miscellaneous Technical
		{0x2400, 0x243F},  //  Control Pictures // 63
		{0x2440, 0x245F},  //  Optical Character Recognition
		{0x2460, 0x24FF},  //  Enclosed Alphanumerics
		{0x2500, 0x257F},  //  Box Drawing
		{0x2580, 0x259F},  //  Block Elements  // 67
		{0x25A0, 0x25FF},  //  Geometric Shapes
		{0x2600, 0x26FF},  //  Miscellaneous Symbols
		{0x2700, 0x27BF},  //  Dingbats 
		{0x27C0, 0x27EF},  //  Miscellaneous Mathematical Symbols-A  // 71
		{0x27F0, 0x27FF},  //  Supplemental Arrows-A
		{0x2800, 0x28FF},  //  Braille Patterns
		{0x2900, 0x297F},  //  Supplemental Arrows-B
		{0x2980, 0x29FF},  //  Miscellaneous Mathematical Symbols-B // 75
		{0x2A00, 0x2AFF},  //  Supplemental Mathematical Operators
		{0x2B00, 0x2BFF},  //  Miscellaneous Symbols and Arrows
		{0x2C00, 0x2C5F},  //  Glagolitic
		{0x2C80, 0x2CFF},  //  Coptic  // 79
		{0x2D00, 0x2D2F},  //  Georgian Supplement
		{0x2D30, 0x2D7F},  //  Tifinagh
		{0x2D80, 0x2DDF},  //  Ethiopic Extended
		{0x2E00, 0x2E7F},  //  Supplemental Punctuation  // 83
		{0x2E80, 0x2EFF},  //  CJK Radicals Supplement
		{0x2F00, 0x2FDF},  //  Kangxi Radicals
		{0x2FF0, 0x2FFF},  //  Ideographic Description Characters
		{0x3000, 0x303F},  //  CJK Symbols and Punctuation  // 87
		{0x3040, 0x309F},  //  Hiragana
		{0x30A0, 0x30FF},  //  Katakana
		{0x3100, 0x312F},  //  Bopomofo
		{0x3130, 0x318F},  //  Hangul Compatibility Jamo  // 91
		{0x3190, 0x319F},  //  Kanbun
		{0x31A0, 0x31BF},  //  Bopomofo Extended
		{0x31C0, 0x31EF},  //  CJK Strokes
		{0x31F0, 0x31FF},  //  Katakana Phonetic Extensions // 95
		{0x3200, 0x32FF},  //  Enclosed CJK Letters and Months
		{0x3300, 0x33FF},  //  CJK Compatibility
		{0x3400, 0x4DBF},  //  CJK Unified Ideographs Extension A
		{0x4DC0, 0x4DFF},  //  Yijing Hexagram Symbols // 99
		{0x4E00, 0x9FFF},  //  CJK Unified Ideographs
		{0xA000, 0xA48F},  //  Yi Syllables
		{0xA490, 0xA4CF},  //  Yi Radicals
		{0xA700, 0xA71F},  //  Modifier Tone Letters  // 103
		{0xA800, 0xA82F},  //  Syloti Nagri
		{0xAC00, 0xD7AF},  //  Hangul Syllables
		{0xD800, 0xDB7F},  //  High Surrogates
		{0xDB80, 0xDBFF},  //  High Private Use Surrogates // 107
		{0xDC00, 0xDFFF},  //  Low Surrogates
		{0xE000, 0xF8FF},  //  Private Use Area
		{0xF900, 0xFAFF},  //  CJK Compatibility Ideographs
		{0xFB00, 0xFB4F},  //  Alphabetic Presentation Forms  // 111
		{0xFB50, 0xFDFF},  //  Arabic Presentation Forms-A
		{0xFE00, 0xFE0F},  //  Variation Selectors
		{0xFE10, 0xFE1F},  //  Vertical Forms
		{0xFE20, 0xFE2F},  //  Combining Half Marks  // 115
		{0xFE30, 0xFE4F},  //  CJK Compatibility Forms
		{0xFE50, 0xFE6F},  //  Small Form Variants
		{0xFE70, 0xFEFF},  //  Arabic Presentation Forms-B
		{0xFF00, 0xFFEF},  //  Halfwidth and Fullwidth Forms  // 119
		{0xFFF0, 0xFFFF},  //  Specials
		{0x10000, 0x1007F},  //  Linear B Syllabary
		{0x10080, 0x100FF},  //  Linear B Ideograms
		{0x10100, 0x1013F},  //  Aegean Numbers  // 123
		{0x10140, 0x1018F},  //  Ancient Greek Numbers
		{0x10300, 0x1032F},  //  Old Italic
		{0x10330, 0x1034F},  //  Gothic
		{0x10380, 0x1039F},  //  Ugaritic
		{0x103A0, 0x103DF},  //  Old Persian
		{0x10400, 0x1044F},  //  Deseret  // 129
		{0x10450, 0x1047F},  //  Shavian
		{0x10480, 0x104AF},  //  Osmanya
		{0x10800, 0x1083F},  //  Cypriot Syllabary
		{0x10A00, 0x10A5F},  //  Kharoshthi
		{0x1D000, 0x1D0FF},  //  Byzantine Musical Symbols
		{0x1D100, 0x1D1FF},  //  Musical Symbols  // 135
		{0x1D200, 0x1D24F},  //  Ancient Greek Musical Notation
		{0x1D300, 0x1D35F},  //  Tai Xuan Jing Symbols
		{0x1D400, 0x1D7FF},  //  Mathematical Alphanumeric Symbols
		{0x20000, 0x2A6DF},  //  CJK Unified Ideographs Extension B  // 139
		{0x2F800, 0x2FA1F},  //  CJK Compatibility Ideographs Supplement
		{0xE0000, 0xE007F},  //  Tags
		{0xE0100, 0xE01EF},  //  Variation Selectors Supplement
		{0xF0000, 0xFFFFF},  //  Supplementary Private Use Area-A  // 143
		{0x100000, 0x10FFFF},  //  Supplementary Private Use Area-B
		
		{0xFFFFFF, 0xFFFFFF} //End
	};
	
	
	const int rangemapping[][8] = {
		{0, -1}, //	Basic Latin	
		{1, -1}, //	Latin-1 Supplement	
		{2, -1}, //	Latin Extended-A	
		{3, -1}, //	Latin Extended-B	
		{4, -1}, //	IPA Extensions	
		{5, -1}, //	Spacing Modifier Letters	
		{6, -1}, //	Combining Diacritical Marks	
		{7, -1}, //	Greek and Coptic	
		{-1}, //	Reserved for Unicode SubRanges	
		{8,  //	Cyrillic	
		 9, -1}, // Cyrillic Supplementary	
		{10, -1}, //	Armenian	
		{11, -1}, //	Hebrew	
		{-1}, //	Reserved for Unicode SubRanges	
		{12, -1}, //	Arabic	
		{-1}, //	Reserved for Unicode SubRanges	
		{16, -1}, //	Devanagari	
		{17, -1}, // Bengali	
		{18, -1}, //	Gurmukhi	
		{19, -1}, //	Gujarati	
		{20, -1}, //	Oriya	
		{21, -1}, //	Tamil	
		{22, -1}, //	Telugu	
		{23, -1}, //	Kannada	
		{24, -1}, //	Malayalam	
		{26, -1}, //	Thai	
		{27, -1}, //	Lao	
		{30, -1}, //	Georgian	
		{-1}, //	Reserved for Unicode SubRanges	
		{31, -1}, //	Hangul Jamo	
		{52, -1}, //	Latin Extended Additional	
		{53, -1}, //	Greek Extended	
		{54, -1}, //	General Punctuation	
		{55, -1}, //	Superscripts And Subscripts	
		{56, -1}, //	Currency Symbols	
		{57, -1}, //	Combining Diacritical Marks For Symbols	
		{58, -1}, //	Letterlike Symbols	
		{59, -1}, //	Number Forms	
		{60,  //	Arrows	
		 72,  //	Supplemental Arrows-A	
		 74, -1}, //	Supplemental Arrows-B	
		{61,  //	Mathematical Operators	
		 76,  //	Supplemental Mathematical Operators
		 71,  //	Miscellaneous Mathematical Symbols-A	
		 75, -1}, // 	Miscellaneous Mathematical Symbols-B	
		{62, -1}, //	Miscellaneous Technical	
		{63, -1}, //	Control Pictures	
		{64, -1}, //	Optical Character Recognition	
		{65, -1}, //	Enclosed Alphanumerics	
		{66, -1}, //	Box Drawing	
		{67, -1}, //	Block Elements	
		{68, -1}, //	Geometric Shapes	
		{69, -1}, //	Miscellaneous Symbols	
		{70, -1}, //	Dingbats	
		{87, -1}, //	CJK Symbols And Punctuation	
		{88, -1}, //	Hiragana	
		{89,  //	Katakana	
		 95, -1}, //	Katakana Phonetic Extensions	
		{90,  //	Bopomofo	
		 93, -1}, //	Bopomofo Extended	
		{91, -1}, //	Hangul Compatibility Jamo	
		{-1}, //	Reserved for Unicode SubRanges	
		{96, -1}, //	Enclosed CJK Letters And Months	
		{97, -1}, //	CJK Compatibility	
		{105, -1},//	Hangul Syllables	
		{-1}, //	Non-Plane 0 * (subrange) 	
		{-1}, //	Reserved for Unicode SubRanges	
		{100, //	CJK Unified Ideographs	
		 84,  //	CJK Radicals Supplement	
		 85,  //	Kangxi Radicals	
		 86,  //	Ideographic Description Characters	
		 98,  //	CJK Unified Ideograph Extension A	
		 139, //	CJK Unified Ideographs Extension B	
		 92, -1}, //	Kanbun	
		{109, -1}, //	Private Use Area	
		{110,  //	CJK Compatibility Ideographs	
		 140, -1}, //	CJK Compatibility Ideographs Supplement	
		{111, -1}, //	Alphabetic Presentation Forms	
		{112, -1}, //	Arabic Presentation Forms-A	
		{115, -1}, //	Combining Half Marks	
		{116, -1}, //	CJK Compatibility Forms	
		{117, -1}, //	Small Form Variants	
		{118, -1}, //	Arabic Presentation Forms-B	
		{119, -1}, //	Halfwidth And Fullwidth Forms	
		{120, -1}, //	Specials	
		{28, -1},  //	Tibetan	
		{13, -1},  //	Syriac	
		{15, -1},  //	Thaana	
		{25, -1},  //	Sinhala	
		{29, -1},  //	Myanmar	
		{32, -1},  //	Ethiopic	
		{34, -1},  //	Cherokee	
		{35, -1},  //	Unified Canadian Aboriginal Syllabics	
		{36, -1},  //	Ogham	
		{37, -1},  //	Runic	
		{42, -1},  //	Khmer	
		{43, -1},  //	Mongolian	
		{73, -1},  //	Braille Patterns	
		{101,  //	Yi Syllables	
		 102, -1}, //	Yi Radicals	
		{38, -1},  //	Tagalog	
		{39,   //	Hanunoo	
		 40,   //	Buhid	
		 41, -1},  //	Tagbanwa	
		{125, -1}, //	Old Italic	
		{126, -1}, //	Gothic	
		{129, -1}, //	Deseret	
		{134,  //	Byzantine Musical Symbols	
		 135, -1}, //	Musical Symbols	
		{138, -1}, //	Mathematical Alphanumeric Symbols	
		{-1, //	Private Use (plane 15)	
		 -1},//	Private Use (plane 16)	
		{113, -1}, //	Variation Selectors	
		{141, -1}, //	Tags	
		{-1},//	93-127	Reserved for Unicode SubRanges
		
		{-1000}  // End
	};


	int range_idx = 0;
	while (true)
	{
		for (int sub=0; ;++sub)
		{
			int block_idx = rangemapping[range_idx][sub];
			if (block_idx == -1)
				break;

			if (block_idx == -1000)
			{
				return false;
			}
		
			uint32_t block_start = unicode_blocks[ block_idx ][0];
			uint32_t block_end = unicode_blocks[ block_idx ][1];		

			if ( block_start <= ch && ch <= block_end )
			{
				block_no = range_idx;
				block_lowest = block_start;
				block_highest = block_end;
				return true;
			}
		}

		++range_idx;
	}
	
}


//This class holds an FT_Stream and buffers the data.
//Used to be able to read data from gzipped pcf font files.
class FontTableStream
{
	public:
	explicit FontTableStream(FT_Stream stream, int initial_buf_size = 128)
			: m_stream(stream),
			  m_buf(NULL),
			  m_bytes_read(0),
			  m_buf_start(0),
			  m_font_table_max_size(initial_buf_size)
		{}

	~FontTableStream()
		{
			if (m_stream->read)
			{
				OP_DELETEA(m_buf);
			}
		}
	

	//Second phase constructor. Must be used.
	OP_STATUS Construct()
		{
			// If the read function is not NULL we haven to read
			// and buffer, otherwise the stream is already buffered
			// in m_stream->base.
			if (m_stream->read)
			{
				m_buf = OP_NEWA(byte,  m_font_table_max_size );
			} else {
				m_buf = m_stream->base;
			}
			
			return m_buf ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
		}
	
	byte* seek_and_buffer(int offset, int nr_of_bytes);
	
	private:
	//Standard procedure
	FontTableStream(const FontTableStream&);
	FontTableStream& operator=(const FontTableStream&);
	
	FT_Stream m_stream;
	byte* m_buf;
	int m_bytes_read; //The actual number off buffered bytes.
	int m_buf_start;  //Offset from start of stream
	int m_font_table_max_size; //Buffer max size
	
};


//Seek to the offset in stream. nr_of_bytes is the number of bytes the
//caller will/can read from the returned pointer.
byte* FontTableStream::seek_and_buffer(int offset, int nr_of_bytes)
{
	int requested_end = offset + nr_of_bytes;

	if (m_stream->read) 
	{
		if (nr_of_bytes > m_font_table_max_size) 
		{
			m_font_table_max_size = nr_of_bytes;
			OP_DELETEA(m_buf);
			m_buf = OP_NEWA(byte,  m_font_table_max_size );
			if (!m_buf)
			{
				return NULL;
			}
			
		}
			
		int available_end = m_buf_start + m_bytes_read;
		if (nr_of_bytes != 0 &&
			(offset < m_buf_start || available_end < requested_end)
			) 
		{
			m_buf_start = offset;
			m_bytes_read = m_stream->read(m_stream, m_buf_start, m_buf, m_font_table_max_size );
			if (m_bytes_read < nr_of_bytes)
			{
				return NULL;
			}
		}
	} 
	else if (m_stream->size < requested_end) 
	{
		return NULL;
	}
			
	byte* return_ptr = m_buf + offset - m_buf_start;
	return return_ptr;
}



//Freetype does not supply unicode ranges for pcf files.
bool GetPcfUnicodeRanges(const FT_Face& face, unsigned int ranges[4])
{
	const int PREL_SIZE_OF_GLYPHIDX_TAB = 128512;
	
	FontTableStream table_stream(face->stream, PREL_SIZE_OF_GLYPHIDX_TAB);
	OP_STATUS status = table_stream.Construct();
	if (OpStatus::IsError(status))
	{
		return false;
	}
	
	int buf_pos = 0;
	byte* buf_ptr =  table_stream.seek_and_buffer(buf_pos, 4);
	if (buf_ptr == NULL)
	{
		return false;
	}
	
	uint32 header = get_long(buf_ptr);
 	if ( header != FT_MAKE_TAG('\1', 'f', 'c', 'p') ) 
	{
		//No PCF bitmap font
		return false;
	}
	buf_pos +=4;
	
	buf_ptr = table_stream.seek_and_buffer(buf_pos, 4);
	if (buf_ptr == NULL)
	{
		return false;
	}
	
	int32 table_count = b2lendian32(get_long(buf_ptr));
	buf_pos += 4;
	
	for ( int i = 0; i < table_count; ++i )
	{
		buf_ptr = table_stream.seek_and_buffer(buf_pos, 16);
		if (buf_ptr == NULL)
		{
			return false;
		}
		
		buf_pos += 16;
		toc_entry table;
		table.init(buf_ptr);
		
		if ( table.type == PCF_BDF_ENCODINGS ) {

			int enc_pos = table.offset;
			byte* enc_tab = table_stream.seek_and_buffer(enc_pos, 14);
			if (enc_tab == NULL)
			{
				return false;
			}
	
			enc_pos += 14;

			enc_tab +=4; //format
					
			int16 min_char_or_byte2 = get_word(enc_tab);
			enc_tab += 2;
			
			int16 max_char_or_byte2 = get_word(enc_tab);
			enc_tab +=2;

			int16 min_byte1 = get_word(enc_tab);	
			enc_tab +=2;

			int16 max_byte1 = get_word(enc_tab);
			enc_tab +=2;
			
			int16 default_char = get_word(enc_tab);
			enc_tab +=2;
			
			//ranges[4] = {0, 0, 0, 0};
			int nr_of_indeces = (max_char_or_byte2-min_char_or_byte2+1)*(max_byte1-min_byte1+1);
			byte* glyph_indeces = table_stream.seek_and_buffer(enc_pos, nr_of_indeces*2);
			if (glyph_indeces == NULL)
			{
				return false;
			}

			for ( int16 enc1 = min_byte1; enc1 <= max_byte1; enc1++ )
			{
				for (int16 enc2 = min_char_or_byte2; enc2 <= max_char_or_byte2; enc2++)
				{
					int enc_idx = (enc1-min_byte1)*(max_char_or_byte2-min_char_or_byte2+1) + enc2-min_char_or_byte2;
					uint16 glyph_idx = get_word(glyph_indeces + enc_idx*2);
					if (glyph_idx != 0xffff && glyph_idx != default_char)
					{
						UnicodePoint encoding = (enc1 << 8) | enc2;
						int block_no = 0;
						UnicodePoint block_lowest = 0;
						UnicodePoint block_highest = 0;
						BOOL block_found = GetUnicodeBlockInfo(encoding, block_no, block_lowest, block_highest);
						if (block_found)
						{
							if (block_no < 32)
								ranges[0] |= (1 << block_no);
							else if (32 <= block_no && block_no < 64)
								ranges[1] |= 1 << (block_no-32);
							else if (64 <= block_no && block_no < 96)
								ranges[2] |= 1 << (block_no-64);
							else if (96 <= block_no && block_no < 128)
								ranges[3] |= 1 << (block_no-96);
						}
						
					}
					
				}
			}
		}
	}
	
	return true;
	
}

void GetPcfMetrics(FT_Face& face)
{
	FontTableStream table_stream(face->stream, 20);
	OP_STATUS status = table_stream.Construct();
	if (OpStatus::IsError(status))
	{
		return;
	}
	
	int buf_pos = 4; //Skip header

	byte* buf_ptr = table_stream.seek_and_buffer(buf_pos, 4);
	if (buf_ptr == NULL)
	{
		return;
	}
	int32 table_count = b2lendian32(get_long(buf_ptr));
	buf_pos += 4;
	
	for ( int i = 0; i < table_count; ++i )
	{
		buf_ptr = table_stream.seek_and_buffer(buf_pos, 16);
		if (buf_ptr == NULL)
		{
			return;
		}
		buf_pos += 16;
		toc_entry table;
		table.init(buf_ptr);
		
		if ( table.type == PCF_BDF_ACCELERATORS ) 
		{			
			byte* accel_tab = table_stream.seek_and_buffer(table.offset, 20);
			if (accel_tab == NULL)
			{
				return;
			}
			accel_tab += 12;
			
			face->size->metrics.ascender = get_long(accel_tab);
			accel_tab += 4;
			face->size->metrics.descender = get_long(accel_tab);
			accel_tab +=4;

			break;
		}
	}
}

bool IsPcfFont(const FT_Face& face)
{
	FontTableStream table_stream(face->stream, 4);
	OP_STATUS status = table_stream.Construct();
	if (OpStatus::IsError(status))
	{
		return false;
	}
	
	int buf_pos = 0;
	byte* buf_ptr =  table_stream.seek_and_buffer(buf_pos, 4);
	if (buf_ptr == NULL)
	{
		return false;
	}
	
	uint32 header = get_long(buf_ptr);
 	if ( header != FT_MAKE_TAG('\1', 'f', 'c', 'p') ) 
	{
		//No PCF bitmap font
		return false;
	}

	return true;
}

# endif // MDF_FREETYPE_PCF_FONT_SUPPORT
#endif // MDF_FREETYPE_SUPPORT

#endif // MDEFONT_MODULE
