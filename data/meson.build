datadir = get_option('datadir')
sysconfdir = get_option('sysconfdir')

# Desktop file
install_data('sm.puri.Calls.desktop',
             install_dir : join_paths(datadir, 'applications'))
install_data('sm.puri.Calls-daemon.desktop',
             rename : 'sm.puri.Calls.desktop',
             install_dir : join_paths(sysconfdir, 'xdg/autostart'))

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils,
       args: [join_paths(meson.current_source_dir(),
                         'sm.puri.Calls.desktop')
             ])
  test('Validate daemon desktop file', desktop_utils,
       args: [join_paths(meson.current_source_dir(),
                         'sm.puri.Calls-daemon.desktop')
             ])
endif

# Appdata file
install_data('sm.puri.Calls.appdata.xml',
    install_dir: join_paths(datadir, 'metainfo'),
)

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test('Validate appstream file', appstream_util,
       args: ['validate-relax',
              join_paths(meson.current_source_dir(),
                         'sm.puri.Calls.appdata.xml')
             ])
endif

# Icons
install_data(
  'sm.puri.Calls.svg',
  install_dir: join_paths(
    datadir,
    'icons',
    'hicolor',
    'scalable',
    'apps'
  )
)
install_data(
  'sm.puri.Calls-symbolic.svg',
  install_dir: join_paths(
    datadir,
    'icons',
    'hicolor',
    'symbolic',
    'apps'
  )
)
