project(
  'waydraw',
  'c',
  meson_version : '>= 1.8.0',
  version : '0.1',
  default_options : ['warning_level=3'],
)

wayland_client_dep = dependency('wayland-client', version : '>=1.2.8')
wayland_mod = import('wayland')
wayland_protocols = wayland_mod.scan_xml([
  'protocols/xdg-shell.xml',
  'protocols/wlr-layer-shell-unstable-v1.xml',
])

xkbcommon_dep = dependency('xkbcommon')
cairo_dep = dependency('cairo')

dependencies = [
  wayland_client_dep,
  xkbcommon_dep,
  cairo_dep,
]

sources = [
  'waydraw.c',
  'shm.c',
  'snapshot.c',
]

exe = executable(
  'waydraw',
  sources,
  wayland_protocols,
  dependencies : dependencies,
  install : true,
)
