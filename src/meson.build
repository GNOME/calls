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

calls_deps = [ dependency('gobject-2.0'),
               dependency('gtk+-3.0'),
               dependency('libhandy-0.0'),
             ]

calls_sources = ['calls-message-source.c', 'calls-message-source.h',
                 'calls-call.c',
                 'calls-origin.c', 'calls-origin.h',
                 'calls-provider.c', 'calls-provider.h',
                 'calls-ofono-call.c', 'calls-ofono-call.h',
                 'calls-ofono-origin.c', 'calls-ofono-origin.h',
                 'calls-ofono-provider.c', 'calls-ofono-provider.h',
                 'calls-party.c', 'calls-party.h',
                 'calls-call-data.c', 'calls-call-data.h',
                 'calls-call-holder.c', 'calls-call-holder.h',
                 'calls-call-display.c', 'calls-call-display.h',
                 'calls-call-selector-item.c', 'calls-call-selector-item.h',
                 'calls-encryption-indicator.c', 'calls-encryption-indicator.h',
                 'calls-main-window.c', 'calls-main-window.h',
                 'util.c', 'util.h',
                ]

calls_dummy_sources = ['calls-dummy-call.c', 'calls-dummy-call.h',
                       'calls-dummy-origin.c', 'calls-dummy-origin.h',
                       'calls-dummy-provider.c', 'calls-dummy-provider.h',
                      ]

calls_enum_headers = ['calls-call.h']
calls_enum_sources = gnome.mkenums_simple('enum-types',
                                          sources : calls_enum_headers)

calls_resources = gnome.compile_resources(
   'calls-resources',
   'calls.gresources.xml',
   source_dir: 'ui',
   c_name: 'call',
)

executable('calls',
           calls_sources, calls_enum_sources, calls_resources, 'main.c',
           dependencies : calls_deps,
           link_with : gdbofono_lib,
           include_directories : include_directories('..'),
           install : true)
