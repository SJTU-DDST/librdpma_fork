NAME = 'host'

dependencies = [
  dependency('doca-common'), 
  dependency('doca-dma'),
  dependency('doca-comch'),
  dependency('threads'),
]

srcs = [
  'main.cpp',
  'host.cpp',
  'dma_server.cpp',
  'levelhashing_host.cpp',
  '../common/dma_common.cpp',
  '../common/comch_common.cpp',
  '../levelhashing/hash.c',
  '../levelhashing/level_hashing.c'
]

inc_dirs = [
  include_directories('..'),
  include_directories('../common'),
  include_directories('../levelhashing'),
  include_directories('/opt/mellanox/doca/include'),
]

executable(
  NAME,
  srcs,
  c_args: ['-Wno-missing-braces', '-O2'],
  dependencies: dependencies,
  include_directories: inc_dirs,
  install: false,
)