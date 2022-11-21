#
# Copyright (C) 2018 Purism SPC
#
# This file is part of Calls.
#
# Calls is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Calls is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Calls.  If not, see <http://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

project(
  'calls',
  'c', 'vala',
  version: '44.alpha.0',
  license: 'GPLv3+',
  meson_version: '>= 0.56.0',
  default_options: [
    'warning_level=1',
    'buildtype=debugoptimized',
    'c_std=gnu11'
  ]
)

calls_id = 'org.gnome.Calls'
calls_homepage = 'https://gitlab.gnome.org/GNOME/calls'
calls_name = meson.project_name()
calls_version = meson.project_version()

top_include = include_directories('.')

prefix = get_option('prefix')
builddir = meson.current_build_dir()
full_bindir = join_paths(prefix, get_option('bindir'))
libdir = get_option('libdir')
full_servicedir = join_paths(prefix, get_option('datadir'), 'dbus-1', 'services')
localedir = get_option('localedir')
full_localedir = join_paths(prefix, localedir)
full_calls_plugin_libdir = join_paths(prefix, libdir, calls_name, 'plugins')

libcall_ui_dep = dependency('call-ui',
  fallback:  ['libcall-ui', 'libcall_ui_dep'],
  default_options: ['tests=false', 'examples=false', 'gtk_doc=false'],
)

config_data = configuration_data()
config_data.set_quoted('APP_ID', calls_id)
config_data.set_quoted('APP_DATA_NAME', calls_name)
config_data.set_quoted('GETTEXT_PACKAGE', calls_name)
config_data.set_quoted('LOCALEDIR', full_localedir)
config_data.set_quoted('PLUGIN_LIBDIR', full_calls_plugin_libdir)
config_data.set_quoted('PACKAGE_URL', calls_homepage)
config_data.set_quoted('PACKAGE_VERSION', calls_version)
config_data.set('PACKAGE_URL_RAW', calls_homepage)

run_data = configuration_data()
run_data.set('ABS_BUILDDIR', meson.current_build_dir())
run_data.set('ABS_SRCDIR', meson.current_source_dir())

configure_file(
  input: 'run.in',
  output: 'run',
  configuration: run_data)

cc = meson.get_compiler('c')

global_c_args = []
test_c_args = [
  '-Wcast-align',
  '-Wdate-time',
  '-Wdeclaration-after-statement',
  ['-Werror=format-security', '-Werror=format=2'],
  '-Wendif-labels',
  '-Werror=incompatible-pointer-types',
  '-Werror=missing-declarations',
  '-Werror=overflow',
  '-Werror=return-type',
  '-Werror=shift-count-overflow',
  '-Werror=shift-overflow=2',
  '-Wfloat-equal',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Winit-self',
  '-Wmaybe-uninitialized',
  '-Wmisleading-indentation',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wold-style-definition',
  '-Wshadow',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wno-switch-enum',
  '-Wtype-limits',
  '-Wunused-function',
  '-Wunused-variable',
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_64',
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_70',
  # see https://gitlab.gnome.org/GNOME/evolution-data-server/-/issues/332
  '-DEDS_DISABLE_DEPRECATED',
  # in preparation for the switch to Gtk4 we should make sure not to use deprecated symbols
  '-DGDK_DISABLE_DEPRECATED',
  '-DGTK_DISABLE_DEPRECATED',
]

if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif

foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    global_c_args += arg
  endif
endforeach

add_project_arguments(
  global_c_args,
  language: 'c'
)

subdir('po')
subdir('src')
subdir('plugins')
subdir('tests')
subdir('doc')
subdir('data')

# gnome.post_install() is available since meson 0.59.0
# Distributions use their own tooling (e.g. postinst, triggers, etc)
# so it is okay if the post_install() is not run on distro builds
m_ver = meson.version().split('.')
if m_ver[0].to_int() > 0 or m_ver[1].to_int() > 58
  gnome.post_install(
    glib_compile_schemas: true,
    gtk_update_icon_cache: true,
    update_desktop_database: true,
  )
endif
