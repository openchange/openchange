PHP_ARG_ENABLE(hello, whether to enable Hello
World support,
[ --enable-hello   Enable Hello World support])

if test "$PHP_HELLO" = "yes"; then
  AC_DEFINE(HAVE_HELLO, 1, [Whether you have Hello World])
  PHP_NEW_EXTENSION(hello, hello.c, $ext_shared)
fi

LIBMAPI_INCLINE=`pkg-config --cflags  /usr/local/samba/lib/pkgconfig/libmapi.pc`
echo "XXX INCS $LIBMAPI_INCLINE"
LIBMAPI_LIBLINE=`pkg-config --libs /usr/local/samba/lib/pkgconfig/libmapi.pc`
echo "XXX LIBS $LIBMAPI_LIBLINE"


dnl PHP_EVAL_INCLINE($LIBMAPI_INCLINE)
dnl PHP_EVAL_LIBLINE($LIBMAPI_LIBLINE, LIBMAPI_SHARED_LIBADD)

dnl PHP_ADD_INCLUDE(..)

LDFLAGS="$LDFLAGS $LIBMAPI_LIBLINE"
INCLUDES="$INCLUDES $LIBMAPI_INCLINE -I/home/jag/work/openchange"

echo "XXX $INCLUDES | $LDFLAGS"
dnl echo "XXX LIBMAPI_SHARD $LIBMAPI_SHARED_LIBADD"