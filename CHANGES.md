# Changes

All notable changes to this project will be documented in this file.
The descriptions should be useful and understandable for end users of OpenChange.
Unreleased changes refer to our current [master branch](https://github.com/openchange/openchange/).

## [2.4-zentyal23] - 2016-05-04

### Added

* Give support to sharing calendars, contacts, tasks and
  notes with different roles including Author, Editor or Folder Owner

## [2.4-zentyal22] - 2016-02-22

### Fixes
* Fixed crash due to unsupported property type (multivalue instance properties)
* Changed some return errors value to avoid sync issues
* Fixed crash on oxosfld, stop on PERSIST_SENTINEL
* Avoid crash on libmapi, RAWIDSET_push_eid
* Avoid exceptions on openchange_migrate tool

## [2.4-zentyal21] - 2016-02-04

### Fixes
* Crash and performance due to memory management on asyncemsmdb
* Crash when samba was down on ocsmanager

### Added
* Dump PT_MV_LONG properties in FastTransfer Buffer NDR dump

### Improvements
* Indexing warm up much more efficient
* Indexing keys store on memcached normally have username as prefix


## [2.4-zentyal20] - 2016-01-26

### Fixes
* Import deletes of already deleted messages now works
* Deny the removal of special folders on provisioning

### Added
* Log timers for each operation done
* Constants for PidLidClientIntent masks
* Constants for PidLidMeetingType values


## [2.4-zentyal19] - 2016-01-22

### Fixes
* Changed :days parameter in ocsmanager to a value which is RFC-compliant
* Reconnect broken LDAP connections for some missing NSPI calls


## [2.4-zentyal18] - 2016-01-18

### Fixes
* Persist autocomplete on recipients in Outlook 2010 onwards between
  profiles and closing/opening Outlook process


## [2.4-zentyal17] - 2015-12-30

### Fixes
* Do not lose a mail sent when SMTP server is down and notify the client
* Do not crash if we receive OAuth2 Auth Request in NTLM handler

### Improvements
* Retry the samba connection attempt on ocsmanager startup
* Sharing messages are now managed in Online mode as well
* Perform Change Number restrictions to gather missing messages from
  the current status of the client


## [2.4-zentyal16] - 2015-12-01

### Fixes
* Fix table restriction wrong sql queries

### Improvements
* Manage NULL as query values in resolving names in Global Address List


## [2.4-zentyal15] - 2015-11-26

### Fixes
* Do not fail receiving an empty idset on SyncUploadStateStreamEnd
* Reconnect broken LDAP connections

### Improvements
* Notify missing --profile when using --create in mapiprofile tool
* Give support to Autodiscover requests done by MS Remote Connectivity Analyzer


## [2.4-zentyal14] - 2015-11-10

### Improvements
* Upgrade to samba 4.3.1 (no compatible with 4.1.x series)


## [2.4-zentyal13] - 2015-11-09

### Fixes
* Uniform the hack CNSetRead = CNSetSeen until MS-OXCFXICS Section 3.2.5.6 is implemented
* Perform proper decoding of idsets which contain GLOBSETs encoded using bitmask command.
  That avoids sync issues when returning the new state after badly parsing incoming one.

### Improvements
* Optimise idset data usage on downloading changes with consecutive ranges


## [2.4-zentyal12] - 2015-11-03

### Fixes
* Do not assume that string8 variables have ascii encoding (display names can have any character now)
* Do not abort on IDSET parsing but returning a format error
* Do not abort on syncing in some errors from backend


## [2.4-zentyal11] - 2015-10-26

### Fixes
* Openchange log level back to normal (no need to use log level +1 anymore)
* Do not return invalid GLOBSET range of identifiers if the first operation
  of a folder (no data in client) is an upload.
* Returning only affected change numbers after uploading changes
  that involves several messages
* Return proper identifier states after performing upload operations
  that involves more than one message

### Improvements
* Reimplemented replica ID - GUID mapping in openchangedb
* Decode Multiple Value Unicode strings in FastTransfer buffer when dumping


## [2.4-zentyal10] - 2015-10-09

### Fixes
* Fix openchange_user_cleanup.py bug with user names with non-alphanumerics characters
* Folder deletion using cached mode


## [2.4-zentyal9] - 2015-10-02

### Fixes
* Fix double free crash on asyncemsmdb sessions


## [2.4-zentyal8] - 2015-09-29

### Fixes
* Fixed crash about sessions: SIGABRT in GUID_string()
* Openchangedb: Index added on mapistore_indexing table of mysql
* Support notifications when the username is different from mail address (e.g. user bob with bobby@domain.com as his email)


## [2.4-zentyal7] - 2015-09-23

### Added
* Allow custom `AuthPackage` settings for autodiscover

### Improvements
* Improvements over SyncImportReadStateChanges

### Fixes
* Telephone and location fields are now shown in the Global Address List
* Fix infinite loop when ndrdump is enabled


## [2.4-zentyal6] - 2015-09-07

### Fixes

* Openchange sessions are handled much better
* Rpcproxy handles client disconnections better
* Avoid race condition uploading changes which made new objects be missed


## [2.4-zentyal5] - 2015-08-26

### Fixes

* Avoid disconnections between rpcproxy and openchange
* Openchangedb fix mysql_affected_rows()
* Use only one connection for all memcached
* Out of office message supports non-ascii characters


## [2.4-zentyal4] - 2015-08-06

### Fixes

* File name and correct size in small sized attachments, and submit time are now sent
  by OpenChange client against OpenChange server
* Less crashes on asyncemsmdb and oxcfxics code
* Added initial decoder for FastTransferBuffer blobs available using `mapiproxy:ndrdump = yes`
* Fix provision using (>= 1.2.5) `python-mysqldb` library


## [2.4-zentyal3] - 2015-07-28

### Fixes

* Several crash fixes
* Fix several synchronisation issues
* Provision ActiveSync LDAP attribute on `openchange_newuser`
* Add option to remove openchange user's LDAP attributes (`openchange_newuser --delete`)
* Option to enable all groups from an organization on `openchange_group` command (`--enable-all-groups`)


## [2.4-zentyal2] - 2015-06-24

### Fixes

* Support notifications when the username is a mail address (e.g. Zentyal Cloud)
* Open a shared calendar from address list in Outlook 2013
* Send event invitation mails to several attendees, mixing internal and external recipients
* Fix folder hierarchy synchronization issues on mailbox subfolders
* Old mails are now synchronized after account cleanup

### Performances
* Optimize the download of contents when you were in the middle of the first synchronization process in a business size mailbox.


## [2.4-zentyal1] - 2015-05-20

### Added
* Sharing request and invitation of folders among different Outlook versions
* Automatic Outlook inbox refresh when receiving new emails

### Fixes
* Deny the removal of a special folder
* Reuse special folders if a new one is being created with the same name
* No more `Deleted Items (1)`-like duplicated folders
* Fixed creation of root folders on online mode and some special folders such as Sync Issues.
* Fixed `Invalid bookmark` error when clicking on `All address lists` entry in recipient selection dialog box

### Improvements
* More records returned when searching for ambiguous names.
* Script improving initial time access and loading of a migrated IMAP mailbox in Outlook


## [2.3-zentyal11] - 2015-05-08

### Fixes
* Fix cache issues when cleaning the user data


## [2.3-zentyal10] - 2015-04-14

### Fixes
* Address book working much better than before


## [2.3-zentyal9] - 2015-03-16

### Added
* Sharing request and invitation of folders among different Outlook versions

### Fixes
* Fixed `Too many connections to ldap` when openchange runs on samba as member of a domain.
* Fixed `Mark All as Read` feature regression bug introduced by 7737bdf6

### Performances
* Fixed performance issue affecting initial synchronization of business size mailbox contents


[//]: # (unreleased compare link should be changed to the latest release)
[unreleased]: https://github.com/Zentyal/openchange/compare/2.4-zentyal23...HEAD
[2.4-zentyal23]: https://github.com/Zentyal/openchange/compare/2.4-zentyal22...2.4-zentyal23
[2.4-zentyal22]: https://github.com/Zentyal/openchange/compare/2.4-zentyal21...2.4-zentyal22
[2.4-zentyal21]: https://github.com/Zentyal/openchange/compare/2.4-zentyal20...2.4-zentyal21
[2.4-zentyal20]: https://github.com/Zentyal/openchange/compare/2.4-zentyal19...2.4-zentyal20
[2.4-zentyal19]: https://github.com/Zentyal/openchange/compare/2.4-zentyal18...2.4-zentyal19
[2.4-zentyal18]: https://github.com/Zentyal/openchange/compare/2.4-zentyal17...2.4-zentyal18
[2.4-zentyal17]: https://github.com/Zentyal/openchange/compare/2.4-zentyal16...2.4-zentyal17
[2.4-zentyal16]: https://github.com/Zentyal/openchange/compare/2.4-zentyal15...2.4-zentyal16
[2.4-zentyal15]: https://github.com/Zentyal/openchange/compare/2.4-zentyal14...2.4-zentyal15
[2.4-zentyal14]: https://github.com/Zentyal/openchange/compare/2.4-zentyal13...2.4-zentyal14
[2.4-zentyal13]: https://github.com/Zentyal/openchange/compare/2.4-zentyal12...2.4-zentyal13
[2.4-zentyal12]: https://github.com/Zentyal/openchange/compare/2.4-zentyal11...2.4-zentyal12
[2.4-zentyal11]: https://github.com/Zentyal/openchange/compare/2.4-zentyal10...2.4-zentyal11
[2.4-zentyal10]: https://github.com/Zentyal/openchange/compare/2.4-zentyal9...2.4-zentyal10
[2.4-zentyal9]: https://github.com/Zentyal/openchange/compare/2.4-zentyal8...2.4-zentyal9
[2.4-zentyal8]: https://github.com/Zentyal/openchange/compare/2.4-zentyal7...2.4-zentyal8
[2.4-zentyal7]: https://github.com/Zentyal/openchange/compare/2.4-zentyal6...2.4-zentyal7
[2.4-zentyal6]: https://github.com/Zentyal/openchange/compare/2.4-zentyal5...2.4-zentyal6
[2.4-zentyal5]: https://github.com/Zentyal/openchange/compare/2.4-zentyal4...2.4-zentyal5
[2.4-zentyal4]: https://github.com/Zentyal/openchange/compare/2.4-zentyal3...2.4-zentyal4
[2.4-zentyal3]: https://github.com/Zentyal/openchange/compare/2.4-zentyal2...2.4-zentyal3
[2.4-zentyal2]: https://github.com/Zentyal/openchange/compare/2.4-zentyal1...2.4-zentyal2
[2.4-zentyal1]: https://github.com/Zentyal/openchange/compare/2.3-zentyal11...2.4-zentyal1
[2.3-zentyal11]: https://github.com/Zentyal/openchange/compare/2.3-zentyal10...2.3-zentyal11
[2.3-zentyal10]: https://github.com/Zentyal/openchange/compare/2.3-zentyal9...2.3-zentyal10
[2.3-zentyal9]: https://github.com/Zentyal/openchange/compare/2.3-zentyal8...2.3-zentyal9
