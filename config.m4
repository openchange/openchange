PHP_ARG_ENABLE(mapi, whether to enable mapi extension,
[  --enable-mapi enable mapi extension])

PHP_MAPI_H="php_mapi.h mapi_profile_db.h mapi_profile.h mapi_session.h  mapi_mailbox.h  mapi_mailbox.h mapi_folder.h mapi_message.h mapi_contact.h  mapi_task.h mapi_appointment.h  mapi_table.h  mapi_message_table.h mapi_folder_table.h"
PHP_MAPI_SRC="mapi.c mapi_profile_db.c mapi_profile.c  mapi_session.c mapi_mailbox.c  mapi_folder.c mapi_message.c  mapi_contact.c mapi_task.c  mapi_appointment.c  mapi_table.c mapi_message_table.c mapi_folder_table.c  ../utils/openchange-tools.c  php_mapi_constants.c"
dnl remove php_mapi_constants.c when it is genration is included in the build

if test "$PHP_MAPI" = "yes"; then
  AC_DEFINE(HAVE_MAPI, 1, [Whether you have LibMapi ])
  PHP_NEW_EXTENSION(mapi, $PHP_MAPI_SRC, $ext_shared)
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

$PHP_MAPI_SRC : $PHP_MAPI_H
