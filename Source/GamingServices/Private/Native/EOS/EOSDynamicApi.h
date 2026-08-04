#pragma once

#include "EOSSdkHeaders.h"

#if defined(GS_WITH_EOS)

namespace GamingServices
{
	/**
	 * Runtime symbol table for the EOS SDK.
	 *
	 * The SDK is not linked. Every EOS entry point this module calls is a member here, resolved from the
	 * shared library at runtime by LoadEOSApi(). Each member's type is taken with decltype from the SDK's
	 * own declaration, so a signature change in a newer SDK is a compile error rather than a silent
	 * mismatch, and the member name is the entry point minus its "EOS_" prefix.
	 *
	 * Call sites do not mention this table: EOSDynamicApiRedirect.h rewrites every EOS_* call onto it, so
	 * the capability code still reads as plain SDK usage.
	 *
	 * GENERATED. To add an entry point, add its member here, its redirect in EOSDynamicApiRedirect.h and
	 * its GS_EOS_BIND line in EOSDynamicApi.cpp; all three lists are kept in the same (sorted) order.
	 */
	struct FEOSApi
	{
		decltype(&::EOS_Achievements_CopyAchievementDefinitionV2ByIndex) Achievements_CopyAchievementDefinitionV2ByIndex = nullptr;
		decltype(&::EOS_Achievements_CopyPlayerAchievementByAchievementId) Achievements_CopyPlayerAchievementByAchievementId = nullptr;
		decltype(&::EOS_Achievements_DefinitionV2_Release) Achievements_DefinitionV2_Release = nullptr;
		decltype(&::EOS_Achievements_GetAchievementDefinitionCount) Achievements_GetAchievementDefinitionCount = nullptr;
		decltype(&::EOS_Achievements_PlayerAchievement_Release) Achievements_PlayerAchievement_Release = nullptr;
		decltype(&::EOS_Achievements_QueryDefinitions) Achievements_QueryDefinitions = nullptr;
		decltype(&::EOS_Achievements_QueryPlayerAchievements) Achievements_QueryPlayerAchievements = nullptr;
		decltype(&::EOS_Achievements_UnlockAchievements) Achievements_UnlockAchievements = nullptr;
		decltype(&::EOS_Auth_CopyUserAuthToken) Auth_CopyUserAuthToken = nullptr;
		decltype(&::EOS_Auth_Login) Auth_Login = nullptr;
		decltype(&::EOS_Auth_Token_Release) Auth_Token_Release = nullptr;
		decltype(&::EOS_Connect_CopyProductUserInfo) Connect_CopyProductUserInfo = nullptr;
		decltype(&::EOS_Connect_CreateUser) Connect_CreateUser = nullptr;
		decltype(&::EOS_Connect_ExternalAccountInfo_Release) Connect_ExternalAccountInfo_Release = nullptr;
		decltype(&::EOS_Connect_GetExternalAccountMapping) Connect_GetExternalAccountMapping = nullptr;
		decltype(&::EOS_Connect_Login) Connect_Login = nullptr;
		decltype(&::EOS_Connect_QueryExternalAccountMappings) Connect_QueryExternalAccountMappings = nullptr;
		decltype(&::EOS_Connect_QueryProductUserIdMappings) Connect_QueryProductUserIdMappings = nullptr;
		decltype(&::EOS_Ecom_CopyEntitlementByIndex) Ecom_CopyEntitlementByIndex = nullptr;
		decltype(&::EOS_Ecom_Entitlement_Release) Ecom_Entitlement_Release = nullptr;
		decltype(&::EOS_Ecom_GetEntitlementsByNameCount) Ecom_GetEntitlementsByNameCount = nullptr;
		decltype(&::EOS_Ecom_GetEntitlementsCount) Ecom_GetEntitlementsCount = nullptr;
		decltype(&::EOS_Ecom_QueryEntitlements) Ecom_QueryEntitlements = nullptr;
		decltype(&::EOS_EpicAccountId_FromString) EpicAccountId_FromString = nullptr;
		decltype(&::EOS_EpicAccountId_IsValid) EpicAccountId_IsValid = nullptr;
		decltype(&::EOS_EpicAccountId_ToString) EpicAccountId_ToString = nullptr;
		decltype(&::EOS_EResult_ToString) EResult_ToString = nullptr;
		decltype(&::EOS_Friends_AddNotifyFriendsUpdate) Friends_AddNotifyFriendsUpdate = nullptr;
		decltype(&::EOS_Friends_GetFriendAtIndex) Friends_GetFriendAtIndex = nullptr;
		decltype(&::EOS_Friends_GetFriendsCount) Friends_GetFriendsCount = nullptr;
		decltype(&::EOS_Friends_GetStatus) Friends_GetStatus = nullptr;
		decltype(&::EOS_Friends_QueryFriends) Friends_QueryFriends = nullptr;
		decltype(&::EOS_Friends_RemoveNotifyFriendsUpdate) Friends_RemoveNotifyFriendsUpdate = nullptr;
		decltype(&::EOS_Initialize) Initialize = nullptr;
		decltype(&::EOS_Leaderboards_CopyLeaderboardDefinitionByIndex) Leaderboards_CopyLeaderboardDefinitionByIndex = nullptr;
		decltype(&::EOS_Leaderboards_CopyLeaderboardRecordByIndex) Leaderboards_CopyLeaderboardRecordByIndex = nullptr;
		decltype(&::EOS_Leaderboards_Definition_Release) Leaderboards_Definition_Release = nullptr;
		decltype(&::EOS_Leaderboards_GetLeaderboardDefinitionCount) Leaderboards_GetLeaderboardDefinitionCount = nullptr;
		decltype(&::EOS_Leaderboards_GetLeaderboardRecordCount) Leaderboards_GetLeaderboardRecordCount = nullptr;
		decltype(&::EOS_Leaderboards_LeaderboardRecord_Release) Leaderboards_LeaderboardRecord_Release = nullptr;
		decltype(&::EOS_Leaderboards_QueryLeaderboardDefinitions) Leaderboards_QueryLeaderboardDefinitions = nullptr;
		decltype(&::EOS_Leaderboards_QueryLeaderboardRanks) Leaderboards_QueryLeaderboardRanks = nullptr;
		decltype(&::EOS_LobbyDetails_CopyAttributeByIndex) LobbyDetails_CopyAttributeByIndex = nullptr;
		decltype(&::EOS_LobbyDetails_CopyAttributeByKey) LobbyDetails_CopyAttributeByKey = nullptr;
		decltype(&::EOS_LobbyDetails_CopyInfo) LobbyDetails_CopyInfo = nullptr;
		decltype(&::EOS_LobbyDetails_CopyMemberAttributeByKey) LobbyDetails_CopyMemberAttributeByKey = nullptr;
		decltype(&::EOS_LobbyDetails_GetAttributeCount) LobbyDetails_GetAttributeCount = nullptr;
		decltype(&::EOS_LobbyDetails_GetLobbyOwner) LobbyDetails_GetLobbyOwner = nullptr;
		decltype(&::EOS_LobbyDetails_Info_Release) LobbyDetails_Info_Release = nullptr;
		decltype(&::EOS_LobbyDetails_Release) LobbyDetails_Release = nullptr;
		decltype(&::EOS_LobbyModification_AddAttribute) LobbyModification_AddAttribute = nullptr;
		decltype(&::EOS_LobbyModification_AddMemberAttribute) LobbyModification_AddMemberAttribute = nullptr;
		decltype(&::EOS_LobbyModification_Release) LobbyModification_Release = nullptr;
		decltype(&::EOS_LobbyModification_SetInvitesAllowed) LobbyModification_SetInvitesAllowed = nullptr;
		decltype(&::EOS_LobbyModification_SetMaxMembers) LobbyModification_SetMaxMembers = nullptr;
		decltype(&::EOS_LobbyModification_SetPermissionLevel) LobbyModification_SetPermissionLevel = nullptr;
		decltype(&::EOS_LobbySearch_CopySearchResultByIndex) LobbySearch_CopySearchResultByIndex = nullptr;
		decltype(&::EOS_LobbySearch_Find) LobbySearch_Find = nullptr;
		decltype(&::EOS_LobbySearch_GetSearchResultCount) LobbySearch_GetSearchResultCount = nullptr;
		decltype(&::EOS_LobbySearch_Release) LobbySearch_Release = nullptr;
		decltype(&::EOS_LobbySearch_SetLobbyId) LobbySearch_SetLobbyId = nullptr;
		decltype(&::EOS_LobbySearch_SetParameter) LobbySearch_SetParameter = nullptr;
		decltype(&::EOS_Lobby_AddNotifyLobbyInviteAccepted) Lobby_AddNotifyLobbyInviteAccepted = nullptr;
		decltype(&::EOS_Lobby_AddNotifyLobbyInviteReceived) Lobby_AddNotifyLobbyInviteReceived = nullptr;
		decltype(&::EOS_Lobby_AddNotifyLobbyMemberStatusReceived) Lobby_AddNotifyLobbyMemberStatusReceived = nullptr;
		decltype(&::EOS_Lobby_Attribute_Release) Lobby_Attribute_Release = nullptr;
		decltype(&::EOS_Lobby_CopyLobbyDetailsHandle) Lobby_CopyLobbyDetailsHandle = nullptr;
		decltype(&::EOS_Lobby_CopyLobbyDetailsHandleByInviteId) Lobby_CopyLobbyDetailsHandleByInviteId = nullptr;
		decltype(&::EOS_Lobby_CreateLobby) Lobby_CreateLobby = nullptr;
		decltype(&::EOS_Lobby_CreateLobbySearch) Lobby_CreateLobbySearch = nullptr;
		decltype(&::EOS_Lobby_DestroyLobby) Lobby_DestroyLobby = nullptr;
		decltype(&::EOS_Lobby_GetInviteCount) Lobby_GetInviteCount = nullptr;
		decltype(&::EOS_Lobby_GetInviteIdByIndex) Lobby_GetInviteIdByIndex = nullptr;
		decltype(&::EOS_Lobby_JoinLobby) Lobby_JoinLobby = nullptr;
		decltype(&::EOS_Lobby_JoinLobbyById) Lobby_JoinLobbyById = nullptr;
		decltype(&::EOS_Lobby_LeaveLobby) Lobby_LeaveLobby = nullptr;
		decltype(&::EOS_Lobby_QueryInvites) Lobby_QueryInvites = nullptr;
		decltype(&::EOS_Lobby_RejectInvite) Lobby_RejectInvite = nullptr;
		decltype(&::EOS_Lobby_RemoveNotifyLobbyInviteAccepted) Lobby_RemoveNotifyLobbyInviteAccepted = nullptr;
		decltype(&::EOS_Lobby_RemoveNotifyLobbyInviteReceived) Lobby_RemoveNotifyLobbyInviteReceived = nullptr;
		decltype(&::EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived) Lobby_RemoveNotifyLobbyMemberStatusReceived = nullptr;
		decltype(&::EOS_Lobby_SendInvite) Lobby_SendInvite = nullptr;
		decltype(&::EOS_Lobby_UpdateLobby) Lobby_UpdateLobby = nullptr;
		decltype(&::EOS_Lobby_UpdateLobbyModification) Lobby_UpdateLobbyModification = nullptr;
		decltype(&::EOS_Logging_SetCallback) Logging_SetCallback = nullptr;
		decltype(&::EOS_Logging_SetLogLevel) Logging_SetLogLevel = nullptr;
		decltype(&::EOS_P2P_AcceptConnection) P2P_AcceptConnection = nullptr;
		decltype(&::EOS_P2P_AddNotifyPeerConnectionClosed) P2P_AddNotifyPeerConnectionClosed = nullptr;
		decltype(&::EOS_P2P_AddNotifyPeerConnectionRequest) P2P_AddNotifyPeerConnectionRequest = nullptr;
		decltype(&::EOS_P2P_CloseConnections) P2P_CloseConnections = nullptr;
		decltype(&::EOS_P2P_GetNextReceivedPacketSize) P2P_GetNextReceivedPacketSize = nullptr;
		decltype(&::EOS_P2P_ReceivePacket) P2P_ReceivePacket = nullptr;
		decltype(&::EOS_P2P_RemoveNotifyPeerConnectionClosed) P2P_RemoveNotifyPeerConnectionClosed = nullptr;
		decltype(&::EOS_P2P_RemoveNotifyPeerConnectionRequest) P2P_RemoveNotifyPeerConnectionRequest = nullptr;
		decltype(&::EOS_P2P_SendPacket) P2P_SendPacket = nullptr;
		decltype(&::EOS_Platform_Create) Platform_Create = nullptr;
		decltype(&::EOS_Platform_GetAchievementsInterface) Platform_GetAchievementsInterface = nullptr;
		decltype(&::EOS_Platform_GetAuthInterface) Platform_GetAuthInterface = nullptr;
		decltype(&::EOS_Platform_GetConnectInterface) Platform_GetConnectInterface = nullptr;
		decltype(&::EOS_Platform_GetEcomInterface) Platform_GetEcomInterface = nullptr;
		decltype(&::EOS_Platform_GetFriendsInterface) Platform_GetFriendsInterface = nullptr;
		decltype(&::EOS_Platform_GetLeaderboardsInterface) Platform_GetLeaderboardsInterface = nullptr;
		decltype(&::EOS_Platform_GetLobbyInterface) Platform_GetLobbyInterface = nullptr;
		decltype(&::EOS_Platform_GetP2PInterface) Platform_GetP2PInterface = nullptr;
		decltype(&::EOS_Platform_GetPresenceInterface) Platform_GetPresenceInterface = nullptr;
		decltype(&::EOS_Platform_GetPlayerDataStorageInterface) Platform_GetPlayerDataStorageInterface = nullptr;
		decltype(&::EOS_Platform_GetStatsInterface) Platform_GetStatsInterface = nullptr;
		decltype(&::EOS_Platform_GetUserInfoInterface) Platform_GetUserInfoInterface = nullptr;
		decltype(&::EOS_Platform_Release) Platform_Release = nullptr;
		decltype(&::EOS_Platform_Tick) Platform_Tick = nullptr;
		decltype(&::EOS_PlayerDataStorage_DeleteFile) PlayerDataStorage_DeleteFile = nullptr;
		decltype(&::EOS_PlayerDataStorage_ReadFile) PlayerDataStorage_ReadFile = nullptr;
		decltype(&::EOS_PlayerDataStorage_WriteFile) PlayerDataStorage_WriteFile = nullptr;
		decltype(&::EOS_Presence_CopyPresence) Presence_CopyPresence = nullptr;
		decltype(&::EOS_Presence_Info_Release) Presence_Info_Release = nullptr;
		decltype(&::EOS_Presence_QueryPresence) Presence_QueryPresence = nullptr;
		decltype(&::EOS_ProductUserId_FromString) ProductUserId_FromString = nullptr;
		decltype(&::EOS_ProductUserId_IsValid) ProductUserId_IsValid = nullptr;
		decltype(&::EOS_ProductUserId_ToString) ProductUserId_ToString = nullptr;
		decltype(&::EOS_Shutdown) Shutdown = nullptr;
		decltype(&::EOS_Stats_CopyStatByIndex) Stats_CopyStatByIndex = nullptr;
		decltype(&::EOS_Stats_GetStatsCount) Stats_GetStatsCount = nullptr;
		decltype(&::EOS_Stats_IngestStat) Stats_IngestStat = nullptr;
		decltype(&::EOS_Stats_QueryStats) Stats_QueryStats = nullptr;
		decltype(&::EOS_Stats_Stat_Release) Stats_Stat_Release = nullptr;
		decltype(&::EOS_UserInfo_BestDisplayName_Release) UserInfo_BestDisplayName_Release = nullptr;
		decltype(&::EOS_UserInfo_CopyBestDisplayName) UserInfo_CopyBestDisplayName = nullptr;
		decltype(&::EOS_UserInfo_CopyBestDisplayNameWithPlatform) UserInfo_CopyBestDisplayNameWithPlatform = nullptr;
		decltype(&::EOS_UserInfo_QueryUserInfo) UserInfo_QueryUserInfo = nullptr;
	};

	/** The one table. Only valid between a successful LoadEOSApi() and UnloadEOSApi(). */
	extern FEOSApi GEOSApi;

	/**
	 * Loads the EOS shared library out of the common SDK folder and binds every entry point above.
	 * All-or-nothing: if the library is missing, or any symbol fails to resolve, the table is left
	 * cleared and false is returned so the EOS backend can report itself unavailable. Idempotent.
	 */
	bool LoadEOSApi();

	/** True once LoadEOSApi() has succeeded — i.e. it is safe to call EOS through the redirects. */
	bool IsEOSApiLoaded();

	void UnloadEOSApi();
}

#endif // GS_WITH_EOS
