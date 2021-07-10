generated_dbus_sources = []

# DBus server protocols
generated_dbus_sources += gnome.gdbus_codegen('calls-call-dbus',
                'org.gnome.Calls.Call.xml',
                docbook: 'calls',
                interface_prefix: 'org.gnome',
                object_manager: true,
                namespace: 'CallsDBus')
