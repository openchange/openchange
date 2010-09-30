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

#import <NGCards/NSArray+NGCards.h>

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

@end

@interface MAPIStoreMailContext : MAPIStoreContext

@end

@interface MAPIStoreContactsContext : MAPIStoreContext

@end

@implementation MAPIStoreContext : NSObject

/* sogo://username:password@{contacts,calendar,tasks,journal,notes,mail}/dossier/id */

Class NSNullK;
Class SOGoMailAccountsK, SOGoMailAccountK, SOGoMailFolderK, SOGoUserFolderK, SOGoObjectK;
static MAPIStoreMapping *mapping = nil;

+ (void) initialize
{
        NSNullK = NSClassFromString (@"NSNull");

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
        MAPIStoreAuthenticator *authenticator;
        NSString *contextClass, *module, *completeURLString, *urlString;
        NSURL *baseURL;

	NSLog (@"METHOD '%s' (%d) -- uri: '%s'", __FUNCTION__, __LINE__, newUri);

        context = nil;

        urlString = [NSString stringWithUTF8String: newUri];
        if (urlString) {
                completeURLString = [@"sogo://" stringByAppendingString: urlString];
                baseURL = [NSURL URLWithString: completeURLString];
                if (baseURL) {
                        module = [baseURL host];
                        if (module) {
                                if ([module isEqualToString: @"mail"])
                                        contextClass = @"MAPIStoreMailContext";
                                else if ([module isEqualToString: @"contacts"])
                                        contextClass = @"MAPIStoreContactsContext";
                                else {
                                        NSLog (@"ERROR: unrecognized module name '%@'", module);
                                        contextClass = nil;
                                }
                                
                                if (contextClass) {
                                        [mapping registerURL: completeURLString];
                                        context = [NSClassFromString (contextClass) new];
                                        [context autorelease];

                                        authenticator = [MAPIStoreAuthenticator new];
                                        [authenticator setUsername: [baseURL user]];
                                        [authenticator setPassword: [baseURL password]];
                                        [context setAuthenticator: authenticator];
                                        [authenticator release];

                                        [context setupRequest];
                                        [context setupModuleFolder];
                                        [context tearDownRequest];
                                }
                        }
                }
                else
                        NSLog (@"ERROR: url could not be parsed");
        }
        else
                NSLog (@"ERROR: url is an invalid UTF-8 string");

        return context;
}

- (id) init
{
        if ((self = [super init])) {
                // objectCache = [NSMutableDictionary new];
                messageCache = [NSMutableDictionary new];
                subfolderCache = [NSMutableDictionary new];
                woContext = [WOContext contextWithRequest: nil];
                [woContext retain];
                moduleFolder = nil;
        }

        [self logWithFormat: @"-init"];

        return self;
}

- (void) dealloc
{
        [self logWithFormat: @"-dealloc"];

        // [objectCache release];
        [messageCache release];
        [subfolderCache release];

        [moduleFolder release];
        [woContext release];
        [authenticator release];
        [super dealloc];
}

- (void) setAuthenticator: (MAPIStoreAuthenticator *) newAuthenticator
{
        ASSIGN (authenticator, newAuthenticator);
}

- (MAPIStoreAuthenticator *) authenticator
{
        return authenticator;
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

- (void) setupModuleFolder
{
        [self subclassResponsibility: _cmd];
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
        object = moduleFolder;
        
        pathString = [objectURL path];
        if ([pathString hasPrefix: @"/"])
                pathString = [pathString substringFromIndex: 1];
        if ([pathString length] > 0) {
                path = [pathString componentsSeparatedByString: @"/"];
                max = [path count];
                if (max > 0) {
                        for (count = 0;
                             object && count < max;
                             count++) {
                                nameInContainer = [[path objectAtIndex: count]
                                                          stringByUnescapingURL];
                                object = [object lookupName: nameInContainer
                                                  inContext: woContext
                                                    acquire: NO];
                                if ([object isKindOfClass: SOGoObjectK])
                                        [woContext setClientObject: object];
                                else
                                        object = nil;
                        }
                }
        } else
                object = nil;

        [woContext setClientObject: object];
        // if (object && [object isKindOfClass: SOGoObjectK])
        //         [objectCache setObject: object
        //                         forKey: objectURLString];
        // else {
        //         object = nil;
        //         [woContext setClientObject: nil];
        // }
        
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
                if (folder) {
                        keys = [folder toManyRelationshipKeys];
                        if (!keys)
                                keys = (NSArray *) [NSNull null];
                }
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

- (int) getCommonTableChildproperty: (void **) data
                              atURL: (NSString *) childURL
                            withTag: (uint32_t) proptag
                           inFolder: (SOGoFolder *) folder
                            withFID: (uint64_t) fid
{
        NSString *stringData;
        id child;
        // uint64_t *llongValue;
        // uint32_t *longValue;
        int rc;

        rc = MAPI_E_SUCCESS;
        switch (proptag) {
        case PR_DISPLAY_NAME_UNICODE:
                child = [self _lookupObject: childURL];
                stringData = [child displayName];
                *data = talloc_strdup(memCtx,
                                      [stringData UTF8String]);
                break;
        default:
                // if ((proptag & 0x001F) == 0x001F) {
                //         stringValue = [NSString stringWithFormat: @"Unhandled unicode value: 0x%x", proptag];
                //         *data = talloc_strdup(memCtx, [stringValue UTF8String]);
                //         break;
                // }
                // else
                [self errorWithFormat: @"Unknown proptag: %.8x for child '%@'",
                      proptag, childURL];
                *data = NULL;
                rc = MAPI_E_NOT_FOUND;
        }

        return rc;
}

- (int) getMessageTableChildproperty: (void **) data
                               atURL: (NSString *) childURL
                             withTag: (uint32_t) proptag
                            inFolder: (SOGoFolder *) folder
                             withFID: (uint64_t) fid
{
        uint32_t *longValue;
        uint64_t *llongValue;
        int rc;

        rc = MAPI_E_SUCCESS;
        switch (proptag) {
        case PR_INST_ID: // TODO: DOUBT
                llongValue = talloc_zero(memCtx, uint64_t);
                // *llongValue = 1;
                *llongValue = [childURL hash]; /* we return a unique id based on the url */
                *data = llongValue;
                break;
                // case PR_INST_ID: // TODO: DOUBT
        case PR_INSTANCE_NUM: // TODO: DOUBT
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 0;
                *data = longValue;
                break;
        case PR_VD_VERSION:
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 8; /* mandatory value... wtf? */
                *data = longValue;
                break;
        // case PR_DEPTH: // TODO: DOUBT
        //         longValue = talloc_zero(memCtx, uint32_t);
        //         *longValue = 1;
        //         *data = longValue;
        //         break;
        case PR_FID:
                llongValue = talloc_zero(memCtx, uint64_t);
                *llongValue = fid;
                *data = llongValue;
        case PR_MID:
                llongValue = talloc_zero(memCtx, uint64_t);
                *llongValue = [mapping idFromURL: childURL];
                if (*llongValue == NSNotFound) {
                        [mapping registerURL: childURL];
                        *llongValue = [mapping idFromURL: childURL];
                }
                *data = llongValue;
                break;

                /* those are queried while they really pertain to the
                   addressbook module */
// #define PR_OAB_LANGID                                       PROP_TAG(PT_LONG      , 0x6807) /* 0x68070003 */
                // case PR_OAB_NAME_UNICODE:
                // case PR_OAB_CONTAINER_GUID_UNICODE:

// 0x68420102  PidTagScheduleInfoDelegatorWantsCopy (BOOL)


        default:
                rc = [self getCommonTableChildproperty: data
                                                 atURL: childURL
                                               withTag: proptag
                                              inFolder: folder
                                               withFID: fid];
        }

        return rc;
}

- (NSString *) _parentURLFromURL: (NSString *) urlString
{
        NSString *newURL;
        NSArray *parts;
        NSMutableArray *newParts;

        parts = [urlString componentsSeparatedByString: @"/"];
        if ([parts count] > 3) {
                newParts = [parts mutableCopy];
                [newParts autorelease];
                [newParts removeLastObject];
                newURL = [newParts componentsJoinedByString: @"/"];
        }
        else
                newURL = nil;

        return newURL;
}

- (int) getFolderTableChildproperty: (void **) data
                              atURL: (NSString *) childURL
                            withTag: (uint32_t) proptag
                           inFolder: (SOGoFolder *) folder
                            withFID: (uint64_t) fid
{
        // id child;
        uint64_t *llongValue;
        uint8_t *boolValue;
        uint32_t *longValue;
        struct Binary_r *binaryValue;
        int rc;
        NSString *parentURL;

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
        case PR_PARENT_FID:
                llongValue = talloc_zero(memCtx, uint64_t);
                parentURL = [self _parentURLFromURL: childURL];
                if (parentURL) {
                        *llongValue = [mapping idFromURL: childURL];
                        if (*llongValue == NSNotFound) {
                                [mapping registerURL: childURL];
                                *llongValue = [mapping idFromURL: childURL];
                        }
                        *data = llongValue;
                }
                else {
                        *data = NULL;
                        rc = MAPISTORE_ERR_NOT_FOUND;
                }
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
                *boolValue = ([[self _subfolderKeysForFolderURL: childURL]
                                      count] > 0);
                *data = boolValue;
                break;
        case PR_CONTENT_COUNT:
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = ([[self _messageKeysForFolderURL: childURL]
                                      count] > 0);
                *data = longValue;
                break;
        case PR_EXTENDED_FOLDER_FLAGS: // TODO: DOUBT: how to indicate the
                                       // number of subresponses ?
                binaryValue = talloc_zero(memCtx, struct Binary_r);
                *data = binaryValue;
                break;
        default:
                rc = [self getCommonTableChildproperty: data
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

                        if (tableType == MAPISTORE_FOLDER_TABLE) {
                                [self logWithFormat: @"  querying child folder at URL: %@", childURL];
                                rc = [self getFolderTableChildproperty: data
                                                                 atURL: childURL
                                                               withTag: proptag
                                                              inFolder: folder
                                                               withFID: fid];
                        }
                        else {
                                [self logWithFormat: @"  querying child message at URL: %@", childURL];
                                rc = [self getMessageTableChildproperty: data
                                                                  atURL: childURL
                                                                withTag: proptag
                                                               inFolder: folder
                                                                withFID: fid];
                        }
                        /* Unhandled: */
// #define PR_EXPIRY_TIME                                      PROP_TAG(PT_SYSTIME   , 0x0015) /* 0x00150040 */
// #define PR_REPLY_TIME                                       PROP_TAG(PT_SYSTIME   , 0x0030) /* 0x00300040 */
// #define PR_SENSITIVITY                                      PROP_TAG(PT_LONG      , 0x0036) /* 0x00360003 */
// #define PR_MESSAGE_DELIVERY_TIME                            PROP_TAG(PT_SYSTIME   , 0x0e06) /* 0x0e060040 */
// #define PR_FOLLOWUP_ICON                                    PROP_TAG(PT_LONG      , 0x1095) /* 0x10950003 */
// #define PR_ITEM_TEMPORARY_FLAGS                             PROP_TAG(PT_LONG      , 0x1097) /* 0x10970003 */
// #define PR_SEARCH_KEY                                       PROP_TAG(PT_BINARY    , 0x300b) /* 0x300b0102 */
// #define PR_CONTENT_COUNT                                    PROP_TAG(PT_LONG      , 0x3602) /* 0x36020003 */
// #define PR_CONTENT_UNREAD                                   PROP_TAG(PT_LONG      , 0x3603) /* 0x36030003 */
// #define PR_FID                                              PROP_TAG(PT_I8        , 0x6748) /* 0x67480014 */
// unknown 36de0003 http://social.msdn.microsoft.com/Forums/en-US/os_exchangeprotocols/thread/17c68add-1f62-4b68-9d83-f9ec7c1c6c9b
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

#import <Mailer/SOGoMailFolder.h>
#import <Mailer/SOGoMailObject.h>

@implementation MAPIStoreMailContext

- (void) setupModuleFolder
{
        id userFolder, accountsFolder;

        userFolder = [SOGoUserFolderK objectWithName: [authenticator username]
                                         inContainer: MAPIApp];
        [woContext setClientObject: userFolder];
        [userFolder retain]; // LEAK

        accountsFolder = [userFolder lookupName: @"Mail"
                                      inContext: woContext
                                        acquire: NO];
        [woContext setClientObject: accountsFolder];
        [accountsFolder retain]; // LEAK

        moduleFolder = [accountsFolder lookupName: @"0"
                                        inContext: woContext
                                          acquire: NO];
        [moduleFolder retain];
}

// - (int) getCommonTableChildproperty: (void **) data
//                               atURL: (NSString *) childURL
//                             withTag: (uint32_t) proptag
//                            inFolder: (SOGoFolder *) folder
//                             withFID: (uint64_t) fid
// {
//         int rc;

//         rc = MAPI_E_SUCCESS;
//         switch (proptag) {
//         default:
//                 rc = [super getCommonTableChildproperty: data
//                                                   atURL: childURL
//                                                 withTag: proptag
//                                                inFolder: folder
//                                                 withFID: fid];
//         }

//         return rc;
// }

- (int) getMessageTableChildproperty: (void **) data
                               atURL: (NSString *) childURL
                             withTag: (uint32_t) proptag
                            inFolder: (SOGoFolder *) folder
                             withFID: (uint64_t) fid
{
        NSCalendarDate *refDate;
        uint64_t timeCompValue;
        struct FILETIME *timeValue;

        uint8_t *boolValue;
        uint32_t *longValue;
        NSString *stringData;
        SOGoMailObject *child;
        int rc;

        rc = MAPI_E_SUCCESS;
        switch (proptag) {
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
        case PR_HASATTACH:
                boolValue = talloc_zero(memCtx, uint8_t);
                *boolValue = NO;
                *data = boolValue;
                break;
        case PR_MESSAGE_DELIVERY_TIME:
                // llongValue = talloc_zero(memCtx, int64_t);
                // child = [self _lookupObject: childURL];
                
                // refDate = [NSCalendarDate dateWithYear: 1601 month: 1 day: 1
                //                                   hour: 0 minute: 0 second: 0
                //                               timeZone: [NSTimeZone timeZoneWithName: @"UTC"]];
                // *llongValue = (uint64_t) [[child date] timeIntervalSinceDate:
                //                                   refDate];

                child = [self _lookupObject: childURL];
                refDate = [NSCalendarDate dateWithYear: 1601 month: 1 day: 1
                                                  hour: 0 minute: 0 second: 0
                                              timeZone: [NSTimeZone timeZoneWithName: @"UTC"]];
                timeCompValue = (((uint64_t) [[child date] timeIntervalSinceDate: refDate] + (86400 * -365))
                                 * 10000000);
                // timeCompValue = ((uint64_t) 86400 * 31 );
                timeValue = talloc_zero(memCtx, struct FILETIME);
                timeValue->dwLowDateTime = (uint32_t) (timeCompValue & 0xffffffff);
                timeValue->dwHighDateTime = (uint32_t) ((timeCompValue >> 32) & 0xffffffff);
                NSLog (@"%@ -> %lld", [child date], timeCompValue);
                NSLog (@"time conversion: %llx  -> %lx, %lx", timeCompValue,
                       timeValue->dwHighDateTime, timeValue->dwLowDateTime);
                *data = timeValue;
                break;
        // case PR_ROW_TYPE: // TODO: DOUBT
        //         longValue = talloc_zero(memCtx, uint32_t);
        //         *longValue = 1;
        //         *data = longValue;
        //         break;
        case PR_FLAG_STATUS: // TODO
        case PR_MSG_STATUS: // TODO
        case PR_MESSAGE_FLAGS: // TODO
        case PR_SENSITIVITY: // TODO
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 0;
                *data = longValue;
                break;
        case PR_IMPORTANCE:
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 1;
                *data = longValue;
                break;
        case PR_MESSAGE_SIZE: // TODO
                child = [self _lookupObject: childURL];
                longValue = talloc_zero(memCtx, uint32_t);
                /* TODO: choose another name for that method */
                *longValue = [[child davContentLength] intValue];
                *data = longValue;
                break;
// #define PR_REPLY_TIME                                       PROP_TAG(PT_SYSTIME   , 0x0030) /* 0x00300040 */
// #define PR_EXPIRY_TIME                                      PROP_TAG(PT_SYSTIME   , 0x0015) /* 0x00150040 */
// #define PR_MESSAGE_DELIVERY_TIME                            PROP_TAG(PT_SYSTIME   , 0x0e06) /* 0x0e060040 */
// #define PR_FOLLOWUP_ICON                                    PROP_TAG(PT_LONG      , 0x1095) /* 0x10950003 */
// #define PR_ITEM_TEMPORARY_FLAGS                             PROP_TAG(PT_LONG      , 0x1097) /* 0x10970003 */
// #define PR_SEARCH_KEY                                       PROP_TAG(PT_BINARY    , 0x300b) /* 0x300b0102 */



        case PR_SENT_REPRESENTING_NAME_UNICODE:
                child = [self _lookupObject: childURL];
                stringData = [child from];
                *data = talloc_strdup(memCtx, [stringData UTF8String]);
                break;
        case PR_INTERNET_MESSAGE_ID_UNICODE:
                child = [self _lookupObject: childURL];
                stringData = [child messageId];
                *data = talloc_strdup(memCtx, [stringData UTF8String]);
                break;
        default:
                rc = [super getMessageTableChildproperty: data
                                                   atURL: childURL
                                                 withTag: proptag
                                                inFolder: folder
                                                 withFID: fid];
        }
        
        return rc;
}

- (int) getFolderTableChildproperty: (void **) data
                              atURL: (NSString *) childURL
                            withTag: (uint32_t) proptag
                           inFolder: (SOGoFolder *) folder
                            withFID: (uint64_t) fid
{
        uint32_t *longValue;
        int rc;

        rc = MAPI_E_SUCCESS;
        switch (proptag) {
        case PR_CONTENT_UNREAD:
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 0;
                *data = longValue;
                break;
        case PR_CONTAINER_CLASS_UNICODE:
                *data = talloc_strdup(memCtx, "IPF.Note");
                break;
        default:
                rc = [super getFolderTableChildproperty: data
                                          atURL: childURL
                                                withTag: proptag
                                               inFolder: folder
                                                withFID: fid];
        }
        
        return rc;
}

@end

#import <Contacts/SOGoContactObject.h>
#import <NGCards/NGVCard.h>

@implementation MAPIStoreContactsContext

- (void) setupModuleFolder
{
        id userFolder;

        userFolder = [SOGoUserFolderK objectWithName: [authenticator username]
                                         inContainer: MAPIApp];
        [woContext setClientObject: userFolder];
        [userFolder retain]; // LEAK

        moduleFolder = [userFolder lookupName: @"Contacts"
                                    inContext: woContext
                                      acquire: NO];
        [moduleFolder retain];
}

// - (int) getCommonTableChildproperty: (void **) data
//                               atURL: (NSString *) childURL
//                             withTag: (uint32_t) proptag
//                            inFolder: (SOGoFolder *) folder
//                             withFID: (uint64_t) fid
// {
//         int rc;

//         rc = MAPI_E_SUCCESS;
//         switch (proptag) {
//         default:
//                 rc = [super getCommonTableChildproperty: data
//                                                   atURL: childURL
//                                                 withTag: proptag
//                                                inFolder: folder
//                                                 withFID: fid];
//         }

//         return rc;
// }

- (NSString *) _phoneOfType: (NSString *) aType
                  excluding: (NSString *) aTypeToExclude
                     inCard: (NGVCard *) card
{
  NSArray *elements;
  NSArray *phones;
  NSString *phone;

  phones = [card childrenWithTag: @"tel"];

  elements = [phones cardElementsWithAttribute: @"type"
                                   havingValue: aType];

  phone = nil;

  if ([elements count] > 0)
    {
      CardElement *ce;
      int i;

      for (i = 0; i < [elements count]; i++)
	{
	  ce = [elements objectAtIndex: i];
	  phone = [ce value: 0];

	  if (!aTypeToExclude)
	    break;
	  
	  if (![ce hasAttribute: @"type" havingValue: aTypeToExclude])
	    break;

	  phone = nil;
	}
    }

  if (!phone)
          phone = @"";

  return phone;
}

- (int) getMessageTableChildproperty: (void **) data
                               atURL: (NSString *) childURL
                             withTag: (uint32_t) proptag
                            inFolder: (SOGoFolder *) folder
                             withFID: (uint64_t) fid
{
        NSString *stringValue;
        uint32_t *longValue;
        id child;
        int rc;

        rc = MAPI_E_SUCCESS;
        switch (proptag) {
        case PR_ICON_INDEX: // TODO
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 0x00000200; /* see http://msdn.microsoft.com/en-us/library/cc815472.aspx */
                *data = longValue;
                break;
        case PR_MESSAGE_CLASS_UNICODE:
                *data = talloc_strdup(memCtx, "IPM.Contact");
                break;
        // case PR_VD_NAME_UNICODE:
        //         *data = talloc_strdup(memCtx, "PR_VD_NAME_UNICODE");
        //         break;
        // case PR_EMS_AB_DXA_REMOTE_CLIENT_UNICODE: "Home:" ???
        //         *data = talloc_strdup(memCtx, "PR_EMS...");
        //         break;
        case PR_SUBJECT_UNICODE:
                *data = talloc_strdup(memCtx, "PR_SUBJECT...");
                break;
        case PR_OAB_NAME_UNICODE:
                *data = talloc_strdup(memCtx, "PR_OAB_NAME_UNICODE");
                break;
        case PR_OAB_LANGID:
                longValue = talloc_zero(memCtx, uint32_t);
                *longValue = 1033; /* English US */
                *data = longValue;
                break;

        case PR_TITLE_UNICODE:
                child = [self _lookupObject: childURL];
                stringValue = [[child vCard] title];
                *data = talloc_strdup(memCtx, [stringValue UTF8String]);
                break;

        case PR_COMPANY_NAME_UNICODE:
                child = [self _lookupObject: childURL];
                /* that's buggy but it's for the demo */
                stringValue = [[[child vCard] org] componentsJoinedByString: @", "];
                *data = talloc_strdup(memCtx, [stringValue UTF8String]);
                break;

        case 0x3001001f: // Full Name
        case 0x81c2001f: // contact block title name
                rc = [super getMessageTableChildproperty: data
                                                   atURL: childURL
                                                 withTag: PR_DISPLAY_NAME_UNICODE
                                                inFolder: folder
                                                 withFID: fid];
                break;
        case 0x81b0001f: // E-mail
                child = [self _lookupObject: childURL];
                stringValue = [[child vCard] preferredEMail];
                *data = talloc_strdup(memCtx, [stringValue UTF8String]);
                break;
        case PR_BODY_UNICODE:
                child = [self _lookupObject: childURL];
                stringValue = [[child vCard] note];
                *data = talloc_strdup(memCtx, [stringValue UTF8String]);
                break;

        case PR_OFFICE_TELEPHONE_NUMBER_UNICODE:
                child = [self _lookupObject: childURL];
                stringValue = [self _phoneOfType: @"work"
                                       excluding: @"fax"
                                          inCard: [child vCard]];
                *data = talloc_strdup(memCtx, [stringValue UTF8String]);
                break;
        case PR_HOME_TELEPHONE_NUMBER_UNICODE:
                child = [self _lookupObject: childURL];
                stringValue = [self _phoneOfType: @"home"
                                       excluding: @"fax"
                                          inCard: [child vCard]];
                *data = talloc_strdup(memCtx, [stringValue UTF8String]);
                break;
        case PR_MOBILE_TELEPHONE_NUMBER_UNICODE:
                child = [self _lookupObject: childURL];
                stringValue = [self _phoneOfType: @"cell"
                                       excluding: nil
                                          inCard: [child vCard]];
                *data = talloc_strdup(memCtx, [stringValue UTF8String]);
                break;
        case PR_PRIMARY_TELEPHONE_NUMBER_UNICODE:
                child = [self _lookupObject: childURL];
                stringValue = [self _phoneOfType: @"pref"
                                       excluding: nil
                                          inCard: [child vCard]];
                *data = talloc_strdup(memCtx, [stringValue UTF8String]);
                break;

        // case PR_POSTAL_ADDRESS_UNICODE:
        // case PR_INTERNET_MESSAGE_ID_UNICODE:
        // case PR_CAR_TELEPHONE_NUMBER_UNICODE:
        // case PR_OTHER_TELEPHONE_NUMBER_UNICODE:
        // case PR_BUSINESS_FAX_NUMBER_UNICODE:
        // case PR_HOME_FAX_NUMBER_UNICODE:
        // case PR_COMPANY_MAIN_PHONE_NUMBER_UNICODE:
        // case PR_EMS_AB_GROUP_BY_ATTR_4_UNICODE:
        // case PR_CALLBACK_TELEPHONE_NUMBER_UNICODE:
        // case PR_DEPARTMENT_NAME_UNICODE:
        // case PR_OFFICE2_TELEPHONE_NUMBER_UNICODE:
        // case PR_RADIO_TELEPHONE_NUMBER_UNICODE:
        // case PR_PAGER_TELEPHONE_NUMBER_UNICODE:
        // case PR_PRIMARY_FAX_NUMBER_UNICODE:
        // case PR_TELEX_NUMBER_UNICODE:
        // case PR_ISDN_NUMBER_UNICODE:
        // case PR_ASSISTANT_TELEPHONE_NUMBER_UNICODE:
        // case PR_HOME2_TELEPHONE_NUMBER_UNICODE:
        // case PR_TTYTDD_PHONE_NUMBER_UNICODE:
        // case PR_BUSINESS_HOME_PAGE_UNICODE:
        //         *data = talloc_strdup(memCtx, "[Generic and fake unicode value]");
        //         break;

// (18:54:45) Wolfgang-: 0x80a7001f (  Business: ) -> don't ask me which "business"
// (18:55:05) Wolfgang-: 0x809c001f   (  Other: )
// (18:55:58) Wolfgang-: 0x81b5001f: E-mail  2


// #define PR_REPLY_TIME                                       PROP_TAG(PT_SYSTIME   , 0x0030) /* 0x00300040 */
// #define PR_SENSITIVITY                                      PROP_TAG(PT_LONG      , 0x0036) /* 0x00360003 */
// #define PR_FOLLOWUP_ICON                                    PROP_TAG(PT_LONG      , 0x1095) /* 0x10950003 */
// #define PR_SEARCH_KEY                                       PROP_TAG(PT_BINARY    , 0x300b) /* 0x300b0102 */
// #define PR_VIEW_STYLE                                       PROP_TAG(PT_LONG      , 0x6834) /* 0x68340003 */
// #define PR_VD_VERSION                                       PROP_TAG(PT_LONG      , 0x7007) /* 0x70070003 */

        default:
                rc = [super getMessageTableChildproperty: data
                                                   atURL: childURL
                                                 withTag: proptag
                                                inFolder: folder
                                                 withFID: fid];
        }
        
        return rc;
}

- (int) getFolderTableChildproperty: (void **) data
                              atURL: (NSString *) childURL
                            withTag: (uint32_t) proptag
                           inFolder: (SOGoFolder *) folder
                            withFID: (uint64_t) fid
{
        int rc;

        [self logWithFormat: @"XXXXX unexpected!!!!!!!!!"];
        rc = MAPI_E_SUCCESS;
        switch (proptag) {
        default:
                rc = [super getFolderTableChildproperty: data
                                          atURL: childURL
                                                withTag: proptag
                                               inFolder: folder
                                                withFID: fid];
        }
        
        return rc;
}

@end
