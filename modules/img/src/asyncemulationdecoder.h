#ifndef ASYNCEMULATIONDECODER_H
#define ASYNCEMULATIONDECODER_H

#include "image.h"

#include "modules/util/simset.h"

class AsyncEmulationDecoder : public ImageDecoder, public Link
{
public:
	AsyncEmulationDecoder();
	~AsyncEmulationDecoder();

	OP_STATUS DecodeData(BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL load_all = FALSE);

	void SetImageDecoderListener(ImageDecoderListener* imageDecoderListener);

	void SetDecoder(ImageDecoder* decoder);

	void Decode();

private:
	ImageDecoder* image_decoder;
	ImageDecoderListener* listener;
	char* buf;
	int buf_len;
};

#endif // !ASYNCEMULATIONDECODER_H
