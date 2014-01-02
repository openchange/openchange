PROJECT(OPENCHANGE_TEST C)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(OPENCHANGE_TEST_SRCS
    ${CMAKE_SOURCE_DIR}/openchange_test_suite.c
    ${CMAKE_SOURCE_DIR}/test_common.c
    # =========================================================================
    # Libmapistore
    # -------------------------------------------------------------------------
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_interface.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_processing.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_backend.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_backend_defaults.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_tdb_wrap.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_indexing.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_replica_mapping.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_namedprops.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_notification.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/backends/indexing_tdb.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/backends/indexing_mysql.c
    # -------------------------------------------------------------------------
    # Test suites
    # -------------------------------------------------------------------------
    ${CMAKE_SOURCE_DIR}/test_suites/libmapistore/indexing.c
    ${CMAKE_SOURCE_DIR}/test_suites/libmapistore/namedprops_mysql.c
    ${CMAKE_SOURCE_DIR}/test_suites/libmapistore/namedprops_ldb.c
    # =========================================================================
    # Libmapiproxy
    # -------------------------------------------------------------------------
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapiproxy/openchangedb.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapiproxy/openchangedb_table.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapiproxy/openchangedb_message.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapiproxy/openchangedb_property.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapiproxy/backends/openchangedb_ldb.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapiproxy/backends/openchangedb_mysql.c
    # -------------------------------------------------------------------------
    # Util
    # -------------------------------------------------------------------------
    ${CMAKE_SOURCE_DIR}/../mapiproxy/util/mysql.c
    # -------------------------------------------------------------------------
    # Test suites
    # -------------------------------------------------------------------------
    ${CMAKE_SOURCE_DIR}/test_suites/libmapiproxy/openchangedb.c
    # =========================================================================
)

IF(DEFINED ENV{SAMBA_PREFIX})
    SET(SAMBA_PREFIX $ENV{SAMBA_PREFIX})
ELSE()
    SET(SAMBA_PREFIX /usr/local/samba)
ENDIF()

message("Using samba ${SAMBA_PREFIX}")

ADD_DEFINITIONS(
    -Wall
    -fprofile-arcs -ftest-coverage
    -g -rdynamic -static
    -DMAPISTORE_LDIF="${CMAKE_SOURCE_DIR}/../setup/mapistore"
    -DMAPISTORE_BACKEND_INSTALLDIR="/not/used/for/now/on/this/test"
    -DOPENCHANGEDB_DATA_DIR="${CMAKE_SOURCE_DIR}/../setup/openchangedb"
    -DRESOURCES_DIR="${CMAKE_SOURCE_DIR}/resources"
)

INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/..
    ${SAMBA_PREFIX}/include
)

LINK_DIRECTORIES(
    ${SAMBA_PREFIX}/lib
    ${CMAKE_SOURCE_DIR}/..
)

ADD_EXECUTABLE(
    openchange_test
    ${OPENCHANGE_TEST_SRCS}
)

TARGET_LINK_LIBRARIES(openchange_test
    -fprofile-arcs gcov
    # Dependencies
    mysqlclient
    # unittest framework
    check m pthread
    # Samba shit
    #samba-credentials dcerpc dcerpc-binding
    ndr samba-hostconfig samba-util tevent talloc
    dl rt
    ldb tdb
    # Openchange
    -Wl,../libmapi.so.2.0
)

ADD_CUSTOM_TARGET(test ALL
    openchange_test
    DEPENDS openchange_test
)
