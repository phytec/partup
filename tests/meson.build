tests = [
  'checksum',
  'command',
  'config',
  'emmc',
  'file',
  'package',
  'utils'
]

tests_root = [
  'mount-root',
  'package-root',
  'utils-root'
]

foreach t : tests + tests_root
  exe = executable(
    t,
    ['@0@.c'.format(t), 'helper.c', 'helper.h'],
    c_args : ['-DPARTUP_TEST_SRCDIR="@0@"'.format(meson.current_source_dir())],
    dependencies : partup_dep
  )
  test(
    t,
    exe,
    env : [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'G_DEBUG_MESSAGES=all',
    ],
    suite : tests_root.contains(t) ? 'root' : 'user'
  )
endforeach
