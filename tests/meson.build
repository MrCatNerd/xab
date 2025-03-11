# setup common variables
tests_common_src_dir = join_paths(meson.global_source_root(), 'src')
tests_common_include_dirs = [
  include_directories('.'),
  include_directories('../src'),
]
tests_common_deps = []

if get_option('video_reader') == 'ffmpeg'
  subdir('ffmpeg_reader')
endif
