/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#ifndef _IMG_TESTUTILS_H_
#define _IMG_TESTUTILS_H_

#include "modules/formats/base64_decode.h"
#include "modules/img/image.h"
#include "modules/util/opfile/opfile.h"


/**
   this file contains some helper functionality for the img module
   selftests. there's probably more stuff in the various selftest
   files that should be moved to here eventually.
 */


/**
   naive implementation of content provider - stores an entire image in memory
 */
class TestImageContentProvider : public ImageContentProvider
{
public:
    /**
       obtains ownership of data, OP_DETLETEAs when destroyed.
    */
	TestImageContentProvider(char* data, const size_t numbytes)
	: m_data(data), m_numbytes(numbytes), m_content_type(0)
    {}
    ~TestImageContentProvider() { OP_DELETEA(m_data); }

    OP_STATUS GetData(const char*& data, INT32& data_len, BOOL& more) { data = m_data; data_len = m_numbytes; more = FALSE; return OpStatus::OK; }
    OP_STATUS Grow() { return OpStatus::ERR; }
    void  ConsumeData(INT32 len) { }

    void Reset() {}
	void Rewind() {};
    BOOL IsLoaded() { return TRUE; }
    INT32 ContentType() { return m_content_type; }
    void SetContentType(INT32 type) { m_content_type = type; }
    INT32 ContentSize() { return m_numbytes; }
    BOOL IsEqual(ImageContentProvider* content_provider) { return FALSE; } // FIXME:IMG-EMIL

	/**
	   creates a content provider from an image file
	 */
    static OP_STATUS Create(const uni_char* image_file_path, ImageContentProvider*& provider)
    {
        // open file for reading
        OpFile image;
        RETURN_IF_ERROR(image.Construct(image_file_path));
        RETURN_IF_ERROR(image.Open(OPFILE_READ));
        OpFileLength len;
        RETURN_IF_ERROR(image.GetFileLength(len));
		const size_t size = (size_t)len;

        // create storage for data
        OpAutoArray<char> data(OP_NEWA(char, size));
        if (!data.get())
            return OpStatus::ERR_NO_MEMORY;
        // read
        OpFileLength read;
        RETURN_IF_ERROR(image.Read(data.get(), len, &read));
        if (read != len)
	        return OpStatus::ERR;

        // create provider
        TestImageContentProvider* p = OP_NEW(TestImageContentProvider, (data.get(), size));
        if (!p)
            return OpStatus::ERR_NO_MEMORY;
        data.release();
        provider = p;
        return OpStatus::OK;
    }

	/**
	   creates a content provider from base64-decoded data
	 */
	static OP_STATUS Create(const TempBuffer& buffer, ImageContentProvider*& provider)
	{
		// create storeage for decoded data
		const size_t base64_len = buffer.Length();
		OpAutoArray<char> data(OP_NEWA(char, ((base64_len+3)/4)*3));
		if (!data.get())
			return OpStatus::ERR_NO_MEMORY;

		// create single-byte represenatation of base64-encoded data
		OpString8 base64;
		char* base64_buf = base64.Reserve(base64_len);
		if (!base64_buf)
			return OpStatus::ERR_NO_MEMORY;
		uni_cstrlcpy((char*)base64_buf, buffer.GetStorage(), base64_len + 1);

		// decode base64-encoded data
		unsigned long read_pos;
		BOOL decode_warning = FALSE;
		const unsigned long decoded =
			GeneralDecodeBase64((unsigned char*)base64_buf, base64_len, read_pos, (unsigned char*)data.get(), decode_warning);
		if (decode_warning)
			return OpStatus::ERR;

		// create content provider
		TestImageContentProvider* p = OP_NEW(TestImageContentProvider, (data.get(), decoded));
		if (!p)
			return OpStatus::ERR_NO_MEMORY;
		data.release();
		provider = p;
		return OpStatus::OK;
	}

private:
    const char* const m_data;
    const size_t m_numbytes;
    INT32 m_content_type;
};

/**
   small wrapper that aggregates Image class, used to make
   clean-up less of a mess
*/
class TestImage
{
public:
    TestImage(ImageListener* listener = null_image_listener)
        : m_listener(listener)
        , m_bitmap(0)
        , m_visible(FALSE)
        , m_content_provider(0)
    {}
    ~TestImage() { Reset(); }

	/**
	   use with caution, since accessing m_image directly will prevent
	   TestImage from doing the needed clean-up. especially, do not
	   mix TestImage::Bitmap with Image::GetBitmap.
	 */
	Image* operator->() { return &m_image; }

    // releases all data associated with the image
    void Reset()
    {
        if (m_visible)
        {
            m_image.DecVisible(m_listener);
            m_visible = FALSE;
        }
        if (m_bitmap)
        {
            m_image.ReleaseBitmap();
            m_bitmap = 0;
        }
        m_image.Empty();
        if (m_content_provider)
        {
	        OP_DELETE(m_content_provider);
	        m_content_provider = 0;
        }
    }

	// caution: do not mix with Image::GetBitmap (via operator->)
    OpBitmap* Bitmap()
    {
        if (!m_bitmap)
            m_bitmap = m_image.GetBitmap(m_listener);
        return m_bitmap;
    }

    UINT32 Width()  const { return m_image.Width();  }
    UINT32 Height() const { return m_image.Height(); }

	/**
	   loads and decodes an image
	 */
	OP_STATUS Load(ImageContentProvider* content)
	{
        // should not be called when an image is loaded - use Reset() before calling
        OP_ASSERT(!m_visible && !m_bitmap && m_image.IsEmpty());

        // decode image
        m_image = imgManager->GetImage(content);
        OP_STATUS status;
        status = m_image.IncVisible(m_listener);
        if (OpStatus::IsError(status))
        {
            Reset();
            return status;
        }
        m_visible = TRUE;
        status = m_image.OnLoadAll(content);
        if (OpStatus::IsError(status))
        {
            Reset();
            return status;
        }
        OP_ASSERT(m_image.ImageDecoded());
        return OpStatus::OK;
	}

	/**
	   load and decode an image from file
	 */
    OP_STATUS Load(const uni_char* image_file_path)
    {
        // create content provider
        OP_ASSERT(!m_content_provider);
        RETURN_IF_ERROR(TestImageContentProvider::Create(image_file_path, m_content_provider));

		// load image
        return Load(m_content_provider);
    }
	/**
	   load and decode an image from file
	 */
    OP_STATUS Load(const char* image_file_path)
    {
        OpString path;
        RETURN_IF_ERROR(path.Set(image_file_path));
		return Load(path.CStr());
    }

	/**
	   load and decode a base64-encoded image
	 */
    OP_STATUS Load(TempBuffer& buffer)
    {
        // create content provider
        OP_ASSERT(!m_content_provider);
        RETURN_IF_ERROR(TestImageContentProvider::Create(buffer, m_content_provider));

		// load image
		return Load(m_content_provider);
    }

private:
    Image m_image;
    ImageListener* const m_listener;
    OpBitmap* m_bitmap;
    BOOL m_visible; // TRUE if IncVisible has been called on m_image
	ImageContentProvider* m_content_provider;
};

#endif // !_IMG_TESTUTILS_H_
