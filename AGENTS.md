# Agent Guidelines

Rules for AI coding agents and human contributors working on partup, a
C/GLib tool for flashing partitioned images to block devices and MTD.

## Project Layout

- `src/`: C sources. Files are prefixed with `pu-`, symbols with `pu_`.
- `tests/`: Unit tests. Tests needing root privileges are in `*-root.c`
  files and belong to the `root` suite, all others to the `user` suite.
- `doc/`: Sphinx documentation (reStructuredText).
- `CHANGELOG.rst`: User-facing release notes.
- `debian/`, `tools/`, `.github/`: Packaging, helper scripts and CI.

## Build and Test

Use Meson. Do not commit the `build/` directory.

```
meson setup build --buildtype=debug
meson test --suite user -C build --print-errorlogs
sudo meson test --suite root -C build --print-errorlogs
```

- Run the `user` suite after every source change. Only run the `root`
  suite when asked to or when sure it is safe, since it mounts and formats
  loop devices.
- Build the documentation with `-Ddoc=true` or
  `sphinx-build -W -b html doc <outdir>`. Warnings are errors, so the build
  must be warning-free.
- Add or update unit tests for every behavior change or bug fix.

## Safety

- Never run partup against a real block device (`/dev/sd*`,
  `/dev/mmcblk*`, `/dev/nvme*`) or MTD device on the development machine.
  Use image files and loop devices only.
- Never run destructive commands (`mkfs`, `dd`, `parted`, `flashcp`)
  on anything that is not a disposable test image.
- Do not push, force-push, tag or publish releases unless explicitly asked.
- Do not modify `.git`, lock files or generated files by hand.

## C Code Style

- Follow the surrounding code: 4 spaces indentation, no tabs, return type on
  its own line in function definitions, no space between a function name and
  its opening parenthesis, `snake_case` naming.
- Use GLib types and helpers (`gchar`, `gboolean`, `g_autofree`,
  `g_autoptr`, `g_debug`, `g_message`) instead of plain libc equivalents
  where GLib offers one.
- Functions that can fail return `gboolean` (or a pointer, `NULL` on
  failure) and take a trailing `GError **error`. Set errors with the domain
  `PU_ERROR` and add new codes to `src/pu-error.h`. Use
  `g_prefix_error` to add context when propagating.
- Validate arguments with `g_return_val_if_fail`, including
  `error == NULL || *error == NULL`.
- Free everything you allocate. Prefer `g_autofree`/`g_autoptr`.
- Keep the SPDX license header and copyright line in new files
  (`GPL-3.0-or-later`).
- Do not add new dependencies without discussing it first.

## Documentation

- Keep documentation in sync with the code. When adding or changing a
  command-line option or layout configuration option, update
  `doc/usage.rst` and `doc/layout-config-reference.rst`.
- Mark new options with a line `Available since:` followed by a `:ref:` to
  the release label, as done for existing options.
- Verify claims against the source before documenting them. Do not describe
  behavior that has not been confirmed in the code.
- Wrap lines at 80 characters. Use `.. _label:` targets and `:ref:` for
  cross references.
- Indent RST code and documentation with 3 spaces, including directive content,
  definition lists and enumerations. Bullet items are written as `-` followed
  by two spaces, so that continuation lines align at 3 spaces. Numbered items
  are written as `1.` followed by one space, with continuation lines also
  indented by 3 spaces. See the existing files in `doc/` for examples, e.g.
  `doc/index.rst` and `doc/usage.rst`.
- Spell command-line options and layout configuration keys exactly as they are
  defined in the source (e.g. `--skip-checksums` in `src/pu-main.c`). Do not
  guess or abbreviate them.

## Changelog

- Add user-visible changes (features, bug fixes) to `CHANGELOG.rst` under
  the upcoming release. Pure refactorings, tests and CI changes do not need an
  entry.

## Commits

- One logical change per commit. Keep commits small and bisectable: each
  commit must build and pass the tests.
- Subject format: `<path>: <component>: <Imperative summary>`, e.g.
  `src: emmc: Allow writing any binary to partition with fstype null` or
  `doc: usage: Describe checksum verification during install`. Keep the
  subject under about 72 characters.
- Explain *why* in the body, wrapped at 72 characters.
- Sign off every commit (`git commit -s`).
- Do not mix source, test and documentation changes of unrelated topics.
- Use `fixup!` commits for corrections to unpushed commits and do not
  rewrite already pushed history.

## Agent Behavior

- Read the relevant code before changing it, and state assumptions.
- Keep changes minimal and focused on the request. Do not reformat unrelated
  code.
- Do not commit scratch files such as `plan.md` or build output.
- Ask before destructive or hard to reverse actions.
