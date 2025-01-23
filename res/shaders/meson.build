if not disable_bce
  bce_subproj = subproject('bce')
  bce_prog = bce_subproj.get_variable('bce')

  bce_gen = generator(bce_prog, output: '@BASENAME@.h', arguments: ['@INPUT@', '@OUTPUT@'])

  c = run_command('shader_grabber.sh', check: true)
  shader_source_files = c.stdout().strip().split('\n')

  src_files += bce_gen.process(shader_source_files)
endif
