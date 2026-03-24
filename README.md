# My Heartbeat App

This is a standalone C++ daemon that simulates a heartbeat sensor value on D-Bus.
It is designed to be built using Meson and deployed via Yocto/OpenBMC.

## Build Requirements
* Meson & Ninja
* sdbusplus
* Boost

## Build Instructions (Standalone)
```bash
meson setup builddir
ninja -C builddir