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

subdir('wayland')

gnome = import('gnome')

src_include = include_directories('.')
calls_includes = [ top_include, src_include ]

calls_deps = [ dependency('gobject-2.0', version: '>= 2.58'),
               dependency('gtk+-3.0'),
               dependency('libhandy-1', version: '>= 1.0.0'),
               dependency('libfeedback-0.0'),
               dependency('libpeas-1.0'),
               dependency('gom-1.0'),
               dependency('libebook-contacts-1.2'),
               dependency('folks'),
               dependency('libcallaudio-0'),
             ]

if wl_scanner.found()
  calls_includes += include_directories('wayland')
  calls_deps += dependency('wayland-client', version: '>=1.14')
endif


calls_vala_deps = [
  dependency('libebook-contacts-1.2'),
  dependency('folks'),
]

calls_vala_sources = files (
  [
    'calls-phone-number-query.vala',
    'calls-best-match-view.vala',
  ]
)

calls_vala = static_library (
  'calls-vala',
  calls_vala_sources,
  vala_header : 'calls-vala.h',
  dependencies : calls_vala_deps,
)


calls_sources = files(['calls-message-source.c', 'calls-message-source.h',
                       'calls-call.c',
                       'calls-ussd.c',
                       'calls-origin.c', 'calls-origin.h',
                       'calls-provider.c', 'calls-provider.h',
                       'calls-party.c', 'calls-party.h',
                       'calls-call-data.c', 'calls-call-data.h',
                       'calls-call-holder.c', 'calls-call-holder.h',
                       'calls-call-display.c', 'calls-call-display.h',
                       'calls-call-selector-item.c', 'calls-call-selector-item.h',
                       'calls-call-window.c', 'calls-call-window.h',
                       'calls-encryption-indicator.c', 'calls-encryption-indicator.h',
                       'calls-history-box.c', 'calls-history-box.h',
                       'calls-new-call-box.c', 'calls-new-call-box.h',
                       'calls-main-window.c', 'calls-main-window.h',
                       'calls-ringer.c', 'calls-ringer.h',
                       'calls-application.c', 'calls-application.h',
                       'util.c', 'util.h',
                       'calls-call-record.c', 'calls-call-record.h',
                       'calls-record-store.c', 'calls-record-store.h',
                       'calls-call-record-row.c', 'calls-call-record-row.h',
                       'calls-contacts.c', 'calls-contacts.h',
                       'calls-best-match.c', 'calls-best-match.h',
                       'calls-in-app-notification.c', 'calls-in-app-notification.h',
                       'calls-manager.c', 'calls-manager.h',
                       'calls-notifier.c', 'calls-notifier.h',
                      ])

calls_config_data = config_data
calls_config_data.set_quoted('VCS_TAG', '@VCS_TAG@')
calls_config_data.set10('WL_SCANNER_FOUND', wl_scanner.found())

config_h_in = configure_file(
  output: 'config.h.in',
  configuration: calls_config_data
)

config_h = vcs_tag(
    fallback: '',
       input: config_h_in,
      output: 'config.h',
)

calls_enum_headers = files(['calls-call.h', 'calls-ussd.h', 'calls-manager.h'])
calls_enum_sources = gnome.mkenums_simple('enum-types',
                                          sources : calls_enum_headers)

calls_resources = gnome.compile_resources(
   'calls-resources',
   'calls.gresources.xml',
   source_dir: ['ui', '../data'],
   c_name: 'call',
)

executable (
  'calls',
  calls_sources, calls_enum_sources, calls_resources,
  wl_proto_sources, wayland_sources, 'main.c',
  dependencies : calls_deps,
  export_dynamic : true,
  include_directories : calls_includes,
  install : true,
  link_with : calls_vala,
  c_args : '-Wno-error=deprecated-declarations',
)
