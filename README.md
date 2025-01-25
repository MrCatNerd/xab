<img src="res/logo.webp" alt="logo" style="width:25em;"/>

# __X11 Animated Background__

> [!WARNING]
> THIS PROJECT IS HIGHLY EXPERIMENTAL

> [!WARNING]
> If you are using a picom compositor, using the `egl` backend is *highly* recommended

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

## Introduction
xab (X11 Animated Background) is an animated wallpaper setter for X11 that
strives to be as feature complete as possible while maintaining reasonable resource usage
<!-- TODO: video -->

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
| `-M=n`, `--monitor=n` | which monitor to use ([optional dependencies](#optional-dependencies) required) | -1 (fullscreen) |
| `-v=0\|1`, `--vsync=0\|1` | synchronize framerate to monitor framerate | 1 |
<!-- | `--max_framerate=0\|n` | limit framerate to n fps (overrides vsync) | 0 | -->

per video/monitor options:
| Option | Description | default |
|--------|-------------|---------|
| `-p=0\|1`, `--pixelated=0\|1` | use point instead of bilinear filtering for rendering the background | 0 (bilinear) |
| `--hw_accel=yes\|no\|auto` | use hardware acceleration for video decoding (hardware needs to support it) | auto |
<!-- TODO (not implemented yet)-->
<!-- * -x, --offset_x=n    | offset wallpaper x coordinate (default: 0) -->
<!-- * -y, --offset_y=n    | offset wallpaper y coordinate (default: 0) -->

## Prerequisites

### Hardware requirements
Anything that supports OpenGL 3.3

### Dependencies

Assuming you already have the building tools installed (e.g. gcc, meson, ninja, etc.), you still need:
* xcb
* xcb-util
* xproto
* libepoxy
* libGL
* libEGL
* video reader dependencies

for video reading, you must have one of the following:
- mpv video reader (default):
    * mpv (libmpv)
- ffmpeg video reader:
    * libavutil
    * libavcodec
    * libavformat
    * libavfilter
    * libswscale

<details>
<summary>Debian distributions (e.g. Ubuntu) with apt</summary>

```sh
sudo apt-get install libepoxy-dev libxcb1-dev libxcb-util0-dev x11proto-dev \
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

* ffmpeg (xab_custom) -
i made this video reader for educational purposes,
this is an ffmpeg-based video reader, it is way less performant compared to the the other options,
this is my fault because i suck at ffmpeg,
also many of the features are not implemented (yet?) like frame timing,
frame dropping and hardware acceleration

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
meson configure build -Dvideo_reader=xab_custom

# disable BCE files
meson configure build -Ddisable_bce=true

# disable ANSII colored logging
meson configure build -Denable_ansii_log=false
```
