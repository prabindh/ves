get_property(VES_RELATIVE_INCLUDE_DIRS GLOBAL PROPERTY VES_RELATIVE_INCLUDE_DIRS)
get_property(VES_RELATIVE_BUILD_INCLUDE_DIRS GLOBAL PROPERTY VES_RELATIVE_BUILD_INCLUDE_DIRS)
get_property(VES_LIBRARIES GLOBAL PROPERTY VES_LIBRARIES)

file(RELATIVE_PATH VES_INCLUDE_DIR_RELATIVE_TO_CONFIG_DIR
  "${CMAKE_INSTALL_PREFIX}/${VES_INSTALL_LIB_DIR}"
  "${CMAKE_INSTALL_PREFIX}/${VES_INSTALL_INCLUDE_DIR}")

export(TARGETS ${VES_LIBRARIES} FILE "${VES_BINARY_DIR}/${VES_TARGETS_NAME}.cmake")

configure_file("${VES_SOURCE_DIR}/CMake/ves-config.cmake.in" "${VES_BINARY_DIR}/ves-config.cmake" @ONLY)
configure_file("${VES_SOURCE_DIR}/CMake/ves-build-config.cmake.in" "${VES_BINARY_DIR}/ves-build-config.cmake" @ONLY)

install(FILES "${VES_BINARY_DIR}/ves-config.cmake" DESTINATION ${VES_INSTALL_LIB_DIR})
install(EXPORT ${VES_TARGETS_NAME} DESTINATION ${VES_INSTALL_LIB_DIR})
