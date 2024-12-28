project(
  'xab',
  'c',
  version: run_command('cat', join_paths(meson.project_source_root(), 'VERSION'), check: true).stdout().strip(), # thx hyprland
  default_options: ['warning_level=3', 'b_ndebug=if-release'],
)

# --- general ---
c_compiler = meson.get_compiler('c')
if c_compiler.get_id() not in ['gcc']
  warning(
    '\''
    + c_compiler.get_id()
    + '\' compiler is not officially supported by xab, gcc is recommended, this may or may not work',
  )
endif

project_include_dirs = ['src']
src_files = []
extra_link_args = []

if get_option('static_build')
  extra_link_args += '-static'
endif

# --- logging and stuff---

log_level = get_option('log')

# automatic log level by build type
buildtype = get_option('buildtype')
if log_level == 'auto'
  if buildtype == 'plain'
    log_level = 'trace'
  elif buildtype == 'debug' or buildtype == 'debugoptimized'
    log_level = 'debug'
  elif buildtype == 'release'
    log_level = 'warn'
  else
    # all of the other buildtypes will default to info log level
    log_level = 'info'
  endif
endif

# each log level as a number, trace=0 -> fatal=6
log_num = -1
if log_level == 'trace'
  log_num = 0
elif log_level == 'verbose'
  log_num = 1
elif log_level == 'debug'
  log_num = 2
elif log_level == 'info'
  log_num = 3
elif log_level == 'warn'
  log_num = 4
elif log_level == 'error'
  log_num = 5
elif log_level == 'fatal'
  log_num = 6
elif log_level == 'auto'
  warning('automatic buildtype didn\'t work for some reason, defaulting to info...')
  log_level = 3
endif
assert(log_num >= 0)
add_global_arguments('-DLOG_LEVEL=' + log_num.to_string(), language: 'c')

# --- video reader ---
# choose video reader interface
video_library = get_option('video_reader')
video_reader_deps = []
if video_library == 'mpv'
  src_files += files('src/mpv_reader.c')
  video_reader_deps += dependency('mpv')
elif video_library == 'xab_custom'
  src_files += files('src/xab_reader.c')
  video_reader_deps += [
    dependency('libavcodec'),
    dependency('libavutil'),
    dependency('libavformat'),
    dependency('libswscale'),
  ]
else
  error('invalid video reader: \'' + video_library + '\'')
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

# --- optional dependencies ---
cglm_dep = dependency('cglm', version: '>= 0.8.4', required: false)
if cglm_dep.found()
  add_project_arguments('-DHAVE_LIBCGLM', language: 'c')
endif

xrandr_dep = dependency('xcb-randr', version: '>= 1.5', required: false)
if xrandr_dep.found()
  add_project_arguments('-DHAVE_LIBXRANDR', language: 'c')
endif

deps = xcb_deps + [
  epoxy_dep,
  egl_dep,
  cglm_dep,
  xrandr_dep,
  c_compiler.find_library('m'),
  video_reader_deps,
]

# --- source files ---
subdir('src')

# --- make executable ---
executable(
  meson.project_name(),
  src_files,
  c_args: [
    '-DXAB_VERSION="' + meson.project_version() + '"',
  ],
  link_args: extra_link_args,
  dependencies: deps,
  include_directories: project_include_dirs,
  install: true,
)
