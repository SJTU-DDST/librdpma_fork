# project(
#   'DOCA_SAMPLE',
#   'C',
#   'CPP',
#   version: run_command(
#     find_program('cat'),
#     files('/opt/mellanox/doca/applications/VERSION'),
#     check: true,
#   ).stdout().strip(),
#   license: 'Proprietary',
#   default_options: ['buildtype=debug'],
#   meson_version: '>= 0.61.2',
# )

NAME = 'dma_copy_dpu'

# Comment this line to restore warnings of experimental DOCA features

sample_dependencies = []
# Required for all DOCA programs
sample_dependencies += dependency('doca-common')
# The DOCA library of the sample itself
sample_dependencies += dependency('doca-dma')
# Utility DOCA library for executables
sample_dependencies += dependency('doca-argp')

sample_dependencies += dependency('threads')

sample_srcs = [
  'dma_dpu_main.c',
  '../dma_common.c',
  '../benchmark.c',
]

inc_dirs = []
inc_dirs += include_directories('..')
inc_dirs += include_directories('../..')
inc_dirs += include_directories('/opt/mellanox/doca/include')

flags = [
  '-Wno-missing-braces',
  # '-DLATENCY_BENCHMARK',
]

executable(
  'doca_' + NAME,
  sample_srcs,
  c_args: flags,
  dependencies: sample_dependencies,
  include_directories: inc_dirs,
  install: false,
)