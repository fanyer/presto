OPERA=opera
ZIP=zip
WIDGETNAME=probelogviewer.wgt
WIDGETFILES=config.xml index.html probelogviewer.js helpers.js

widget: $(WIDGETNAME)

$(WIDGETNAME): $(WIDGETFILES) Makefile
	@$(ZIP) -q $(WIDGETNAME) $(WIDGETFILES)
	@echo \'$(WIDGETNAME)\' created/updated. Type \'$(MAKE) install\' to install into Opera

install: $(WIDGETNAME)
	@$(OPERA) $(WIDGETNAME)

.PHONY: install widget