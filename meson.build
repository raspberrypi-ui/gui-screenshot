project ('gui-screenshot','c')

share_dir = join_paths(get_option('prefix'), 'share')
resource_dir = join_paths(share_dir, meson.project_name())
desktop_dir = join_paths(share_dir, 'applications')

i18n = import('i18n')
gettext_package = meson.project_name()

wayland_scanner = find_program('wayland-scanner', native: true)
wayland_client = dependency('wayland-client')

add_project_arguments('-DGETTEXT_PACKAGE="' + gettext_package + '"', language : 'c' )
add_project_arguments('-DPACKAGE_LOCALE_DIR="' + share_dir + '/locale"', language : 'c' )
add_project_arguments('-DPACKAGE_DATA_DIR="' + resource_dir + '"', language : 'c' )

subdir('po')
subdir('src')
subdir('data')
