/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef CORS_CONFIG_H
#define CORS_CONFIG_H

#ifdef CORS_SUPPORT

/** If a response to an actual credentialled CORS request fails, the
    cookies it brought with it do not have to be ignored with the
    current spec (2011-09-30.) If the specification adopts a stricter
    failure mode, enable the following define. */
/* #define SECMAN_CROSS_ORIGIN_STRICT_FAILURE */

/** (Experimental gadget XHR+CORS support)
    Define if you want XHR to attempt using CORS if the gadget
    security policy do not allow such access.

    CORS doesn't compose well with gadgets in general (no origin),
    but by enabling this define, anonymous CORS requests over XHR
    will be considered even if the DOM_LOADSAVE security check
    for gadgets do not allow them.

    This would seem to match user expectations for XMLHttpRequest
    use better. Do notice that such CORS requests will have a 'null'
    origin, so only resources that do allow such anonymous
    cross-domain use will respond favourably. */
/* #define SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT */

#endif // CORS_SUPPORT
#endif // CORS_CONFIG_H
