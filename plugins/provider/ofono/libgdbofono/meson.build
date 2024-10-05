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
dbus_interfaces = ['manager', 'modem', 'call', 'ussd']

gdbofono_src = []
gdbofono_headers = []
foreach iface: dbus_interfaces
  src = gnome.gdbus_codegen(
    'gdbo-' + iface,
    iface + '.xml',
    interface_prefix: 'org.ofono.',
    namespace: 'GDBO'
  )
  gdbofono_src += src
  gdbofono_headers += src[1]
endforeach

gdbofono_deps = [
  dependency('gio-2.0'),
  dependency('gio-unix-2.0'),
]

gdbofono_lib = static_library(
  'gdbofono',
  gdbofono_src,
  include_directories: top_include,
  dependencies: gdbofono_deps
)
