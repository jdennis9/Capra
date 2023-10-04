#!/bin/sh

CC=${CC:-cc}
CFLAGS=${CFLAGS:--O2 -Wall}

DEPS="$(pkg-config --cflags --libs wlroots) $(pkg-config --cflags --libs wayland-server) \
	$(pkg-config --cflags --libs xkbcommon)  $(pkg-config --cflags --libs freetype2)"
SOURCES="capra.c xdg-shell-protocol.c"

# Uncomment to enable XWayland
XWAYLAND_CFLAGS="-DUSE_XWAYLAND=1"
XWAYLAND_LIBS="-lxcb -lxcb-icccm"

# Uncomment to enable MPD support on status bar
DEPS="$DEPS -DENABLE_MPD_STATUS=1 -lmpdclient"

set -x
$CC $CFLAGS $XWAYLAND_CFLAGS $SOURCES $DEPS -I. -Wall -DWLR_USE_UNSTABLE -lm -linput $XWAYLAND_LIBS -o capra


