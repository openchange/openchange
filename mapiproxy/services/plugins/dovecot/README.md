Dovecot mail notification plugin for OpenChange
===============================================

Julien Kerihuel, April 2015


In a nutshell
-------------

With this plugin written for dovecot series 2.2.X, OpenChange server
can notify Outlook whenever an event for which the user subscribed
has occured on the server.


In a cornshell
--------------

Outlook refreshes its mailbox upon new mail arrival or other mailbox
operations.


Introduction
------------

The architecture of the OpenChange server has until now always used a
poll model for Outlook notifications. This model finds its origins in
the early OpenChange server code designed as a synchronous rpc
service, implemented as a dcerpc endpoint plugin for samba server,
hence running in a single threaded environment.

It means OpenChange server had no way to be notified by the outside
world when something changed. It always had to be polled by Outlook in
order to be able to dispatch notifications to the client.

The Exchange notification model has ever since evolved and what was
acceptable in the 90' is not any further. The model has successively
used poll model, then UDP notifications and more recently to RPC
asynchronous service.

This asynchronous emsmdb service is the method implemented by Outlook
2007 and superior independently from the chosen transport layer. It is
the solution OpenChange has decided to implement.


Purpose and Scope
-----------------

The mail notification plugin primarily targets the dispatch of newmail
notifications to Outlook, therefore intended to trigger an action on
Outlook side to update its mailbox and reflect the mailbox changes.

Future iterations of this plugin should include support for message
update (flags), deletion, move and copy.


Requirements
------------

The openchange dovecot plugin relies on the following libraries:

 * libmapistore > 2.2 (and its deps)
 * libnanomsg > 0.5 (http://nanomsg.org/)

The notification system is not implementing any authentication
mechanism. It is furthermore assumed openchange server and dovecot
service to be working in a trusted environment.


Configuration options
---------------------

The plugin is relying on the lda protocol to intercept email
operations and triggers the newmail notification payload.

*You MUST add `notify` to the list of `mail_plugins` in the `protocol
lda` section of your dovecot.conf*

```
protocol lda {
       mail_plugins = notify openchange
}
```

The following configuration options can be defined in your dovecot
configuration file inside the plugin section:

```
plugin {
       openchange_resolver = "--SERVER=127.0.0.1:11211"
       openchange_cn = "username"
}
```

  * __openchange_resolver = STRING__ This option specifies the
    connection string used by the dovecot server to access the
    mapistore notification resolver service. This string is currently
    used to connect to a memcached server and its format must be
    compliant with
    http://docs.libmemcached.org/libmemcached_configuration.html. If
    no option is provided `--SERVER=127.0.0.1:11211` is used by
    default.

  * __openchange_cn = STRING__ This option specifies the field used by
    the dovecot service to query the resolver service. Acceptable
    values are: __username__ or __email__. If no option is specified,
    username is used by default.

  * __openchange_backend = STRING__ This option specifies the
    mapistore backend used by OpenChange server to deliver and fetch
    emails from this dovecot service. If no option is specified,
    "sogo" is used by default.


Compilation
-----------

It requires to have `dovecot-config` file which is available in Ubuntu
within the *dovecot-dev* package.

    ./autogen.sh
    ./configure --prefix=/usr
    make && sudo make install
