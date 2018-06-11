if get_option('tests')

test_env = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly,fatal-warnings',
  'GSETTINGS_BACKEND=memory',
  'PYTHONDONTWRITEBYTECODE=yes',
  'MALLOC_CHECK_=2',
]

test_cflags = [
  '-fPIE',
]

test_link_args = [
  '-fPIC',
]

test_names = [
  'test-provider',
]

test_calls_source_arrays = [ calls_sources, calls_dummy_sources ]

test_calls_sources = []
foreach source_array : test_calls_source_arrays
  foreach source : source_array
    test_calls_sources += join_paths('..', 'src', source)
  endforeach
endforeach

foreach test_name : test_names
  t = executable(test_name, test_name + '.c',
                 test_calls_sources, calls_enum_sources, calls_resources,
		 c_args : test_cflags,
		 link_args: test_link_args,
                 link_with : gdbofono_lib,
	         dependencies: calls_deps,
                 include_directories : include_directories('..',
                                                           join_paths('..', 'src')),
		)
  test(test_name, t, env: test_env)
endforeach

endif
