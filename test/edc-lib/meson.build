project('batsim_edc_libs', 'cpp',
  version: '0.1.0',
  license: 'LGPL-3.0',
  default_options: ['cpp_std=c++17'],
  meson_version: '>=0.40.0'
)

batprotocol_cpp_dep = dependency('batprotocol-cpp')
boost_dep = dependency('boost')
intervalset_dep = dependency('intervalset')
nlohmann_json_dep = dependency('nlohmann_json')
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

exec1by1 = shared_library('exec1by1', common + ['exec1by1.cpp'],
  dependencies: deps + [boost_dep, intervalset_dep],
  install: true,
)

fcfs = shared_library('fcfs', common + ['fcfs.cpp'],
  dependencies: deps + [boost_dep, intervalset_dep],
  install: true,
)

easy = shared_library('easy', common + ['easy.cpp'],
  dependencies: deps + [boost_dep, intervalset_dep],
  install: true,
)

static = shared_library('static', common + ['static.cpp'],
  dependencies: deps + [boost_dep, intervalset_dep, nlohmann_json_dep],
  install: true,
)
