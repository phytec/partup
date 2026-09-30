# -- Project information -----------------------------------------------------

project = u'partup'
copyright = u'2026, PHYTEC Messtechnik GmbH'
author = u'Martin Schwan'

extensions = [
    'sphinx_phytec_theme',
]

# -- Options for HTML output -------------------------------------------------

html_theme = 'sphinx_phytec_theme'
html_logo = 'data/partup-logo-2x-white.svg'
html_static_path = ['sphinx/static']
html_css_files = [
    'css/rtd-fixups.css',
]
html_favicon = 'sphinx/static/favicon.ico'

pygments_style = 'bw'

badges_release_notes_ref = 'release-{version}'
