project('batsim', 'cpp',
    version: '5.0.0-rc1',
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
libzmq_dep = dependency('libzmq')
intervalset_dep = dependency('intervalset')
batprotocol_cpp_dep = dependency('batprotocol-cpp')
cli11_dep = dependency('CLI11')
dl_dep = meson.get_compiler('cpp').find_library('dl', required : true) # dlmopen and friends

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
    libzmq_dep,
    intervalset_dep,
    batprotocol_cpp_dep,
    cli11_dep,
    dl_dep,
]

# Source files
src_without_main = [
    'src/batsim.hpp',
    'src/cli.cpp',
    'src/cli.hpp',
    'src/context.cpp',
    'src/context.hpp',
    'src/edc.cpp',
    'src/edc.hpp',
    'src/external_events.cpp',
    'src/external_events.hpp',
    'src/external_event_submitter.cpp',
    'src/external_event_submitter.hpp',
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
    'src/periodic.cpp',
    'src/periodic.hpp',
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
    'src/task_execution.cpp',
    'src/task_execution.hpp',
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

# Tests that are internal to Batsim's source code
# - func-tests checks that Batsim functions behave as expected in isolation
# - actor-tests checks that Batsim SimGrid actors behave as expected in isolation
if get_option('do_internal_tests')
    gtest_dep = dependency('gtest_main', version: '>=1.8.0', required: true)
    test_incdir = include_directories('src/test', 'src')
    func_test_src = [
        'src/test/func_test_buffered_outputting.cpp',
        'src/test/func_test_numeric_strcmp.cpp',
    ]
    func_test = executable('batsim-func-tests',
        func_test_src,
        dependencies: batsim_deps + [batlib_dep, gtest_dep],
        include_directories: [test_incdir],
        install: true
    )
    test('batsim-func-test', func_test)
endif
