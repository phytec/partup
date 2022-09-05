tests = [
  'checksum',
  'config',
  'emmc'
]

foreach t : tests
  exe = executable(
    t,
    '@0@.c'.format(t),
    c_args : ['-DPARTUP_TEST_SRCDIR="@0@"'.format(meson.current_source_dir())],
    dependencies : partup_dep
  )
  test(
    t,
    exe,
    env: [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'G_DEBUG_MESSAGES=all',
    ]
  )
endforeach
