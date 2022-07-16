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
  dependency('libhandy-1', version: '>= 1.1.90'),
  dependency('libpeas-1.0'),
  dependency('sofia-sip-ua-glib'),
  dependency('libsecret-1'),
]

sip_sources = files(
  [
    'calls-sdp-crypto-context.c', 'calls-sdp-crypto-context.h',
    'calls-sip-call.c', 'calls-sip-call.h',
    'calls-sip-origin.c', 'calls-sip-origin.h',
    'calls-sip-provider.c', 'calls-sip-provider.h',
    'calls-sip-util.c', 'calls-sip-util.h',
    'calls-sip-media-manager.c', 'calls-sip-media-manager.h',
    'calls-sip-media-pipeline.c', 'calls-sip-media-pipeline.h',
    'calls-sip-account-widget.c', 'calls-sip-account-widget.h',
    'calls-srtp-utils.c', 'calls-srtp-utils.h',
    'gst-rfc3551.c', 'gst-rfc3551.h',
  ]
)

pipeline_enum_headers = [
  'calls-sip-media-pipeline.h',
]

pipeline_enums = gnome.mkenums_simple('calls-media-pipeline-enums',
                                      sources: pipeline_enum_headers)
sip_sources += pipeline_enums

sip_enum_headers = [
  'calls-sdp-crypto-context.h',
  'calls-sip-util.h',
]

sip_enums = gnome.mkenums_simple('calls-sip-enums',
                                 sources: sip_enum_headers,
)

sip_sources += sip_enums

sip_resources = gnome.compile_resources(
  'sip-resources',
  'sip.gresources.xml',
  source_dir: '.',
  c_name: 'call',
)

sip_sources += sip_resources

calls_sip = shared_module(
  'sip',
  sip_sources,
  dependencies: sip_deps,
  include_directories: src_include,
  link_with: libcalls,
  install: true,
  install_dir: sip_install_dir
)
