put_get_test = executable(
  'ffmpeg_reader-packet_queue-put_get_test',
  [
    'test.c',
    # packet queue source
    join_paths(
      tests_common_src_dir,
      'ffmpeg_reader',
      'packet_queue.c',
    ),
    # logger source
    join_paths(tests_common_src_dir, 'logger.c'),
  ],
  dependencies: tests_common_deps + video_reader_deps,
  include_directories: tests_common_include_dirs,
)
