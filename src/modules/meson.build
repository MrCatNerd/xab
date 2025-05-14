if pluginator_dep.found()
  project_include_dirs += [include_directories('.')]

  # compile the modules
  dummy_mod = shared_module('dummy', 'dummy_module.c', install: true, install_dir: join_paths(get_option('moduledir'), 'dummy.so'))
  mouse_mod = shared_module('mouse', 'mouse_module.c', dependencies: xcb_deps, install: true, install_dir: join_paths(get_option('moduledir'), 'mouse.so'))
endif
