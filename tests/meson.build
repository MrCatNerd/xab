# setup common variables
tests_common_src_dir = join_paths(meson.global_source_root(), 'src')
tests_common_include_dirs = [
  include_directories('.'),
  include_directories('../src'),
]
tests_common_deps = [
  # NOTE: there might be a way to abuse macros/C to disable tracy while testing but im too dumb for that stuff
  tracy_dep # tracy is in the pch so if it's enabled, you gotta have it
]

if get_option('video_reader') == 'ffmpeg'
  subdir('ffmpeg_reader')
endif
