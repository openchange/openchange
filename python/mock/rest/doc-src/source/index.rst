Mapistore HTTP REST API
=======================

This documentation is work in progress.

.. toctree::
   :maxdepth: 2

Authentication
--------------

TBD

Backend info
------------
.. http:get:: /info/

   Backend information: name, version, implemented capabilities...

   **Example request**:

   .. sourcecode:: http

      GET /info/ HTTP/1.1
      Host: example.com
      Accept: application/json

   **Example response**:

   .. sourcecode:: http

      HTTP/1.1 200 OK
      Vary: Accept
      Content-Type: application/json

      {
        "name": "My MAPISTORE Backend",
        "version": 3,
        "capabilities": {
          "soft_delete": true,
          "...": "...",
        },
      },

   :reqheader Authorization: auth token
   :reqheader Accept: the response content type depends on
                      :mailheader:`Accept` header
   :resheader Content-Type: this depends on :mailheader:`Accept`
                            header of request
   :statuscode 200: Ok


Folders
-------

.. http:post:: /folders/

   Create a new folder and return it

   **Example request**:

   .. sourcecode:: http

      POST /folders/ HTTP/1.1
      Host: example.com
      Accept: application/json

      {
        "parent_id": "c7e77cc9999908ec54ae32f1faf17e0e",
        "name": "new folder name"
      }

   **Example response**:

   .. sourcecode:: http

      HTTP/1.1 200 OK
      Vary: Accept
      Content-Type: application/json

      {
        "folder_id": "68b329da9893e34099c7d8ad5cb9c940"
      }

   :reqheader Authorization: auth token
   :statuscode 200: Ok

.. http:get:: /folders/(folder_id)/

   Folder info for given `folder_id`

   **Example request**:

   .. sourcecode:: http

      GET /folders/c7e77cc9999908ec54ae32f1faf17e0e/ HTTP/1.1
      Host: example.com
      Accept: application/json

   **Example response**:

   .. sourcecode:: http

      HTTP/1.1 200 OK
      Vary: Accept
      Content-Type: application/json

      {
        "folder_id": "c7e77cc9999908ec54ae32f1faf17e0e",
        "item_count": 37,
      },

   :reqheader Authorization: auth token
   :reqheader Accept: the response content type depends on
                      :mailheader:`Accept` header
   :resheader Content-Type: this depends on :mailheader:`Accept`
                            header of request
   :statuscode 200: Ok
   :statuscode 404: Folder does not exist


.. http:head:: /folders/(folder_id)/

   Check if the folder exists

   **Example request**:

   .. sourcecode:: http

      HEAD /folders/c7e77cc9999908ec54ae32f1faf17e0e/ HTTP/1.1
      Host: example.com

   **Example response**:

   .. sourcecode:: http

      HTTP/1.1 200 OK

   :reqheader Authorization: auth token
   :statuscode 200: Ok
   :statuscode 404: Folder does not exist


.. http:delete:: /folders/(folder_id)/

   Recursively delete the folder

   **Example request**:

   .. sourcecode:: http

      DELETE /folders/c7e77cc9999908ec54ae32f1faf17e0e/ HTTP/1.1
      Host: example.com

   **Example response**:

   .. sourcecode:: http

      HTTP/1.1 204 No content

   :reqheader Authorization: auth token
   :statuscode 204: Ok
   :statuscode 404: Folder does not exist


.. http:get:: /folders/(folder_id)/messages

   List of messages inside the folder

   **Example request**:

   .. sourcecode:: http

      GET /folders/c7e77cc9999908ec54ae32f1faf17e0e/messages HTTP/1.1
      Host: example.com
      Accept: application/json

   **Example response**:

   .. sourcecode:: http

      HTTP/1.1 200 OK
      Vary: Accept
      Content-Type: application/json

      [
        {
          "id": "7be92d92557702c8eb2e764266119346",
          "type": "email",
        },
        {
          "id": "fa21ee2b607ac6e327ecb39021be5469",
          "type": "calendar",
        },
        ...
      ]

   :query properties: List of wanted properties, response will only
                      contain these. If not set all properties will
                      be returned.
   :reqheader Authorization: auth token
   :reqheader Accept: the response content type depends on
                      :mailheader:`Accept` header
   :resheader Content-Type: this depends on :mailheader:`Accept`
                            header of request
   :statuscode 200: Ok
   :statuscode 404: Folder does not exist


.. http:get:: /folders/(folder_id)/folders

   List of folders inside the folder

   **Example request**:

   .. sourcecode:: http

      GET /folders/c7e77cc9999908ec54ae32f1faf17e0e/folders HTTP/1.1
      Host: example.com
      Accept: application/json

   **Example response**:

   .. sourcecode:: http

      HTTP/1.1 200 OK
      Vary: Accept
      Content-Type: application/json

      [
        {
          "id": "7be92d92557702c8eb2e764266119346",
          "type": "folder",
        },
        {
          "id": "fa21ee2b607ac6e327ecb39021be5469",
          "type": "folder",
        },
        ...
      ]

   :query properties: List of wanted properties, response will only
                      contain these. If not set all properties will
                      be returned.
   :reqheader Authorization: auth token
   :reqheader Accept: the response content type depends on
                      :mailheader:`Accept` header
   :resheader Content-Type: this depends on :mailheader:`Accept`
                            header of request
   :statuscode 200: Ok
   :statuscode 404: Folder does not exist

.. http:post:: /folders/(folder_id)

   Execute the given action on the folder

   **Example request**:

   .. sourcecode:: http

      POST /folders/c7e77cc9999908ec54ae32f1faf17e0e/?action=empty HTTP/1.1
      Host: example.com
      Accept: application/json

   **Example response**:

   .. sourcecode:: http

      HTTP/1.1 200 OK

   :query action: Action to execute, see list of folder actions
   :reqheader Authorization: auth token
   :statuscode 200: Ok

Folder actions
~~~~~~~~~~~~~~

 * empty - Recursively delete all the content of the folder
 * deletemessages -


Emails
------
TBD

Calendars
---------
TBD

Tasks
-----
TBD

Attachments
-----------
TBD

Notes
-----
TBD
