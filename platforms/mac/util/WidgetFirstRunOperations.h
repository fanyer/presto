#if !defined(WIDGETFIRSTRUNOPERATIONS_H)
#define WIDGETFIRSTRUNOPERATIONS_H

/** A class which contains code which should be run the first time a new version of Opera starts
  * The functions in this class can be used to set default preferences for Opera as well
  * as display dialogs
  */
class WidgetFirstRunOperations
{
public:
	WidgetFirstRunOperations();
	~WidgetFirstRunOperations();

	void loadSystemLanguagePreferences();
	void setDefaultSystemFonts();
	void importSafariBookmarks();
};

#endif
