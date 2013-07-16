PHP_ARG_ENABLE(mapi, whether to enable LibMapi
support,
[ --enable-mapi   Enable LibMapi Wo support])

if test "$PHP_MAPI" = "yes"; then
  AC_DEFINE(HAVE_MAPI, 1, [Whether you have LibMapi ])
  PHP_NEW_EXTENSION(mapi, mapi.c, $ext_shared)
fi

OPENCHANGE_SRC="/home/jag/work/openchange"
LIBMAPI_INCLINE=`pkg-config --cflags  /usr/local/samba/lib/pkgconfig/libmapi.pc`
echo "XXX INCS $LIBMAPI_INCLINE"
LIBMAPI_LIBLINE=`pkg-config --libs /usr/local/samba/lib/pkgconfig/libmapi.pc`
echo "XXX LIBS $LIBMAPI_LIBLINE"


dnl PHP_EVAL_INCLINE($LIBMAPI_INCLINE)
dnl PHP_EVAL_LIBLINE($LIBMAPI_LIBLINE, LIBMAPI_SHARED_LIBADD)

dnl PHP_ADD_INCLUDE(..)

LDFLAGS="$LDFLAGS $LIBMAPI_LIBLINE"
INCLUDES="$INCLUDES $LIBMAPI_INCLINE -I$OPENCHANGE_SRC"

echo "XXX $INCLUDES | $LDFLAGS"
dnl echo "XXX LIBMAPI_SHARD $LIBMAPI_SHARED_LIBADD"