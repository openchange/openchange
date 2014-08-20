OC REST API mocked server
==================================================

Implements a Mock server to be used for testing openchange layer
and mapistore-to-python layer

Usage
-----

You may want to make REST API help first:

    ./build-doc.sh

Run the server:

    python api_server.py

By default, server listens on http://127.0.0.1:5000

If you have built the docs, default page is going to serve
the help page for the API (convenien).

TODO:

* "messages" support is not ready yet
* no sessions support at the moment
* no authentication support - everyone rules
* some test page to demonstrate basic calls using jQuery.ajax
* And administrative endpoint so that server could be tuned in automated environment

Enjoy!
