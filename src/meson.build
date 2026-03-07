# list source files
src_files += files(
  'arg_parser.c',
  'context.c',
  'logger.c',
  'utils.c',
  'wallpaper.c',
  'xab.c',
)

if enable_experimental_changes
  src_files += files('ipc.c')
endif

subdir('render')
subdir('Xserver')

# configure the file
configure_file(input: 'version.h.in', output: 'version.h', configuration: version_conf)
