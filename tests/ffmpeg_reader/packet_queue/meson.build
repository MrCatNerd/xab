packet_tests_prefix = 'ffmpeg_reader-packet_queue'
packet_tests_sources = [
    # packet queue source
    join_paths(
      tests_common_src_dir,
      'ffmpeg_reader',
      'packet_queue.c',
    ),
    # logger source
    join_paths(tests_common_src_dir, 'logger.c'),
]

# basic test
test(packet_tests_prefix + 'basic_test',
executable(
  packet_tests_prefix + 'basic_test',
  [ 'basic_test.c', packet_tests_sources ],
  dependencies: tests_common_deps + video_reader_deps,
  include_directories: tests_common_include_dirs,
), args: [])

# put get test
test(packet_tests_prefix + 'put_get_test',
executable(
  packet_tests_prefix +'put_get_test',
  [ 'put_get_test.c', packet_tests_sources ],
  dependencies: tests_common_deps + video_reader_deps,
  include_directories: tests_common_include_dirs,
), args: [])

# multi packet put get test
test(
  packet_tests_prefix + 'multi_packet_put_get_test',
executable(packet_tests_prefix + 'multi_packet_put_get_test',
  [ 'multi_packet_put_get_test.c', packet_tests_sources ],
  dependencies: tests_common_deps + video_reader_deps,
  include_directories: tests_common_include_dirs,
), args: [])
