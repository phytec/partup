import os
import subprocess

# -- Project information -----------------------------------------------------

project = u'partup'
copyright = u'2023, PHYTEC Messtechnik GmbH'
author = u'Martin Schwan'

tag = subprocess.check_output(['git', 'describe', '--tags', '--abbrev=0']).decode()
release = tag[1:]

# -- General configuration ---------------------------------------------------

extensions = [
    'sphinx.ext.githubpages'
]

# -- Options for HTML output -------------------------------------------------

html_theme = 'sphinx_rtd_theme'
html_static_path = ['data']
html_logo = 'data/partup-logo-2x-white.svg'
html_theme_options = {
    'style_external_links': True
}
