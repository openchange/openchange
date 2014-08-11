PHP_ARG_ENABLE(mapi, whether to enable mapi extension,
[  --enable-mapi enable mapi extension])

PHP_MAPI_H="php_mapi.h mapi_exception.h mapi_profile_db.h mapi_profile.h mapi_session.h  mapi_mailbox.h  mapi_mailbox.h mapi_folder.h mapi_message.h mapi_contact.h  mapi_task.h mapi_appointment.h  mapi_attachment.h mapi_table.h  mapi_message_table.h mapi_folder_table.h mapi_attachment_table.h"
PHP_MAPI_SRC="mapi.c mapi_exception.c mapi_profile_db.c mapi_profile.c  mapi_session.c mapi_mailbox.c  mapi_folder.c mapi_message.c  mapi_contact.c mapi_task.c  mapi_appointment.c  mapi_attachment.c mapi_table.c mapi_message_table.c mapi_folder_table.c mapi_attachment_table.c php_mapi_constants.c "
dnl remove php_mapi_constants.c when it is genration is included in the build

AC_DEFINE(HAVE_MAPI, 1, [Whether you have LibMapi ])
PHP_NEW_EXTENSION(openchange, $PHP_MAPI_SRC, $ext_shared)

LIBMAPI_INCLINE=`pkg-config --cflags libmapi`
LIBMAPI_LIBLINE=`pkg-config --libs libmapi`

if test x"$LIBMAPI_INCLINE" = x""; then
   AC_MSG_ERROR([libmapi not found])
fi

if test x"$LIBMAPI_LIBLINE" = x""; then
   AC_MSG_ERROR([libmapi not found])
fi

LDFLAGS="$LDFLAGS $LIBMAPI_LIBLINE"
INCLUDES="$INCLUDES $LIBMAPI_INCLINE -I$PHP_OPENCHANGE_PATH -I$PHP_OPENCHANGE_PATH/utils"

$PHP_MAPI_SRC : $PHP_MAPI_H
