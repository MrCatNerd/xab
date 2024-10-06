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

flags:
* --pixelated=0|1
* --vsync=0|1
* --max_framerate=0|n

<!-- readme totally not similar to picom -->

#### Hardware requirements
Anything that supports OpenGL 3.3.

## Build

### Dependencies

Assuming you already have the building tools installed (e.g. gcc, make, etc.), you still need:
* xcb
* xcb-util
* xcb-randr
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
    libxcb1-dev libxcb-atom-dev libxcb-aux0-dev libxproto-dev libxcb-util0-dev libxcb-randr0 \
    libavcodec-dev libavformat-dev libavfilter-dev libavutil-dev libswresample-dev libswscale-dev
```

TODO: fedora and arch <!-- maybe -->

### To build
```sh
make compile RELEASE=1
```
Built binary can be found in `bin/Release/xab`

---

### To install
```sh
make compile RELEASE=1
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
<summary>make options and variables</summary>

```sh
make
make all # creates compile_commands.json and compiles
make run
make compile
make clean
make compile_commands.json
sudo make install

### VARIABLES:

make ARGV=TODO

# If on release mode, verbose won't do a thing
make RELEASE=1
make VERBOSE=1

# defaults:
# VERBOSE=0
# RELEASE=0
```

</details>
