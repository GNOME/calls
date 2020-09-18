#
# Copyright (C) 2018 Purism SPC
#
# This file is part of Dialer.
#
# Dialer is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Dialer is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Dialer.  If not, see <http://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

project(
  'calls',
  'c', 'vala',
  version: '0.1.8',
  license: 'GPLv3+',
  meson_version: '>= 0.47.0',
  default_options: [
    'warning_level=1',
    'buildtype=debugoptimized',
    'c_std=gnu11'
  ]
)

calls_id = 'sm.puri.Calls'
calls_homepage = 'https://source.puri.sm/Librem5/calls'
calls_name = meson.project_name()
calls_version = meson.project_version()

top_include = include_directories('.')

prefix = get_option('prefix')
builddir = meson.current_build_dir()
libdir = get_option('libdir')
localedir = get_option('localedir')
full_localedir = join_paths(prefix, localedir)
full_calls_plugin_libdir = join_paths(prefix, libdir, calls_name, 'plugins')
# Path to plugins inside the build dir, used for testing
full_calls_plugin_builddir = join_paths(builddir, 'plugins')

config_data = configuration_data()
config_data.set_quoted('APP_ID', calls_id)
config_data.set_quoted('APP_DATA_NAME', calls_name)
config_data.set_quoted('GETTEXT_PACKAGE', calls_name)
config_data.set_quoted('LOCALEDIR', full_localedir)
config_data.set_quoted('PLUGIN_LIBDIR', full_calls_plugin_libdir)
config_data.set_quoted('PACKAGE_URL', calls_homepage)
config_data.set_quoted('PACKAGE_VERSION', calls_version)
config_data.set('PACKAGE_URL_RAW', calls_homepage)

add_project_arguments([
 '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_58',
 '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_60',
], language: 'c')

subdir('po')
subdir('src')
subdir('plugins')
subdir('doc')
subdir('data')
subdir('tests')
