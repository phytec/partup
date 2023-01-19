static_data = [
  'layout-ab-emmc.yaml',
  'layout-simple.yaml',
  'partup-logo-2x.svg',
  'partup-logo-2x-white.svg'
]

foreach d : static_data
  configure_file(input : d,
                 output : d,
                 copy : true)
endforeach
