This file documents possible smb.conf configuration options for
OpenChange server, components and modules.

openchange server debugging
---------------------------

- __dcerpc_mapiproxy:ndrdump = true|false__

mapistore named properties backend
----------------------------------

- __mapistore:namedproperties = STRING__ This option specifies the
  adapter to use to store and manage named properties. Possible value
  for this parametric option are _mysql_ to use the MySQL backend or
  _ldb_ to use the LDB one. If the option is not provided, LDB will be
  used by default and storage path sets to default.

- __namedproperties:ldb_url = STRING__ This option specifies the LDB
  database URL of the LDB named properties backend.

- __namedproperties:ldb_data = STRING__ This option specifies
  the path where provisioning content is located.

- __namedproperties:mysql_sock = STRING__ This option is mandatory
  unless _namedproperties:mysql_host_ is specified. The option defines
  the UNIX socket path of the MySQL named properties backend.

- __namedproperties:mysql_user = STRING__ This option defines the
  mysql username of the account to use for the MySQL named properties
  backend.

- __namedproperties:mysql_pass = STRING__ This option defines
  the mysql password of the account to use for the MySQL named
  properties backend.

- __namedproperties:mysql_host = STRING__ This option is mandatory
  unless _namedproperties:mysql_sock_ is specified.  This option
  defines the host to connect to for the MySQL named properties
  backend.

- __namedproperties:mysql_port = STRING__ This option specifies the
  port on which server is available for the MySQL named properties
  backend. The option is set to 3306 if not specified.

- __namedproperties:mysql_db = STRING__ This option is
  mandatory. It defines the database to use for the MySQL named
  properties backend.

- __namedproperties:mysql_data = STRING__ This option specifies
  the path where MySQL schema file and provisioning content is
  located.

mapistore indexing backend
--------------------------

- __mapistore:indexing_backend = STRING__ This option specifies the
  database URL to be used for indexing backend. If not present LDB
  backend will be use. The URL must have a format like:
  mysql://user:pass@hostname/database

- __mapistore:indexing_cache = STRING__ This option specifies the
  connection string to use to connect to the memcached server used for
  fmid caching. The format of the string must be compliant with
  http://docs.libmemcached.org/libmemcached_configuration.html. For
  example, `--SERVER=127.0.0.1:11211` would use memcached server
  located on 127.0.0.1 and running on port 11211.

mapistore notification
----------------------

- __mapistore:notification_cache = STRING__ This option specifies the
  connection string to use to connect to the memcached server used for
  transitory notification data storage between emsmdb/asyncemsmdb
  endpoints. The format of the string must be compliant with
  http://docs.libmemcached.org/libmemcached_configuration.html. For
  example, `--SERVER=127.0.0.1:11211` would use memcached server
  located on 127.0.0.1 and running on port 11211.

mapiproxy openchangedb backend
------------------------------

- __mapiproxy:openchangedb = STRING__ This option specifies the
  database URL to be used for openchangedb backend. If not present LDB
  backend will be used. The URL must have a format like:
  mysql://user:pass@hostname/database

asyncemsmdb endpoint options
----------------------------

- __asyncesmsmdb:listen = STRING__ This option specifies the ip
  address on which the asyncemsmdb endpoint binds to receive external
  notifications. If not present "127.0.0.1" will be used.
