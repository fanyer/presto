#include "core/pch.h"

#ifdef ASYNC_IMAGE_DECODERS_EMULATION

#include "modules/img/src/asyncemulationdecoder.h"
#include "modules/img/src/imagemanagerimp.h"

AsyncEmulationDecoder::AsyncEmulationDecoder()
{
	buf = NULL;
	buf_len = 0;
}

AsyncEmulationDecoder::~AsyncEmulationDecoder()
{
	OP_DELETEA(buf);
	Out();
}

OP_STATUS AsyncEmulationDecoder::DecodeData(BYTE* data, INT32 numBytes, BOOL more, int& resendBytes)
{
	resendBytes = 0;
	// Append data to the image.
	if (data != NULL && numBytes > 0)
	{
		if (buf_len <= 0 || buf == NULL)
		{
			buf = OP_NEWA(char, numBytes);
			if (!buf)
				return OpStatus::ERR_NO_MEMORY;
			op_memcpy(buf, data, numBytes);
			buf_len = numBytes;
		}
		else
		{
			char* temp_buf = OP_NEWA(char, buf_len + numBytes);
			if (!temp_buf)
				return OpStatus::ERR_NO_MEMORY;
			op_memcpy(temp_buf, buf, buf_len);
			op_memcpy(temp_buf + buf_len, data, numBytes);
			OP_DELETEA(buf);
			buf = temp_buf;
			buf_len += numBytes;
		}
		((ImageManagerImp*)imgManager)->AddBufferedImageDecoder(this);
	}
	return OpStatus::OK;
}

void AsyncEmulationDecoder::SetImageDecoderListener(ImageDecoderListener* imageDecoderListener)
{
	listener = imageDecoderListener;
}

void AsyncEmulationDecoder::SetDecoder(ImageDecoder* decoder)
{
	image_decoder = decoder;
}

void AsyncEmulationDecoder::Decode()
{
	int resendBytes;
	OP_STATUS ret_val = image_decoder->DecodeData((unsigned char*)buf, buf_len, TRUE, resendBytes); // FIXME
	OP_ASSERT(OpStatus::IsSuccess(ret_val));
	OP_DELETEA(buf);
	buf = NULL;
	buf_len = 0;
}

#endif // ASYNC_IMAGE_DECODERS_EMULATION
