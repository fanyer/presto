
#ifndef OP_SPEEDDIALLISTENER_H
#define OP_SPEEDDIALLISTENER_H

class DesktopSpeedDial;
class DesktopWindow;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SpeedDialConfigListener is a class to that fires events when the speed dial configuration changes. 
// Configuration changes can include changes in the number of visible speed dials or changes to the layout
// or contents of the background image.
//
//
class SpeedDialConfigurationListener
{
public:
	virtual ~SpeedDialConfigurationListener() {}

	/**
	 * Sent each time the (global) speed dial configuration dialog is opened.
	 *
	 * @param window the window "hosting" the speed dial configuration dialog
	 */
	virtual void OnSpeedDialConfigurationStarted(const DesktopWindow& window) = 0;

	/**
	* Sent each time the number of visible speed dial changes.  The listening
	* speed dial should recreate the visible interface to reflect the new layout.
	*
	*/
	virtual void OnSpeedDialLayoutChanged() = 0;

	/**
	* Sent each time the number of speed dial columns changes, i.e. if the
	* amount of speed dials in a row is determined automatically or fixed to
	* a certain column size.
	*
	*/
	virtual void OnSpeedDialColumnLayoutChanged() = 0;

	/**
	* Sent each time the background image has changed.  The listening
	* speed dial should repaint the background image as a response to this
	* event.
	*
	*/
	virtual void OnSpeedDialBackgroundChanged() = 0;

	/**
	* Sent whenever the zoom scale changes so that the speeddial can adapt its
	* thumbnails for the new scale factor.
	*/
	virtual void OnSpeedDialScaleChanged() = 0;
};

/**
 * Events describing changes in the set of DesktopSpeedDial objects managed by
 * SpeedDialManager.
 */
class SpeedDialListener
{
public:
	virtual ~SpeedDialListener() {}

	virtual void OnSpeedDialAdded(const DesktopSpeedDial& entry) = 0;
	virtual void OnSpeedDialRemoving(const DesktopSpeedDial& entry) = 0;
	virtual void OnSpeedDialReplaced(const DesktopSpeedDial& old_entry, const DesktopSpeedDial& new_entry) = 0;
	virtual void OnSpeedDialMoved(const DesktopSpeedDial& from_entry, const DesktopSpeedDial& to_entry) = 0;
	virtual void OnSpeedDialPartnerIDAdded(const uni_char* partner_id) = 0;
	virtual void OnSpeedDialPartnerIDDeleted(const uni_char* partner_id) = 0;
	virtual void OnSpeedDialsLoaded() = 0;
};


/**
 * Events describing changes in individual DesktopSpeedDial objects.
 */
class SpeedDialEntryListener
{
public:
	virtual ~SpeedDialEntryListener() {}

	virtual void OnSpeedDialDataChanged(const DesktopSpeedDial &sd) {};
	virtual void OnSpeedDialUIChanged(const DesktopSpeedDial &sd) {};
	virtual void OnSpeedDialExpired() {};
	virtual void OnSpeedDialEntryScaleChanged() {};
};

#endif // OP_SPEEDDIALLISTENER_H
