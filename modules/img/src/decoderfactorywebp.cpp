#include "core/pch.h"

#include "modules/img/decoderfactorywebp.h"
#include "modules/img/src/imagedecoderwebp.h"
#include "modules/webp/webp.h"

#ifdef WEBP_SUPPORT

ImageDecoder* DecoderFactoryWebP::CreateImageDecoder(ImageDecoderListener* listener)
{
    ImageDecoderWebP* webp_decoder = OP_NEW(ImageDecoderWebP, ());
    if (webp_decoder)
            webp_decoder->SetImageDecoderListener(listener);
    return webp_decoder;
}

BOOL3 DecoderFactoryWebP::CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height)
{
    const BOOL3 res = WebPDecoder::Peek(data, len, width, height);
    return res;
}

BOOL3 DecoderFactoryWebP::CheckType(const UCHAR* data, INT32 len)
{
    return WebPDecoder::Check(data, len);
}

#endif // WEBP_SUPPORT
