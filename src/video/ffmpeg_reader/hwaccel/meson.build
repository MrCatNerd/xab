src_files += files('hwdec.c')

# More hwaccels coming soon™
# planned hwaccels:
# - vdpau
# - vaapi
# - qsv
# - cuda
# - opencl
hwaccels = [dependency('vdpau', required: false)]

foreach hwa : hwaccels
  if hwa.found()
    hwa_name = hwa.name()
    src_files += files(hwa_name + '.c')
    add_project_arguments('-DHWA_' + hwa_name.to_upper(), language: 'c')
  endif
endforeach
video_reader_deps += hwaccels
