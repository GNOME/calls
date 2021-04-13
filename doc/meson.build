if get_option('gtk_doc')

subdir('xml')

glib_prefix = dependency('glib-2.0').get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')
docpath = join_paths(get_option('datadir'), 'gtk-doc', 'html')

if get_option('gtk_doc')
  # only build the shared library for gtk-doc
  calls_vala_doc_lib = library('calls-vala-doc',
                               calls_vala_sources,
                               dependencies: calls_vala_deps)
  calls_doc_lib = library('calls-doc',
                          [calls_sources, gdbofono_src],
                          include_directories: calls_includes,
                          link_with: calls_vala_doc_lib,
                          dependencies: calls_deps)
  calls_doc_deps = declare_dependency(sources: [calls_sources, calls_vala_sources],
                                      include_directories: calls_includes,
                                      link_with: [calls_doc_lib, calls_vala_doc_lib],
                                      dependencies: [calls_deps, calls_vala_deps])
endif

gnome.gtkdoc('calls',
           main_xml: 'calls-docs.xml',
            src_dir: [
                       join_paths(meson.source_root(), 'src'),
                       join_paths(meson.build_root(), 'src'),
                       join_paths(meson.build_root(), 'plugins/ofono/libgdbofono'),
                     ],
       dependencies: calls_doc_deps,
          scan_args: [
                       '--rebuild-types',
                     ],
       fixxref_args: [
                       '--html-dir=@0@'.format(docpath),
                       '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
                       '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
                       '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
                       '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gi')),
                       '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gtk3')),
                     ],
        install_dir: 'calls',
            install: true)

endif
