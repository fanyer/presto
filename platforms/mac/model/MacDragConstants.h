#ifndef MAC_DRAG_CONSTANTS_H
#define MAC_DRAG_CONSTANTS_H

enum {
	kOperaDragFirstItemRef	= 'Odrg'
};

	// These shouldn't change!
enum {
	kOperaDragFlavourObject	= 'Oobj',

		// NOTE: this is compatibility with older versions of Opera.
		// Do not change, do not send this data flavour. only listen.
	kOperaBookmarkDataKind		= 'OBok',
	kOperaOldBookmarkDataKind	= 'urls'
};


#endif // MAC_DRAG_CONSTANTS_H
