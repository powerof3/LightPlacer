# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO powerof3/CLibUtil
    REF aca80148853a8c0cb3e3cab3eaa20f1886cc99e7
    SHA512 f967330622ae56f3ff543d6ff41d8abb38562d9afc849aa9926a3428e68838785cbbec4b47df2cfd7f02a9bf07c684d0b9682925f7fb789290e9d2c514ac5ced
    HEAD_REF master
)

# Install codes
set(CLIBUTIL_SOURCE	${SOURCE_PATH}/include/ClibUtil)
file(INSTALL ${CLIBUTIL_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
