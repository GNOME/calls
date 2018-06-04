datadir = get_option('datadir')

# Desktop file
install_data('sm.puri.Calls.desktop',
             install_dir : join_paths(datadir, 'applications'))

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
    test('Validate desktop file', desktop_utils,
        args: ['sm.puri.Calls.desktop']
    )
endif

# Icons
install_data('sm.puri.Calls.png',
             install_dir : join_paths(datadir, 'icons', 'hicolor', '256x256', 'apps'))
