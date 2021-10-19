datadir = get_option('datadir')
sysconfdir = get_option('sysconfdir')

# Desktop file
desktop_file = i18n.merge_file(
  input : 'org.gnome.Calls.desktop.in',
  output : 'org.gnome.Calls.desktop',
  type : 'desktop',
  po_dir : '../po',
  install : true,
  install_dir : join_paths(datadir, 'applications')
)

desktop_file = i18n.merge_file(
  input : 'org.gnome.Calls-daemon.desktop.in',
  output : 'org.gnome.Calls-daemon.desktop',
  type : 'desktop',
  po_dir : '../po',
  install : true,
  install_dir : join_paths(sysconfdir, 'xdg/autostart')
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils,
       args: [join_paths(meson.current_build_dir(),
                         'org.gnome.Calls.desktop')
             ])
  test('Validate daemon desktop file', desktop_utils,
       args: [join_paths(meson.current_build_dir(),
                         'org.gnome.Calls-daemon.desktop')
             ])
endif

# Metainfo file
install_data('org.gnome.Calls.metainfo.xml',
    install_dir: join_paths(datadir, 'metainfo'),
)

appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test('Validate metainfo file', appstreamcli,
       args: ['validate',
              join_paths(meson.current_source_dir(),
                         'org.gnome.Calls.metainfo.xml')
             ])
endif

# Icons
install_data(
  'org.gnome.Calls.svg',
  install_dir: join_paths(
    datadir,
    'icons',
    'hicolor',
    'scalable',
    'apps'
  )
)
install_data(
  'org.gnome.Calls-symbolic.svg',
  install_dir: join_paths(
    datadir,
    'icons',
    'hicolor',
    'symbolic',
    'apps'
  )
)

schema_src = 'org.gnome.Calls.gschema.xml'
compiled = gnome.compile_schemas(build_by_default: true,
                                 depend_files: files(schema_src))

install_data(schema_src,
  install_dir: join_paths(get_option('datadir'), 'glib-2.0/schemas')
)

compile_schemas = find_program('glib-compile-schemas', required: false)
if compile_schemas.found()
  test('Validate schema file', compile_schemas,
  args: ['--strict', '--dry-run', meson.current_source_dir()]
  )
endif
