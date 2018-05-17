gdbus_codegen = find_program('gdbus-codegen')

gen_sh = find_program('gen.sh')

gdbofono_gen = generator(gen_sh,
                         output  : [ 'gdbo-@BASENAME@.c',
                                     'gdbo-@BASENAME@.h' ],
                         arguments : [ '@INPUT@', '@OUTPUT0@' ])

#manager_src = gdbus_ofono_gen.process('manager.xml')
#modem_src = gdbus_ofono_gen.process('modem.xml')
#call_src = gdbus_ofono_gen.process('call.xml')

manager_src = custom_target('manager',
                            input : 'manager.xml',
                            output : [ 'gdbo-manager.h',
                                       'gdbo-manager.c' ],
                            command : [gen_sh, '@INPUT@', '@OUTPUT0@'])

modem_src = custom_target('modem',
                          input : 'modem.xml',
                          output : [ 'gdbo-modem.h',
                                     'gdbo-modem.c' ],
                          command : [gen_sh, '@INPUT@', '@OUTPUT0@'])

call_src = custom_target('call',
                         input : 'call.xml',
                         output : [ 'gdbo-call.h',
                                    'gdbo-call.c' ],
                         command : [gen_sh, '@INPUT@', '@OUTPUT0@'])

deps = [ dependency('gio-2.0'),
         dependency('gio-unix-2.0'),
       ]

gdbofono_lib = shared_library('gdbofono',
                              manager_src, modem_src, call_src,
                              dependencies : deps )
