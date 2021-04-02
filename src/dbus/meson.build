generated_dbus_sources = []

# DBus server protocols
generated_dbus_sources += gnome.gdbus_codegen('calls-call-dbus',
                'sm.puri.Calls.Call.xml',
                docbook: 'calls',
                interface_prefix: 'sm.puri',
                object_manager: true,
                namespace: 'CallsDBus')
