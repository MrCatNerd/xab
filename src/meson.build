# list source files
src_files += files(
  'arg_parser.c',
  'context.c',
  'ipc.c',
  'logger.c',
  'utils.c',
  'wallpaper.c',
  'xab.c',
)

subdir('render')
subdir('Xserver')

# configure the file
configure_file(input:'version.h.in', output:  'version.h', configuration: version_conf)
