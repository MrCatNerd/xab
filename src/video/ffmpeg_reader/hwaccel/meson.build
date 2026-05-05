src_files += files('hwdec.c')

# More hwaccels coming soon™
# planned hwaccels:
# - vdpau
# - vaapi
# - qsv
# - cuda
# - opencl
hwaccels = {
  'vdpau': {
    'deps': [
      dependency('vdpau', required: false),
      dependency('x11', required: false),
    ],
    'files': ['vdpau.c', 'vdpau_vtable.c'],
  },
  'vaapi':{
    'deps': [
      dependency('libva', required: false),
    ],
    'files': ['vaapi.c']
  },
}

hwaccelsxmacro = '' # macro abuse go brrr
foreach hwa_name : hwaccels.keys()
  hwa_info = hwaccels.get(hwa_name)

  # ensure all dependency requirements are met
  hwa_deps = hwa_info.get('deps')

  has_required_deps = true
  foreach dep : hwa_deps
    if not dep.found()
      has_required_deps = false
    endif
  endforeach

  if not has_required_deps
    message('Skipping ' + hwa_name + ' - missing dependencies')
    continue
  endif

  # if they are then add the hwaaccel
  hwa_files = hwa_info.get('files')
  src_files += files(hwa_files)
  add_project_arguments('-DHWA_' + hwa_name.to_upper(), language: 'c')
  video_reader_deps += hwa_deps

  hwaccelsxmacro += '\t\\\n\tX(' + hwa_name.to_lower() + ','  + hwa_name.to_upper() + ')'
endforeach

hwaccelslist_config = configuration_data()
hwaccelslist_config.set('hwaccelsxmacro', hwaccelsxmacro)
configure_file(input: 'hwaccelslist.h.in', output: 'hwaccelslist.h', configuration: hwaccelslist_config)
