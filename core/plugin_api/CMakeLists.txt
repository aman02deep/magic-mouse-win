# plugin_api is a header-only interface library.
# Link it to any target that needs to expose or consume the plugin ABI.
#
# Usage in a plugin project:
#   find_package(MagicMousePluginAPI REQUIRED)
#   target_link_libraries(my_plugin PRIVATE MagicMouse::plugin_api)

add_library(plugin_api INTERFACE)
add_library(MagicMouse::plugin_api ALIAS plugin_api)

target_include_directories(plugin_api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(plugin_api INTERFACE
    MAGIC_MOUSE_PLUGIN_API_VERSION=1
)
