doc_install_dir = get_option('datadir') / 'doc' / meson.project_name() + '-' + meson.project_version()

sphinx = find_program('sphinx-build', required : get_option('doc'))

subdir('data')

if sphinx.found()
  doc_sources = [
    'conf.py',
    'layout-config.rst',
    'layout-config-reference.rst',
    'layout-config-examples.rst',
    'contributing.rst',
    'index.rst',
    'usage.rst',
    'changelog.rst',
    '../CHANGELOG.rst'
  ]
  custom_target('doc',
                command : [sphinx, '-W', '-b', 'html', meson.current_source_dir(), '@OUTPUT@'],
                input : doc_sources,
                output : 'html',
                build_by_default : false,
                install : true,
                install_dir : doc_install_dir)
endif
