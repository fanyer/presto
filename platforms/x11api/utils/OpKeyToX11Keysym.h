/**
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OPKEYTOX11KEYSYM_H_
#define OPKEYTOX11KEYSYM_H_

#include "modules/hardcore/keys/opkeys.h"

/** Method for converting an OpKey to X11Keysym.
 *
 * @param key The virtual key pressed/released.
 * @param location The location of the key. If unknown
 *        or in the generic/standard area, OpKey::LOCATION_STANDARD.
 * @param shiftstate The state of the modifiers at the
 *        time of the event.
 * @param key_value The character value produced by the key. NULL
 *        if a key producing no value (function keys, modifier keys, etc.)
 * @return The correspoinding X11 keysym. Unknown keys are
 *         mapped to XK_VoidSymbol.
 */
unsigned int OpKeyToX11Keysym(OpKey::Code key, OpKey::Location location, ShiftKeyState shiftstate, const uni_char *key_value);

#endif  // OPKEYTOX11KEYSYM_H_
