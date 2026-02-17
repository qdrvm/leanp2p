vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF 51e7a4f483433bed8ddca5bb8df969c27c590d4b
  SHA512 4259788d600848c4d51b16e971fac8f8a13ad88f694cee551bf9461ba7869e2f0debc06548f883912d051161ed29c1b7c7dc15dbfc5a70b96b8d4c2927741dfe
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
