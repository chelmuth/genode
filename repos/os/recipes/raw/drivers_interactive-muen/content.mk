content: drivers.config fb_drv.config event_filter.config en_us.chargen special.chargen

drivers.config fb_drv.config event_filter.config:
	cp $(REP_DIR)/recipes/raw/drivers_interactive-muen/$@ $@

en_us.chargen special.chargen:
	cp $(REP_DIR)/src/server/event_filter/$@ $@