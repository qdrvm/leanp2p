vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF f5d7e82fa0d16488ebc4974407e7fe11615bb638
  SHA512 d93160336590b70379256547b4b584a107743e8c84aa2ccc9fff8615409dbcd47ac9a51d4dc05bc841db907c97a3733c0f4307a472ec9fafefb9fb8260669f7b
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
