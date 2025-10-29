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

subdir('dbus')

src_include = include_directories('.')
calls_includes = [top_include, src_include]

calls_deps = [
  dependency('folks'),
  dependency('gmobile', version: '>= 0.3.0'),
  dependency('gobject-2.0', version: '>= 2.74'),
  dependency('gom-1.0'),
  dependency('gtk4', version: '>= @0@'.format(gtk_version)),
  dependency('libadwaita-1', version: '>= 1.6'),
  dependency('libcallaudio-0.1'),
  dependency('libebook-contacts-1.2'),
  dependency('libfeedback-0.0'),
  dependency('libpeas-2'),
  dependency('libsecret-1'),
  libcall_ui_dep,
]


calls_vala_deps = [dependency('libebook-contacts-1.2'), dependency('folks')]

calls_vala_sources = files('calls-phone-number-query.vala')

calls_vala = static_library(
  'calls-vala',
  calls_vala_sources,
  vala_header: 'calls-vala.h',
  dependencies: calls_vala_deps,
)


calls_enum_headers = files(
  'calls-account.h',
  'calls-call.h',
  'calls-manager.h',
  'calls-ringer.h',
  'calls-ussd.h',
)
calls_enum_sources = gnome.mkenums_simple(
  'enum-types',
  sources: calls_enum_headers,
)

calls_resources = gnome.compile_resources(
  'calls-resources',
  'calls.gresources.xml',
  source_dir: ['ui', '../data'],
  c_name: 'call',
  dependencies: metainfo_file,
)

calls_generated_sources = [
  calls_enum_sources,
  calls_resources,
  generated_dbus_sources,
]

calls_sources = files(
  'calls-account-overview.c',
  'calls-account-provider.c',
  'calls-account-row.c',
  'calls-account.c',
  'calls-application.c',
  'calls-best-match.c',
  'calls-call-record-row.c',
  'calls-call-record.c',
  'calls-call-selector-item.c',
  'calls-call-window.c',
  'calls-call.c',
  'calls-contacts-box.c',
  'calls-contacts-provider.c',
  'calls-contacts-row.c',
  'calls-dbus-manager.c',
  'calls-emergency-call-types.c',
  'calls-emergency-calls-manager.c',
  'calls-history-box.c',
  'calls-in-app-notification.c',
  'calls-log.c',
  'calls-main-window.c',
  'calls-manager.c',
  'calls-message-source.c',
  'calls-network-watch.c',
  'calls-new-call-box.c',
  'calls-notifier.c',
  'calls-origin.c',
  'calls-plugin-manager.c',
  'calls-plugin.c',
  'calls-provider.c',
  'calls-record-store.c',
  'calls-ringer.c',
  'calls-secret-store.c',
  'calls-service-providers.c',
  'calls-settings.c',
  'calls-ui-call-data.c',
  'calls-ussd.c',
  'calls-util.c',
) + calls_generated_sources

calls_config_data = config_data

config_h = configure_file(
  output: 'calls-config.h',
  configuration: calls_config_data,
)

version_h = vcs_tag(fallback: '', input: 'version.h.in', output: 'version.h')

calls_sources += [version_h]

libcalls = static_library(
  'calls',
  calls_sources,
  install: false,
  dependencies: calls_deps,
  link_with: calls_vala,
  include_directories: src_include,
)

executable(
  'gnome-calls',
  calls_resources,
  'main.c',
  dependencies: calls_deps,
  export_dynamic: true,
  include_directories: calls_includes,
  install: true,
  link_with: [calls_vala, libcalls],
  c_args: '-Wno-error=deprecated-declarations',
)
