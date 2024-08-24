<!-- xab temporary name, probably -->

<!-- TODO: -->
<!-- TOC -->
<!-- supported file formats -->

<img src="res/logo.webp" alt="logo" style="width:30em;"/>

__X11 Animated Background__

> [!WARNING]
> THIS PROJECT NOT FULLY BAKED YET

---

### Run
xab \<path/to/file.mp4> \<screen-number>

<!-- readme totally not similar to picom -->

#### Hardware requirements
Anything that supports OpenGL ES 3.3.

## Build

### Dependencies

Assuming you already have the building tools installed (e.g. gcc, make, etc.), you still need:
* xcb
* xcb-util
* xproto
* libGL
* libEGL
* libepoxy
* libavutil
* libavcodec
* libavformat
* libavfilter
* libswscale

On Debian distributions (e.g. Ubuntu), the needed packages are
```sh
sudo apt-get install \
    libegl2-dev libepoxy-dev \
    libxcb1-dev libxcb-atom-dev libxcb-aux0-dev libxproto-dev libxcb-util0-dev \
    libavcodec-dev libavformat-dev libavfilter-dev libavutil-dev libswresample-dev libswscale-dev
```

TODO: fedora and arch

### To build
```sh
make compile RELEASE=1
```
Built binary can be found in `bin/Release/xab` <!-- TODO: find a name for da program -->

---

### To install
```sh
sudo make install RELEASE=1
```
this will install xab at `/usr/local/bin`

---

### Compilation databases
requires [__bear__](https://github.com/rizsotto/Bear)

```sh
make compile_commands.json
```

---

<details>
<summary>all of the make options</summary>

```sh
make
make all # creates compile_commands.json and compiles
make run
make compile
make clean
make compile_commands.json
sudo make install

### VARIABLES:

# If on release mode, verbose won't do a thing
make RELEASE=1
make VERBOSE=1

# defaults:
# VERBOSE=0
# RELEASE=0
```

</details>
