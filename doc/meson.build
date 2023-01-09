doc_install_dir = get_option('datadir') / 'doc' / meson.project_name() + '-' + meson.project_version()

doxygen = find_program('doxygen', required : get_option('doc'))
sphinx = find_program('sphinx-build', required : get_option('doc'))

subdir('data')

if doxygen.found() and sphinx.found()
  doxyconfig = configuration_data({
    'PROJECT_VERSION' : meson.project_version(),
    'SOURCE_ROOT' : meson.source_root()
  })
  doxyfile = configure_file(
    input : 'Doxyfile.in',
    output : 'Doxyfile',
    configuration : doxyconfig
  )
  doxygen_input = [
    doxyfile
  ]
  doxygen_target = custom_target(
    'doxygen',
    input : doxygen_input,
    output : 'api-html',
    command : [doxygen, doxyfile],
    build_by_default : false
  )

  sphinx_data = configuration_data({
    'PROJECT_NAME' : meson.project_name(),
    'PROJECT_VERSION' : meson.project_version(),
    'CURRENT_BUILD_DIR' : meson.current_build_dir()
  })
  sphinx_file = configure_file(input : 'conf.py.in',
                               output : 'conf.py',
                               configuration : sphinx_data)
  doc_sources = [
    doxyfile,
    sphinx_file,
    'api.rst',
    'api-config.rst',
    'api-flash.rst',
    'api-utils.rst',
    'layout-config.rst',
    'contributing.rst',
    'index.rst',
    'usage.rst',
    'changelog.rst',
    '../CHANGELOG.rst',
    '../src/config.h',
    '../src/flash.h'
  ]
  custom_target('doc',
                command : [sphinx, '-W', '-b', 'html', '-c', meson.current_build_dir(),
                           meson.current_source_dir(), '@OUTPUT@'],
                input : doc_sources,
                output : 'html',
                depends : doxygen_target,
                build_by_default : false,
                install : true,
                install_dir : doc_install_dir)
endif
