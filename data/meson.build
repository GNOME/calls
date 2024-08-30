gnome = import('gnome')
i18n = import('i18n')

datadir = get_option('datadir')
sysconfdir = get_option('sysconfdir')

# Desktop file
desktop_file = i18n.merge_file(
  input : 'org.gnome.Calls.desktop.in',
  output : 'org.gnome.Calls.desktop',
  type : 'desktop',
  po_dir : po_dir,
  install : true,
  install_dir : join_paths(datadir, 'applications')
)

desktop_daemon_file = i18n.merge_file(
  input : 'org.gnome.Calls-daemon.desktop.in',
  output : 'org.gnome.Calls-daemon.desktop',
  type : 'desktop',
  po_dir : po_dir,
  install : true,
  install_dir : join_paths(sysconfdir, 'xdg/autostart')
)

service_data = configuration_data()
service_data.set('bindir', full_bindir)

# DBus service
service_file = 'org.gnome.Calls.service'
configure_file(
  input : service_file + '.in',
  output : service_file,
  configuration : service_data,
  install : true,
  install_dir : full_servicedir,
)

# Systemd user service
systemd_unit = 'calls-daemon.service'
configure_file(
  input : systemd_unit + '.in',
  output : systemd_unit,
  configuration : service_data,
  install : true,
  install_dir : systemd_user_unit_dir,
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file',
    desktop_utils,
    args: [
      desktop_file.full_path()
    ],
    depends: [
      desktop_file,
    ],
  )
  test('Validate daemon desktop file',
    desktop_utils,
    args: [
      desktop_daemon_file.full_path()
    ],
    depends: [
      desktop_daemon_file,
    ],
  )
endif


# Metainfo file
metainfo_file = i18n.merge_file(
  input: 'org.gnome.Calls.metainfo.xml.in',
  output: 'org.gnome.Calls.metainfo.xml',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(datadir, 'metainfo')
)

appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test('Validate metainfo file', appstreamcli,
       args: ['validate',
              '--pedantic',
              '--no-net',
              metainfo_file
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
