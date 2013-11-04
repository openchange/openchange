PROJECT(OPENCHANGE_TEST C)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(OPENCHANGE_TEST_SRCS
    ${CMAKE_SOURCE_DIR}/openchange_test_suite.c
    # =========================================================================
    # Libmapistore
    # -------------------------------------------------------------------------
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mgmt/gen_ndr/ndr_mapistore_mgmt.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_interface.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mgmt/mapistore_mgmt.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mgmt/mapistore_mgmt_messages.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mgmt/mapistore_mgmt_send.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_processing.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_backend.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_backend_defaults.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_tdb_wrap.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_indexing.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_replica_mapping.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_namedprops.c
    ${CMAKE_SOURCE_DIR}/../mapiproxy/libmapistore/mapistore_notification.c
    # Test suites
    # -------------------------------------------------------------------------
    ${CMAKE_SOURCE_DIR}/test_suites/namedprops_mysql.c
    ${CMAKE_SOURCE_DIR}/test_suites/namedprops_ldb.c
    # =========================================================================

)

ADD_DEFINITIONS(
    -Wall
    -fprofile-arcs -ftest-coverage
    -DMAPISTORE_LDIF="${CMAKE_SOURCE_DIR}/../setup/mapistore"
    -DMAPISTORE_BACKEND_INSTALLDIR="/not/used/for/now/on/this/test"
)

INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/..
    /usr/local/samba/include
)

LINK_DIRECTORIES(
    /usr/local/samba/lib
)

ADD_EXECUTABLE(
    openchange_test
    ${OPENCHANGE_TEST_SRCS}
)

TARGET_LINK_LIBRARIES(openchange_test
    -fprofile-arcs
    # Dependencies
    mysqlclient
    # unittest framework
    check
    # Samba shit
    #samba-credentials dcerpc dcerpc-binding
    ndr samba-hostconfig samba-util tevent talloc
    dl rt
    ldb tdb
)

ADD_CUSTOM_TARGET(test ALL
    openchange_test
    DEPENDS openchange_test
)
