vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO xDimon/soralog
  REF refs/tags/v0.2.6
  SHA512 8ad2698cf029b70e909d7ace1e500957dceecc80e2331cecd1d66e775bb19456c81957e0c375ce81010f0aa5c8b6f13dc38fbf25c8f8547833548e350f6ae3f3
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
#vcpkg_cmake_config_fixup(PACKAGE_NAME "soralog")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
