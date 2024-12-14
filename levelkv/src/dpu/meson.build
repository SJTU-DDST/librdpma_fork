NAME = 'dpu'

dependencies = [
  dependency('doca-common'), 
  dependency('doca-dma'),
]

srcs = [
  'main.cpp',
  'dpu.cpp',
  'dma_client.cpp',
  '../common/dma_common.cpp',
  '../common/level_hashing.cpp',
]

inc_dirs = [
  include_directories('..'),
  include_directories('../common'),
  include_directories('/opt/mellanox/doca/include'),
]

executable(
  NAME,
  srcs,
  c_args: '-Wno-missing-braces',
  dependencies: dependencies,
  include_directories: inc_dirs,
  install: false,
)