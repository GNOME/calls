gnome = import('gnome')
dbus_interfaces = ['manager', 'modem', 'call']

gdbus_src = []
foreach iface: dbus_interfaces
  gdbus_src += gnome.gdbus_codegen('gdbo-' + iface,
                                   iface + '.xml',
                                   interface_prefix: 'org.ofono.',
                                   namespace: 'GDBO')
endforeach

deps = [ dependency('gio-2.0'),
         dependency('gio-unix-2.0'),
       ]

gdbofono_lib = shared_library('gdbofono',
                               gdbus_src,
                               include_directories : include_directories('..'),
                               dependencies : deps )
