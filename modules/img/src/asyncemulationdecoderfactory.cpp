#include "core/pch.h"

#ifdef ASYNC_IMAGE_DECODERS_EMULATION

#include "asyncemulationdecoderfactory.h"
#include "modules/img/src/asyncemulationdecoder.h"
#include "modules/img/src/imagemanagerimp.h"

ImageDecoder* AsyncEmulationDecoderFactory::CreateImageDecoder(ImageDecoderListener* listener, int type)
{
	ImageManagerImp* imageManagerImp = (ImageManagerImp*)imgManager;
	ImageDecoderFactory* factory = imageManagerImp->GetImageDecoderFactory(type);
	if (factory != NULL)
	{
		AsyncEmulationDecoder* decoder = OP_NEW(AsyncEmulationDecoder, ());
		if (decoder)
		{
			decoder->SetImageDecoderListener(listener);
			decoder->SetDecoder(factory->CreateImageDecoder(listener));
		}
		return decoder;
	}

	return NULL;
}

#endif // ASYNC_IMAGE_DECODERS_EMULATION
