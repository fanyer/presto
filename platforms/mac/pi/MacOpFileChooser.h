#ifndef MacOpFileChooser_H
#define MacOpFileChooser_H

class MacOpFileChooser : public OpFileChooser
{
public:
	virtual OP_STATUS ShowOpenSelector(const OpWindow* parent, const OpString& caption, const OpFileChooserSettings& settings);
	virtual OP_STATUS ShowSaveSelector(const OpWindow* parent, const OpString& caption, const OpFileChooserSettings& settings);
	virtual OP_STATUS ShowFolderSelector(const OpWindow* parent, const OpString& caption, const OpFileChooserSettings& settings);

private:
	static pascal void NavigationDialogEventProc(NavEventCallbackMessage inSelector, NavCBRecPtr ioParams, void *ioUserData);
	static pascal Boolean NavigationDialogFilterProc(AEDesc *theItem, void *info, void *callBackUD, NavFilterModes filterMode);
};

#endif // MacOpFileChooser_H
