/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef JSON_BLOCKLIST_FILE_LOADER
#define JSON_BLOCKLIST_FILE_LOADER

#include "modules/ecmascript/json_listener.h"
#include "platforms/vega_backends/vega_blocklist_file.h"

/*
  format: a json file with "list entry" objects. all values except
  version number arrays are strings.

  an entry consists of:

  * an arbitrary number of regex entries - these are matched against
    strings provided by the platform. the key is passed to the
    platform so that it knows what string to return.

  * blocklist status - the status for the current entry

  * blocklist reason - a free-form string explaining why the entry
    exists. eg "buggy driver version".

  * an arbitrary number of free-form strings. these will be made
    available to the platform to facilitate quirks/workarounds.

  NOTE: "status2d", "reason2d", "status3d", "reason3d", "list entry",
  "blocklist version", "driver links", "driver entry" and "URL" are
  reserved keys, and may not be used as arbitrary regex or string
  keys.



  a slightly more formal definition of the format:



  {
    VERSION_ENTRY
    [DRIVER_LINK_LIST]
    LIST_ENTRY*
  }



  VERSION_ENTRY:
    "blocklist version": "<blocklist version string>",



  DRIVER_LINK_LIST: list of links to the download page of respective vendors' drivers
    "driver links":
    {
      DRIVER_LINK_ENTRY*
    },

  DRIVER_LINK_ENTRY: a driver link entry, consisting on an arbitrary number of regex and a URL
    "driver entry":
	{
      ARBITRARY_REGEX*
      "URL": "<http://link.to/driver>",
	},



  LIST_ENTRY: an entry in the blocklist
    "list entry":
    {
      ARBITRARY_REGEX*
      "status2d":          STATUS,
      ["reason2d":          REASON,]
      ["status3d":          STATUS,]
      ["reason3d":          REASON,]
      ARBITRARY_STRING*
    },



    ARBITRARY_REGEX: a regex entry with 0-2 number comparisons
    attached. number comparisons are made to any sub-expressions,
    which are assumed to be integers. <key> will be passed to
    platform, which will provide the string to match against.
      "<key>": { "regex": "<regex string>", [COMPARISON [COMPARISON]] },

    COMPARISON: a comparison operator and a set of associated integers, as a JSON array
      COMPARISON_OP: <array of integers>,

    COMPARISON_OP: comparison made against installed driver version
      "==" | "!=" | ">" | ">=" | "<" | "<="



    STATUS: the blocklist status for either 2D or 3D rendering;
    if not specified for 3D, the same status as for 2D is used
      "supported" | "blocked driver version" | "blocked device" | "discouraged"

    REASON: optional - the reason for the blocklist entry, as string
      "<reason>"



    ARBITRARY_STRING_ENTRY: a key/string pair
      "<key>": "<string>"

 */

class JSONBlocklistFileLoader : public VEGABlocklistFileLoader, public JSONListener
{
public:
	enum JSONState
	{
		Start = 0,
		EnterTop,
			// list version
			AttrVersion,
			StrVersion,

			DriverLinkListOrListEntryOrLeaveTop,

			// driver list
			AttrDriverLinkList,
			EnterDriverLinkList,
			DriverEntryOrLeaveDriverLinkList,
				// n entries
				AttrDriverEntry,
				EnterDriverEntry,
					RegexKeyOrURL,
					AttrURLKey,
					StrURL,
				LeaveDriverEntry,
			LeaveDriverLinkList,

			ListEntryOrLeaveTop,

			// n list entries
			AttrListEntry,
			EnterEntry,
				RegexKeyOrStatus2D,

				// n regex entries
				AttrRegexKey,
				EnterRegex,
					AttrRegex,
					StrRegex,
					Comp1OrLeaveRegex,
					AttrComp1,
						EnterComp1Array,
						CompValue1N,
						LeaveComp1Array,
					Comp2OrLeaveRegex,
					AttrComp2,
						EnterComp2Array,
						CompValue2N,
						LeaveComp2Array,
				LeaveRegex,

				// blocklist status (2D)
				AttrStatus2D,
				StrStatus2D,

				LastMandatoryInEntry = StrStatus2D,

				// optional reason (2D)
				Reason2D_Optional,
				AttrReason2D,
				StrReason2D,

				// blocklist status (3D)
				Status3D_Optional,
				AttrStatus3D,
				StrStatus3D,

				// optional reason
				Reason3D_Optional,
				AttrReason3D,
				StrReason3D,

				// n arbitrary string entries
				ArbStrKey_Optional,
				AttrArbStrKey,
				StrArbStr,
			LeaveEntry,

		LeaveTop,
		Done
	};

	JSONBlocklistFileLoader(VEGABlocklistFileLoaderListener* listener)
		: VEGABlocklistFileLoader(listener)
		, m_json_state(Start), m_current_entry(0), m_current_driver_entry(0), m_current_regex(0), m_current_key(0), m_object_level(0)
	{}

	/**
	   see VEGABlocklistFileLoader::Load
	 */
	OP_STATUS Load(OpFile* file);

	// JSON
	virtual OP_STATUS EnterObject();
	virtual OP_STATUS LeaveObject();
	virtual OP_STATUS AttributeName(const OpString& str);
	virtual OP_STATUS String(const OpString& str);
	virtual OP_STATUS Number(double num);
	virtual OP_STATUS EnterArray();
	virtual OP_STATUS LeaveArray();
	// unused
	virtual OP_STATUS Bool(BOOL val)     { return OpStatus::ERR; }
	virtual OP_STATUS Null()             { return OpStatus::ERR; }

private:
	OP_STATUS CheckAttrName(const OpString& str);

	JSONState m_json_state;
	VEGABlocklistFileEntry* m_current_entry;
	VEGABlocklistDriverLinkEntry* m_current_driver_entry;
	VEGABlocklistRegexEntry* m_current_regex;
	VEGABlocklistRegexCollection* m_current_regex_collection;
	JSONState m_regex_return_state;
	const uni_char* m_current_key;
	int m_object_level;
};

#endif // !JSON_BLOCKLIST_FILE_LOADER
