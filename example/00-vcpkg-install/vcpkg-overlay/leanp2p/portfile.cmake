vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF c4deae2f4269983253ce67a2f14506577a720972
  SHA512 be2542a3d51161abd7e8fc99b25ba9557c53fb7ee2dc6edccef16c87db76d23290b7f6df2cc197a1ae5365d49e7bef07e83948a9e62cb5a218dffc72cbfa9fc7
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
