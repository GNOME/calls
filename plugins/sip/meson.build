#
# Copyright (C) 2021 Purism SPC
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
# Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

sip_include = include_directories('.')

sip_install_dir = join_paths(full_calls_plugin_libdir, 'sip')

sip_plugin = configure_file(
  input: 'sip.plugin.in',
  output: 'sip.plugin',
  configuration: config_data,
  install_dir: sip_install_dir
)

sip_deps = [
  dependency('gobject-2.0'),
  dependency('gstreamer-1.0'),
  dependency('gtk+-3.0'),
  dependency('libpeas-1.0'),
  dependency('sofia-sip-ua-glib'),
]

sip_sources = files(
  [
    'calls-sip-call.c', 'calls-sip-call.h',
    'calls-sip-origin.c', 'calls-sip-origin.h',
    'calls-sip-provider.c', 'calls-sip-provider.h',
    'calls-sip-util.c', 'calls-sip-util.h',
    'calls-sip-media-manager.c', 'calls-sip-media-manager.h',
    'calls-sip-media-pipeline.c', 'calls-sip-media-pipeline.h',
    'gst-rfc3551.c', 'gst-rfc3551.h',
  ]
)

sip_enum_headers = [
  'calls-sip-util.h',
]

sip_enums = gnome.mkenums('calls-sip-enums',
  h_template: 'calls-sip-enums.h.in',
  c_template: 'calls-sip-enums.c.in',
  sources: sip_enum_headers,
)

sip_sources += sip_enums

calls_sip = shared_module(
  'sip',
  sip_sources,
  dependencies: sip_deps,
  include_directories: src_include,
  link_with: libcalls,
  install: true,
  install_dir: sip_install_dir
)
