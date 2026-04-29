add_global_arguments('-Wno-unused-result', language : 'c')

wlr_data_control_xml = files('protocol/wlr-data-control-unstable-v1.xml')

wlr_data_control_client_h = custom_target('wlr-data-control-client-header',
  input: wlr_data_control_xml,
  output: 'wlr-data-control-unstable-v1-client-protocol.h',
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)

wlr_data_control_code = custom_target('wlr-data-control-private-code',
  input: wlr_data_control_xml,
  output: 'wlr-data-control-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

executable ('clipserve', 'clipboard.c', wlr_data_control_client_h, wlr_data_control_code, dependencies: [ wayland_client ], install: true)

gtk = dependency ('gtk+-3.0')

executable (meson.project_name(), 'gui-screenshot.c', dependencies: [ gtk ], install: true)
