{
  'target_defaults': {
    'conditions': [
      ['OS=="win"', {
        'defines': [
          '_WIN32_WINNT=0x0600',
        ],
      }],
    ],
    'xcode_settings': {
      'CLANG_CXX_LANGUAGE_STANDARD': 'c++11',
      'CLANG_CXX_LIBRARY': 'libc++',
      'CLANG_ENABLE_OBJC_ARC': 'YES',
      'GCC_OPTIMIZATION_LEVEL': '3',
      'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
      'GCC_WARN_SHADOW': 'YES',
      'GCC_WARN_UNUSED_VARIABLE': 'YES',
      'WARNING_CFLAGS': ['-Wall', '-Wno-cast-function-type-mismatch'],
    },
  },
  'targets': [
    {
      'target_name': 'addon',
      'include_dirs': [
        '<!(node -e "require(\'nan\')")',
      ],
      'sources': [
        'addon.cpp',
      ],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'cflags_cc': [ '-Wno-cast-function-type', '-Wno-cast-function-type-strict', '-Wno-deprecated-declarations' ],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        'OTHER_CFLAGS': [ '-Wno-cast-function-type-mismatch', '-Wno-deprecated-declarations' ],
      },
      'conditions': [
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '-framework AppKit',
            ],
          },
          'sources': [
            'system_icon_mac.mm',
          ],
        }],
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              'Gdiplus.lib',
            ],
          },
          'sources': [
            'system_icon_win.cpp',
          ],
        }],
        ['OS=="linux"', {
          'cflags_cc': [
            '<!@(pkg-config --cflags gio-2.0 gio-unix-2.0 gdk-pixbuf-2.0)',
          ],
          'link_settings': {
            'libraries': [
              '<!@(pkg-config --libs gio-2.0 gio-unix-2.0 gdk-pixbuf-2.0)',
            ],
          },
          'sources': [
            'system_icon_linux.cpp',
          ],
        }],
        ['OS!="mac" and OS!="win" and OS!="linux"', {
          'sources': [
            'system_icon.cpp',
          ],
        }],
      ],
    },
  ],
}
