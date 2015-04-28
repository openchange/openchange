# Changes

All notable changes to this project will be documented in this file.
The descriptions should be useful and understandable for end users of OpenChange.
Unreleased changes refer to our current [master branch](https://github.com/openchange/openchange/).

## [Unreleased]

### Added
* Sharing request and invitation of folders among different Outlook versions
* Automatic Outlook inbox refresh when receiving new emails

### Fixes
* Fixed `Too many connections to ldap` when openchange runs on samba as member of a domain.
* Fixed `Mark All as Read` feature regression bug introduced by 7737bdf6
* Address book working much better than before
* Fixed `Invalid bookmark` error when clicking on `All address lists` entry in recipient selection dialog box

### Improvements
* More records returned when searching for ambiguous names.

### Performances
* Fixed performance issue affecting initial synchronization of business size mailbox contents


[//]: # (unreleased compare link should be changed to the latest release)
[//]: # (the current hash was master when this CHANGES.md file was created)
[unreleased]: https://github.com/openchange/openchange/compare/0460ace70d03de001de11d8fb54f0571128b0b2f...HEAD
