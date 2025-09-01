vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/qtils
  REF 4749d1f8332e848dbce2ac74fc1a0c1992e9cbdb
  SHA512 5a3366d5a162b70461e73740313b5c1f43f4958bc52aab64372b9992d67f338b0c869125b00fe3cad599e368bd6918309a2aa4b1981f595eccb014f1bc655887
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "qtils")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
