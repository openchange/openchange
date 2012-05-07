"""This module defines the minimum set of SOAP types required for providing
the Exchange Web Services. The definitions here are loosely based on the wsdl
files provided by the Exchange doc.

"""

from rpclib.model.complex import *
from rpclib.model.primitive import *

#aliases
Short = Integer16

EWS_T_NS = "http://schemas.microsoft.com/exchange/services/2006/types" # t
EWS_M_NS = "http://schemas.microsoft.com/exchange/services/2006/messages" # m


"""
  <xs:simpleType name="DayOfWeekType">
    <xs:restriction base="xs:string">
      <xs:enumeration value="Sunday"/>
      <xs:enumeration value="Monday"/>
      <xs:enumeration value="Tuesday"/>
      <xs:enumeration value="Wednesday"/>
      <xs:enumeration value="Thursday"/>
      <xs:enumeration value="Friday"/>
      <xs:enumeration value="Saturday"/>
      <xs:enumeration value="Day"/>
      <xs:enumeration value="Weekday"/>
      <xs:enumeration value="WeekendDay"/>
    </xs:restriction>
  </xs:simpleType>

"""
DayOfWeekType = String


"""
  <xs:complexType name="SerializableTimeZoneTime">
    <xs:sequence>
      <xs:element minOccurs="1" maxOccurs="1" name="Bias" type="xs:int"/>
      <xs:element minOccurs="1" maxOccurs="1" name="Time" type="xs:string"/>
      <xs:element minOccurs="1" maxOccurs="1" name="DayOrder" type="xs:short"/>
      <xs:element minOccurs="1" maxOccurs="1" name="Month" type="xs:short"/>
      <xs:element minOccurs="1" maxOccurs="1" name="DayOfWeek" type="t:DayOfWeekType"/>
    </xs:sequence>
  </xs:complexType>
"""
class SerializableTimeZoneTime(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"Bias": Integer,
                  "Time": String,
                  "DayOrder": Short,
                  "Month": Short,
                  "DayOfWeek": DayOfWeekType}


"""
  <xs:complexType name="SerializableTimeZone">
    <xs:sequence>
      <xs:element minOccurs="1" maxOccurs="1" name="Bias" type="xs:int"/>
      <xs:element minOccurs="1" maxOccurs="1" name="StandardTime" type="t:SerializableTimeZoneTime"/>
      <xs:element minOccurs="1" maxOccurs="1" name="DaylightTime" type="t:SerializableTimeZoneTime"/>
    </xs:sequence>
  </xs:complexType>
"""
class SerializableTimeZone(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"Bias": Integer,
                  "StandardTime": SerializableTimeZoneTime,
                  "DaylightTime": SerializableTimeZoneTime}


"""
  <xs:complexType name="EmailAddress">
    <xs:sequence>
      <xs:element minOccurs="0" maxOccurs="1" name="Name" type="xs:string"/>
      <xs:element minOccurs="1" maxOccurs="1" name="Address" type="xs:string"/>
      <xs:element minOccurs="0" maxOccurs="1" name="RoutingType" type="xs:string"/>
    </xs:sequence>
  </xs:complexType>
"""
class EmailAddress(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"Name": String,
                  "Address": String,
                  "RoutingType": String}


"""
  <xs:simpleType name="MeetingAttendeeType">
    <xs:restriction base="xs:string">
      <xs:enumeration value="Organizer"/>
      <xs:enumeration value="Required"/>
      <xs:enumeration value="Optional"/>
      <xs:enumeration value="Room"/>
      <xs:enumeration value="Resource"/>
    </xs:restriction>
  </xs:simpleType>

"""
MeetingAttendeeType = String


"""
  <xs:complexType name="MailboxData">
    <xs:sequence>
      <xs:element minOccurs="1" maxOccurs="1" name="Email" type="t:EmailAddress"/>
      <xs:element minOccurs="1" maxOccurs="1" name="AttendeeType" type="t:MeetingAttendeeType"/>
      <xs:element minOccurs="0" maxOccurs="1" name="ExcludeConflicts" type="xs:boolean"/>
    </xs:sequence>
  </xs:complexType>
"""
class MailboxData(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"Email": EmailAddress,
                  "AttendeeType": MeetingAttendeeType,
                  "ExcludeConflicts": Boolean}


"""
  ArrayOfMailboxData
  <xs:complexType name="ArrayOfMailboxData">
    <xs:sequence>
      <xs:element minOccurs="0" maxOccurs="unbounded" name="MailboxData" nillable="true" type="t:MailboxData"/>
    </xs:sequence>
  </xs:complexType>
"""
ArrayOfMailboxData = Array(MailboxData)

class Duration(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"StartTime": DateTime,
                  "EndTime": DateTime}

"""
  <xs:simpleType name="FreeBusyViewType">
    <xs:list>
      <xs:simpleType>
        <xs:restriction base="xs:string">
          <xs:enumeration value="None"/>
          <xs:enumeration value="MergedOnly"/>
          <xs:enumeration value="FreeBusy"/>
          <xs:enumeration value="FreeBusyMerged"/>
          <xs:enumeration value="Detailed"/>
          <xs:enumeration value="DetailedMerged"/>
        </xs:restriction>
      </xs:simpleType>
    </xs:list>
  </xs:simpleType>

"""
FreeBusyViewType = String


"""
  FreeBusyViewOptions
  <xs:complexType name="FreeBusyViewOptionsType">
    <xs:sequence>
      <xs:element minOccurs="1" maxOccurs="1" name="TimeWindow" type="t:Duration"/>
      <xs:element minOccurs="0" maxOccurs="1" name="MergedFreeBusyIntervalInMinutes" type="xs:int"/>
      <xs:element minOccurs="0" maxOccurs="1" name="RequestedView" type="t:FreeBusyViewType"/>
    </xs:sequence>
  </xs:complexType>
  <xs:element name="FreeBusyViewOptions" type="t:FreeBusyViewOptionsType"/>
"""
class FreeBusyViewOptionsType(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"TimeWindow": Duration,
                  "MergedFreeBusyIntervalInMinutes": Integer,
                  "RequestedView": FreeBusyViewType}


"""
  <xs:simpleType name="SuggestionQuality">
    <xs:restriction base="xs:string">
      <xs:enumeration value="Excellent"/>
      <xs:enumeration value="Good"/>
      <xs:enumeration value="Fair"/>
      <xs:enumeration value="Poor"/>
    </xs:restriction>
  </xs:simpleType>

"""
SuggestionQuality = String


"""
  <xs:complexType name="SuggestionsViewOptionsType">
    <xs:sequence>
      <xs:element minOccurs="0" maxOccurs="1" name="GoodThreshold" type="xs:int"/>
      <xs:element minOccurs="0" maxOccurs="1" name="MaximumResultsByDay" type="xs:int"/>
      <xs:element minOccurs="0" maxOccurs="1" name="MaximumNonWorkHourResultsByDay" type="xs:int"/>
      <xs:element minOccurs="0" maxOccurs="1" name="MeetingDurationInMinutes" type="xs:int"/>
      <xs:element minOccurs="0" maxOccurs="1" name="MinimumSuggestionQuality" type="t:SuggestionQuality"/>
      <xs:element minOccurs="1" maxOccurs="1" name="DetailedSuggestionsWindow" type="t:Duration"/>
      <xs:element minOccurs="0" maxOccurs="1" name="CurrentMeetingTime" type="xs:dateTime"/>
      <xs:element minOccurs="0" maxOccurs="1" name="GlobalObjectId" type="xs:string"/>
    </xs:sequence>
  </xs:complexType>
  <xs:element name="SuggestionsViewOptions" type="t:SuggestionsViewOptionsType"/>

"""
class SuggestionsViewOptionsType(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"GoodThreshold": Integer,
                  "MaximumResultsByDay": Integer,
                  "MaximumNonWorkHourResultsByDay": Integer,
                  "MeetingDurationInMinutes": Integer,
                  "MinimumSuggestionQuality": SuggestionQuality,
                  "DetailedSuggestionsWindow": Duration,
                  "CurrentMeetingTime": DateTime,
                  "GlobalObjectId": String}


"""
INPUT:
  <xs:complexType name="GetUserAvailabilityRequestType">
    <xs:complexContent mixed="false">
      <xs:extension base="m:BaseRequestType">
        <xs:sequence>
          <xs:element ref="t:TimeZone"/>
          <xs:element name="MailboxDataArray" type="t:ArrayOfMailboxData"/>
          <xs:element minOccurs="0" maxOccurs="1" ref="t:FreeBusyViewOptions"/>
          <xs:element minOccurs="0" maxOccurs="1" ref="t:SuggestionsViewOptions"/>
        </xs:sequence>
      </xs:extension>
    </xs:complexContent>
  </xs:complexType>
"""
class GetUserAvailabilityRequestType(ComplexModel):
    __namespace__ = EWS_M_NS
    _type_info = {"TimeZone": SerializableTimeZone,
                  "MailboxDataArray": ArrayOfMailboxData,
                  "FreeBusyViewOptions": FreeBusyViewOptionsType,
                  "SuggestionsViewOptions": SuggestionsViewOptionsType}


"""
  <xs:simpleType name="ResponseCodeType">
    <xs:annotation>
      <xs:documentation>
		Represents the message keys that can be returned by response error messages
	  </xs:documentation>
    </xs:annotation>
    <xs:restriction base="xs:string">
      <xs:enumeration value="NoError"/>
      <xs:enumeration value="ErrorAccessDenied"/>
      <xs:enumeration value="ErrorAccountDisabled"/>
      <xs:enumeration value="ErrorAddressSpaceNotFound"/>
      <xs:enumeration value="ErrorADOperation"/>
      <xs:enumeration value="ErrorADSessionFilter"/>
      <xs:enumeration value="ErrorADUnavailable"/>
      <xs:enumeration value="ErrorAutoDiscoverFailed"/>
      <xs:enumeration value="ErrorAffectedTaskOccurrencesRequired"/>
      <xs:enumeration value="ErrorAttachmentSizeLimitExceeded"/>
      <xs:enumeration value="ErrorAvailabilityConfigNotFound"/>
      <xs:enumeration value="ErrorBatchProcessingStopped"/>
      <xs:enumeration value="ErrorCalendarCannotMoveOrCopyOccurrence"/>
      <xs:enumeration value="ErrorCalendarCannotUpdateDeletedItem"/>
      <xs:enumeration value="ErrorCalendarCannotUseIdForOccurrenceId"/>
      <xs:enumeration value="ErrorCalendarCannotUseIdForRecurringMasterId"/>
      <xs:enumeration value="ErrorCalendarDurationIsTooLong"/>
      <xs:enumeration value="ErrorCalendarEndDateIsEarlierThanStartDate"/>
      <xs:enumeration value="ErrorCalendarFolderIsInvalidForCalendarView"/>
      <xs:enumeration value="ErrorCalendarInvalidAttributeValue"/>
      <xs:enumeration value="ErrorCalendarInvalidDayForTimeChangePattern"/>
      <xs:enumeration value="ErrorCalendarInvalidDayForWeeklyRecurrence"/>
      <xs:enumeration value="ErrorCalendarInvalidPropertyState"/>
      <xs:enumeration value="ErrorCalendarInvalidPropertyValue"/>
      <xs:enumeration value="ErrorCalendarInvalidRecurrence"/>
      <xs:enumeration value="ErrorCalendarInvalidTimeZone"/>
      <xs:enumeration value="ErrorCalendarIsDelegatedForAccept"/>
      <xs:enumeration value="ErrorCalendarIsDelegatedForDecline"/>
      <xs:enumeration value="ErrorCalendarIsDelegatedForRemove"/>
      <xs:enumeration value="ErrorCalendarIsDelegatedForTentative"/>
      <xs:enumeration value="ErrorCalendarIsNotOrganizer"/>
      <xs:enumeration value="ErrorCalendarIsOrganizerForAccept"/>
      <xs:enumeration value="ErrorCalendarIsOrganizerForDecline"/>
      <xs:enumeration value="ErrorCalendarIsOrganizerForRemove"/>
      <xs:enumeration value="ErrorCalendarIsOrganizerForTentative"/>
      <xs:enumeration value="ErrorCalendarOccurrenceIndexIsOutOfRecurrenceRange"/>
      <xs:enumeration value="ErrorCalendarOccurrenceIsDeletedFromRecurrence"/>
      <xs:enumeration value="ErrorCalendarOutOfRange"/>
      <xs:enumeration value="ErrorCalendarViewRangeTooBig"/>
      <xs:enumeration value="ErrorCannotCreateCalendarItemInNonCalendarFolder"/>
      <xs:enumeration value="ErrorCannotCreateContactInNonContactFolder"/>
      <xs:enumeration value="ErrorCannotCreateTaskInNonTaskFolder"/>
      <xs:enumeration value="ErrorCannotDeleteObject"/>
      <xs:enumeration value="ErrorCannotOpenFileAttachment"/>
      <xs:enumeration value="ErrorCannotDeleteTaskOccurrence"/>
      <xs:enumeration value="ErrorCannotUseFolderIdForItemId"/>
      <xs:enumeration value="ErrorCannotUseItemIdForFolderId"/>
      <xs:enumeration value="ErrorChangeKeyRequired"/>
      <xs:enumeration value="ErrorChangeKeyRequiredForWriteOperations"/>
      <xs:enumeration value="ErrorConnectionFailed"/>
      <xs:enumeration value="ErrorContentConversionFailed"/>
      <xs:enumeration value="ErrorCorruptData"/>
      <xs:enumeration value="ErrorCreateItemAccessDenied"/>
      <xs:enumeration value="ErrorCreateManagedFolderPartialCompletion"/>
      <xs:enumeration value="ErrorCreateSubfolderAccessDenied"/>
      <xs:enumeration value="ErrorCrossMailboxMoveCopy"/>
      <xs:enumeration value="ErrorDataSizeLimitExceeded"/>
      <xs:enumeration value="ErrorDataSourceOperation"/>
      <xs:enumeration value="ErrorDeleteDistinguishedFolder"/>
      <xs:enumeration value="ErrorDeleteItemsFailed"/>
      <xs:enumeration value="ErrorDuplicateInputFolderNames"/>
      <xs:enumeration value="ErrorEmailAddressMismatch"/>
      <xs:enumeration value="ErrorEventNotFound"/>
      <xs:enumeration value="ErrorExpiredSubscription"/>
      <xs:enumeration value="ErrorFolderCorrupt"/>
      <xs:enumeration value="ErrorFolderNotFound"/>
      <xs:enumeration value="ErrorFolderPropertRequestFailed"/>
      <xs:enumeration value="ErrorFolderSave"/>
      <xs:enumeration value="ErrorFolderSaveFailed"/>
      <xs:enumeration value="ErrorFolderSavePropertyError"/>
      <xs:enumeration value="ErrorFolderExists"/>
      <xs:enumeration value="ErrorFreeBusyGenerationFailed"/>
      <xs:enumeration value="ErrorGetServerSecurityDescriptorFailed"/>
      <xs:enumeration value="ErrorImpersonateUserDenied"/>
      <xs:enumeration value="ErrorImpersonationDenied"/>
      <xs:enumeration value="ErrorImpersonationFailed"/>
      <xs:enumeration value="ErrorIncorrectUpdatePropertyCount"/>
      <xs:enumeration value="ErrorIndividualMailboxLimitReached"/>
      <xs:enumeration value="ErrorInsufficientResources"/>
      <xs:enumeration value="ErrorInternalServerError"/>
      <xs:enumeration value="ErrorInternalServerTransientError"/>
      <xs:enumeration value="ErrorInvalidAccessLevel"/>
      <xs:enumeration value="ErrorInvalidAttachmentId"/>
      <xs:enumeration value="ErrorInvalidAttachmentSubfilter"/>
      <xs:enumeration value="ErrorInvalidAttachmentSubfilterTextFilter"/>
      <xs:enumeration value="ErrorInvalidAuthorizationContext"/>
      <xs:enumeration value="ErrorInvalidChangeKey"/>
      <xs:enumeration value="ErrorInvalidClientSecurityContext"/>
      <xs:enumeration value="ErrorInvalidCompleteDate"/>
      <xs:enumeration value="ErrorInvalidCrossForestCredentials"/>
      <xs:enumeration value="ErrorInvalidExcludesRestriction"/>
      <xs:enumeration value="ErrorInvalidExpressionTypeForSubFilter"/>
      <xs:enumeration value="ErrorInvalidExtendedProperty"/>
      <xs:enumeration value="ErrorInvalidExtendedPropertyValue"/>
      <xs:enumeration value="ErrorInvalidFolderId"/>
      <xs:enumeration value="ErrorInvalidFractionalPagingParameters"/>
      <xs:enumeration value="ErrorInvalidFreeBusyViewType"/>
      <xs:enumeration value="ErrorInvalidId"/>
      <xs:enumeration value="ErrorInvalidIdEmpty"/>
      <xs:enumeration value="ErrorInvalidIdMalformed"/>
      <xs:enumeration value="ErrorInvalidIdMonikerTooLong"/>
      <xs:enumeration value="ErrorInvalidIdNotAnItemAttachmentId"/>
      <xs:enumeration value="ErrorInvalidIdReturnedByResolveNames"/>
      <xs:enumeration value="ErrorInvalidIdStoreObjectIdTooLong"/>
      <xs:enumeration value="ErrorInvalidIdTooManyAttachmentLevels"/>
      <xs:enumeration value="ErrorInvalidIdXml"/>
      <xs:enumeration value="ErrorInvalidIndexedPagingParameters"/>
      <xs:enumeration value="ErrorInvalidInternetHeaderChildNodes"/>
      <xs:enumeration value="ErrorInvalidItemForOperationCreateItemAttachment"/>
      <xs:enumeration value="ErrorInvalidItemForOperationCreateItem"/>
      <xs:enumeration value="ErrorInvalidItemForOperationAcceptItem"/>
      <xs:enumeration value="ErrorInvalidItemForOperationDeclineItem"/>
      <xs:enumeration value="ErrorInvalidItemForOperationCancelItem"/>
      <xs:enumeration value="ErrorInvalidItemForOperationExpandDL"/>
      <xs:enumeration value="ErrorInvalidItemForOperationRemoveItem"/>
      <xs:enumeration value="ErrorInvalidItemForOperationSendItem"/>
      <xs:enumeration value="ErrorInvalidItemForOperationTentative"/>
      <xs:enumeration value="ErrorInvalidManagedFolderProperty"/>
      <xs:enumeration value="ErrorInvalidManagedFolderQuota"/>
      <xs:enumeration value="ErrorInvalidManagedFolderSize"/>
      <xs:enumeration value="ErrorInvalidMergedFreeBusyInterval"/>
      <xs:enumeration value="ErrorInvalidNameForNameResolution"/>
      <xs:enumeration value="ErrorInvalidNetworkServiceContext"/>
      <xs:enumeration value="ErrorInvalidOofParameter"/>
      <xs:enumeration value="ErrorInvalidPagingMaxRows"/>
      <xs:enumeration value="ErrorInvalidParentFolder"/>
      <xs:enumeration value="ErrorInvalidPercentCompleteValue"/>
      <xs:enumeration value="ErrorInvalidPropertyAppend"/>
      <xs:enumeration value="ErrorInvalidPropertyDelete"/>
      <xs:enumeration value="ErrorInvalidPropertyForExists"/>
      <xs:enumeration value="ErrorInvalidPropertyForOperation"/>
      <xs:enumeration value="ErrorInvalidPropertyRequest"/>
      <xs:enumeration value="ErrorInvalidPropertySet"/>
      <xs:enumeration value="ErrorInvalidPropertyUpdateSentMessage"/>
      <xs:enumeration value="ErrorInvalidPullSubscriptionId"/>
      <xs:enumeration value="ErrorInvalidPushSubscriptionUrl"/>
      <xs:enumeration value="ErrorInvalidRecipients"/>
      <xs:enumeration value="ErrorInvalidRecipientSubfilter"/>
      <xs:enumeration value="ErrorInvalidRecipientSubfilterComparison"/>
      <xs:enumeration value="ErrorInvalidRecipientSubfilterOrder"/>
      <xs:enumeration value="ErrorInvalidRecipientSubfilterTextFilter"/>
      <xs:enumeration value="ErrorInvalidReferenceItem"/>
      <xs:enumeration value="ErrorInvalidRequest"/>
      <xs:enumeration value="ErrorInvalidRestriction"/>
      <xs:enumeration value="ErrorInvalidRoutingType"/>
      <xs:enumeration value="ErrorInvalidScheduledOofDuration"/>
      <xs:enumeration value="ErrorInvalidSecurityDescriptor"/>
      <xs:enumeration value="ErrorInvalidSendItemSaveSettings"/>
      <xs:enumeration value="ErrorInvalidSerializedAccessToken"/>
      <xs:enumeration value="ErrorInvalidSid"/>
      <xs:enumeration value="ErrorInvalidSmtpAddress"/>
      <xs:enumeration value="ErrorInvalidSubfilterType"/>
      <xs:enumeration value="ErrorInvalidSubfilterTypeNotAttendeeType"/>
      <xs:enumeration value="ErrorInvalidSubfilterTypeNotRecipientType"/>
      <xs:enumeration value="ErrorInvalidSubscription"/>
      <xs:enumeration value="ErrorInvalidSyncStateData"/>
      <xs:enumeration value="ErrorInvalidTimeInterval"/>
      <xs:enumeration value="ErrorInvalidUserOofSettings"/>
      <xs:enumeration value="ErrorInvalidUserPrincipalName"/>
      <xs:enumeration value="ErrorInvalidUserSid"/>
      <xs:enumeration value="ErrorInvalidUserSidMissingUPN"/>
      <xs:enumeration value="ErrorInvalidValueForProperty"/>
      <xs:enumeration value="ErrorInvalidWatermark"/>
      <xs:enumeration value="ErrorIrresolvableConflict"/>
      <xs:enumeration value="ErrorItemCorrupt"/>
      <xs:enumeration value="ErrorItemNotFound"/>
      <xs:enumeration value="ErrorItemPropertyRequestFailed"/>
      <xs:enumeration value="ErrorItemSave"/>
      <xs:enumeration value="ErrorItemSavePropertyError"/>
      <xs:enumeration value="ErrorLegacyMailboxFreeBusyViewTypeNotMerged"/>
      <xs:enumeration value="ErrorLocalServerObjectNotFound"/>
      <xs:enumeration value="ErrorLogonAsNetworkServiceFailed"/>
      <xs:enumeration value="ErrorMailboxConfiguration"/>
      <xs:enumeration value="ErrorMailboxDataArrayEmpty"/>
      <xs:enumeration value="ErrorMailboxDataArrayTooBig"/>
      <xs:enumeration value="ErrorMailboxLogonFailed"/>
      <xs:enumeration value="ErrorMailboxMoveInProgress"/>
      <xs:enumeration value="ErrorMailboxStoreUnavailable"/>
      <xs:enumeration value="ErrorMailRecipientNotFound"/>
      <xs:enumeration value="ErrorManagedFolderAlreadyExists"/>
      <xs:enumeration value="ErrorManagedFolderNotFound"/>
      <xs:enumeration value="ErrorManagedFoldersRootFailure"/>
      <xs:enumeration value="ErrorMeetingSuggestionGenerationFailed"/>
      <xs:enumeration value="ErrorMessageDispositionRequired"/>
      <xs:enumeration value="ErrorMessageSizeExceeded"/>
      <xs:enumeration value="ErrorMimeContentConversionFailed"/>
      <xs:enumeration value="ErrorMimeContentInvalid"/>
      <xs:enumeration value="ErrorMimeContentInvalidBase64String"/>
      <xs:enumeration value="ErrorMissingArgument"/>
      <xs:enumeration value="ErrorMissingEmailAddress"/>
      <xs:enumeration value="ErrorMissingEmailAddressForManagedFolder"/>
      <xs:enumeration value="ErrorMissingInformationEmailAddress"/>
      <xs:enumeration value="ErrorMissingInformationReferenceItemId"/>
      <xs:enumeration value="ErrorMissingItemForCreateItemAttachment"/>
      <xs:enumeration value="ErrorMissingManagedFolderId"/>
      <xs:enumeration value="ErrorMissingRecipients"/>
      <xs:enumeration value="ErrorMoreThanOneAccessModeSpecified"/>
      <xs:enumeration value="ErrorMoveCopyFailed"/>
      <xs:enumeration value="ErrorMoveDistinguishedFolder"/>
      <xs:enumeration value="ErrorNameResolutionMultipleResults"/>
      <xs:enumeration value="ErrorNameResolutionNoMailbox"/>
      <xs:enumeration value="ErrorNameResolutionNoResults"/>
      <xs:enumeration value="ErrorNoCalendar"/>
      <xs:enumeration value="ErrorNoFolderClassOverride"/>
      <xs:enumeration value="ErrorNoFreeBusyAccess"/>
      <xs:enumeration value="ErrorNonExistentMailbox"/>
      <xs:enumeration value="ErrorNonPrimarySmtpAddress"/>
      <xs:enumeration value="ErrorNoPropertyTagForCustomProperties"/>
      <xs:enumeration value="ErrorNotEnoughMemory"/>
      <xs:enumeration value="ErrorObjectTypeChanged"/>
      <xs:enumeration value="ErrorOccurrenceCrossingBoundary"/>
      <xs:enumeration value="ErrorOccurrenceTimeSpanTooBig"/>
      <xs:enumeration value="ErrorParentFolderIdRequired"/>
      <xs:enumeration value="ErrorParentFolderNotFound"/>
      <xs:enumeration value="ErrorPasswordChangeRequired"/>
      <xs:enumeration value="ErrorPasswordExpired"/>
      <xs:enumeration value="ErrorPropertyUpdate"/>
      <xs:enumeration value="ErrorPropertyValidationFailure"/>
      <xs:enumeration value="ErrorProxyRequestNotAllowed"/>
      <xs:enumeration value="ErrorProxyRequestProcessingFailed"/>
      <xs:enumeration value="ErrorPublicFolderRequestProcessingFailed"/>
      <xs:enumeration value="ErrorPublicFolderServerNotFound"/>
      <xs:enumeration value="ErrorQueryFilterTooLong"/>
      <xs:enumeration value="ErrorQuotaExceeded"/>
      <xs:enumeration value="ErrorReadEventsFailed"/>
      <xs:enumeration value="ErrorReadReceiptNotPending"/>
      <xs:enumeration value="ErrorRecurrenceEndDateTooBig"/>
      <xs:enumeration value="ErrorRecurrenceHasNoOccurrence"/>
      <xs:enumeration value="ErrorRequestAborted"/>
      <xs:enumeration value="ErrorRequestStreamTooBig"/>
      <xs:enumeration value="ErrorRequiredPropertyMissing"/>
      <xs:enumeration value="ErrorResponseSchemaValidation"/>
      <xs:enumeration value="ErrorRestrictionTooLong"/>
      <xs:enumeration value="ErrorRestrictionTooComplex"/>
      <xs:enumeration value="ErrorResultSetTooBig"/>
      <xs:enumeration value="ErrorInvalidExchangeImpersonationHeaderData"/>
      <xs:enumeration value="ErrorSavedItemFolderNotFound"/>
      <xs:enumeration value="ErrorSchemaValidation"/>
      <xs:enumeration value="ErrorSearchFolderNotInitialized"/>
      <xs:enumeration value="ErrorSendAsDenied"/>
      <xs:enumeration value="ErrorSendMeetingCancellationsRequired"/>
      <xs:enumeration value="ErrorSendMeetingInvitationsOrCancellationsRequired"/>
      <xs:enumeration value="ErrorSendMeetingInvitationsRequired"/>
      <xs:enumeration value="ErrorSentMeetingRequestUpdate"/>
      <xs:enumeration value="ErrorSentTaskRequestUpdate"/>
      <xs:enumeration value="ErrorServerBusy"/>
      <xs:enumeration value="ErrorServiceDiscoveryFailed"/>
      <xs:enumeration value="ErrorStaleObject"/>
      <xs:enumeration value="ErrorSubscriptionAccessDenied"/>
      <xs:enumeration value="ErrorSubscriptionDelegateAccessNotSupported"/>
      <xs:enumeration value="ErrorSubscriptionNotFound"/>
      <xs:enumeration value="ErrorSyncFolderNotFound"/>
      <xs:enumeration value="ErrorTimeIntervalTooBig"/>
      <xs:enumeration value="ErrorTimeoutExpired"/>
      <xs:enumeration value="ErrorToFolderNotFound"/>
      <xs:enumeration value="ErrorTokenSerializationDenied"/>
      <xs:enumeration value="ErrorUpdatePropertyMismatch"/>
      <xs:enumeration value="ErrorUnableToGetUserOofSettings"/>
      <xs:enumeration value="ErrorUnsupportedSubFilter"/>
      <xs:enumeration value="ErrorUnsupportedCulture"/>
      <xs:enumeration value="ErrorUnsupportedMapiPropertyType"/>
      <xs:enumeration value="ErrorUnsupportedMimeConversion"/>
      <xs:enumeration value="ErrorUnsupportedPathForQuery"/>
      <xs:enumeration value="ErrorUnsupportedPathForSortGroup"/>
      <xs:enumeration value="ErrorUnsupportedPropertyDefinition"/>
      <xs:enumeration value="ErrorUnsupportedQueryFilter"/>
      <xs:enumeration value="ErrorUnsupportedRecurrence"/>
      <xs:enumeration value="ErrorUnsupportedTypeForConversion"/>
      <xs:enumeration value="ErrorVoiceMailNotImplemented"/>
      <xs:enumeration value="ErrorVirusDetected"/>
      <xs:enumeration value="ErrorVirusMessageDeleted"/>
      <xs:enumeration value="ErrorWebRequestInInvalidState"/>
      <xs:enumeration value="ErrorWin32InteropError"/>
      <xs:enumeration value="ErrorWorkingHoursSaveFailed"/>
      <xs:enumeration value="ErrorWorkingHoursXmlMalformed"/>
    </xs:restriction>
  </xs:simpleType>

"""
ResponseCodeType = String


"""
  <xs:simpleType name="ResponseClassType">
    <xs:restriction base="xs:NMTOKEN">
      <xs:enumeration value="Success"/>
      <xs:enumeration value="Warning"/>
      <xs:enumeration value="Error"/>
    </xs:restriction>
  </xs:simpleType>

"""
ResponseClassType = String


"""
  <xs:complexType name="ResponseMessageType">
    <xs:sequence minOccurs="0">
      <xs:element name="MessageText" type="xs:string" minOccurs="0"/>
      <xs:element name="ResponseCode" type="m:ResponseCodeType" minOccurs="0"/>
      <xs:element name="DescriptiveLinkKey" type="xs:int" minOccurs="0"/>
      <xs:element name="MessageXml" minOccurs="0">
        <xs:complexType>
          <xs:sequence>
            <xs:any processContents="lax" minOccurs="0" maxOccurs="unbounded"/>
          </xs:sequence>
        </xs:complexType>
      </xs:element>
    </xs:sequence>
    <xs:attribute name="ResponseClass" type="t:ResponseClassType" use="required"/>
  </xs:complexType>

"""
class ResponseMessageType(ComplexModel):
    __namespace__ = EWS_M_NS
    _type_info = {"ResponseClass": XmlAttribute(String),
                  
                  "MessageText": String,
                  "ResponseCode": ResponseCodeType,
                  "DescriptiveLinkKey": Integer,
                  "MessageXml": Array(AnyXml)}


"""
  <xs:simpleType name="LegacyFreeBusyType">
    <xs:restriction base="xs:string">
      <xs:enumeration value="Free"/>
      <xs:enumeration value="Tentative"/>
      <xs:enumeration value="Busy"/>
      <xs:enumeration value="OOF"/>
      <xs:enumeration value="NoData"/>
    </xs:restriction>
  </xs:simpleType>

"""
LegacyFreeBusyType = String


"""
  <xs:complexType name="CalendarEventDetails">
    <xs:sequence>
      <xs:element minOccurs="0" maxOccurs="1" name="ID" type="xs:string"/>
      <xs:element minOccurs="0" maxOccurs="1" name="Subject" type="xs:string"/>
      <xs:element minOccurs="0" maxOccurs="1" name="Location" type="xs:string"/>
      <xs:element minOccurs="1" maxOccurs="1" name="IsMeeting" type="xs:boolean"/>
      <xs:element minOccurs="1" maxOccurs="1" name="IsRecurring" type="xs:boolean"/>
      <xs:element minOccurs="1" maxOccurs="1" name="IsException" type="xs:boolean"/>
      <xs:element minOccurs="1" maxOccurs="1" name="IsReminderSet" type="xs:boolean"/>
      <xs:element minOccurs="1" maxOccurs="1" name="IsPrivate" type="xs:boolean"/>
    </xs:sequence>
  </xs:complexType>

"""
class CalendarEventDetails(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"ID": String,
                  "Subject": String,
                  "Location": String,
                  "IsMeeting": Boolean,
                  "IsRecurring": Boolean,
                  "IsException": Boolean,
                  "IsReminderSet": Boolean,
                  "IsPrivate": Boolean}


"""
  <xs:complexType name="CalendarEvent">
    <xs:sequence>
      <xs:element minOccurs="1" maxOccurs="1" name="StartTime" type="xs:dateTime"/>
      <xs:element minOccurs="1" maxOccurs="1" name="EndTime" type="xs:dateTime"/>
      <xs:element minOccurs="1" maxOccurs="1" name="BusyType" type="t:LegacyFreeBusyType"/>
      <xs:element minOccurs="0" maxOccurs="1" name="CalendarEventDetails" type="t:CalendarEventDetails"/>
    </xs:sequence>
  </xs:complexType>

"""
class CalendarEvent(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"StartTime": DateTime,
                  "EndTime": DateTime,
                  "BusyType": LegacyFreeBusyType,
                  "CalendarEventDetails": CalendarEventDetails}


"""
  <xs:complexType name="ArrayOfCalendarEvent">
    <xs:sequence>
      <xs:element minOccurs="0" maxOccurs="unbounded" name="CalendarEvent" type="t:CalendarEvent"/>
    </xs:sequence>
  </xs:complexType>

"""
ArrayOfCalendarEvent = Array(CalendarEvent)


"""
  <xs:simpleType name="DaysOfWeekType">
    <xs:list itemType="t:DayOfWeekType"/>
  </xs:simpleType>

"""
DaysOfWeekType = String


"""
  <xs:complexType name="WorkingPeriod">
    <xs:sequence>
      <xs:element minOccurs="1" maxOccurs="1" name="DayOfWeek" type="t:DaysOfWeekType"/>
      <xs:element minOccurs="1" maxOccurs="1" name="StartTimeInMinutes" type="xs:int"/>
      <xs:element minOccurs="1" maxOccurs="1" name="EndTimeInMinutes" type="xs:int"/>
    </xs:sequence>
  </xs:complexType>

"""
class WorkingPeriod(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"DayOfWeek": DaysOfWeekType,
                  "StartTimeInMinutes": Integer,
                  "EndTimeInMinutes": Integer}


"""
  <xs:complexType name="ArrayOfWorkingPeriod">
    <xs:sequence>
      <xs:element minOccurs="0" maxOccurs="unbounded" name="WorkingPeriod" type="t:WorkingPeriod"/>
    </xs:sequence>
  </xs:complexType>

"""
ArrayOfWorkingPeriod = Array(WorkingPeriod)


"""
  <xs:complexType name="WorkingHours">
    <xs:sequence>
      <xs:element minOccurs="1" maxOccurs="1" name="TimeZone" type="t:SerializableTimeZone"/>
      <xs:element minOccurs="1" maxOccurs="1" name="WorkingPeriodArray" type="t:ArrayOfWorkingPeriod"/>
    </xs:sequence>
  </xs:complexType>

"""
class WorkingHours(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"TimeZone": SerializableTimeZone,
                  "WorkingPeriodArray": ArrayOfWorkingPeriod}

"""
  <xs:complexType name="FreeBusyView">
    <xs:sequence>
      <xs:element minOccurs="1" maxOccurs="1" name="FreeBusyViewType" type="t:FreeBusyViewType"/>
      <xs:element minOccurs="0" maxOccurs="1" name="MergedFreeBusy" type="xs:string"/>
      <xs:element minOccurs="0" maxOccurs="1" name="CalendarEventArray" type="t:ArrayOfCalendarEvent"/>
      <xs:element minOccurs="0" maxOccurs="1" name="WorkingHours" type="t:WorkingHours"/>
    </xs:sequence>
  </xs:complexType>

"""
class FreeBusyView(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"FreeBusyViewType": FreeBusyViewType,
                  "MergedFreeBusy": String,
                  "CalendarEventArray": ArrayOfCalendarEvent,
                  "WorkingHours": WorkingHours}


"""
  <xs:complexType name="FreeBusyResponseType">
    <xs:sequence>
      <xs:element minOccurs="0" maxOccurs="1" name="ResponseMessage" type="m:ResponseMessageType"/>
      <xs:element minOccurs="0" maxOccurs="1" name="FreeBusyView" type="t:FreeBusyView"/>
    </xs:sequence>
  </xs:complexType>

"""
class FreeBusyResponse(ComplexModel):
    __namespace__ = EWS_M_NS
    _type_info = {"ResponseMessage": ResponseMessageType,
                  "FreeBusyView": FreeBusyView}


"""
  <xs:complexType name="ArrayOfFreeBusyResponse">
    <xs:sequence>
      <xs:element minOccurs="0" maxOccurs="unbounded" name="FreeBusyResponse" type="m:FreeBusyResponseType"/>
    </xs:sequence>
  </xs:complexType>

"""
ArrayOfFreeBusyResponse = Array(FreeBusyResponse)


"""
  <xs:complexType name="AttendeeConflictData" abstract="true"/>
  <xs:complexType name="UnknownAttendeeConflictData">
    <xs:complexContent mixed="false">
      <xs:extension base="t:AttendeeConflictData"/>
    </xs:complexContent>
  </xs:complexType>
  <xs:complexType name="TooBigGroupAttendeeConflictData">
    <xs:complexContent mixed="false">
      <xs:extension base="t:AttendeeConflictData"/>
    </xs:complexContent>
  </xs:complexType>
  <xs:complexType name="IndividualAttendeeConflictData">
    <xs:complexContent mixed="false">
      <xs:extension base="t:AttendeeConflictData">
        <xs:sequence>
          <xs:element minOccurs="1" maxOccurs="1" name="BusyType" type="t:LegacyFreeBusyType"/>
        </xs:sequence>
      </xs:extension>
    </xs:complexContent>
  </xs:complexType>

"""
class UnknownAttendeeConflictData(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {}


class IndividualAttendeeConflictData(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {}


class TooBigGroupAttendeeConflictData(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {}


"""
  <xs:complexType name="GroupAttendeeConflictData">
    <xs:complexContent mixed="false">
      <xs:extension base="t:AttendeeConflictData">
        <xs:sequence>
          <xs:element minOccurs="1" maxOccurs="1" name="NumberOfMembers" type="xs:int"/>
          <xs:element minOccurs="1" maxOccurs="1" name="NumberOfMembersAvailable" type="xs:int"/>
          <xs:element minOccurs="1" maxOccurs="1" name="NumberOfMembersWithConflict" type="xs:int"/>
          <xs:element minOccurs="1" maxOccurs="1" name="NumberOfMembersWithNoData" type="xs:int"/>
        </xs:sequence>
      </xs:extension>
    </xs:complexContent>
  </xs:complexType>

"""
class GroupAttendeeConflictData(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"NumberOfMembers": Integer,
                  "NumberOfMembersAvailable": Integer,
                  "NumberOfMembersWithConflict": Integer,
                  "NumberOfMembersWithNoData": Integer}


"""
  <xs:complexType name="ArrayOfAttendeeConflictData">
    <xs:choice minOccurs="0" maxOccurs="unbounded">
      <xs:element minOccurs="1" maxOccurs="1" name="UnknownAttendeeConflictData" nillable="true" type="t:UnknownAttendeeConflictData"/>
      <xs:element minOccurs="1" maxOccurs="1" name="IndividualAttendeeConflictData" nillable="true" type="t:IndividualAttendeeConflictData"/>
      <xs:element minOccurs="1" maxOccurs="1" name="TooBigGroupAttendeeConflictData" nillable="true" type="t:TooBigGroupAttendeeConflictData"/>
      <xs:element minOccurs="1" maxOccurs="1" name="GroupAttendeeConflictData" nillable="true" type="t:GroupAttendeeConflictData"/>
    </xs:choice>
  </xs:complexType>
"""
class ArrayOfAttendeeConflictData(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"UnknownAttendeeConflictData": UnknownAttendeeConflictData,
                  "IndividualAttendeeConflictData": IndividualAttendeeConflictData,
                  "TooBigGroupAttendeeConflictData": TooBigGroupAttendeeConflictData,
                  "GroupAttendeeConflictData": GroupAttendeeConflictData}

"""
  <xs:complexType name="Suggestion">
    <xs:sequence>
      <xs:element minOccurs="1" maxOccurs="1" name="MeetingTime" type="xs:dateTime"/>
      <xs:element minOccurs="1" maxOccurs="1" name="IsWorkTime" type="xs:boolean"/>
      <xs:element minOccurs="1" maxOccurs="1" name="SuggestionQuality" type="t:SuggestionQuality"/>
      <xs:element minOccurs="0" maxOccurs="1" name="AttendeeConflictDataArray" type="t:ArrayOfAttendeeConflictData"/>
    </xs:sequence>
  </xs:complexType>

"""
class Suggestion(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"MeetingType": DateTime,
                  "IsWorkTime": Boolean,
                  "SuggestionQuality": SuggestionQuality,
                  "AttendeeConflictDataArray": ArrayOfAttendeeConflictData}


"""
  <xs:complexType name="ArrayOfSuggestion">
    <xs:sequence>
      <xs:element minOccurs="0" maxOccurs="unbounded" name="Suggestion" type="t:Suggestion"/>
    </xs:sequence>
  </xs:complexType>

"""
ArrayOfSuggestion = Array(Suggestion)


"""
  <xs:complexType name="SuggestionDayResult">
    <xs:sequence>
      <xs:element minOccurs="1" maxOccurs="1" name="Date" type="xs:dateTime"/>
      <xs:element minOccurs="1" maxOccurs="1" name="DayQuality" type="t:SuggestionQuality"/>
      <xs:element minOccurs="0" maxOccurs="1" name="SuggestionArray" type="t:ArrayOfSuggestion"/>
    </xs:sequence>
  </xs:complexType>

"""
class SuggestionDayResult(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"Date": DateTime,
                  "DayQuality": SuggestionQuality,
                  "SuggestionArray": ArrayOfSuggestion}


"""
  <xs:complexType name="ArrayOfSuggestionDayResult">
    <xs:sequence>
      <xs:element minOccurs="0" maxOccurs="unbounded" name="SuggestionDayResult" type="t:SuggestionDayResult"/>
    </xs:sequence>
  </xs:complexType>

"""
ArrayOfSuggestionDayResult = Array(SuggestionDayResult)
ArrayOfSuggestionDayResult.__namespace__ = EWS_M_NS

"""
  <xs:complexType name="SuggestionsResponseType">
    <xs:sequence>
      <xs:element minOccurs="0" maxOccurs="1" name="ResponseMessage" type="m:ResponseMessageType"/>
      <xs:element minOccurs="0" maxOccurs="1" name="SuggestionDayResultArray" type="t:ArrayOfSuggestionDayResult"/>
    </xs:sequence>
  </xs:complexType>

"""
class SuggestionsResponseType(ComplexModel):
    __namespace__ = EWS_M_NS
    _type_info = {"ResponseMessage": ResponseMessageType,
                  "SuggestionDayResultArray": ArrayOfSuggestionDayResult}


"""
OUTPUT:
  <xs:complexType name="GetUserAvailabilityResponseType">
    <xs:sequence>
      <xs:element minOccurs="0" maxOccurs="1" name="FreeBusyResponseArray" type="m:ArrayOfFreeBusyResponse"/>
      <xs:element minOccurs="0" maxOccurs="1" name="SuggestionsResponse" type="m:SuggestionsResponseType"/>
    </xs:sequence>
  </xs:complexType>

"""
class GetUserAvailabilityResponseType(ComplexModel):
    __namespace__ = EWS_M_NS
    _type_info = {"FreeBusyResponseArray": ArrayOfFreeBusyResponse,
                  "SuggestionsResponse": SuggestionsResponseType}


class ServerVersionInfo(ComplexModel):
    __namespace__ = EWS_T_NS
    _type_info = {"MajorVersion": XmlAttribute(Integer),
                  "MinorVersion": XmlAttribute(Integer),
                  "MajorBuildNumber": XmlAttribute(Integer),
                  "MinorBuildNumber": XmlAttribute(Integer)}
