/*
   libmapiserver - MAPI library for Server side

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
   \file libmapiserver_oxcnotif.c

   \brief OXCNOTIF ROP Response size calculations
 */


#include "libmapiserver.h"
#include <util/debug.h>

/**
   \details Calculate RegisterNotification Rop size

   \return Size of RegisterNotification response
 */
_PUBLIC_ uint16_t libmapiserver_RopRegisterNotification_size(void)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}

/**
   \details Calculate Notify Rop size

   \return Size of Notify response
 */
_PUBLIC_ uint16_t libmapiserver_RopNotify_size(struct EcDoRpc_MAPI_REPL *response)
{
        uint16_t size = SIZE_DFLT_ROPNOTIFY;
        union NotificationData *NotificationData;

        /* TODO: to be completed... */

        NotificationData = &response->u.mapi_Notify.NotificationData;
        switch (response->u.mapi_Notify.NotificationType) {
                /* Folders */
        case 0x3010: /* different forms of folder modifications */
                size += sizeof (int64_t) + sizeof (uint32_t) * 2 + sizeof (uint16_t);
                if (NotificationData->FolderModifiedNotification_3010.TagCount != 0xFFFF) {
			size += sizeof(enum MAPITAGS) * NotificationData->FolderModifiedNotification_3010.TagCount;
                }
		break;
        case 0x1010:
		size += sizeof (int64_t) + sizeof (uint16_t) + sizeof(uint32_t);
		if (NotificationData->FolderModifiedNotification_1010.TagCount != 0xFFFF) {
			size += sizeof(enum MAPITAGS) * NotificationData->FolderModifiedNotification_1010.TagCount;
		}
		break;
        case 0x2010:
		size += sizeof (int64_t) + sizeof (uint16_t) + sizeof(uint32_t);
		if (NotificationData->FolderModifiedNotification_2010.TagCount != 0xFFFF) {
			size += sizeof(enum MAPITAGS) * NotificationData->FolderModifiedNotification_2010.TagCount;
		}
		break;
        case 0x0010: /* folder modified */
                size += sizeof (int64_t) + sizeof (uint16_t);
                if (NotificationData->FolderModifiedNotification_10.TagCount != 0xffff) {
			size += sizeof(enum MAPITAGS) * NotificationData->FolderModifiedNotification_10.TagCount;
                }
                break;
        case 0x0004: /* folder created */
		size += sizeof (int64_t) * 2 + sizeof (uint16_t);
		if (NotificationData->FolderModifiedNotification_10.TagCount != 0xffff) {
			size += sizeof(enum MAPITAGS) * NotificationData->FolderCreatedNotification.TagCount;
		}
		break;
	case 0x0002: /* newmail */
	case 0x8002:
		size += sizeof (int64_t) * 2 + sizeof (uint32_t) + sizeof (uint8_t);
		if (NotificationData->NewMailNotification.UnicodeFlag == false) {
			size += strlen(NotificationData->NewMailNotification.MessageClass.lpszA) + 1;
		} else {
			size += strlen(NotificationData->NewMailNotification.MessageClass.lpszW) * 2 + 2;
		}
		break;
        case 0x8004: /* message created */
		size += sizeof (int64_t) * 2 + sizeof(uint16_t);
		if (NotificationData->MessageCreatedNotification.TagCount != 0xffff) {
		        size += sizeof(enum MAPITAGS) * NotificationData->MessageCreatedNotification.TagCount;
		}
		break;
        case 0x8010: /* message modified */
                size += sizeof (int64_t) * 2 + sizeof(uint16_t);
                if (NotificationData->MessageCreatedNotification.TagCount != 0xffff) {
                        size += sizeof(enum MAPITAGS) * NotificationData->MessageModifiedNotification.TagCount;
                }
		break;
        case 0x8008: /* message deleted */
        case 0x0008: /* folder deleted */
                size += 2 * sizeof(int64_t);
                break;

                /* Tables */
        case 0x0100: /* hierarchy table changed */
                size += sizeof(uint16_t); /* TableEventType */
                switch (NotificationData->HierarchyTableChange.TableEvent) {
                case TABLE_ROW_ADDED:
                        size += 2 * sizeof(int64_t); /* FID and InsertAfterFID */
                        size += sizeof(uint16_t) + NotificationData->HierarchyTableChange.HierarchyTableChangeUnion.HierarchyRowAddedNotification.Columns.length; /* blob length */
                        break;
                case TABLE_ROW_DELETED:
                        size += sizeof(int64_t); /* FID */
                        break;
                case TABLE_ROW_MODIFIED:
                        size += 2 * sizeof(int64_t); /* FID and InsertAfterFID */
                        size += sizeof(uint16_t) + NotificationData->HierarchyTableChange.HierarchyTableChangeUnion.HierarchyRowModifiedNotification.Columns.length; /* blob length */
                        break;
                default: /* TABLE_CHANGED and TABLE_RESTRICT_DONE */
                        size += 0;
                }
                break;
        case 0x8100: /* contents table changed */
                size += sizeof(uint16_t); /* TableEventType */
                switch (NotificationData->ContentsTableChange.TableEvent) {
                case TABLE_ROW_ADDED:
                        size += 2 * (2 * sizeof(int64_t) + sizeof(uint32_t)); /* FID, MID, Instance, InsertAfterFID, InsertAfterMID, InsertAfterInstance */
                        size += sizeof(uint16_t) + NotificationData->ContentsTableChange.ContentsTableChangeUnion.ContentsRowAddedNotification.Columns.length; /* blob length */
                        break;
                case TABLE_ROW_DELETED:
                        size += 2 * sizeof(int64_t) + sizeof(uint32_t); /* FID, MID, Instance */
                        break;
                case TABLE_ROW_MODIFIED:
                        size += 2 * (2 * sizeof(int64_t) + sizeof(uint32_t)); /* FID, MID, Instance, InsertAfterFID, InsertAfterMID, InsertAfterInstance */
                        size += sizeof(uint16_t) + NotificationData->ContentsTableChange.ContentsTableChangeUnion.ContentsRowModifiedNotification.Columns.length;
                        break;
                default: /* TABLE_CHANGED and TABLE_RESTRICT_DONE */
                        size += 0;
                }
                break;
        case 0xc100: /* search table changed */
                size += sizeof(uint16_t); /* TableEventType */
                switch (NotificationData->SearchTableChange.TableEvent) {
                case TABLE_ROW_ADDED:
                        size += 2 * (2 * sizeof(int64_t) + sizeof(uint32_t)); /* FID, MID, Instance, InsertAfterFID, InsertAfterMID, InsertAfterInstance */
                        size += sizeof(uint16_t) + NotificationData->SearchTableChange.ContentsTableChangeUnion.ContentsRowAddedNotification.Columns.length; /* blob length */
                        break;
                case TABLE_ROW_DELETED:
                        size += 2 * sizeof(int64_t) + sizeof(uint32_t); /* FID, MID, Instance */
                        break;
                case TABLE_ROW_MODIFIED:
                        size += 2 * (2 * sizeof(int64_t) + sizeof(uint32_t)); /* FID, MID, Instance, InsertAfterFID, InsertAfterMID, InsertAfterInstance */
                        size += sizeof(uint16_t) + NotificationData->SearchTableChange.ContentsTableChangeUnion.ContentsRowModifiedNotification.Columns.length;
                        break;
                default: /* TABLE_CHANGED and TABLE_RESTRICT_DONE */
                        size += 0;
                }
                break;

        /* case 0x0002: */
        /* case 0x8002: */
        /*         size += sizeof(struct NewMailNotification); */
        /*         if (response->u.mapi_Notify.NewMailNotification.UnicodeFlag) { */
        /*                 size += strlen_m_ext(response->u.mapi_Notify.NewMailNotification.MessageClass.lpszW, CH_UTF8, CH_UTF16BE) * 2 + 2; */
        /*         } */
        /*         else { */
        /*                 size += strlen(response->u.mapi_Notify.NewMailNotification.MessageClass.lpszA) + 1; */
        /*         } */
        /*         break; */
        /* case 0x0020: */
        /* case 0x0040: */
        /*         size += sizeof(struct FolderMoveCopyNotification); */
        /*         break; */
        /* case 0x0080: */
        /*         size += sizeof(struct SearchCompleteNotification); */
        /*         break; */
        /* case 0x0100: */
        /*         size += sizeof(struct HierarchyTableChange); */
        /*         break; */
        /* case 0x0200: */
        /*         size += sizeof(struct IcsNotification); */
        /*         break; */
        /* case 0x1010: */
        /*         size += sizeof(struct FolderModifiedNotification_1010); */
        /*         break; */
        /* case 0x2010: */
        /*         size += sizeof(struct FolderModifiedNotification_2010); */
        /*         break; */
        /* case 0x3010: */
        /*         size += sizeof(struct FolderModifiedNotification_3010); */
        /*         break; */
        case 0x8020:
        case 0x8040:
                 size += sizeof (int64_t) * 4;
                 break; 
        /* case 0x8100: */
        /* case 0xc100: */
        /*         size += sizeof(struct ContentsTableChange); */
        /*         break; */
        /* case 0xc004: */
        /*         size += sizeof(struct SearchMessageCreatedNotification); */
        /*         break; */
        /* case 0xc008: */
        /*         size += sizeof(struct SearchMessageRemovedNotification); */
        /*         break; */
        /* case 0xc010: */
        /*         size += sizeof(struct SearchMessageModifiedNotification); */
        /*         break; */
        default:
                DEBUG(0, (__location__": unhandled size case %.4x, expect buffer errors soon\n", response->u.mapi_Notify.NotificationType));
        }

        return size;
}
