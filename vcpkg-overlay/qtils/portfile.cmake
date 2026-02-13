vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qtils
  REF refs/tags/v0.1.6
  SHA512 7f4bcc40f1201b40510b9f9f5aee6c65b4f611b5b001e7e0996f3b9cdd852c388ceed29cb3a3d3302f90edee7dd62422e33c712752c58dc9b2a95141169d8fec
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qtils")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
