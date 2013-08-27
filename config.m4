PHP_ARG_ENABLE(mapi, whether to enable mapi extension,
[  --enable-mapi enable mapi extension])

if test "$PHP_MAPI" = "yes"; then
  AC_DEFINE(HAVE_MAPI, 1, [Whether you have LibMapi ])
  PHP_NEW_EXTENSION(mapi, mapi.c mapi_profile_db.c mapi_profile.c mapi_session.c mapi_mailbox.c mapi_folder.c ../utils/openchange-tools.c, $ext_shared)
fi

PHP_ARG_WITH(openchange-path, path to openchange source,
[  --with-openchange-path Path to openchange source], /usr/scr/openchange)

dnl PHP_ADD_SOURCES($PHP_OPENCHANGE_PATH, utils/openchange-tools.c)

PHP_ARG_WITH(pkgconfig-path, path to libmapi pkgconfig directory,
[  --with-pkgconfig-path Path to libmapi pkgconfig directory], /usr/local/samba/lib/pkgconfig)

LIBMAPI_INCLINE=`pkg-config --cflags  $PHP_PKGCONFIG_PATH/libmapi.pc`
LIBMAPI_LIBLINE=`pkg-config --libs $PHP_PKGCONFIG_PATH/libmapi.pc`

LDFLAGS="$LDFLAGS $LIBMAPI_LIBLINE"
INCLUDES="$INCLUDES $LIBMAPI_INCLINE -I$PHP_OPENCHANGE_PATH -I$PHP_OPENCHANGE_PATH/utils"
