# Capra
Dynamic tiling window manager for Wayland. Configured using a C header file. 
XWayland support is experimental and likely very buggy, use with caution.
Comes with a built-in programmable status bar (configured in config.h).

Supported protocols:
- layer-shell-v1
- pointer-constraints-unstable-v1 (partial)

## Building
Requires FreeType, Wayland, X11, XWayland and wlroots.

- Copy config.h.defaults to config.h
- Change config.h to suit your setup
- run setup.sh
- run build.sh
- copy capra to /usr/local/bin/capra

Note: setup.sh only needs to run once.

