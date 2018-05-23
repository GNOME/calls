if get_option('gtk_doc')

subdir('xml')

glib_prefix = dependency('glib-2.0').get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')
docpath = join_paths(get_option('datadir'), 'gtk-doc', 'html')

gnome.gtkdoc('calls',
           main_xml: 'calls-docs.xml',
            src_dir: [
                       join_paths(meson.source_root(), 'src'),
                       join_paths(meson.build_root(), 'src'),
                       join_paths(meson.build_root(), 'libgdbofono'),
                     ],
       dependencies: calls_deps,
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
