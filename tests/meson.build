if not get_option('tests')
  subdir_done()
endif

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
  'CALLS_SIP_ACCOUNT_FILE=@0@/tests/sip-account.cfg'.format(
    meson.project_build_root(),
  ),
]

test_cflags = [
  '-DFOR_TESTING',
  '-Wno-error=deprecated-declarations',
  '-DTEST_DATABASE="@0@"'.format(
    meson.current_source_dir() / 'data' / 'serviceproviders.xml',
  ),
]

test_link_args = ['-fPIC']

dbus_service_conf = configuration_data()
dbus_service_conf.set('CALLS_BUILD_DIR', '@0@'.format(builddir))
dbus_service_conf.set('CALLS_BUILD_DIR_STR', '"@0@"'.format(builddir))
dbus_service_conf.set('CALLS_DBUS_NAME', '"org.gnome.Calls"')
dbus_service_conf.set('CALLS_DBUS_OBJECT_PATH', '"/org/gnome/Calls"')

dbus_config_h = configure_file(
  output: 'calls-dbus-config.h',
  configuration: dbus_service_conf,
)

test_includes = include_directories('.')

subdir('mock')
subdir('services')

tests = [
  ['contacts', ['test-contacts.c']],
  [
    'dbus',
    ['test-dbus.c', generated_dbus_sources],
    {'extra_test_deps': [dbus_server]},
  ],
  ['emergency-call-types', ['test-emergency-call-types.c']],
  [
    'manager',
    ['test-manager.c'],
    {'extra_test_deps': [calls_plugins, plugins_test_depends]},
  ],
  [
    'plugins',
    ['test-plugins.c', calls_sources],
    {'extra_test_deps': [calls_plugins, plugins_test_depends]},
  ],
  [
    'ringer',
    ['mock-call.c', 'test-ringer.c'],
    {'extra_link_with': libfeedback},
  ],
  ['service-providers', ['test-service-providers.c']],
  ['settings', ['test-settings.c']],
  ['ui-call', ['test-ui-call.c', 'mock-call.c']],
  ['util', ['test-util.c']],
]

foreach t : tests
  test_name = t[0]
  test_sources = t[1]
  link_with = [calls_vala, libcalls]

  if t.length() == 3 and t[2].has_key('extra_link_with')
    link_with += t[2]['extra_link_with']
  endif

  exe = executable(
    test_name,
    test_sources,
    c_args: test_cflags,
    link_args: test_link_args,
    pie: true,
    link_with: link_with,
    dependencies: calls_deps,
    include_directories: [calls_includes, test_includes],
  )

  test_deps = [exe]
  if t.length() == 3 and t[2].has_key('extra_test_deps')
    test_deps += t[2]['extra_test_deps']
  endif

  test(t[0], exe, env: test_env, depends: test_deps)
endforeach


dbus_run_session = find_program('dbus-run-session')

if dbus_run_session.found()
  test_sources = ['test-application.c']
  t = executable(
    'application',
    test_sources,
    calls_resources,
    c_args: test_cflags,
    link_args: test_link_args,
    pie: true,
    link_with: [calls_vala, libcalls],
    dependencies: calls_deps,
    include_directories: [calls_includes],
  )
  test(
    'application',
    dbus_run_session,
    args: t.full_path(),
    env: test_env,
    timeout: 300,
    depends: t,
  )
endif
