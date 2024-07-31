#
# Copyright (C) 2018 Purism SPC
#
# This file is part of Calls.
#
# Calls is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Calls is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Calls.  If not, see <http://www.gnu.org/licenses/>.
#
# Author: Bob Ham <bob.ham@puri.sm>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

dummy_include = include_directories('.')

dummy_install_dir = join_paths(calls_provider_plugin_libdir, 'dummy')

dummy_plugin = configure_file(
  input: 'dummy.plugin.in.in',
  output: 'dummy.plugin.in',
  configuration: config_data,
)

i18n_plugin = i18n.merge_file(
  input: dummy_plugin,
  output: 'dummy.plugin',
  po_dir: po_dir,
  type: 'desktop',
  build_by_default: true,
  install: true,
  install_dir: dummy_install_dir,
)


dummy_deps = [
  dependency('gobject-2.0'),
  dependency('gtk4', version: '>= @0@'.format(gtk_version)),
  dependency('libpeas-2'),
]

dummy_sources = files(
  [
    'calls-dummy-call.c', 'calls-dummy-call.h',
    'calls-dummy-origin.c', 'calls-dummy-origin.h',
    'calls-dummy-provider.c', 'calls-dummy-provider.h'
  ]
)

calls_dummy = shared_module(
  'dummy',
  dummy_sources,
  dependencies: dummy_deps,
  include_directories: src_include,
  link_with: libcalls,
  install: true,
  install_dir: dummy_install_dir
)
