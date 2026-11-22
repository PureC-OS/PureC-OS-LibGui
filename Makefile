ROOT_DIR ?= $(abspath ..)
include $(ROOT_DIR)/mk/toolchain.mk

VERSION := $(shell cat VERSION)
PKGREL ?= 1
OUT := $(BIN_DIR)/libgui
OBJ := $(OUT)/obj
LIB_DIR := $(BIN_DIR)/lib
LIBPUREC := $(LIB_DIR)/libpurec.a
CRT0 := $(LIB_DIR)/crt0.o
LINKER_SCRIPT := $(ROOT_DIR)/linker-userspace.ld
VB_DIR ?= $(ROOT_DIR)/video-backend
LIBVB := $(BIN_DIR)/video-backend/libvb.a
LIBVB_SO := $(BIN_DIR)/video-backend/libvb.so

CFLAGS := $(CPPFLAGS) -I$(LIBC_DIR)/include/hosted -I$(CURDIR)/include -I$(VB_DIR)/include \
	$(USER_CFLAGS) -O2 -Wall -Wextra $(DEPFLAGS)

SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:%.c=$(OBJ)/%.o)
LIBPG := $(OUT)/libpg.a
HEADERS := $(wildcard include/pg/*.h)

.PHONY: all clean package
SOMAJOR := $(firstword $(subst ., ,$(VERSION)))
LIBPG_SO_REAL := $(OUT)/libpg.so.$(VERSION)
LIBPG_SO_NAME := $(OUT)/libpg.so.$(SOMAJOR)
LIBPG_SO := $(OUT)/libpg.so
SO_OBJECTS := $(SOURCES:%.c=$(OBJ)/pic/%.o)

all: $(LIBPG) $(LIBPG_SO)

$(OBJ)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBPG): $(OBJECTS)
	@mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

$(OBJ)/pic/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -I$(LIBC_DIR)/include/hosted -I$(CURDIR)/include -I$(VB_DIR)/include $(DYN_CFLAGS) -O2 -Wall -Wextra $(DEPFLAGS) -c $< -o $@

$(LIBPG_SO_REAL): $(SO_OBJECTS) $(LIBVB_SO) $(LIB_DIR)/libpurec.so
	@mkdir -p $(@D)
	$(LD) $(SHLIB_LDFLAGS) $(call SHLIB_SONAME,libpg.so.$(SOMAJOR)) -L$(dir $(LIBVB_SO)) -L$(LIB_DIR) -o $@ $(SO_OBJECTS) -lvb $(DYN_LIBPUREC)

$(LIBPG_SO_NAME): $(LIBPG_SO_REAL)
	ln -sf $(notdir $<) $@

$(LIBPG_SO): $(LIBPG_SO_NAME)
	ln -sf $(notdir $<) $@

$(LIBVB_SO):
	$(MAKE) -C $(VB_DIR) ROOT_DIR=$(ROOT_DIR)

$(LIB_DIR)/libpurec.so:
	$(MAKE) -C $(LIBC_DIR)

$(LIBVB):
	$(MAKE) -C $(VB_DIR) ROOT_DIR=$(ROOT_DIR)

$(CRT0) $(LIBPUREC):
	$(MAKE) -C $(LIBC_DIR)

PKG_FILE := $(OUT)/libgui-$(VERSION)-$(PKGREL)-x86_64.pkg.tar.gz
SIGN_KEY ?= $(HOME)/.config/purec-pkg/signing.pem

package: all
	rm -f $(OUT)/libgui-*.pkg.tar.gz*
	python3 $(ROOT_DIR)/mk/pkg/mkpkg.py --name libgui --version $(VERSION)-$(PKGREL) \
		--desc "PureGUI toolkit: widgets, layout and windows on top of videod" -o $(OUT) \
		--depend "video-backend>=1.1.0" \
		--file usr/lib/libpg.a=$(LIBPG) \
		--file usr/lib/libpg.so.$(VERSION)=$(LIBPG_SO_REAL) \
		--link usr/lib/libpg.so.$(SOMAJOR)=libpg.so.$(VERSION) \
		--link usr/lib/libpg.so=libpg.so.$(SOMAJOR) \
		$(foreach h,$(HEADERS),--file usr/include/pg/$(notdir $(h))=$(CURDIR)/$(h))
	if [ -f "$(SIGN_KEY)" ]; then python3 $(ROOT_DIR)/mk/pkg/pkgsign.py sign $(SIGN_KEY) $(PKG_FILE) $(PKG_FILE).sig; fi
	@ls -l $(OUT)/libgui-*

clean:
	rm -rf $(OUT)

-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
