#ifndef WEBFEEDS_CAPABILITIES_H
#define WEBFEEDS_CAPABILITIES_H

#define WEBFEEDS_CAP_PRESENT     // Webfeeds module present
#define WEBFEEDS_CAP_HAS_ONFEEDLOADED	// has OpFeedListener::OnFeedLoaded()
#define WEBFEEDS_CAP_FEED_HAS_SUBSCRIBE // has OpFeed::Subscribe()
#define WEBFEEDS_CAP_FEED_USE_URL_OBJECTS // OpFeed::GetURL returns URL object, not string
#define WEBFEEDS_CAP_HAS_REMOVE_LISTENER  // has WebFeedsAPI::RemoveListener(OpFeedListener*)
#define WEBFEEDS_CAP_HAS_IS_FAILURE_STATUS // has OpFeed::IsFailureStatus
#define WEBFEEDS_CAP_NEW_DOM_BINDINGS // has methods needed by the new DOM bindings design
#define WEBFEEDS_CAP_HAS_CHECK_EXPIRES // has the Check*ForExpiredEntries() and Check*ForNumEntries() functions
#define WEBFEEDS_CAP_HAS_ENTRY_PREFETCH // has capability to prefetch linked article in new entries, and the (Set)PrefetchPrimaryWhenNewEntries method for enabeling it
#define WEBFEEDS_CAP_DOM_ENVIRONMENT  // Get/Set DOMObject takes an environment object, and stores one object per DOM_Environment
#define WEBFEEDS_CAP_FOLDER_SAVE_ONLY  // Will only use OPFILE_WEBFEEDS_FOLDER if saved store support is enabled
#define WEBFEEDS_CAP_FEED_ENTRY_HAS_AUTHOR_EMAIL // Has OpFeedEntry::GetAuthorEmail()
#define WEBFEEDS_CAP_OPSTRING_TITLE // OpFeed::GetUserDefinedTitle returns OpString, not uni_char*
#endif // WEBFEEDS_CAPABILITIES_H
