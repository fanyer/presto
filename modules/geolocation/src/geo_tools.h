/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef GEOLOCATION_TOOLS_H
#define GEOLOCATION_TOOLS_H

#ifdef GEOLOCATION_SUPPORT
#include "modules/pi/OpGeolocation.h"

#ifdef OPERA_CONSOLE
#include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE


struct GeoCoordinate
{
	double longitude;
	double latitude;
};

class GeoTools
{
public:
	/** Calculate the distance between two points on the earth's surface.
	 *
	 *  Uses the haversine formula, assumes a perfectly round earth, etc., but
	 *  it should be good enough for our purposes.
	 *  
	 *  @param lat1 Latitude, in degrees, of the first point
	 *  @param long1 Longitude, in degrees, of the first point
	 *  @param lat2 Latitude, in degrees, of the second point
	 *  @param long2 Longitude, in degrees, of the second point
	 *  @return distance between points in meters
	 */
	static double Distance(double lat1, double long1, double lat2, double long2);

	/** Calculate the distance between two points on the earth's surface.
	 *
	 *  Same as overloaded function with four parameters.
	 *  
	 *  @param point1 Coordinates of first point
	 *  @param point2 Coordinates of second point
	 *  @return distance between points in meters
	 */
	static double Distance(const GeoCoordinate &point1, const GeoCoordinate &point2);

	/** Check if the difference between two WiFi data sets is significant.
	 *
	 * @param data_a A set of WiFi data, sorted by MAC address.
	 * @param data_b A set of WiFi data, sorted by MAC address.
	 * return TRUE if the difference between WiFi data is to be considered significant.
	 */
	static BOOL SignificantWifiChange(OpWifiData *data_a, OpWifiData *data_b);

	/** Check if the difference between two radio cell data sets is significant.
	 *
	 * @param data_a A set of cell towers data.
	 * @param data_b A set of cell towers data.
	 * return TRUE if the difference between cell towers data is to be considered significant.
	 */
	static BOOL SignificantCellChange(OpRadioData *data_a, OpRadioData *data_b);

#ifdef OPERA_CONSOLE
	/** Post message to error console.
	 *
	 * @param msg Message string to display on error console.
	 * @param msg_part2 Optional second part of the message, to be concatenated with the first one.
	 */
	static void PostConsoleMessage(OpConsoleEngine::Severity severity, const uni_char *msg, const uni_char *msg_part2 = NULL);

	/** Returns a note about a custom location provider URL being set.
	 *
	 * It is to be used as part of console messages.
	 * The note is either empty or informs about the preference setting that is modified.
	 */
	static const uni_char *CustomProviderNote();
#endif // OPERA_CONSOLE
};

#endif //GEOLOCATION_SUPPORT

#endif // GEOLOCATION_TOOLS_H
