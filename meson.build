project('batsim', 'cpp',
    version: '4.2.0',
    license: 'LGPL-3.0',
    default_options: ['cpp_std=c++17'],
    meson_version: '>=0.40.0'
)

batversion = meson.project_version()
# Override version name from git information if possible
git = find_program('git', required: false)
if git.found()
    git_describe_cmd = run_command(git, 'describe', '--dirty')
    if git_describe_cmd.returncode() == 0
        batversion = git_describe_cmd.stdout().strip()
    endif
endif
message('batsim version set to: ' + batversion)

# Dependencies
simgrid_dep = dependency('simgrid')
boost_dep = dependency('boost')
rapidjson_dep = dependency('RapidJSON')
redox_dep = dependency('redox')
libzmq_dep = dependency('libzmq')
docopt_dep = dependency('docopt')
pugixml_dep = dependency('pugixml')
intervalset_dep = dependency('intervalset')

# old gcc/llvm c++ std libraries have implemented the filesystem lib in a separate lib
# - https://releases.llvm.org/11.0.1/projects/libcxx/docs/UsingLibcxx.html#using-filesystem
# - https://gcc.gnu.org/gcc-9/changes.html
if meson.get_compiler('cpp').get_id() == 'clang' and meson.get_compiler('cpp').version().version_compare('<9.0')
  add_project_link_arguments(['-lstdc++fs'], language : 'cpp')
elif meson.get_compiler('cpp').get_id() == 'gcc' and meson.get_compiler('cpp').version().version_compare('<9.0')
  add_project_link_arguments(['-lstdc++fs'], language : 'cpp')
endif

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
    'src/context.cpp',
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
    'src/permissions.cpp',
    'src/permissions.hpp',
    'src/pointers.hpp',
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
include_dir = include_directories('src')

batlib = static_library('batlib', src_without_main,
    include_directories: include_dir,
    dependencies: batsim_deps,
    cpp_args: '-DBATSIM_VERSION=@0@'.format(batversion),
    install: false)
batlib_dep = declare_dependency(
    link_with: batlib,
    include_directories: include_dir
)

batsim = executable('batsim', ['src/batsim.cpp'],
    include_directories: include_dir,
    dependencies: batsim_deps + [batlib_dep],
    cpp_args: '-DBATSIM_VERSION=@0@'.format(batversion),
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
        test_src,
        dependencies: batsim_deps + [batlib_dep, gtest_dep],
        include_directories: [test_incdir]
    )
    test('unittest', unittest)
endif
