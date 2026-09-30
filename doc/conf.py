import os
import sys

sys.path.insert(0, os.path.abspath('sphinx/ext'))

# -- Project information -----------------------------------------------------

project = u'partup'
copyright = u'2026, PHYTEC Messtechnik GmbH'
author = u'Martin Schwan'

extensions = [
    'badges',
]

# -- Options for HTML output -------------------------------------------------

html_theme = 'sphinx_rtd_theme'
html_static_path = ['data']
html_logo = 'data/partup-logo-2x-white.svg'
html_theme_options = {
    'style_external_links': True
}
html_static_path = ['sphinx/static']
html_css_files = [
    'css/phytec-theme.css',
    'css/rtd-fixups.css',
    'css/badges.css',
]
html_favicon = 'sphinx/static/favicon.ico'

pygments_style = 'bw'

badges_release_notes_ref = 'release-{version}'
