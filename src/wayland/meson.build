# Copied from squeekboard and modified for Calls by Bob Ham

wl_scanner = find_program('wayland-scanner',
                          required: get_option('wayland'))

wl_proto_sources = []
wayland_source_files = []

if wl_scanner.found()
  wayland_protos = dependency('wayland-protocols', version: '>=1.12')
  wl_protocol_dir = wayland_protos.get_pkgconfig_variable('pkgdatadir')

  gen_scanner_client_header = generator(wl_scanner,
      output: '@BASENAME@-client-protocol.h',
      arguments: ['client-header', '@INPUT@', '@OUTPUT@'])
  gen_scanner_client_code = generator(wl_scanner,
      output: '@BASENAME@-protocol.c',
      arguments: ['private-code', '@INPUT@', '@OUTPUT@'])

  wl_protos = [
    wl_protocol_dir + '/stable/xdg-shell/xdg-shell.xml',
    'wlr-layer-shell-unstable-v1.xml',
  ]
  foreach proto: wl_protos
    wl_proto_sources += gen_scanner_client_header.process(proto)
    wl_proto_sources += gen_scanner_client_code.process(proto)
  endforeach

  wayland_source_files += ['layersurface.c', 'layersurface.h']
endif

wayland_sources = files(wayland_source_files)
