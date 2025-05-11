project(
  'xab',
  'c',
  version: run_command('cat', join_paths(meson.project_source_root(), 'VERSION'), check: true).stdout().strip(), # thx hyprland
  default_options: [
    'warning_level=3',
    'b_ndebug=if-release',
    'buildtype=release',
  ],
)

# -- set version and git information -- #

# get the git info
git_prog = find_program('git', required: false)
if git_prog.found()
  git_branch = run_command(git_prog,'rev-parse','--abbrev-ref','HEAD', check: false).stdout().strip()
  git_commit = run_command(git_prog,'rev-parse', '--short', 'HEAD', check: false).stdout().strip()
else
  git_branch = 'null'
  git_commit = 'null'
endif

# set the configuration data
version_conf = configuration_data()
version_conf.set('version', meson.project_version())
version_conf.set('branch',git_branch)
version_conf.set('commit',git_commit)

# i configure the file in src/meson.build cuz i can't set output with path segments :(

# --- general ---
c_compiler = meson.get_compiler('c')
if c_compiler.get_id() not in ['gcc']
  warning(
    '\''
    + c_compiler.get_id()
    + '\' compiler is not officially supported by xab, gcc is recommended, this may or may not work',
  )
endif

project_include_dirs = [include_directories('src'), include_directories('res')]
src_files = []
extra_link_args = []

# --- logging and stuff ---

log_level = get_option('log')
should_enable_ansii_colors = get_option('ansii_log')

if should_enable_ansii_colors
  add_project_arguments('-DLOG_ANSII_COLORS', language: 'c')
endif

# automatic log level by build type
buildtype = get_option('buildtype')
if log_level == 'auto'
  if buildtype == 'plain'
    log_level = 'trace'
  elif buildtype == 'debug'
    log_level = 'debug'
  elif buildtype == 'release' or buildtype == 'debugoptimized'
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
  log_num = 3
endif
assert(log_num >= 0)
add_project_arguments('-DLOG_LEVEL=' + log_num.to_string(), language: 'c')

# --- video reader ---
# choose video reader interface
video_library = get_option('video_reader')
video_reader_deps = []
if video_library == 'mpv'
  src_files += files('src/mpv_reader.c')
  video_reader_deps += dependency('mpv')
elif video_library == 'ffmpeg'
  subdir('src/ffmpeg_reader')
  video_reader_deps += [
    dependency('threads'),
    dependency('libavcodec'),
    dependency('libavutil'),
    dependency('libavformat'),
    dependency('libswscale'),
    dependency('threads'),
  ]
else
  error('invalid video reader: \'' + video_library + '\'')
endif

# --- tracy profiler ---
# TODO: _opt suffix for get_option vars
tracy_enable = get_option('tracy_enable')

# will be an empty list unless tracy is enabled, which in that case this will be a dependency
tracy_dep = []
if tracy_enable
  tracy_dep = dependency('tracy', static: true, version: '>= 0.11.2')
  tracy_version = tracy_dep.version().strip()

  if tracy_version != '0.11.2'
    warning(
      'tracy ' + tracy_dep.version().strip(),
      'is not supported, tracy 0.11.2 is preferred',
    )
  endif

  if buildtype != 'debugoptimized'
    warning('Profiling builds should set --buildtype=debugoptimized')
  endif

  # to check out
  # add_project_arguments('-DTRACY_NO_SAMPLING', language: 'c')
  # add_project_arguments('-DTRACY_NO_SYSTEM_TRACING', language: 'c')

  add_project_arguments('-DTRACY_ENABLE', language: 'c') # enable tracy
  add_project_arguments('-DTRACY_ON_DEMAND', language: 'c') # i forgor
  add_project_arguments('-DTRACY_NO_BROADCAST', language: 'c') # don't broadcast to network
  # TRACY_MANUAL_LIFETIME doesn't work for some reason :( maybe its cuz im using C and not C++
endif

# --- OpenGL stuff ---
enable_opengl_debug_callback = get_option('opengl_debug_callback')
# auto and debug or less or enabled
if (enable_opengl_debug_callback.auto() and log_num <= 2) or enable_opengl_debug_callback.enabled()
  add_project_arguments('-DENABLE_OPENGL_DEBUG_CALLBACK', language: 'c')
endif

# --- bce ---
disable_bce = get_option('nobce')
if disable_bce
  add_project_arguments('-DDISABLE_BCE', language: 'c')
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

hashmap_c_dep = dependency('hashmap.c')

# --- optional dependencies ---
multimonitor_feature = get_option('multimonitor')

cglm_dep = dependency('cglm', version: '>= 0.8.4', required: multimonitor_feature)
if cglm_dep.found()
  add_project_arguments('-DHAVE_LIBCGLM', language: 'c')
endif

xrandr_dep = dependency('xcb-randr', version: '>= 1.5', required: multimonitor_feature)
if xrandr_dep.found()
  add_project_arguments('-DHAVE_LIBXRANDR', language: 'c')
endif

deps = xcb_deps + [
  epoxy_dep,
  egl_dep,
  cglm_dep,
  xrandr_dep,
  hashmap_c_dep,
  c_compiler.find_library('m'),
  video_reader_deps,
  tracy_dep,
]

# --- pch ---

pch_files = [join_paths('src', 'pch.h')]
if (get_option('nopch'))
  pch_files = []
endif

# --- source files ---
subdir('res/shaders')
subdir('src')

# --- make executable ---
executable(
  meson.project_name(),
  src_files,
  c_pch: pch_files,
  link_args: extra_link_args,
  dependencies: deps,
  include_directories: project_include_dirs,
  install: true,
)

# --- tests ---
if not meson.is_subproject() and get_option('tests')
  subdir('tests')
endif
