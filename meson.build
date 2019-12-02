project('batsim', 'cpp',
    version: '3.1.0',
    license: 'LGPL-3.0',
    default_options: ['cpp_std=c++11'],
    meson_version: '>=0.40.0'
)

# Dependencies
simgrid_dep = dependency('simgrid')
boost_dep = dependency('boost',
    modules : ['locale', 'filesystem', 'regex', 'system']
)
rapidjson_dep = dependency('RapidJSON')
redox_dep = dependency('redox')
libzmq_dep = dependency('libzmq')
docopt_dep = dependency('docopt')
pugixml_dep = dependency('pugixml')
intervalset_dep = dependency('intervalset')

batsim_deps = [
    simgrid_dep,
    boost_dep,
    rapidjson_dep,
    redox_dep,
    libzmq_dep,
    docopt_dep,
    pugixml_dep,
    intervalset_dep
]

# Source files
src_without_main = [
    'src/batsim.hpp',
    'src/context.hpp',
    'src/events.cpp',
    'src/events.hpp',
    'src/event_submitter.cpp',
    'src/event_submitter.hpp',
    'src/export.cpp',
    'src/export.hpp',
    'src/ipp.cpp',
    'src/ipp.hpp',
    'src/jobs.cpp',
    'src/jobs_execution.cpp',
    'src/jobs_execution.hpp',
    'src/jobs.hpp',
    'src/job_submitter.cpp',
    'src/job_submitter.hpp',
    'src/machines.cpp',
    'src/machines.hpp',
    'src/network.cpp',
    'src/network.hpp',
    'src/permissions.hpp',
    'src/profiles.cpp',
    'src/profiles.hpp',
    'src/protocol.cpp',
    'src/protocol.hpp',
    'src/pstate.cpp',
    'src/pstate.hpp',
    'src/server.cpp',
    'src/server.hpp',
    'src/storage.cpp',
    'src/storage.hpp',
    'src/task_execution.cpp',
    'src/task_execution.hpp',
    'src/workflow.cpp',
    'src/workflow.hpp',
    'src/workload.cpp',
    'src/workload.hpp'
]
src = src_without_main + ['src/batsim.cpp']
include_dir = include_directories('src')

batsim = executable('batsim', src,
    include_directories: include_dir,
    dependencies: batsim_deps,
    cpp_args: '-DBATSIM_VERSION=@0@'.format(meson.project_version()),
    install: true
)

# Unit tests.
if get_option('do_unit_tests')
    gtest_dep = dependency('gtest_main', version: '>=1.8.0', required: true)
    test_incdir = include_directories('src/unittest', 'src')
    test_src = [
        'src/unittest/test_buffered_outputting.cpp',
        'src/unittest/test_numeric_strcmp.cpp',
    ]
    unittest = executable('batunittest',
        test_src + src_without_main,
        dependencies: batsim_deps + [gtest_dep],
        include_directories: [test_incdir]
    )
    test('unittest', unittest)
endif
