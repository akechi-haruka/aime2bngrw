aime2bngrw = shared_library(
    'aime2bngrw',
    name_prefix : '',
    include_directories : inc,
    implicit_include_directories : false,
    c_pch : '../precompiled.h',
    vs_module_defs : 'exports.def',
    link_with : [
        segapi.get_variable('api_lib'),
        aimelib.get_variable('aime_lib')
    ],
    dependencies : [
    ],
    sources : [
        'aime2bngrw.c',
        'aime2bngrw.h',
        'bngrw.c',
        'bngrw.h',
        'config.c',
        'config.h',
        'util/dprintf.c',
        'util/dprintf.h',
        'util/dump.c',
        'util/dump.h',
    ],
)
