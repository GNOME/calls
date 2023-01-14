t = executable('calls-dbus-server', 'calls-server.c',
                 dbus_config_h,
                 calls_sources,
                 c_args : test_cflags,
                 link_args: test_link_args,
                 pie: true,
                 link_with : [calls_vala, libcalls],
                 dependencies: calls_deps,
                 include_directories : [
                   calls_includes,
                   test_includes,
                 ]
                )

service_file = 'org.gnome.Calls.service'

service = configure_file (
  input         : service_file + '.in',
  output        : service_file,
  configuration : dbus_service_conf)
