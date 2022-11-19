#
# Copyright (C) 2022 Purism SPC
#
# This file is part of Calls.
#
# Calls is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Calls is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Calls.  If not, see <http://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#


if get_option('tests')

test_env = [
  'G_DEBUG=gc-friendly,fatal-warnings',
  'GSETTINGS_BACKEND=memory',
  'PYTHONDONTWRITEBYTECODE=yes',
  'MALLOC_CHECK_=2',
  'NO_AT_BRIDGE=1',
  'CALLS_SIP_TEST=1',
  'CALLS_AUDIOSRC=audiotestsrc',
  'CALLS_AUDIOSINK=fakesink',
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



test_sources = [ 'test-media.c' ]
test_sources += sip_sources
t = executable('media', test_sources,
               c_args : test_cflags,
               link_args: test_link_args,
               pie: true,
               link_with : [calls_vala, libcalls],
               dependencies: [calls_deps, sip_deps],
               include_directories : [
                   calls_includes,
                   sip_include,
                 ]
               )
test('media', t, env: test_env)

test_sources = [ 'test-sip.c' ]
test_sources += sip_sources
t = executable('sip', test_sources,
               c_args : test_cflags,
               link_args: test_link_args,
               pie: true,
               link_with : [calls_vala, libcalls],
               dependencies: [calls_deps, sip_deps],
               include_directories : [
                   calls_includes,
                   sip_include,
                 ]
               )
test('sip', t, env: test_env)

test_sources = [ 'test-srtp.c' ]
test_sources += sip_sources
t = executable('srtp', test_sources,
               c_args : test_cflags,
               link_args: test_link_args,
               pie: true,
               link_with : [calls_vala, libcalls],
               dependencies: [calls_deps, sip_deps],
               include_directories : [
                   calls_includes,
                   sip_include,
                 ]
               )
test('srtp', t, env: test_env)

test_sources = [ 'test-sdp-crypto.c' ]
test_sources += sip_sources
t = executable('sdp-crypto', test_sources,
               c_args : test_cflags,
               link_args: test_link_args,
               pie: true,
               link_with : [calls_vala, libcalls],
               dependencies: [calls_deps, sip_deps],
               include_directories : [
                   calls_includes,
                   sip_include,
                 ]
               )
test('sdp-crypto', t, env: test_env)


endif
