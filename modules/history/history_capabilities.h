#ifndef HISTORY_CAPABILITIES_H
#define HISTORY_CAPABILITIES_H

#define HISTORY_CAP_PRESENT                    // History module present
#define HISTORY_CAP_DIRECT_HISTORY_PRESENT     // History module has direct history (and not in dochand)
#define HISTORY_CAP_BOOKMARK_ENABLED_PRESENT   // History supports adding and removing bookmarks
#define HISTORY_CAP_IS_ACCEPTABLE_PRESENT      // History supports testing whether a url is acceptable for history
#define HISTORY_CAP_FORCED_SAVE_PRESENT        // History supports forcing a save
#define HISTORY_CAP_GET_PROTOCOL_FOLDERS       // History API contains GetProtocolFolders
#define HISTORY_CAP_GET_BOOKMARK_ITEMS         // History API contains GetBookmarkItems
#define HISTORY_CAP_GET_ITEM		       // History API contains GetItem
#define HISTORY_CAP_HISTORY_2_PRESENT	       // History module version 2 is present (reduced memory usage)
#define HISTORY_CAP_ITEM_HAVE_LISTENERS	       // History items support multiple listeners
#define HISTORY_CAP_IS_VISITED_PRESENT         // History supports visited links function IsVisited
#define HISTORY_CAP_DELETING_PRESENT           // History Listener API contains OnItemDeleting
#define HISTORY_CAP_DIRECT_SAVE_PRESENT        // Direct History supports Save
#define HISTORY_CAP_NICK_ENABLED_PRESENT       // History supports associating nicks with bookmarks
#define HISTORY_CAP_NAME_CHANGE                // All things named CoreHistoryModel* had their names changed
#define HISTORY_CAP_UTF8_ADD_PRESENT           // History supports adding addresses in UTF8
#define HISTORY_CAP_BROADCAST_ACCESSED         // History broadcasts AccessedUpdate to listeners
#define HISTORY_CAP_FINEGRAINED_IMAGE          // History provides a more finegrained image in OpHistoryPage::GetImage()
#define HISTORY_CAP_SYNC_TYPE_HISTORY          // History supports sync'ing of typed history if tweak for it is on
#define HISTORY_CAP_INI_KEYS_ASCII             // Supports ASCII key strings for actions/menus/dialogs/skin
#define HISTORY_CAP_IS_MAKE_UNESCAPED_PRESENT  // History unescaping urls

#endif // HISTORY_CAPABILITIES_H
