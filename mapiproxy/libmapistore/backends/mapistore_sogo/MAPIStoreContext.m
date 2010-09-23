/* MAPIStoreContext.m - this file is part of $PROJECT_NAME_HERE$
 *
 * Copyright (C) 2010 Wolfgang Sourdeau
 *
 * Author: Wolfgang Sourdeau <wsourdeau@inverse.ca>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#import <Foundation/NSCalendarDate.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSThread.h>
#import <Foundation/NSTimeZone.h>
#import <Foundation/NSValue.h>

#import <NGObjWeb/WOContext.h>
#import <NGObjWeb/WOContext+SoObjects.h>

#import <NGExtensions/NSString+misc.h>
#import <NGExtensions/NSObject+Logs.h>

#import <SOGo/NSString+Utilities.h>
#import <SOGo/SOGoFolder.h>

#import "MAPIApplication.h"
#import "MAPIStoreAuthenticator.h"
#import "MAPIStoreMapping.h"

#import "MAPIStoreContext.h"

@interface SOGoFolder (MAPIStoreProtocol)

- (BOOL) create;
- (NSException *) delete;

@end

@interface SOGoObject (MAPIStoreProtocol)

- (NSString *) davContentLength;
- (NSString *) decodedSubject;

@end

@implementation MAPIStoreContext : NSObject

/* sogo://username:password@{contacts,calendar,tasks,journal,notes,mail}/dossier/id */

Class SOGoMailAccountsK, SOGoMailAccountK, SOGoMailFolderK, SOGoUserFolderK, SOGoObjectK;
static MAPIStoreMapping *mapping = nil;

+ (void) initialize
{
        SOGoMailAccountsK = NSClassFromString (@"SOGoMailAccounts");
        SOGoMailAccountK = NSClassFromString (@"SOGoMailAccount");
        SOGoMailFolderK = NSClassFromString (@"SOGoMailFolder");
        SOGoUserFolderK = NSClassFromString (@"SOGoUserFolder");
        SOGoObjectK = NSClassFromString (@"SOGoObject");
        mapping = [MAPIStoreMapping new];
}

+ (id) contextFromURI: (const char *) newUri
{
        MAPIStoreContext *context;

	NSLog (@"METHOD '%s' (%d) -- uri: '%s'", __FUNCTION__, __LINE__, newUri);

        context = [self new];
        [context setURI: newUri];
        [context autorelease];

        return context;
}

- (id) init
{
        if ((self = [super init])) {
                uri = nil;
                objectCache = [NSMutableDictionary new];
                messageCache = [NSMutableDictionary new];
                subfolderCache = [NSMutableDictionary new];
                woContext = [WOContext contextWithRequest: nil];
                [woContext retain];
        }

        [self logWithFormat: @"-init"];

        return self;
}

- (void) dealloc
{
        [self logWithFormat: @"-dealloc"];
        [uri release];
        [objectCache release];
        [messageCache release];
        [subfolderCache release];
        [woContext release];
        [super dealloc];
}

- (id) authenticator
{
        MAPIStoreAuthenticator *authenticator;
        NSURL *url;

        url = [NSURL URLWithString: uri];
        if (!url)
                [self errorWithFormat: @"url string gave nil NSURL: '%@'", uri];

        authenticator = [MAPIStoreAuthenticator new];
        [authenticator setUsername: [url user]];
        [authenticator setPassword: [url password]];
        [authenticator autorelease];

        return authenticator;
}

- (void) setURI: (const char *) newUri
{
        uri = [NSString stringWithFormat: @"sogo://%@",
                        [NSString stringWithCString: newUri
                                           encoding: NSUTF8StringEncoding]];
        [uri retain];

        [mapping registerURL: uri];
}

- (void) setMemCtx: (void *) newMemCtx
{
        memCtx = newMemCtx;
}

- (void) setupRequest
{
        NSMutableDictionary *info;

        [MAPIApp setMAPIStoreContext: self];
        info = [[NSThread currentThread] threadDictionary];
        [info setObject: woContext forKey:@"WOContext"];
}

- (void) tearDownRequest
{
        NSMutableDictionary *info;

        info = [[NSThread currentThread] threadDictionary];
        [info removeObjectForKey:@"WOContext"];
        [MAPIApp setMAPIStoreContext: nil];
}

- (id) _moduleFromURL: (NSURL *) moduleURL
{
        id userFolder, moduleFolder, accountsFolder;
        NSString *userName, *mapiModule;

        userName = [moduleURL user];
        userFolder = [objectCache objectForKey: userName];
        if (!userFolder) {
                userFolder = [SOGoUserFolderK objectWithName: userName
                                                 inContainer: MAPIApp];
                /* Potential crash here as the username might be invalid and
                 * unknown from SOGo and hence the userFolder be nil. */
                [objectCache setObject: userFolder forKey: userName];
                [woContext setClientObject: userFolder];
        }

        mapiModule = [moduleURL host];
        if ([mapiModule isEqualToString: @"mail"]) {
                accountsFolder = [userFolder lookupName: @"Mail"
                                              inContext: woContext
                                                acquire: NO];
                moduleFolder = [accountsFolder lookupName: @"0"
                                                inContext: woContext
                                                  acquire: NO];
        }
        else {
                [self errorWithFormat: @"module name not handled: '%@'",
                      mapiModule];
                moduleFolder = nil;
        }

        return moduleFolder;
}

- (id) _lookupObject: (NSString *) objectURLString
{
        id object;
        NSURL *objectURL;
        NSArray *path;
        int count, max;
        NSString *pathString, *nameInContainer;

        // object = [objectCache objectForKey: objectURLString];
        // if (!object) {
        objectURL = [NSURL URLWithString: objectURLString];
        if (!objectURL)
                [self errorWithFormat: @"url string gave nil NSURL: '%@'", objectURLString];
        object = [self _moduleFromURL: objectURL];
        
        pathString = [objectURL path];
        if ([pathString hasPrefix: @"/"])
                pathString = [pathString substringFromIndex: 1];
        if ([pathString length] > 0) {
                path = [pathString componentsSeparatedByString: @"/"];
                max = [path count];
                if (max > 0) {
                        for (count = 0; count < max; count++) {
                                nameInContainer = [[path objectAtIndex: count]
                                                          stringByUnescapingURL];
                                object = [object lookupName: nameInContainer
                                                  inContext: woContext
                                                    acquire: NO];
                                [woContext setClientObject: object];
                        }
                }
        }
        if (object && [object isKindOfClass: SOGoObjectK])
                [objectCache setObject: object
                                forKey: objectURLString];
        else {
                object = nil;
                [woContext setClientObject: nil];
        }
        
        return object;
}

- (NSString *) _createFolder: (struct SRow *) aRow
                 inParentURL: (NSString *) parentFolderURL
{
        NSString *newFolderURL;
        NSString *folderName, *nameInContainer;
        SOGoFolder *parentFolder, *newFolder;
        int i;

        newFolderURL = nil;

        folderName = nil;
	for (i = 0; !folderName && i < aRow->cValues; i++) {
		if (aRow->lpProps[i].ulPropTag == PR_DISPLAY_NAME_UNICODE) {
			folderName = [NSString stringWithUTF8String: aRow->lpProps[i].value.lpszW];
		}
                else if (aRow->lpProps[i].ulPropTag == PR_DISPLAY_NAME) {
			folderName = [NSString stringWithUTF8String: aRow->lpProps[i].value.lpszA];
                }
	}

        if (folderName) {
                parentFolder = [self _lookupObject: parentFolderURL];
                if (parentFolder) {
                        if ([parentFolder isKindOfClass: SOGoMailAccountK]
                            || [parentFolder isKindOfClass: SOGoMailFolderK]) {
                                nameInContainer = [NSString stringWithFormat: @"folder%@",
                                                            [folderName asCSSIdentifier]];
                                newFolder = [SOGoMailFolderK objectWithName: nameInContainer
                                                                inContainer: parentFolder];
                                if ([newFolder create])
                                        newFolderURL = [NSString stringWithFormat: @"%@/%@",
                                                                 parentFolderURL,
                                                                 [nameInContainer stringByEscapingURL]];
                        }
                }
        }

        return newFolderURL;
}

/**
   \details Create a folder in the sogo backend
   
   \param private_data pointer to the current sogo context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */

- (int) mkDir: (struct SRow *) aRow
      withFID: (uint64_t) fid
  inParentFID: (uint64_t) parentFID
{
        NSString *folderURL, *parentFolderURL;
        int rc;

	[self logWithFormat: @"METHOD '%s' (%d)", __FUNCTION__, __LINE__];

        folderURL = [mapping urlFromID: fid];
        if (folderURL)
                rc = MAPISTORE_ERR_EXIST;
        else {
                parentFolderURL = [mapping urlFromID: parentFID];
                if (!parentFolderURL)
                        [self errorWithFormat: @"No url found for FID: %lld", parentFID];
                if (parentFolderURL) {
                        folderURL = [self _createFolder: aRow inParentURL: parentFolderURL];
                        if (folderURL) {
                                [mapping registerURL: folderURL withID: fid];
                                // if ([sogoFolder isKindOfClass: SOGoMailAccountK])
                                //         [sogoFolder subscribe];
                                rc = MAPISTORE_SUCCESS;
                        }
                        else
                                rc = MAPISTORE_ERR_NOT_FOUND;
                }
                else
                        rc = MAPISTORE_ERR_NO_DIRECTORY;
        }

	return rc;
}


/**
   \details Delete a folder from the sogo backend

   \param private_data pointer to the current sogo context
   \param parentFID the FID for the parent of the folder to delete
   \param fid the FID for the folder to delete

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
- (int) rmDirWithFID: (uint64_t) fid
         inParentFID: (uint64_t) parentFid
{
	[self logWithFormat: @"METHOD '%s' (%d)", __FUNCTION__, __LINE__];

	return MAPISTORE_SUCCESS;
}


/**
   \details Open a folder from the sogo backend

   \param private_data pointer to the current sogo context
   \param parentFID the parent folder identifier
   \param fid the identifier of the colder to open

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
- (int) openDir: (uint64_t) fid
    inParentFID: (uint64_t) parentFID
{
        [self logWithFormat: @"METHOD '%s' (%d)", __FUNCTION__, __LINE__];

	return MAPISTORE_SUCCESS;
}


/**
   \details Close a folder from the sogo backend

   \param private_data pointer to the current sogo context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
- (int) closeDir
{
	[self logWithFormat: @"METHOD '%s' (%d)", __FUNCTION__, __LINE__];

	return MAPISTORE_SUCCESS;
}

- (NSArray *) _messageKeysForFolderURL: (NSString *) folderURL
{
        NSArray *keys;
        SOGoFolder *folder;

        keys = [messageCache objectForKey: folderURL];
        if (!keys) {
                folder = [self _lookupObject: folderURL];
                if (folder)
                        keys = [folder toOneRelationshipKeys];
                else
                        keys = (NSArray *) [NSNull null];
                [messageCache setObject: keys forKey: folderURL];
        }

        return keys;
}

- (NSArray *) _subfolderKeysForFolderURL: (NSString *) folderURL
{
        NSArray *keys;
        SOGoFolder *folder;

        keys = [subfolderCache objectForKey: folderURL];
        if (!keys) {
                folder = [self _lookupObject: folderURL];
                if (folder)
                        keys = [folder toManyRelationshipKeys];
                else
                        keys = (NSArray *) [NSNull null];
                [subfolderCache setObject: keys forKey: folderURL];
        }

        return keys;
}

/**
   \details Read directory content from the sogo backend

   \param private_data pointer to the current sogo context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
- (int) readCount: (uint32_t *) rowCount
      ofTableType: (uint8_t) tableType
            inFID: (uint64_t) fid
{
        NSArray *ids;
        NSString *url;
        int rc;

	[self logWithFormat: @"METHOD '%s' (%d)", __FUNCTION__, __LINE__];

        url = [mapping urlFromID: fid];
        if (!url)
                [self errorWithFormat: @"No url found for FID: %lld", fid];

        switch (tableType) {
        case MAPISTORE_FOLDER_TABLE:
                ids = [self _subfolderKeysForFolderURL: url];
                break;
        case MAPISTORE_MESSAGE_TABLE:
                ids = [self _messageKeysForFolderURL: url];
                break;
        default:
                rc = MAPISTORE_ERR_INVALID_PARAMETER;
                ids = nil;
        }

        if ([ids isKindOfClass: [NSArray class]]) {
                rc = MAPI_E_SUCCESS;
                *rowCount = [ids count];
        }
        else
                rc = MAPISTORE_ERR_NO_DIRECTORY;

        return rc;
}

- (int) _getCommonTableChildproperty: (void **) data
                               atURL: (NSString *) childURL
                             withTag: (uint32_t) proptag
                            inFolder: (SOGoFolder *) folder
                             withFID: (uint64_t) fid
{
        NSString *stringData;
        id child;
        uint64_t *llongValue;
        uint32_t *longValue;
        int rc;

        rc = MAPI_E_SUCCESS;
        switch (proptag) {
        case PR_PARENT_FID:
                llongValue = talloc_zero(memCtx, uint64_t);
                *llongValue = fid;
                *data = llongValue;
                break;
        case PR_INST_ID: // TODO: DOUBT
                llongValue = talloc_zero(memCtx, uint64_t);
                *llongValue = [mapping idFromURL: childURL];
                if (*llongValue == NSNotFound) {
                        [mapping registerURL: childURL];
                        *llongValue = [mapping idFromURL: childURL];
                }
                *data = llongValue;
                break;
        case PR_INSTANCE_NUM: // TODO: DOUBT
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = [childURL hash]; /* we return a unique id based on the url */
                *data = longValue;
                break;
        case PR_DISPLAY_NAME_UNICODE:
                child = [self _lookupObject: childURL];
                if ([child respondsToSelector: @selector (displayName)])
                        stringData = [child displayName];
                else
                        stringData = [child nameInContainer];
                *data = talloc_strdup(memCtx,
                                      [stringData UTF8String]);
                break;
        case PR_DEPTH: // TODO: DOUBT
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 1;
                *data = longValue;
                break;
        default:
                [self errorWithFormat: @"Unknown proptag: %.8x for child '%@'",
                      proptag, childURL];
                *data = NULL;
                rc = MAPI_E_NOT_FOUND;
        }

        return rc;
}

- (int) _getMessageTableChildproperty: (void **) data
                                atURL: (NSString *) childURL
                              withTag: (uint32_t) proptag
                             inFolder: (SOGoFolder *) folder
                              withFID: (uint64_t) fid
{
        NSString *stringData;
        id child;
        uint64_t *llongValue;
        uint8_t *boolValue;
        uint32_t *longValue;
        int rc;

        rc = MAPI_E_SUCCESS;
        switch (proptag) {
        case PR_MID:
                llongValue = talloc_zero(memCtx, uint64_t);
                *llongValue = [mapping idFromURL: childURL];
                if (*llongValue == NSNotFound) {
                        [mapping registerURL: childURL];
                        *llongValue = [mapping idFromURL: childURL];
                }
                *data = llongValue;
                break;
        case PR_ROW_TYPE: // TODO: DOUBT
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 1;
                *data = longValue;
                break;
        case PR_FLAG_STATUS: // TODO
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 0;
                *data = longValue;
                break;
        case PR_MSG_STATUS: // TODO
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 0;
                *data = longValue;
                break;
        case PR_MESSAGE_FLAGS: // TODO
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 0;
                *data = longValue;
                break;
        case PR_ICON_INDEX: // TODO
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 0x00000100; /* read mail, see http://msdn.microsoft.com/en-us/library/cc815472.aspx */
                *data = longValue;
                break;
        case PR_SUBJECT_UNICODE:
                child = [self _lookupObject: childURL];
                stringData = [child decodedSubject];
                *data = talloc_strdup(memCtx,
                                      [stringData UTF8String]);
                break;
        case PR_MESSAGE_CLASS_UNICODE:
                *data = talloc_strdup(memCtx, "IPM.Note");
                break;
        case PR_MESSAGE_SIZE:
                child = [self _lookupObject: childURL];
                longValue = talloc_zero(memCtx, uint32_t);
                /* TODO: choose another name for that method */
                *longValue = [[child davContentLength] intValue];
                *data = longValue;
                break;
        case PR_HASATTACH:
                boolValue = talloc_zero(memCtx, uint8_t);
                *boolValue = NO;
                *data = boolValue;
                break;
                // case PR_MESSAGE_DELIVERY_TIME:
                //         llongValue = talloc_zero(memCtx, int64_t);
                //         child = [self _lookupObject: childURL];

                //         refDate = [NSCalendarDate dateWithYear: 1601 month: 1 day: 1
                //                                           hour: 0 minute: 0 second: 0
                //                                       timeZone: [NSTimeZone timeZoneWithName: @"UTC"]];
                //         *llongValue = (uint64_t) [[child date] timeIntervalSinceDate: refDate];
                //         // timeValue->dwLowDateTime = (uint32_t) (timeCompValue & 0xffffffff);
                //         // timeValue->dwHighDateTime = (uint32_t) ((timeCompValue >> 32) & 0xffffffff);
                //         *data = llongValue;
                //         break;
// #define PR_REPLY_TIME                                       PROP_TAG(PT_SYSTIME   , 0x0030) /* 0x00300040 */
// #define PR_EXPIRY_TIME                                      PROP_TAG(PT_SYSTIME   , 0x0015) /* 0x00150040 */
// #define PR_MESSAGE_DELIVERY_TIME                            PROP_TAG(PT_SYSTIME   , 0x0e06) /* 0x0e060040 */
// #define PR_IMPORTANCE                                       PROP_TAG(PT_LONG      , 0x0017) /* 0x00170003 */
// #define PR_SENSITIVITY                                      PROP_TAG(PT_LONG      , 0x0036) /* 0x00360003 */
// #define PR_FOLLOWUP_ICON                                    PROP_TAG(PT_LONG      , 0x1095) /* 0x10950003 */
// #define PR_ITEM_TEMPORARY_FLAGS                             PROP_TAG(PT_LONG      , 0x1097) /* 0x10970003 */
// #define PR_SEARCH_KEY                                       PROP_TAG(PT_BINARY    , 0x300b) /* 0x300b0102 */


                /* those are queried while they really pertain to the
                   addressbook module */
// #define PR_OAB_LANGID                                       PROP_TAG(PT_LONG      , 0x6807) /* 0x68070003 */
                // case PR_OAB_NAME_UNICODE:
                // case PR_OAB_CONTAINER_GUID_UNICODE:

// 0x68420102  PidTagScheduleInfoDelegatorWantsCopy (BOOL)


        case PR_SENT_REPRESENTING_NAME_UNICODE:
                *data = talloc_strdup(memCtx, "PR_SENT_REPRESENTING_NAME_UNICODE");
                // child = [self _lookupObject: childURL];
                // stringData = [child from];
                // *data = talloc_strdup(memCtx,
                //                       [stringData UTF8String]);
                break;
        case PR_INTERNET_MESSAGE_ID_UNICODE:
                *data = talloc_strdup(memCtx, "PR_INTERNET_MESSAGE_ID_UNICODE");
                break;
        default:
                rc = [self _getCommonTableChildproperty: data
                                                  atURL: childURL
                                                withTag: proptag
                                               inFolder: folder
                                                withFID: fid];
        }

        return rc;
}

- (int) _getFolderTableChildproperty: (void **) data
                               atURL: (NSString *) childURL
                             withTag: (uint32_t) proptag
                            inFolder: (SOGoFolder *) folder
                             withFID: (uint64_t) fid
{
        id child;
        uint64_t *llongValue;
        uint8_t *boolValue;
        uint32_t *longValue;
        struct Binary_r *binaryValue;
        int rc;

        [self logWithFormat: @"  querying child folder at URL: %@", childURL];
        rc = MAPI_E_SUCCESS;
        switch (proptag) {
        case PR_FID:
                llongValue = talloc_zero(memCtx, uint64_t);
                *llongValue = [mapping idFromURL: childURL];
                if (*llongValue == NSNotFound) {
                        [mapping registerURL: childURL];
                        *llongValue = [mapping idFromURL: childURL];
                }
                *data = llongValue;
                break;
        case PR_ATTR_HIDDEN:
        case PR_ATTR_SYSTEM:
        case PR_ATTR_READONLY:
                boolValue = talloc_zero(memCtx, uint8_t);
                *boolValue = NO;
                *data = boolValue;
                break;
        case PR_SUBFOLDERS:
                boolValue = talloc_zero(memCtx, uint8_t);
                child = [self _lookupObject: childURL];
                *boolValue = ([[child toManyRelationshipKeys]
                                      count]
                              > 0);
                *data = boolValue;
                break;
        case PR_CONTENT_COUNT:
                longValue = talloc_zero(memCtx, uint32_t);
                child = [self _lookupObject: childURL];
                *longValue = ([[child toOneRelationshipKeys]
                                      count]
                              > 0);
                *data = longValue;
                break;
        // case 0x36de0003: http://social.msdn.microsoft.com/Forums/en-US/os_exchangeprotocols/thread/17c68add-1f62-4b68-9d83-f9ec7c1c6c9b
        case PR_CONTENT_UNREAD:
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 0;
                *data = longValue;
                break;
        case PR_EXTENDED_FOLDER_FLAGS: // TODO: DOUBT: how to indicate the
                                       // number of subresponses ?
                binaryValue = talloc_zero(memCtx, struct Binary_r);
                *data = binaryValue;
                break;
        case PR_CONTAINER_CLASS_UNICODE:
                *data = talloc_strdup(memCtx, "IPF.Note");
                break;
        default:
                rc = [self _getCommonTableChildproperty: data
                                                  atURL: childURL
                                                withTag: proptag
                                               inFolder: folder
                                                withFID: fid];
        }

        return rc;
}

- (int) getTableProperty: (void **) data
                 withTag: (uint32_t) proptag
              atPosition: (uint32_t) pos
           withTableType: (uint8_t) tableType
                   inFID: (uint64_t) fid
{
        NSArray *children;
        NSString *folderURL, *childURL, *childName;
        SOGoFolder *folder;
        int rc;

	[self logWithFormat: @"METHOD '%s' (%d) -- proptag: 0x%.8x, pos: %ld, tableType: %d, fid: %lld",
              __FUNCTION__, __LINE__, proptag, pos, tableType, fid];

        folderURL = [mapping urlFromID: fid];
        if (folderURL) {
                folder = [self _lookupObject: folderURL];
                switch (tableType) {
                case MAPISTORE_FOLDER_TABLE:
                        children = [self _subfolderKeysForFolderURL: folderURL];
                        break;
                case MAPISTORE_MESSAGE_TABLE:
                        children = [self _messageKeysForFolderURL: folderURL];
                        break;
                default:
                        children = nil;
                        break;
                }

                if ([children count] > pos) {
                        childName = [children objectAtIndex: pos];
                        childURL = [folderURL stringByAppendingFormat: @"/%@",
                                              [childName stringByEscapingURL]];
                        [self logWithFormat: @"  querying child message at URL: %@", childURL];

                        if (tableType == MAPISTORE_FOLDER_TABLE)
                                rc = [self _getFolderTableChildproperty: data
                                                                  atURL: childURL
                                                                withTag: proptag
                                                               inFolder: folder
                                                                withFID: fid];
                        else
                                rc = [self _getMessageTableChildproperty: data
                                                                   atURL: childURL
                                                                 withTag: proptag
                                                                inFolder: folder
                                                                 withFID: fid];

                        /* Unhandled: */
// #define PR_EXPIRY_TIME                                      PROP_TAG(PT_SYSTIME   , 0x0015) /* 0x00150040 */
// #define PR_IMPORTANCE                                       PROP_TAG(PT_LONG      , 0x0017) /* 0x00170003 */
// #define PR_REPLY_TIME                                       PROP_TAG(PT_SYSTIME   , 0x0030) /* 0x00300040 */
// #define PR_SENSITIVITY                                      PROP_TAG(PT_LONG      , 0x0036) /* 0x00360003 */
// #define PR_MESSAGE_DELIVERY_TIME                            PROP_TAG(PT_SYSTIME   , 0x0e06) /* 0x0e060040 */
// #define PR_FOLLOWUP_ICON                                    PROP_TAG(PT_LONG      , 0x1095) /* 0x10950003 */
// #define PR_ITEM_TEMPORARY_FLAGS                             PROP_TAG(PT_LONG      , 0x1097) /* 0x10970003 */
// #define PR_SEARCH_KEY                                       PROP_TAG(PT_BINARY    , 0x300b) /* 0x300b0102 */
// #define PR_CONTENT_COUNT                                    PROP_TAG(PT_LONG      , 0x3602) /* 0x36020003 */
// #define PR_CONTENT_UNREAD                                   PROP_TAG(PT_LONG      , 0x3603) /* 0x36030003 */
// #define PR_FID                                              PROP_TAG(PT_I8        , 0x6748) /* 0x67480014 */
// unknown 36de0003
// unknown 819d0003
// unknown 81f80003
// unknown 81fa000b

                }
                else
                        rc = MAPISTORE_ERROR;
        }
        else {
                [self errorWithFormat: @"No url found for FID: %lld", fid];
                rc = MAPISTORE_ERR_NOT_FOUND;
        }

	return rc;
}


- (int) openMessage: (struct mapistore_message *) msg
            withMID: (uint64_t) mid
              inFID: (uint64_t) fid
{
	[self logWithFormat: @"METHOD '%s' (%d)", __FUNCTION__, __LINE__];

	return MAPI_E_SUCCESS;
}

- (int) createMessageWithMID: (uint64_t) mid
                       inFID: (uint64_t) fid
{
	[self logWithFormat: @"METHOD '%s' (%d)", __FUNCTION__, __LINE__];

	return MAPI_E_SUCCESS;
}

- (int) saveChangesInMessageWithMID: (uint64_t) mid
                           andFlags: (uint8_t) flags
{
	[self logWithFormat: @"METHOD '%s' (%d)", __FUNCTION__, __LINE__];

	return MAPI_E_SUCCESS;
}

- (int) submitMessageWithMID: (uint64_t) mid
                    andFlags: (uint8_t) flags
{
	[self logWithFormat: @"METHOD '%s' (%d)", __FUNCTION__, __LINE__];

	return MAPI_E_SUCCESS;
}

- (int) getProperties: (struct SPropTagArray *) SPropTagArray
                inRow: (struct SRow *) aRow
              withMID: (uint64_t) fmid
                 type: (uint8_t) tableType
{
	[self logWithFormat: @"METHOD '%s' (%d) -- tableType: %d, mid: %lld",
              __FUNCTION__, __LINE__, tableType, fmid];

	switch (tableType) {
	case MAPISTORE_FOLDER:
		break;
	case MAPISTORE_MESSAGE:
		break;
	}

	return MAPI_E_SUCCESS;
}

- (int) getFID: (uint64_t *) fid
        byName: (const char *) foldername
   inParentFID: (uint64_t) parent_fid
{
	[self logWithFormat: @"METHOD '%s' (%d) -- foldername: %s, parent_fid: %lld",
              __FUNCTION__, __LINE__, foldername, parent_fid];

        return MAPISTORE_ERR_INVALID_PARAMETER;
}

- (int) setPropertiesWithMID: (uint64_t) fmid
                        type: (uint8_t) type
                       inRow: (struct SRow *) aRow
{
	[self logWithFormat: @"METHOD '%s' (%d)", __FUNCTION__, __LINE__];

	switch (type) {
	case MAPISTORE_FOLDER:
		break;
	case MAPISTORE_MESSAGE:
		break;
	}

	return MAPI_E_SUCCESS;
}

- (int) deleteMessageWithMID: (uint64_t) mid
                   withFlags: (uint8_t) flags
{
	[self logWithFormat: @"METHOD '%s' (%d)", __FUNCTION__, __LINE__];

	return MAPI_E_SUCCESS;
}

@end
