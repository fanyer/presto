/* -*- Mode: Javascript; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

function hsv_to_rgb(h, s, v) {
    var r, g, b;
    var i;
    var f, p, q, t;

    // Make sure our arguments stay in-range
    h = Math.max(0, Math.min(360, h));
    s = Math.max(0, Math.min(100, s));
    v = Math.max(0, Math.min(100, v));

    // We accept saturation and value arguments from 0 to 100 because that's
    // how Photoshop represents those values. Internally, however, the
    // saturation and value are calculated from a range of 0 to 1. We make
    // That conversion here.
    s /= 100;
    v /= 100;

    if(s == 0) {
		// Achromatic (grey)
		r = g = b = v;
		return [Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];
    }

    h /= 60; // sector 0 to 5
    i = Math.floor(h);
    f = h - i; // factorial part of h
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));

    switch(i) {
    case 0:
		r = v;
		g = t;
		b = p;
		break;

    case 1:
		r = q;
		g = v;
		b = p;
		break;

    case 2:
		r = p;
		g = v;
		b = t;
		break;

    case 3:
		r = p;
		g = q;
		b = v;
		break;

    case 4:
		r = t;
		g = p;
		b = v;
		break;

    default: // case 5:
		r = v;
		g = p;
		b = q;
    }

    return [r, g, b];
}

function to_rgb(arr)
{
    return "rgb(" + Math.floor(arr[0] * 255) + "," + Math.floor(arr[1] * 255) + "," + Math.floor(arr[2] * 255) + ")";
}

