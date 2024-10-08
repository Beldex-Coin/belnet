set(CPACK_PACKAGE_VENDOR "belnet.beldex.io")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://belnet.beldex.io/")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/contrib/readme-installer.txt")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")

if(WIN32)
  include(cmake/win32_installer_deps.cmake)
  install(FILES ${CMAKE_SOURCE_DIR}/contrib/configs/00-exit.ini DESTINATION share/conf.d COMPONENT exit_configs)
  install(FILES ${CMAKE_SOURCE_DIR}/contrib/configs/00-keyfile.ini DESTINATION share/conf.d COMPONENT keyfile_configs)
  install(FILES ${CMAKE_SOURCE_DIR}/contrib/configs/00-debug-log.ini DESTINATION share/conf.d COMPONENT debug_configs)
  get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
  list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Unspecified" "belnet" "gui" "exit_configs" "keyfile_configs" "debug_configs")
  list(APPEND CPACK_COMPONENTS_ALL "belnet" "gui" "exit_configs" "keyfile_configs" "debug_configs")
elseif(APPLE)
  set(CPACK_GENERATOR DragNDrop;ZIP)
  get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
  list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Unspecified")
endif()


include(CPack)

if(WIN32)
  cpack_add_component(belnet
    DISPLAY_NAME "belnet"
    DESCRIPTION "core required belnet files"
    REQUIRED)

  cpack_add_component(gui
    DISPLAY_NAME "belnet gui"
    DESCRIPTION "electron based control panel for belnet")

  cpack_add_component(exit_configs
    DISPLAY_NAME "auto-enable exit"
    DESCRIPTION "automatically enable usage of exit.bdx as an exit node\n"
    DISABLED)

  cpack_add_component(keyfile_configs
    DISPLAY_NAME "persist address"
    DESCRIPTION "persist .bdx address across restarts of belnet\nnot recommended when enabling exit nodes"
    DISABLED)

  cpack_add_component(debug_configs
    DISPLAY_NAME "debug logging"
    DESCRIPTION "enable debug spew log level by default"
    DISABLED)
endif()

