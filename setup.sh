#!/bin/sh

WAYLAND_PROTOCOLS=$(pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(pkg-config --variable=wayland_scanner wayland-scanner)

set -x

$WAYLAND_SCANNER server-header $WAYLAND_PROTOCOLS/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.h
$WAYLAND_SCANNER private-code $WAYLAND_PROTOCOLS/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.c
$WAYLAND_SCANNER server-header \
	$WAYLAND_PROTOCOLS/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml \
	pointer-constraints-unstable-v1-protocol.h
$WAYLAND_SCANNER private-code \
	$WAYLAND_PROTOCOLS/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml \
	pointer-constraints-unstable-v1-protocol.c
$WAYLAND_SCANNER server-header protocols/wlr-layer-shell-unstable-v1.xml wlr-layer-shell-unstable-v1-protocol.h
$WAYLAND_SCANNER private-code protocols/wlr-layer-shell-unstable-v1.xml wlr-layer-shell-unstable-v1-protocol.c

