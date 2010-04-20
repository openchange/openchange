all:	fetchappointment	\
	fetchmail		\
	mapi_sample1		\
	multiple_notif

clean:
	rm -f fetchappointment fetchmail mapi_sample1 multiple_notif

fetchappointment:
		gcc fetchappointment.c -o fetchappointment `pkg-config libmapi --cflags --libs` 

fetchmail:
		gcc fetchmail.c -o fetchmail `pkg-config libmapi --cflags --libs`

mapi_sample1:
		gcc mapi_sample1.c -o mapi_sample1 `pkg-config libmapi --cflags --libs`

multiple_notif:
		gcc multiple_notif.c -o multiple_notif `pkg-config libmapi --cflags --libs` -lpthread -lpopt