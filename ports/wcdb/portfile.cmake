vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Tencent/wcdb
    REF "v${VERSION}"
    SHA512 bf7fef90b17c46ffed7ad4d934bedc6dfe7ef279d46e1e879e74ce7f1f482faae47e451cc662cbafbd0ccabebb2908aad6dcbaa5f5249c447a4baa21d704f2db
    PATCHES vcpkg-build.patch
)

# WCDB requires Tencent's SQLite hooks, which the standard sqlcipher port lacks.
# Keep this revision in sync with the submodule in the pinned WCDB release.
vcpkg_from_github(
    OUT_SOURCE_PATH SQLCIPHER_SOURCE_PATH
    REPO Tencent/sqlcipher
    REF f049bed66ca26741f09a6e4f0603ed3af195ac96
    SHA512 2d3f1319c060761175150fb6c06fbd210d9c438439c701c1a648fe8377d89e67bf0a4ca77304f014bba44b41cc451d5477ccae927ec7738eeb0b84f933fd918e
    PATCHES sqlcipher-runtime.patch
)
file(REMOVE_RECURSE "${SOURCE_PATH}/sqlcipher")
file(COPY "${SQLCIPHER_SOURCE_PATH}/" DESTINATION "${SOURCE_PATH}/sqlcipher")
configure_file("${CMAKE_CURRENT_LIST_DIR}/WCDBConfig.cmake.in"
    "${SOURCE_PATH}/src/WCDBConfig.cmake.in" COPYONLY)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}/src"
    OPTIONS
        -DWCDB_CPP=ON
        -DWCDB_BRIDGE=OFF
        -DWCDB_ZSTD=ON
        -DSKIP_WCONAN=ON
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH share/wcdb)
vcpkg_copy_pdbs()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")


