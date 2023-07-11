doc_install_dir = get_option('datadir') / 'doc' / meson.project_name() + '-' + meson.project_version()

sphinx = find_program('sphinx-build', required : get_option('doc'))

subdir('data')

if sphinx.found()
  sphinx_data = configuration_data({
    'PROJECT_NAME' : meson.project_name(),
    'PROJECT_VERSION' : meson.project_version(),
    'CURRENT_BUILD_DIR' : meson.current_build_dir()
  })
  sphinx_file = configure_file(input : 'conf.py.in',
                               output : 'conf.py',
                               configuration : sphinx_data)
  doc_sources = [
    sphinx_file,
    'layout-config.rst',
    'layout-config-reference.rst',
    'layout-config-examples.rst',
    'package.rst',
    'contributing.rst',
    'index.rst',
    'usage.rst',
    'changelog.rst',
    '../CHANGELOG.rst'
  ]
  custom_target('doc',
                command : [sphinx, '-W', '-b', 'html', '-c', meson.current_build_dir(),
                           meson.current_source_dir(), '@OUTPUT@'],
                input : doc_sources,
                output : 'html',
                build_by_default : false,
                install : true,
                install_dir : doc_install_dir)
endif
