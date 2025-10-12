<img src="res/logo.webp" alt="logo" style="width:25em;"/>

# __X11 Animated Background__

> [!WARNING]
> THIS PROJECT IS EXPERIMENTAL

> [!WARNING]
> If you are using a picom compositor, using the `egl` backend is *highly* recommended

> [!WARNING]
> Performance currently kinda sucks, im trying my best

---

## Table of Contents
1. [Introduction](#introduction)
2. [Features](#features)
3. [Usage](#usage)
4. [Prerequisites](#prerequisites)
   - [Hardware](#hardware-requirements)
   - [Dependencies](#dependencies)
   - [Video Readers](#video-readers)
   - [Optional Dependencies](#optional-dependencies)
5. [Build](#build)
   - [Setup](#setup)
   - [Build Instructions](#to-build)
   - [Installation](#to-install)
7. [Meson Options](#meson-options)
<!-- ill do them later -->
<!-- 8. [Contributing](#contributing) -->
<!-- 9. [License](#license) -->

---

> [!WARNING]
> IPC is WIP!

## About this branch
this is the ipc branch, which implements ipc through unix domain sockets
you can use this with [xab-gui](https://github.com/MRcatNerd/xab-gui)
it will be merged to main when all features are complete and stable enough

> [!NOTE]
> i will not raise the xab ipc protcol version in the spec until i merge to main!

(planned / implemented) featuers:
[x] version compatibilty check
[x] epolls
[x] multiple clients
[ ] ipc message spec implementation
[ ] add an event system to xab
[ ] dynamic client context memory resizing (don't worry bout it)

## Introduction
xab (X11 Animated Background) is an overkill animated wallpaper setter for X11 that
strives to be as feature complete as possible

<!-- TODO: a video demo -->

#### Supported file formats
Any format supported by ffmpeg

### Features
- Compatible with modern compositors (e.g. picom)
- Multimonitor support ([optional dependencies](#optional-dependencies) required)
- Can render lowres pixel art videos without them being blurry
- Custom shader support

## Usage
```sh
# example:
xab bg.mp4 --monitor=0 pixel_bg.gif --monitor=1 --pixelated=1
```

global options:
| Option | Description | default |
|--------|-------------|---------|
| `-v=0\|1`, `--vsync=0\|1` | synchronize framerate to monitor framerate | 1 |
| `--hw_accel=yes\|no\|auto` | use hardware acceleration for video decoding (hardware needs to support it) | auto |
<!-- | `--max_framerate=0\|n` | limit framerate to n fps (overrides vsync) | 0 | -->

per video/monitor options:
| Option | Description | default |
|--------|-------------|---------|
| `-p=0\|1`, `--pixelated=0\|1` | use point instead of bilinear filtering for rendering the background | 0 (bilinear) |
| `-M=n`, `--monitor=n` | which monitor to use ([optional dependencies](#optional-dependencies) required) | -1 (fullscreen) |
| `-x, --offset_x=n`    | offset wallpaper x coordinate | 0 |
| `-y, --offset_y=n`    | offset wallpaper y coordinate | 0 |

## Prerequisites

### Hardware requirements
Anything that supports OpenGL 3.3+

### Dependencies

Assuming you already have the building tools installed (e.g. gcc, meson, ninja, etc.), you still need:
* xcb
* xcb-util
* xproto
* libGL
* libEGL
* video reader dependencies

for video reading, you must have one of the following:
- mpv video reader (default):
    * libmpv
- ffmpeg video reader:
    * libavutil
    * libavcodec
    * libavformat
    * libavfilter
    * libswscale

<details>
<summary>Debian distributions (e.g. Ubuntu) with apt</summary>

```sh
sudo apt-get install libxcb1-dev libxcb-util0-dev x11proto-dev \
    libgl1-mesa-dev libegl1-mesa-dev

# mpv video reader:
sudo apt-get install libmpv-dev

# ffmpeg video reader:
sudo apt-get install libavcodec-dev libavformat-dev libavfilter-dev \
    libavutil-dev libswresample-dev libswscale-dev
```

</details>


### Video readers

If you use a video reader other than the default (mpv), you need to change the 'video_reader' option in Meson to match the chosen video reader's option. see [meson options](#meson-options) for more details

currently there are two options:

* \[default] libmpv (mpv) - this is the recommended video reader, it uses libmpv to read the video

* ffmpeg -
i made this video reader for educational purposes,
this is an ffmpeg-based video reader, it is way less performant compared to the the other options,
this is my fault because i suck at ffmpeg,
also many of the features are not implemented (yet?) like frame timing,
frame dropping

i am also planning to add libVLC support

## Optional dependencies

for multi-monitor support you must have:
* cglm version >= 0.8.4
* xcb-randr version >= 1.5


<details>
<summary>Optional dependencies on Debian distributions with apt</summary>

```sh
# xcb-randr
sudo apt-get install libxcb-randr-dev

# cglm
sudo apt-get install libcglm-dev
```


</details>

<br>

TODO: fedora and arch <!-- maybe -->

## Build

### Setup
```sh
meson setup build
```

### To build
```sh
meson compile -C build
```
Built binary can be found in `build/xab`

---

### To install
```sh
# (sudo)
meson install -C build
```
This will install xab at `/usr/local/bin` (probably)


---

### Meson options
To see the full list of the meson options, run `meson configure build`

```sh
# enable verbose logging
meson configure build -Dlog=verbose

# change video reader
meson configure build -Dvideo_reader=ffmpeg

# disable BCE files
meson configure build -Dnobce=true

# disable PCH
meson configure build -Dnopch=true

# disable ANSII colored logging
meson configure build -Dansii_log=false
```

---

### features that you probably don't care about
* Binary C embed for shader files so you can run your lil executable from anywhere
* PCH for slightly faster build times
* an overkill shader cache system
* fancy colored logging
* a cool mouse light shader

### Profiling
im not gonna fully document this so ummm deal with it

xab uses tracy for profiling:
```sh
meson setup tbuild --buildtype=debugoptimized \
    -Dlog=trace -Dopengl_debug_callback=disabled -Dtracy_enable=true
meson compile -C tbuild

```

### Testing
currently, xab has only tests for some components of the `ffmpeg` video reader,
more comprehensive test for *all of the compenents of xab are (probably) on their way at some point

to run the tests:
```sh
meson configure build -Dtests=true
meson test -C build
```

## Contributing
yes

## License
xab is licensed under the MIT license
