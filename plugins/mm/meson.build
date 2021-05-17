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

mm_install_dir = join_paths(full_calls_plugin_libdir, 'mm')

mm_plugin = configure_file(
  input: 'mm.plugin.in',
  output: 'mm.plugin',
  configuration: config_data,
  install_dir: mm_install_dir
)

mm_deps = [
  dependency('gobject-2.0'),
  dependency('gtk+-3.0'),
  dependency('ModemManager'),
  dependency('mm-glib', version: '>= 1.12.0'),
  dependency('libpeas-1.0'),
]

mm_sources = files(
  [
    'calls-mm-call.c', 'calls-mm-call.h',
    'calls-mm-origin.c', 'calls-mm-origin.h',
    'calls-mm-provider.c', 'calls-mm-provider.h'
  ]
)

calls_mm = shared_module(
  'mm',
  mm_sources,
  dependencies: mm_deps,
  include_directories: src_include,
  link_with: libcalls,
  install: true,
  install_dir: mm_install_dir
)
