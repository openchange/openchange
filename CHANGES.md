# Changes

All notable changes to this project will be documented in this file.
The descriptions should be useful and understandable for end users of OpenChange.
Unreleased changes refer to our current [master branch](https://github.com/openchange/openchange/).

## [Unreleased]

### Fixes

* Open a shared calendar from address list in Outlook 2013
* Send event invitation mails to several attendees, mixing internal and external recipients

## [2.3]

### Added
* Sharing request and invitation of folders among different Outlook versions
* Automatic Outlook inbox refresh when receiving new emails

### Fixes
* Deny the removal of a special folder
* Reuse special folders if a new one is being created with the same name
* No more `Deleted Items (1)`-like duplicated folders
* Fixed creation of root folders on online mode and some special folders such as Sync Issues.
* Fixed `Too many connections to ldap` when openchange runs on samba as member of a domain.
* Fixed `Mark All as Read` feature regression bug introduced by 7737bdf6
* Address book working much better than before
* Fixed `Invalid bookmark` error when clicking on `All address lists` entry in recipient selection dialog box

### Improvements
* More records returned when searching for ambiguous names.

### Performances
* Fixed performance issue affecting initial synchronization of business size mailbox contents
* Script improving initial time access and loading of a migrated IMAP mailbox in Outlook

[//]: # (unreleased compare link should be changed to the latest release)
[//]: # (the current hash was master when this CHANGES.md file was created)
[unreleased]: https://github.com/openchange/openchange/compare/4c18a0039344c93e49faf07cc52a14cec9cee3c7...HEAD
