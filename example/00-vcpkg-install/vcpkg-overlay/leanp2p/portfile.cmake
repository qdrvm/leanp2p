vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF fabc481353506283629ff7be3eb658e0c53b9eee
  SHA512 4d11ca72fe258c5bff259d81061d1bc1bd72b96ba35f1e800a2412a62c465e49199ecfab0e023bc8f6b96bbd0ff31c29c3ececff3d5621008173dcf64ef3daef
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
