if get_option('tests')

subdir('mock')

test_env = [
  'G_DEBUG=gc-friendly,fatal-warnings',
  'GSETTINGS_BACKEND=memory',
  'PYTHONDONTWRITEBYTECODE=yes',
  'MALLOC_CHECK_=2',
  'NO_AT_BRIDGE=1',
  'GSETTINGS_SCHEMA_DIR=@0@/data'.format(meson.project_build_root()),
  'CALLS_PLUGIN_DIR=@0@/plugins'.format(meson.project_build_root()),
]

test_cflags = [
  '-DFOR_TESTING',
  '-Wno-error=deprecated-declarations',
]

test_link_args = [
  '-fPIC',
]

mock_link_args = [ test_link_args,
                   '-Wl,--wrap=calls_contacts_provider_new',
                 ]


test_sources = [ 'test-manager.c' ]

t = executable('manager', test_sources,
                 c_args : test_cflags,
                 link_args: mock_link_args,
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
               link_args : mock_link_args,
               pie: true,
               link_with : [calls_vala, libcalls],
               dependencies : calls_deps,
               include_directories : [
                   calls_includes
                 ]
               )
test('ui-call', t, env: test_env)

test_sources = [
               'mock-call.c', 'mock-call.h', 'mock-libfeedback.h',
               'test-ringer.c'
]

t = executable('ringer', test_sources,
               c_args : test_cflags,
               link_args: mock_link_args,
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

endif
