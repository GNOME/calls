if get_option('gtk_doc')

subdir('xml')

glib_prefix = dependency('glib-2.0').get_variable(pkgconfig : 'prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')
docpath = join_paths(get_option('datadir'), 'gtk-doc', 'html')

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

gnome.gtkdoc('calls',
           main_xml: 'calls-docs.xml',
            src_dir: [
                       join_paths(meson.project_source_root(), 'src'),
                       join_paths(meson.project_build_root(), 'src'),
                       join_paths(meson.project_build_root(), 'src/dbus'),
                       join_paths(meson.project_build_root(), 'plugins/provider/ofono/libgdbofono'),
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

rst2man = find_program('rst2man', 'rst2man.py', required: false)
if get_option('manpages') and not rst2man.found()
   error('No rst2man found, but man pages were explicitly enabled')
endif

if get_option('manpages') and rst2man.found()
  custom_target('man-gnome-calls',
    input: 'gnome-calls.rst',
    output: 'gnome-calls.1',
    command: [rst2man, '@INPUT@'],
    capture: true,
    install: true,
    install_dir: get_option('mandir') / 'man1',
  )
endif
