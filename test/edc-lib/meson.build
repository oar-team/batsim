project('batsim_edc_libs', 'cpp',
  version: '0.1.0',
  license: 'LGPL-3.0',
  default_options: ['cpp_std=c++17'],
  meson_version: '>=0.40.0'
)

batprotocol_cpp_dep = dependency('batprotocol-cpp')
deps = [
  batprotocol_cpp_dep
]

common = [
  'batsim_edc.h'
]

rejecter = shared_library('rejecter', common + ['rejecter.cpp'],
  dependencies: deps,
  install: true,
)
