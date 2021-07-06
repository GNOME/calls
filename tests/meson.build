if get_option('tests')

test_env = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly,fatal-warnings',
  'GSETTINGS_BACKEND=memory',
  'PYTHONDONTWRITEBYTECODE=yes',
  'MALLOC_CHECK_=2',
  'NO_AT_BRIDGE=1',
  'CALLS_SIP_TEST=1',
  'CALLS_AUDIOSRC=audiotestsrc',
  'CALLS_AUDIOSINK=fakesink',
]

test_cflags = [
  '-DFOR_TESTING',
  '-Wno-error=deprecated-declarations',
  '-DPLUGIN_BUILDDIR="@0@"'.format(full_calls_plugin_builddir),
]

test_link_args = [
  '-fPIC',
]

tests = [
  [ 'provider', []                       ],
  [ 'origin',   [ 'provider' ]           ],
  [ 'call',     [ 'provider', 'origin' ] ],
]

foreach test : tests
  name = test[0]

  setup_bases = test[1]
  setup_bases += name

  test_sources = []
  foreach base : setup_bases
    test_sources += [ 'setup-' + base + '.c',
                      'setup-' + base + '.h' ]
  endforeach
  test_sources += [ 'test-' + name + '.c',
                    'common.h' ]

  t = executable(name, test_sources,
                 dummy_sources,
                 c_args : test_cflags,
                 link_args: test_link_args,
                 pie: true,
                 link_with : [calls_vala, libcalls],
                 dependencies: calls_deps,
                 include_directories : [
                   calls_includes,
                   dummy_include,
                 ]
                )
  test(name, t, env: test_env)
endforeach

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
test('manager', t, env: test_env)

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
test('plugins', t, env: test_env)

test_sources = [ 'test-sip.c' ]
t = executable('sip', test_sources,
               c_args : test_cflags,
               link_args: test_link_args,
               pie: true,
               link_with : [calls_vala, calls_sip, libcalls],
               dependencies: [calls_deps, sip_deps],
               include_directories : [
                   calls_includes,
                   sip_include,
                 ]
               )
test('sip', t, env: test_env)

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

endif
