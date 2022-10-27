project(
  'partup', 'c',
  version : '0.2.0',
  license : 'GPL-3.0-or-later',
  default_options : [
    'c_std=c99',
    'warning_level=3'
  ]
)

version_array = meson.project_version().split('.')
version_data = configuration_data({
  'PARTUP_VERSION_MAJOR' : version_array[0].to_int(),
  'PARTUP_VERSION_MINOR' : version_array[1].to_int(),
  'PARTUP_VERSION_MICRO' : version_array[2].to_int(),
  'PARTUP_VERSION' : meson.project_version()
})

configure_file(
  input : 'src/version.h.in',
  output : 'version.h',
  configuration : version_data
)

deps = [
  dependency('glib-2.0', version : '>=2.66.0'),
  dependency('gio-2.0'),
  dependency('yaml-0.1'),
  dependency('libparted')
]
src = [
  'src/checksum.c',
  'src/config.c',
  'src/emmc.c',
  'src/error.c',
  'src/flash.c',
  'src/mount.c',
  'src/utils.c'
]

exec = executable(
  meson.project_name(),
  c_args : ['-DG_LOG_DOMAIN="@0@"'.format(meson.project_name())],
  sources : [src, 'src/main.c'],
  dependencies : deps,
  install : true
)

partup_dep = declare_dependency(
  dependencies : deps,
  include_directories : ['src'],
  sources : src
)

if get_option('doc')
  subdir('doc')
endif

if get_option('tests')
  subdir('tests')
endif
