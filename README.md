# Janetland #

Wayland and wlroots bindings for [Janet](https://janet-lang.org/).

This is a support library for Juno, the Wayland window manager built with Janet.

**Note** that it's built against wlroots v0.16.2, and hasn't been updated since. It can't be built with wlroots 0.17.x, and further development is currently halted.

## Dependencies ##

* Wayland v1.22.0
* wlroots v0.16.2
* GCC toolchain
* pkg-config
* [JPM](https://janet-lang.org/docs/jpm.html)

## Compiling ##

Just run `jpm -l build`.

## Installing ##

To install it as a dependency for Juno, run `jpm --tree=path\to\juno\jpm_tree install`.
