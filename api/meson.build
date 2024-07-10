api_lib = static_library(
    'api',
    include_directories : inc,
    implicit_include_directories : false,
    c_pch : '../precompiled.h',
    dependencies : [
        ws2_lib,
    ],
    link_with : [
    ],
    sources : [
        'api.c',
		'api.h',
        'config.c',
		'config.h',
    ],
)
