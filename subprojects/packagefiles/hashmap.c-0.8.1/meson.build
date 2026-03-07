project('hashmap.c', 'c', version:'0.8.1')

files = files('hashmap.c')
inc_dirs = include_directories('.')

hashmap_c_lib = static_library(meson.project_name(), files ,include_directories: inc_dirs)

hashmap_c_dep = declare_dependency(link_with: hashmap_c_lib, include_directories: inc_dirs)

meson.override_dependency('hashmap.c', hashmap_c_dep)
