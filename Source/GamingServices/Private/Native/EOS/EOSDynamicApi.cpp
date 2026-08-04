#include "EOSDynamicApi.h"

#if defined(GS_WITH_EOS)

#include "GamingSdkLibrary.h"

#ifndef GS_EOS_LIBRARY_NAME
#define GS_EOS_LIBRARY_NAME "EOSSDK-Win64-Shipping.dll"
#endif

// This file deliberately does NOT include EOSDynamicApiRedirect.h: it is the one place that binds the
// table, so it must refer to the members directly. It stays correct even when unity builds merge it
// into a translation unit where the redirect macros are already active, because it never names an
// EOS_* entry point outside of a string literal.

namespace GamingServices
{
	FEOSApi GEOSApi;

	namespace
	{
		bool bEOSApiLoaded = false;

		FGamingSdkLibrary& EOSLibrary()
		{
			static FGamingSdkLibrary Library(TEXT(GS_EOS_LIBRARY_NAME));
			return Library;
		}
	}

	bool IsEOSApiLoaded()
	{
		return bEOSApiLoaded;
	}

	bool LoadEOSApi()
	{
		if (bEOSApiLoaded)
		{
			return true;
		}

		FGamingSdkLibrary& Library = EOSLibrary();
		if (!Library.Load())
		{
			return false;
		}

		TArray<FString> MissingSymbols;

#define GS_EOS_BIND(Member, Symbol) \
	GEOSApi.Member = reinterpret_cast<decltype(GEOSApi.Member)>(Library.GetExport(TEXT(Symbol))); \
	if (!GEOSApi.Member) { MissingSymbols.Add(TEXT(Symbol)); }

		GS_EOS_BIND(Achievements_CopyAchievementDefinitionV2ByIndex,   "EOS_Achievements_CopyAchievementDefinitionV2ByIndex")
		GS_EOS_BIND(Achievements_CopyPlayerAchievementByAchievementId, "EOS_Achievements_CopyPlayerAchievementByAchievementId")
		GS_EOS_BIND(Achievements_DefinitionV2_Release,                 "EOS_Achievements_DefinitionV2_Release")
		GS_EOS_BIND(Achievements_GetAchievementDefinitionCount,        "EOS_Achievements_GetAchievementDefinitionCount")
		GS_EOS_BIND(Achievements_PlayerAchievement_Release,            "EOS_Achievements_PlayerAchievement_Release")
		GS_EOS_BIND(Achievements_QueryDefinitions,                     "EOS_Achievements_QueryDefinitions")
		GS_EOS_BIND(Achievements_QueryPlayerAchievements,              "EOS_Achievements_QueryPlayerAchievements")
		GS_EOS_BIND(Achievements_UnlockAchievements,                   "EOS_Achievements_UnlockAchievements")
		GS_EOS_BIND(Auth_CopyUserAuthToken,                            "EOS_Auth_CopyUserAuthToken")
		GS_EOS_BIND(Auth_Login,                                        "EOS_Auth_Login")
		GS_EOS_BIND(Auth_Token_Release,                                "EOS_Auth_Token_Release")
		GS_EOS_BIND(Connect_CopyProductUserInfo,                       "EOS_Connect_CopyProductUserInfo")
		GS_EOS_BIND(Connect_CreateUser,                                "EOS_Connect_CreateUser")
		GS_EOS_BIND(Connect_ExternalAccountInfo_Release,               "EOS_Connect_ExternalAccountInfo_Release")
		GS_EOS_BIND(Connect_GetExternalAccountMapping,                 "EOS_Connect_GetExternalAccountMapping")
		GS_EOS_BIND(Connect_Login,                                     "EOS_Connect_Login")
		GS_EOS_BIND(Connect_QueryExternalAccountMappings,              "EOS_Connect_QueryExternalAccountMappings")
		GS_EOS_BIND(Connect_QueryProductUserIdMappings,                "EOS_Connect_QueryProductUserIdMappings")
		GS_EOS_BIND(Ecom_CopyEntitlementByIndex,                       "EOS_Ecom_CopyEntitlementByIndex")
		GS_EOS_BIND(Ecom_Entitlement_Release,                          "EOS_Ecom_Entitlement_Release")
		GS_EOS_BIND(Ecom_GetEntitlementsByNameCount,                   "EOS_Ecom_GetEntitlementsByNameCount")
		GS_EOS_BIND(Ecom_GetEntitlementsCount,                         "EOS_Ecom_GetEntitlementsCount")
		GS_EOS_BIND(Ecom_QueryEntitlements,                            "EOS_Ecom_QueryEntitlements")
		GS_EOS_BIND(EpicAccountId_FromString,                          "EOS_EpicAccountId_FromString")
		GS_EOS_BIND(EpicAccountId_IsValid,                             "EOS_EpicAccountId_IsValid")
		GS_EOS_BIND(EpicAccountId_ToString,                            "EOS_EpicAccountId_ToString")
		GS_EOS_BIND(EResult_ToString,                                  "EOS_EResult_ToString")
		GS_EOS_BIND(Friends_AddNotifyFriendsUpdate,                    "EOS_Friends_AddNotifyFriendsUpdate")
		GS_EOS_BIND(Friends_GetFriendAtIndex,                          "EOS_Friends_GetFriendAtIndex")
		GS_EOS_BIND(Friends_GetFriendsCount,                           "EOS_Friends_GetFriendsCount")
		GS_EOS_BIND(Friends_GetStatus,                                 "EOS_Friends_GetStatus")
		GS_EOS_BIND(Friends_QueryFriends,                              "EOS_Friends_QueryFriends")
		GS_EOS_BIND(Friends_RemoveNotifyFriendsUpdate,                 "EOS_Friends_RemoveNotifyFriendsUpdate")
		GS_EOS_BIND(Initialize,                                        "EOS_Initialize")
		GS_EOS_BIND(Leaderboards_CopyLeaderboardDefinitionByIndex,     "EOS_Leaderboards_CopyLeaderboardDefinitionByIndex")
		GS_EOS_BIND(Leaderboards_CopyLeaderboardRecordByIndex,         "EOS_Leaderboards_CopyLeaderboardRecordByIndex")
		GS_EOS_BIND(Leaderboards_Definition_Release,                   "EOS_Leaderboards_Definition_Release")
		GS_EOS_BIND(Leaderboards_GetLeaderboardDefinitionCount,        "EOS_Leaderboards_GetLeaderboardDefinitionCount")
		GS_EOS_BIND(Leaderboards_GetLeaderboardRecordCount,            "EOS_Leaderboards_GetLeaderboardRecordCount")
		GS_EOS_BIND(Leaderboards_LeaderboardRecord_Release,            "EOS_Leaderboards_LeaderboardRecord_Release")
		GS_EOS_BIND(Leaderboards_QueryLeaderboardDefinitions,          "EOS_Leaderboards_QueryLeaderboardDefinitions")
		GS_EOS_BIND(Leaderboards_QueryLeaderboardRanks,                "EOS_Leaderboards_QueryLeaderboardRanks")
		GS_EOS_BIND(LobbyDetails_CopyAttributeByIndex,                 "EOS_LobbyDetails_CopyAttributeByIndex")
		GS_EOS_BIND(LobbyDetails_CopyAttributeByKey,                   "EOS_LobbyDetails_CopyAttributeByKey")
		GS_EOS_BIND(LobbyDetails_CopyInfo,                             "EOS_LobbyDetails_CopyInfo")
		GS_EOS_BIND(LobbyDetails_CopyMemberAttributeByKey,             "EOS_LobbyDetails_CopyMemberAttributeByKey")
		GS_EOS_BIND(LobbyDetails_GetAttributeCount,                    "EOS_LobbyDetails_GetAttributeCount")
		GS_EOS_BIND(LobbyDetails_GetLobbyOwner,                        "EOS_LobbyDetails_GetLobbyOwner")
		GS_EOS_BIND(LobbyDetails_Info_Release,                         "EOS_LobbyDetails_Info_Release")
		GS_EOS_BIND(LobbyDetails_Release,                              "EOS_LobbyDetails_Release")
		GS_EOS_BIND(LobbyModification_AddAttribute,                    "EOS_LobbyModification_AddAttribute")
		GS_EOS_BIND(LobbyModification_AddMemberAttribute,              "EOS_LobbyModification_AddMemberAttribute")
		GS_EOS_BIND(LobbyModification_Release,                         "EOS_LobbyModification_Release")
		GS_EOS_BIND(LobbyModification_SetInvitesAllowed,               "EOS_LobbyModification_SetInvitesAllowed")
		GS_EOS_BIND(LobbyModification_SetMaxMembers,                   "EOS_LobbyModification_SetMaxMembers")
		GS_EOS_BIND(LobbyModification_SetPermissionLevel,              "EOS_LobbyModification_SetPermissionLevel")
		GS_EOS_BIND(LobbySearch_CopySearchResultByIndex,               "EOS_LobbySearch_CopySearchResultByIndex")
		GS_EOS_BIND(LobbySearch_Find,                                  "EOS_LobbySearch_Find")
		GS_EOS_BIND(LobbySearch_GetSearchResultCount,                  "EOS_LobbySearch_GetSearchResultCount")
		GS_EOS_BIND(LobbySearch_Release,                               "EOS_LobbySearch_Release")
		GS_EOS_BIND(LobbySearch_SetLobbyId,                            "EOS_LobbySearch_SetLobbyId")
		GS_EOS_BIND(LobbySearch_SetParameter,                          "EOS_LobbySearch_SetParameter")
		GS_EOS_BIND(Lobby_AddNotifyLobbyInviteAccepted,                "EOS_Lobby_AddNotifyLobbyInviteAccepted")
		GS_EOS_BIND(Lobby_AddNotifyLobbyInviteReceived,                "EOS_Lobby_AddNotifyLobbyInviteReceived")
		GS_EOS_BIND(Lobby_AddNotifyLobbyMemberStatusReceived,          "EOS_Lobby_AddNotifyLobbyMemberStatusReceived")
		GS_EOS_BIND(Lobby_Attribute_Release,                           "EOS_Lobby_Attribute_Release")
		GS_EOS_BIND(Lobby_CopyLobbyDetailsHandle,                      "EOS_Lobby_CopyLobbyDetailsHandle")
		GS_EOS_BIND(Lobby_CopyLobbyDetailsHandleByInviteId,            "EOS_Lobby_CopyLobbyDetailsHandleByInviteId")
		GS_EOS_BIND(Lobby_CreateLobby,                                 "EOS_Lobby_CreateLobby")
		GS_EOS_BIND(Lobby_CreateLobbySearch,                           "EOS_Lobby_CreateLobbySearch")
		GS_EOS_BIND(Lobby_DestroyLobby,                                "EOS_Lobby_DestroyLobby")
		GS_EOS_BIND(Lobby_GetInviteCount,                              "EOS_Lobby_GetInviteCount")
		GS_EOS_BIND(Lobby_GetInviteIdByIndex,                          "EOS_Lobby_GetInviteIdByIndex")
		GS_EOS_BIND(Lobby_JoinLobby,                                   "EOS_Lobby_JoinLobby")
		GS_EOS_BIND(Lobby_JoinLobbyById,                               "EOS_Lobby_JoinLobbyById")
		GS_EOS_BIND(Lobby_LeaveLobby,                                  "EOS_Lobby_LeaveLobby")
		GS_EOS_BIND(Lobby_QueryInvites,                                "EOS_Lobby_QueryInvites")
		GS_EOS_BIND(Lobby_RejectInvite,                                "EOS_Lobby_RejectInvite")
		GS_EOS_BIND(Lobby_RemoveNotifyLobbyInviteAccepted,             "EOS_Lobby_RemoveNotifyLobbyInviteAccepted")
		GS_EOS_BIND(Lobby_RemoveNotifyLobbyInviteReceived,             "EOS_Lobby_RemoveNotifyLobbyInviteReceived")
		GS_EOS_BIND(Lobby_RemoveNotifyLobbyMemberStatusReceived,       "EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived")
		GS_EOS_BIND(Lobby_SendInvite,                                  "EOS_Lobby_SendInvite")
		GS_EOS_BIND(Lobby_UpdateLobby,                                 "EOS_Lobby_UpdateLobby")
		GS_EOS_BIND(Lobby_UpdateLobbyModification,                     "EOS_Lobby_UpdateLobbyModification")
		GS_EOS_BIND(Logging_SetCallback,                               "EOS_Logging_SetCallback")
		GS_EOS_BIND(Logging_SetLogLevel,                               "EOS_Logging_SetLogLevel")
		GS_EOS_BIND(P2P_AcceptConnection,                              "EOS_P2P_AcceptConnection")
		GS_EOS_BIND(P2P_AddNotifyPeerConnectionClosed,                 "EOS_P2P_AddNotifyPeerConnectionClosed")
		GS_EOS_BIND(P2P_AddNotifyPeerConnectionRequest,                "EOS_P2P_AddNotifyPeerConnectionRequest")
		GS_EOS_BIND(P2P_CloseConnections,                              "EOS_P2P_CloseConnections")
		GS_EOS_BIND(P2P_GetNextReceivedPacketSize,                     "EOS_P2P_GetNextReceivedPacketSize")
		GS_EOS_BIND(P2P_ReceivePacket,                                 "EOS_P2P_ReceivePacket")
		GS_EOS_BIND(P2P_RemoveNotifyPeerConnectionClosed,              "EOS_P2P_RemoveNotifyPeerConnectionClosed")
		GS_EOS_BIND(P2P_RemoveNotifyPeerConnectionRequest,             "EOS_P2P_RemoveNotifyPeerConnectionRequest")
		GS_EOS_BIND(P2P_SendPacket,                                    "EOS_P2P_SendPacket")
		GS_EOS_BIND(Platform_Create,                                   "EOS_Platform_Create")
		GS_EOS_BIND(Platform_GetAchievementsInterface,                 "EOS_Platform_GetAchievementsInterface")
		GS_EOS_BIND(Platform_GetAuthInterface,                         "EOS_Platform_GetAuthInterface")
		GS_EOS_BIND(Platform_GetConnectInterface,                      "EOS_Platform_GetConnectInterface")
		GS_EOS_BIND(Platform_GetEcomInterface,                         "EOS_Platform_GetEcomInterface")
		GS_EOS_BIND(Platform_GetFriendsInterface,                      "EOS_Platform_GetFriendsInterface")
		GS_EOS_BIND(Platform_GetLeaderboardsInterface,                 "EOS_Platform_GetLeaderboardsInterface")
		GS_EOS_BIND(Platform_GetLobbyInterface,                        "EOS_Platform_GetLobbyInterface")
		GS_EOS_BIND(Platform_GetP2PInterface,                          "EOS_Platform_GetP2PInterface")
		GS_EOS_BIND(Platform_GetPlayerDataStorageInterface,            "EOS_Platform_GetPlayerDataStorageInterface")
		GS_EOS_BIND(Platform_GetStatsInterface,                        "EOS_Platform_GetStatsInterface")
		GS_EOS_BIND(Platform_GetUserInfoInterface,                     "EOS_Platform_GetUserInfoInterface")
		GS_EOS_BIND(Platform_Release,                                  "EOS_Platform_Release")
		GS_EOS_BIND(Platform_Tick,                                     "EOS_Platform_Tick")
		GS_EOS_BIND(PlayerDataStorage_DeleteFile,                      "EOS_PlayerDataStorage_DeleteFile")
		GS_EOS_BIND(PlayerDataStorage_ReadFile,                        "EOS_PlayerDataStorage_ReadFile")
		GS_EOS_BIND(PlayerDataStorage_WriteFile,                       "EOS_PlayerDataStorage_WriteFile")
		GS_EOS_BIND(Platform_GetPresenceInterface,                     "EOS_Platform_GetPresenceInterface")
		GS_EOS_BIND(Presence_CopyPresence,                             "EOS_Presence_CopyPresence")
		GS_EOS_BIND(Presence_Info_Release,                             "EOS_Presence_Info_Release")
		GS_EOS_BIND(Presence_QueryPresence,                            "EOS_Presence_QueryPresence")
		GS_EOS_BIND(ProductUserId_FromString,                          "EOS_ProductUserId_FromString")
		GS_EOS_BIND(ProductUserId_IsValid,                             "EOS_ProductUserId_IsValid")
		GS_EOS_BIND(ProductUserId_ToString,                            "EOS_ProductUserId_ToString")
		GS_EOS_BIND(Shutdown,                                          "EOS_Shutdown")
		GS_EOS_BIND(Stats_CopyStatByIndex,                             "EOS_Stats_CopyStatByIndex")
		GS_EOS_BIND(Stats_GetStatsCount,                               "EOS_Stats_GetStatsCount")
		GS_EOS_BIND(Stats_IngestStat,                                  "EOS_Stats_IngestStat")
		GS_EOS_BIND(Stats_QueryStats,                                  "EOS_Stats_QueryStats")
		GS_EOS_BIND(Stats_Stat_Release,                                "EOS_Stats_Stat_Release")
		GS_EOS_BIND(UserInfo_BestDisplayName_Release,                  "EOS_UserInfo_BestDisplayName_Release")
		GS_EOS_BIND(UserInfo_CopyBestDisplayName,                      "EOS_UserInfo_CopyBestDisplayName")
		GS_EOS_BIND(UserInfo_CopyBestDisplayNameWithPlatform,          "EOS_UserInfo_CopyBestDisplayNameWithPlatform")
		GS_EOS_BIND(UserInfo_QueryUserInfo,                            "EOS_UserInfo_QueryUserInfo")

#undef GS_EOS_BIND

		if (MissingSymbols.Num() > 0)
		{
			// A library that resolves only partially is a version mismatch, not a usable SDK. Bind
			// nothing rather than leave a table that would crash on the first unbound call.
			UE_LOG(LogTemp, Error,
			       TEXT("GamingServices: EOS library '%s' is missing %d expected entry point(s) (%s%s). "
				       "EOS will be unavailable."),
			       *Library.GetLibraryName(), MissingSymbols.Num(),
			       *FString::Join(TArrayView<const FString>(MissingSymbols.GetData(), FMath::Min(MissingSymbols.Num(), 5)), TEXT(", ")),
			       MissingSymbols.Num() > 5 ? TEXT(", ...") : TEXT(""));

			GEOSApi = FEOSApi();
			Library.Unload();
			return false;
		}

		bEOSApiLoaded = true;
		UE_LOG(LogTemp, Log, TEXT("GamingServices: bound %d EOS entry points"), (int32)(sizeof(FEOSApi) / sizeof(void*)));
		return true;
	}

	void UnloadEOSApi()
	{
		GEOSApi = FEOSApi();
		EOSLibrary().Unload();
		bEOSApiLoaded = false;
	}
}

#endif // GS_WITH_EOS
