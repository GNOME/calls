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

gnome = import('gnome')

src_include = include_directories('.')

calls_deps = [ dependency('gobject-2.0'),
               dependency('gtk+-3.0'),
               dependency('libhandy-0.0', version: '>= 0.0.10'),
               dependency('gsound'),
               dependency('libpeas-1.0'),
             ]

calls_sources = files(['calls-message-source.c', 'calls-message-source.h',
                       'calls-call.c',
                       'calls-origin.c', 'calls-origin.h',
                       'calls-provider.c', 'calls-provider.h',
                       'calls-enumerate-params.c', 'calls-enumerate-params.h',
                       'calls-enumerate.c', 'calls-enumerate.h',
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
                      ])

calls_config_data = config_data
calls_config_data.set_quoted('VCS_TAG', '@VCS_TAG@')

config_h_in = configure_file(
  output: 'config.h.in',
  configuration: calls_config_data
)

config_h = vcs_tag(
    fallback: '',
       input: config_h_in,
      output: 'config.h',
)

calls_enum_headers = files(['calls-call.h'])
calls_enum_sources = gnome.mkenums_simple('enum-types',
                                          sources : calls_enum_headers)

calls_resources = gnome.compile_resources(
   'calls-resources',
   'calls.gresources.xml',
   source_dir: ['ui', '../data'],
   c_name: 'call',
)

executable('calls',
           calls_sources, calls_enum_sources, calls_resources, 'main.c',
           dependencies : calls_deps,
           export_dynamic : true,
           include_directories : include_directories('..'),
           install : true)
