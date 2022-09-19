libfeedback_libdir = get_option('libdir')
libfeedback_link_args = []
libfeedback_symbols_file = 'libfeedback.syms'

lfb_enum_headers = files([
  'lfb-event.h',
])

libfeedback_headers = [
  'libfeedback.h',
  'lfb-event.h',
]

libfeedback_sources = [
  'lfb-enums.c',
  libfeedback_headers,
  'libfeedback.c',
  'lfb-event.c',
]

gio            = dependency('gio-2.0', version: '>=2.50.0')
glib           = dependency('glib-2.0', version: '>=2.50.0')
gobject        = dependency('gobject-2.0', version: '>=2.50.0')

libfeedback_deps = [
  gio,
  glib,
  gobject,
]

libfeedback_c_args = [
  '-DG_LOG_DOMAIN="libfeedback"',
]

libtype = 'shared_library'
libfeedback = build_target(
  'feedback-0',
  libfeedback_sources,
            soversion : 0,
               c_args : libfeedback_c_args,
         dependencies : libfeedback_deps,
            link_args : [],
              install : false,
          target_type : libtype,
)

libfeedback_inc = include_directories('.')
libfeedback_dep = declare_dependency(
  link_with : libfeedback,
  dependencies: libfeedback_deps,
  include_directories : libfeedback_inc,
)
