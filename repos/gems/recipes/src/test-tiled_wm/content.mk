MIRROR_FROM_REP_DIR := src/test/tiled_wm

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

# TODO move to libc
MIRROR_FROM_LIBC := \
	src/lib/libc/internal/thread_create.h \
	src/lib/libc/internal/types.h \
	src/lib/libc/spec/x86_64/internal/call_func.h \

# TODO move to libc
content: $(MIRROR_FROM_LIBC)

# TODO move to libc
$(MIRROR_FROM_LIBC):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/libports/$@ $(dir $@)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
