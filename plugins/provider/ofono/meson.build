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

subdir('libgdbofono')

ofono_install_dir = join_paths(calls_provider_plugin_libdir, 'ofono')

ofono_plugin = configure_file(
  input: 'ofono.plugin.in.in',
  output: 'ofono.plugin.in',
  configuration: config_data,
)

i18n_plugin = i18n.merge_file(
  input: ofono_plugin,
  output: 'ofono.plugin',
  po_dir: po_dir,
  type: 'desktop',
  build_by_default: true,
  install: true,
  install_dir: ofono_install_dir,
)

ofono_deps = [
  dependency('gobject-2.0'),
  dependency('gtk4', version: '>= @0@'.format(gtk_version)),
  dependency('libpeas-2'),
]

ofono_sources = files(
  [
    'calls-ofono-call.c', 'calls-ofono-call.h',
    'calls-ofono-origin.c', 'calls-ofono-origin.h',
    'calls-ofono-provider.c', 'calls-ofono-provider.h'
  ]
)

calls_ofono = shared_module(
  'ofono',
  ofono_sources, gdbofono_headers,
  dependencies: ofono_deps,
  include_directories: [
    src_include,
    include_directories('.')
  ],
  link_with: [gdbofono_lib, libcalls],
  install: true,
  install_dir: ofono_install_dir
)
