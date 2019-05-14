project('batsim', 'cpp',
    version: '3.1.0-dev',
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
openssl_dep = dependency('openssl')
redox_dep = dependency('redox')
libzmq_dep = dependency('libzmq')
docopt_dep = dependency('docopt')
pugixml_dep = dependency('pugixml')
intervalset_dep = dependency('intervalset')

batsim_deps = [
    simgrid_dep,
    boost_dep,
    rapidjson_dep,
    openssl_dep,
    redox_dep,
    libzmq_dep,
    docopt_dep,
    pugixml_dep,
    intervalset_dep
]

# Source files
src = [
    'src/batsim.cpp',
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
    'src/unittest/test_buffered_outputting.cpp',
    'src/unittest/test_buffered_outputting.hpp',
    'src/unittest/test_main.cpp',
    'src/unittest/test_main.hpp',
    'src/unittest/test_numeric_strcmp.cpp',
    'src/unittest/test_numeric_strcmp.hpp',
    'src/workflow.cpp',
    'src/workflow.hpp',
    'src/workload.cpp',
    'src/workload.hpp'
]
include_dir = include_directories('src')

batsim = executable('batsim', src,
    include_directories: include_dir,
    dependencies: batsim_deps,
    cpp_args: '-DBATSIM_VERSION=@0@'.format(meson.project_version()),
    install: true
)
