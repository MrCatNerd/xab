arg_parser_tests_prefix = 'arg_parser-'
arg_parser_tests_sources = [
    # arg_parser source
    join_paths(
      tests_common_src_dir,
      'arg_parser.c',
    ),
    # logger source
    join_paths(tests_common_src_dir, 'logger.c'),
]

# basic test
test(arg_parser_tests_prefix + 'basic_test',
executable(
  arg_parser_tests_prefix + 'basic_test',
  [ 'basic_test.c', arg_parser_tests_sources ],
  dependencies: tests_common_deps + video_reader_deps,
  include_directories: tests_common_include_dirs,
), args: [])

# empty test
test(arg_parser_tests_prefix + 'empty_test',
executable(
  arg_parser_tests_prefix + 'empty_test',
  [ 'empty_test.c', arg_parser_tests_sources ],
  dependencies: tests_common_deps + video_reader_deps,
  include_directories: tests_common_include_dirs,
), args: [])
