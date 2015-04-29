# Changes

All notable changes to this project will be documented in this file.
The descriptions should be useful and understandable for end users of OpenChange.
Unreleased changes refer to our current [master branch](https://github.com/openchange/openchange/).

## [Unreleased]

### Added
* Automatic Outlook inbox refresh when receiving new emails

### Fixes
* Fixed creation of root folders on online mode and some special folders such as Sync Issues.
* Fixed `Invalid bookmark` error when clicking on `All address lists` entry in recipient selection dialog box

### Improvements
* More records returned when searching for ambiguous names.

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
[//]: # (the current hash was master when this CHANGES.md file was created)
[unreleased]: https://github.com/Zentyal/openchange/compare/2.3-zentyal10...HEAD
[2.3-zentyal10]: https://github.com/Zentyal/openchange/compare/2.3-zentyal9...2.3-zentyal10
[2.3-zentyal9]: https://github.com/Zentyal/openchange/compare/2.3-zentyal8...2.3-zentyal9
