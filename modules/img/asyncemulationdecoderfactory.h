#ifndef ASYNCEMULATIONDECODERFACTORY_H
#define ASYNCEMULATIONDECODERFACTORY_H

#ifdef ASYNC_IMAGE_DECODERS_EMULATION

#include "modules/img/imagedecoderfactory.h"

class AsyncEmulationDecoderFactory : public AsyncImageDecoderFactory
{
public:
	ImageDecoder* CreateImageDecoder(ImageDecoderListener* listener, int type);
};

#endif // ASYNC_IMAGE_DECODERS_EMULATION

#endif // !ASYNCEMULATIONDECODERFACTORY_H
