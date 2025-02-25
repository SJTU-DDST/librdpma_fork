project(
  'DOCA_SAMPLE',
  'C',
  'CPP',
  version: run_command(
    find_program('cat'),
    files('/opt/mellanox/doca/applications/VERSION'),
    check: true,
  ).stdout().strip(),
  license: 'Proprietary',
  default_options: ['buildtype=release'],
  meson_version: '>= 0.61.2',
)

add_project_arguments('-D DOCA_ALLOW_EXPERIMENTAL_API', '-O2', language: ['c', 'cpp'])

subdir('doca_dma_dpu')
subdir('doca_dma_host')