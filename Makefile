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

CFLAGS := $(CPPFLAGS) -I$(ROOT_DIR)/src/libc/include/hosted -I$(CURDIR)/include -I$(VB_DIR)/include \
	$(USER_CFLAGS) -O2 -Wall -Wextra $(DEPFLAGS)

SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:%.c=$(OBJ)/%.o)
LIBPG := $(OUT)/libpg.a
HEADERS := $(wildcard include/pg/*.h)

.PHONY: all clean package
all: $(LIBPG)

$(OBJ)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBPG): $(OBJECTS)
	@mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

$(LIBVB):
	$(MAKE) -C $(VB_DIR) ROOT_DIR=$(ROOT_DIR)

$(CRT0) $(LIBPUREC):
	$(MAKE) -C $(ROOT_DIR)/src/libc

PKG_FILE := $(OUT)/libgui-$(VERSION)-$(PKGREL)-x86_64.pkg.tar.gz
SIGN_KEY ?= $(HOME)/.config/purec-pkg/signing.pem

package: all
	rm -f $(OUT)/libgui-*.pkg.tar.gz*
	python3 $(ROOT_DIR)/mk/pkg/mkpkg.py --name libgui --version $(VERSION)-$(PKGREL) \
		--desc "PureGUI toolkit: widgets, layout and windows on top of videod" -o $(OUT) \
		--depend "video-backend>=1.1.0" \
		--file usr/lib/libpg.a=$(LIBPG) \
		$(foreach h,$(HEADERS),--file usr/include/pg/$(notdir $(h))=$(CURDIR)/$(h))
	if [ -f "$(SIGN_KEY)" ]; then python3 $(ROOT_DIR)/mk/pkg/pkgsign.py sign $(SIGN_KEY) $(PKG_FILE) $(PKG_FILE).sig; fi
	@ls -l $(OUT)/libgui-*

clean:
	rm -rf $(OUT)

-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
