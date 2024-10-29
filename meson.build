project(
  'xab',
  'c',
  version: '0.0.1',
  default_options: ['warning_level=3', 'b_ndebug=if-release'],
)

# --- macros and options and stuff ---
extra_link_args = []
buildtype = get_option('buildtype')

if buildtype == 'release'
  add_project_arguments('-DNDEBUG', language: 'c')
endif
if get_option('verbose') == false and buildtype != 'plain'
  add_project_arguments('-DNVERBOSE', language: 'c')
endif

if get_option('static_build')
  extra_link_args += '-static'
endif

# --- dependencies ---
xcb_deps = [
  dependency('xcb'),
  dependency('xcb-atom'),
  dependency('xcb-aux'),
  dependency('xproto'),
  dependency('xcb-util'),
]
epoxy_dep = dependency('epoxy')
egl_dep = dependency('egl')

# video reader
# mpv # todo: ill add this when ill have the mpv / my video decoder thingy
# mpv_dep = dependency('mpv', required: false)
# libavcodec
ffmpeg_deps = [
  dependency('libavcodec'),
  dependency('libavformat'),
  dependency('libavfilter'),
  dependency('libavutil'),
  dependency('libswresample'),
  dependency('libswscale'),
]

# --- optional dependencies ---
cglm_dep = dependency('cglm', version: '>= 0.8.4', required: false)
if cglm_dep.found()
  add_project_arguments('-DHAVE_LIBCGLM', language: 'c')
endif

xrandr_dep = dependency('xcb-randr', version: '>= 1.5', required: false)
if xrandr_dep.found()
  add_project_arguments('-DHAVE_LIBXRANDR', language: 'c')
endif

deps = ffmpeg_deps + xcb_deps + [epoxy_dep, egl_dep, cglm_dep, xrandr_dep]

# --- source files ---
src_files_list_raw = [
  'xab.c',

  'arg_parser.c',
  'atom.c',
  'camera.c',
  'context.c',
  'egl_stuff.c',
  'framebuffer.c',
  'monitor.c',
  'setbg.c',
  'utils.c',
  'video_reader.c',
  'video_renderer.c',
  'wallpaper.c',
]

# <name> -> src/<name>
src_files_list = []
foreach file : src_files_list_raw
  src_files_list += join_paths('src', file)
endforeach

src_files = files(src_files_list)

# --- make executable ---
executable(
  meson.project_name(),
  src_files,
  c_args: [
    '-DXAB_VERSION="' + meson.project_version() + '"',
  ],
  link_args: extra_link_args,
  dependencies: deps,
  include_directories: ['src'],
  install: true,
)
