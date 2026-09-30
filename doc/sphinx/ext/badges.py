from docutils import nodes
from sphinx import addnodes

STATES = {'required', 'optional', 'deprecated', 'experimental'}

def state_kind(text):
    state = text.strip().lower()
    return state if state in STATES else 'default'

def make_badge(text, kind):
    return nodes.inline(text.title(), text.title(), classes=['badge', f'badge-{kind}'])

def make_state_badge(text):
    return make_badge(text, f'state-{state_kind(text)}')

def _with_prefix(text, prefix):
    text = text.strip()
    if text.lower().startswith(prefix.lower()):
        return text
    return f'{prefix}: {text}'

def make_type_badge(type_name):
    return make_badge(_with_prefix(type_name, 'Type'), 'type')

def make_since_badge(version, template=None):
    text = _with_prefix(version, 'Since')
    raw_version = text.split(':', 1)[-1].strip()

    badge = make_badge(text, 'since')

    if not template:
        return badge

    refnode = addnodes.pending_xref(
        '',
        refdomain='std',
        reftype='ref',
        reftarget=template.format(version=raw_version),
        refexplicit=True,
        refwarn=True,
    )
    refnode['classes'].extend(['badge', 'badge-link'])
    refnode += badge
    return refnode

def make_role(builder):
    def role(name, rawtext, text, lineno, inliner, options=None, content=None):
        config = inliner.document.settings.env.config
        return [builder(text, config)], []
    return role

def setup(app):
    app.add_config_value('badges_release_notes_ref', None, 'env', [str, type(None)])

    app.add_role('badge-type', make_role(lambda text, config: make_type_badge(text)))
    app.add_role('badge-state', make_role(lambda text, config: make_state_badge(text)))
    app.add_role('badge-since', make_role(
        lambda text, config: make_since_badge(text, config.badges_release_notes_ref)))
    app.add_role('badge', make_role(lambda text, config: make_badge(text, 'default')))

    app.add_css_file('badges.css')

    return {
        'version': '0.1',
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
