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

project('call', 'c',
          version: '0.0.0',
          license: 'GPLv3+',
    meson_version: '>= 0.42.0',
  default_options: [ 'warning_level=1', 'buildtype=debugoptimized', 'c_std=gnu11' ],
)


subdir('libgdbofono')
subdir('src')
subdir('doc')
subdir('data')
subdir('tests')
