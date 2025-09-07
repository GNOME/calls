if get_option('tests')

test_env = [
  'G_DEBUG=gc-friendly,fatal-warnings',
  'GSETTINGS_BACKEND=memory',
  'LANGUAGE=C',
  'LC_ALL=C',
  'PYTHONDONTWRITEBYTECODE=yes',
  'MALLOC_CHECK_=2',
  'GTK_A11Y=none',
  'GSETTINGS_SCHEMA_DIR=@0@/data'.format(meson.project_build_root()),
  'CALLS_PLUGIN_DIR=@0@/plugins'.format(meson.project_build_root()),
  'FOLKS_BACKENDS_ALLOWED=key-file',
  'FOLKS_PRIMARY_STORE=key-file',
  'CALLS_RECORD_DIR=@0@/tests/record-db'.format(meson.project_build_root()),
  'CALLS_SIP_ACCOUNT_FILE=@0@/tests/sip-account.cfg'.format(meson.project_build_root()),
]

test_cflags = [
  '-DFOR_TESTING',
  '-Wno-error=deprecated-declarations',
  '-DTEST_DATABASE="@0@"'.format(meson.current_source_dir() / 'data' / 'serviceproviders.xml'),
]

test_link_args = [
  '-fPIC',
]

dbus_service_conf = configuration_data()
dbus_service_conf.set('CALLS_BUILD_DIR', '@0@'.format(builddir))
dbus_service_conf.set('CALLS_BUILD_DIR_STR', '"@0@"'.format(builddir))
dbus_service_conf.set('CALLS_DBUS_NAME', '"org.gnome.Calls"')
dbus_service_conf.set('CALLS_DBUS_OBJECT_PATH', '"/org/gnome/Calls"')

dbus_config_h = configure_file(output        : 'calls-dbus-config.h',
                               configuration : dbus_service_conf)

test_includes = include_directories('.')

subdir('mock')
subdir('services')

test_sources = [ 'test-emergency-call-types.c' ]
t = executable('emergency-call-types', test_sources,
               c_args : test_cflags,
               link_args: test_link_args,
               pie: true,
               link_with : [calls_vala, libcalls],
               dependencies: calls_deps,
               include_directories : [
                   calls_includes,
                 ]
               )
test('emergency-call-types', t, env: test_env)

test_sources = [ 'test-service-providers.c' ]
t = executable('service-providers', test_sources,
               c_args : test_cflags,
               link_args: test_link_args,
               pie: true,
               link_with : [calls_vala, libcalls],
               dependencies: calls_deps,
               include_directories : [
                   calls_includes,
                 ]
               )
test('service-providers', t, env: test_env)

test_sources = [ 'test-manager.c' ]

t = executable('manager', test_sources,
                 c_args : test_cflags,
                 link_args: test_link_args,
                 pie: true,
                 link_with : [calls_vala, libcalls],
                 dependencies: calls_deps,
                 include_directories : [
                   calls_includes
                 ]
                )
test('manager', t, env: test_env, depends: calls_plugins)

test_sources = [ 'test-plugins.c' ]

t = executable('plugins', test_sources,
                 calls_sources,
                 c_args : test_cflags,
                 link_args: test_link_args,
                 pie: true,
                 link_with : [calls_vala, libcalls],
                 dependencies: calls_deps,
                 include_directories : [
                   calls_includes
                 ]
                )
test('plugins', t, env: test_env, depends: calls_plugins)

test_sources = [ 'test-util.c' ]
t = executable('util', test_sources,
               c_args : test_cflags,
               link_args: test_link_args,
               pie: true,
               link_with : [calls_vala, libcalls],
               dependencies: calls_deps,
               include_directories : [
                   calls_includes,
                 ]
               )
test('util', t, env: test_env)

test_sources = [ 'test-ui-call.c', 'mock-call.c', 'mock-call.h' ]
t = executable('ui-call', test_sources,
               c_args : test_cflags,
               link_args : test_link_args,
               pie: true,
               link_with : [calls_vala, libcalls],
               dependencies : calls_deps,
               include_directories : [
                   calls_includes
                 ]
               )
test('ui-call', t, env: test_env)

test_sources = [
               'mock-call.c', 'mock-call.h',
               'test-ringer.c'
]

t = executable('ringer', test_sources,
               c_args : test_cflags,
               link_args: test_link_args,
               pie: true,
               link_with : [calls_vala, libcalls, libfeedback],
               dependencies: calls_deps,
               include_directories : [
                   calls_includes,
                 ]
               )
test('ringer', t, env: test_env)

test_sources = [ 'test-contacts.c' ]
t = executable('contacts', test_sources,
               c_args : test_cflags,
               link_args: test_link_args,
               pie: true,
               link_with : [calls_vala, libcalls],
               dependencies: calls_deps,
               include_directories : [
                   calls_includes,
                 ]
               )
test('contacts', t, env: test_env)

test_sources = [ 'test-settings.c' ]
t = executable('settings', test_sources,
               c_args : test_cflags,
               link_args: test_link_args,
               pie: true,
               link_with : [calls_vala, libcalls],
               dependencies: calls_deps,
               include_directories : [
                   calls_includes,
                 ]
               )
test('settings', t, env: test_env)

test_sources = ['test-dbus.c']
t = executable('dbus', test_sources, generated_dbus_sources,
               c_args : test_cflags,
               link_args: test_link_args,
               pie: true,
               link_with : [calls_vala, libcalls],
               dependencies: calls_deps,
               include_directories : [
                   calls_includes,
                 ]
               )
test('dbus', t, env: test_env)

dbus_run_session = find_program('dbus-run-session')

if dbus_run_session.found ()
  test_sources = [ 'test-application.c' ]
  t = executable('application', test_sources, calls_resources,
                 c_args : test_cflags,
                 link_args: test_link_args,
                 pie: true,
                 link_with : [calls_vala, libcalls],
                 dependencies: calls_deps,
                 include_directories : [
                     calls_includes,
                   ]
                 )
  test('application',
      dbus_run_session,
      args: t.full_path(),
      env: test_env,
      timeout : 300,
      depends: t
      )
endif

endif
