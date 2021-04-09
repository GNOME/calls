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

dummy_install_dir = join_paths(full_calls_plugin_libdir, 'dummy')

dummy_plugin = configure_file(
  input: 'dummy.plugin.in',
  output: 'dummy.plugin',
  configuration: config_data,
  install_dir: dummy_install_dir
)

dummy_deps = [
  dependency('gobject-2.0'),
  dependency('gtk+-3.0'),
  dependency('libpeas-1.0'),
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
  install: true,
  install_dir: dummy_install_dir
)
