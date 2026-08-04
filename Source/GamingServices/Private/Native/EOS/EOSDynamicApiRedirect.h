#pragma once

#include "EOSDynamicApi.h"

#if defined(GS_WITH_EOS)

/**
 * Rewrites every EOS entry point this module uses onto the runtime symbol table, so that EOS call sites
 * need no knowledge of the fact that the SDK is loaded dynamically.
 *
 * Include ORDER matters and is enforced by the include above: the SDK headers and FEOSApi (whose members
 * are decltype'd from those declarations) are always processed before the first macro below. Nothing may
 * include an eos_*.h after this header.
 *
 * GENERATED — keep in sync with EOSDynamicApi.h and EOSDynamicApi.cpp.
 */
#define EOS_Achievements_CopyAchievementDefinitionV2ByIndex   ::GamingServices::GEOSApi.Achievements_CopyAchievementDefinitionV2ByIndex
#define EOS_Achievements_CopyPlayerAchievementByAchievementId ::GamingServices::GEOSApi.Achievements_CopyPlayerAchievementByAchievementId
#define EOS_Achievements_DefinitionV2_Release                 ::GamingServices::GEOSApi.Achievements_DefinitionV2_Release
#define EOS_Achievements_GetAchievementDefinitionCount        ::GamingServices::GEOSApi.Achievements_GetAchievementDefinitionCount
#define EOS_Achievements_PlayerAchievement_Release            ::GamingServices::GEOSApi.Achievements_PlayerAchievement_Release
#define EOS_Achievements_QueryDefinitions                     ::GamingServices::GEOSApi.Achievements_QueryDefinitions
#define EOS_Achievements_QueryPlayerAchievements              ::GamingServices::GEOSApi.Achievements_QueryPlayerAchievements
#define EOS_Achievements_UnlockAchievements                   ::GamingServices::GEOSApi.Achievements_UnlockAchievements
#define EOS_Auth_CopyUserAuthToken                            ::GamingServices::GEOSApi.Auth_CopyUserAuthToken
#define EOS_Auth_Login                                        ::GamingServices::GEOSApi.Auth_Login
#define EOS_Auth_Token_Release                                ::GamingServices::GEOSApi.Auth_Token_Release
#define EOS_Connect_CopyProductUserInfo                       ::GamingServices::GEOSApi.Connect_CopyProductUserInfo
#define EOS_Connect_CreateUser                                ::GamingServices::GEOSApi.Connect_CreateUser
#define EOS_Connect_ExternalAccountInfo_Release               ::GamingServices::GEOSApi.Connect_ExternalAccountInfo_Release
#define EOS_Connect_GetExternalAccountMapping                 ::GamingServices::GEOSApi.Connect_GetExternalAccountMapping
#define EOS_Connect_Login                                     ::GamingServices::GEOSApi.Connect_Login
#define EOS_Connect_QueryExternalAccountMappings              ::GamingServices::GEOSApi.Connect_QueryExternalAccountMappings
#define EOS_Connect_QueryProductUserIdMappings                ::GamingServices::GEOSApi.Connect_QueryProductUserIdMappings
#define EOS_Ecom_CopyEntitlementByIndex                       ::GamingServices::GEOSApi.Ecom_CopyEntitlementByIndex
#define EOS_Ecom_Entitlement_Release                          ::GamingServices::GEOSApi.Ecom_Entitlement_Release
#define EOS_Ecom_GetEntitlementsByNameCount                   ::GamingServices::GEOSApi.Ecom_GetEntitlementsByNameCount
#define EOS_Ecom_GetEntitlementsCount                         ::GamingServices::GEOSApi.Ecom_GetEntitlementsCount
#define EOS_Ecom_QueryEntitlements                            ::GamingServices::GEOSApi.Ecom_QueryEntitlements
#define EOS_EpicAccountId_FromString                          ::GamingServices::GEOSApi.EpicAccountId_FromString
#define EOS_EpicAccountId_IsValid                             ::GamingServices::GEOSApi.EpicAccountId_IsValid
#define EOS_EpicAccountId_ToString                            ::GamingServices::GEOSApi.EpicAccountId_ToString
#define EOS_EResult_ToString                                  ::GamingServices::GEOSApi.EResult_ToString
#define EOS_Friends_AddNotifyFriendsUpdate                    ::GamingServices::GEOSApi.Friends_AddNotifyFriendsUpdate
#define EOS_Friends_GetFriendAtIndex                          ::GamingServices::GEOSApi.Friends_GetFriendAtIndex
#define EOS_Friends_GetFriendsCount                           ::GamingServices::GEOSApi.Friends_GetFriendsCount
#define EOS_Friends_GetStatus                                 ::GamingServices::GEOSApi.Friends_GetStatus
#define EOS_Friends_QueryFriends                              ::GamingServices::GEOSApi.Friends_QueryFriends
#define EOS_Friends_RemoveNotifyFriendsUpdate                 ::GamingServices::GEOSApi.Friends_RemoveNotifyFriendsUpdate
#define EOS_Initialize                                        ::GamingServices::GEOSApi.Initialize
#define EOS_Leaderboards_CopyLeaderboardDefinitionByIndex     ::GamingServices::GEOSApi.Leaderboards_CopyLeaderboardDefinitionByIndex
#define EOS_Leaderboards_CopyLeaderboardRecordByIndex         ::GamingServices::GEOSApi.Leaderboards_CopyLeaderboardRecordByIndex
#define EOS_Leaderboards_Definition_Release                   ::GamingServices::GEOSApi.Leaderboards_Definition_Release
#define EOS_Leaderboards_GetLeaderboardDefinitionCount        ::GamingServices::GEOSApi.Leaderboards_GetLeaderboardDefinitionCount
#define EOS_Leaderboards_GetLeaderboardRecordCount            ::GamingServices::GEOSApi.Leaderboards_GetLeaderboardRecordCount
#define EOS_Leaderboards_LeaderboardRecord_Release            ::GamingServices::GEOSApi.Leaderboards_LeaderboardRecord_Release
#define EOS_Leaderboards_QueryLeaderboardDefinitions          ::GamingServices::GEOSApi.Leaderboards_QueryLeaderboardDefinitions
#define EOS_Leaderboards_QueryLeaderboardRanks                ::GamingServices::GEOSApi.Leaderboards_QueryLeaderboardRanks
#define EOS_LobbyDetails_CopyAttributeByIndex                 ::GamingServices::GEOSApi.LobbyDetails_CopyAttributeByIndex
#define EOS_LobbyDetails_CopyAttributeByKey                   ::GamingServices::GEOSApi.LobbyDetails_CopyAttributeByKey
#define EOS_LobbyDetails_CopyInfo                             ::GamingServices::GEOSApi.LobbyDetails_CopyInfo
#define EOS_LobbyDetails_CopyMemberAttributeByKey             ::GamingServices::GEOSApi.LobbyDetails_CopyMemberAttributeByKey
#define EOS_LobbyDetails_GetAttributeCount                    ::GamingServices::GEOSApi.LobbyDetails_GetAttributeCount
#define EOS_LobbyDetails_GetLobbyOwner                        ::GamingServices::GEOSApi.LobbyDetails_GetLobbyOwner
#define EOS_LobbyDetails_Info_Release                         ::GamingServices::GEOSApi.LobbyDetails_Info_Release
#define EOS_LobbyDetails_Release                              ::GamingServices::GEOSApi.LobbyDetails_Release
#define EOS_LobbyModification_AddAttribute                    ::GamingServices::GEOSApi.LobbyModification_AddAttribute
#define EOS_LobbyModification_AddMemberAttribute              ::GamingServices::GEOSApi.LobbyModification_AddMemberAttribute
#define EOS_LobbyModification_Release                         ::GamingServices::GEOSApi.LobbyModification_Release
#define EOS_LobbyModification_SetInvitesAllowed               ::GamingServices::GEOSApi.LobbyModification_SetInvitesAllowed
#define EOS_LobbyModification_SetMaxMembers                   ::GamingServices::GEOSApi.LobbyModification_SetMaxMembers
#define EOS_LobbyModification_SetPermissionLevel              ::GamingServices::GEOSApi.LobbyModification_SetPermissionLevel
#define EOS_LobbySearch_CopySearchResultByIndex               ::GamingServices::GEOSApi.LobbySearch_CopySearchResultByIndex
#define EOS_LobbySearch_Find                                  ::GamingServices::GEOSApi.LobbySearch_Find
#define EOS_LobbySearch_GetSearchResultCount                  ::GamingServices::GEOSApi.LobbySearch_GetSearchResultCount
#define EOS_LobbySearch_Release                               ::GamingServices::GEOSApi.LobbySearch_Release
#define EOS_LobbySearch_SetLobbyId                            ::GamingServices::GEOSApi.LobbySearch_SetLobbyId
#define EOS_LobbySearch_SetParameter                          ::GamingServices::GEOSApi.LobbySearch_SetParameter
#define EOS_Lobby_AddNotifyLobbyInviteAccepted                ::GamingServices::GEOSApi.Lobby_AddNotifyLobbyInviteAccepted
#define EOS_Lobby_AddNotifyLobbyInviteReceived                ::GamingServices::GEOSApi.Lobby_AddNotifyLobbyInviteReceived
#define EOS_Lobby_AddNotifyLobbyMemberStatusReceived          ::GamingServices::GEOSApi.Lobby_AddNotifyLobbyMemberStatusReceived
#define EOS_Lobby_Attribute_Release                           ::GamingServices::GEOSApi.Lobby_Attribute_Release
#define EOS_Lobby_CopyLobbyDetailsHandle                      ::GamingServices::GEOSApi.Lobby_CopyLobbyDetailsHandle
#define EOS_Lobby_CopyLobbyDetailsHandleByInviteId            ::GamingServices::GEOSApi.Lobby_CopyLobbyDetailsHandleByInviteId
#define EOS_Lobby_CreateLobby                                 ::GamingServices::GEOSApi.Lobby_CreateLobby
#define EOS_Lobby_CreateLobbySearch                           ::GamingServices::GEOSApi.Lobby_CreateLobbySearch
#define EOS_Lobby_DestroyLobby                                ::GamingServices::GEOSApi.Lobby_DestroyLobby
#define EOS_Lobby_GetInviteCount                              ::GamingServices::GEOSApi.Lobby_GetInviteCount
#define EOS_Lobby_GetInviteIdByIndex                          ::GamingServices::GEOSApi.Lobby_GetInviteIdByIndex
#define EOS_Lobby_JoinLobby                                   ::GamingServices::GEOSApi.Lobby_JoinLobby
#define EOS_Lobby_JoinLobbyById                               ::GamingServices::GEOSApi.Lobby_JoinLobbyById
#define EOS_Lobby_LeaveLobby                                  ::GamingServices::GEOSApi.Lobby_LeaveLobby
#define EOS_Lobby_QueryInvites                                ::GamingServices::GEOSApi.Lobby_QueryInvites
#define EOS_Lobby_RejectInvite                                ::GamingServices::GEOSApi.Lobby_RejectInvite
#define EOS_Lobby_RemoveNotifyLobbyInviteAccepted             ::GamingServices::GEOSApi.Lobby_RemoveNotifyLobbyInviteAccepted
#define EOS_Lobby_RemoveNotifyLobbyInviteReceived             ::GamingServices::GEOSApi.Lobby_RemoveNotifyLobbyInviteReceived
#define EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived       ::GamingServices::GEOSApi.Lobby_RemoveNotifyLobbyMemberStatusReceived
#define EOS_Lobby_SendInvite                                  ::GamingServices::GEOSApi.Lobby_SendInvite
#define EOS_Lobby_UpdateLobby                                 ::GamingServices::GEOSApi.Lobby_UpdateLobby
#define EOS_Lobby_UpdateLobbyModification                     ::GamingServices::GEOSApi.Lobby_UpdateLobbyModification
#define EOS_Logging_SetCallback                               ::GamingServices::GEOSApi.Logging_SetCallback
#define EOS_Logging_SetLogLevel                               ::GamingServices::GEOSApi.Logging_SetLogLevel
#define EOS_P2P_AcceptConnection                              ::GamingServices::GEOSApi.P2P_AcceptConnection
#define EOS_P2P_AddNotifyPeerConnectionClosed                 ::GamingServices::GEOSApi.P2P_AddNotifyPeerConnectionClosed
#define EOS_P2P_AddNotifyPeerConnectionRequest                ::GamingServices::GEOSApi.P2P_AddNotifyPeerConnectionRequest
#define EOS_P2P_CloseConnections                              ::GamingServices::GEOSApi.P2P_CloseConnections
#define EOS_P2P_GetNextReceivedPacketSize                     ::GamingServices::GEOSApi.P2P_GetNextReceivedPacketSize
#define EOS_P2P_ReceivePacket                                 ::GamingServices::GEOSApi.P2P_ReceivePacket
#define EOS_P2P_RemoveNotifyPeerConnectionClosed              ::GamingServices::GEOSApi.P2P_RemoveNotifyPeerConnectionClosed
#define EOS_P2P_RemoveNotifyPeerConnectionRequest             ::GamingServices::GEOSApi.P2P_RemoveNotifyPeerConnectionRequest
#define EOS_P2P_SendPacket                                    ::GamingServices::GEOSApi.P2P_SendPacket
#define EOS_Platform_Create                                   ::GamingServices::GEOSApi.Platform_Create
#define EOS_Platform_GetAchievementsInterface                 ::GamingServices::GEOSApi.Platform_GetAchievementsInterface
#define EOS_Platform_GetAuthInterface                         ::GamingServices::GEOSApi.Platform_GetAuthInterface
#define EOS_Platform_GetConnectInterface                      ::GamingServices::GEOSApi.Platform_GetConnectInterface
#define EOS_Platform_GetEcomInterface                         ::GamingServices::GEOSApi.Platform_GetEcomInterface
#define EOS_Platform_GetFriendsInterface                      ::GamingServices::GEOSApi.Platform_GetFriendsInterface
#define EOS_Platform_GetLeaderboardsInterface                 ::GamingServices::GEOSApi.Platform_GetLeaderboardsInterface
#define EOS_Platform_GetLobbyInterface                        ::GamingServices::GEOSApi.Platform_GetLobbyInterface
#define EOS_Platform_GetP2PInterface                          ::GamingServices::GEOSApi.Platform_GetP2PInterface
#define EOS_Platform_GetPlayerDataStorageInterface            ::GamingServices::GEOSApi.Platform_GetPlayerDataStorageInterface
#define EOS_Platform_GetStatsInterface                        ::GamingServices::GEOSApi.Platform_GetStatsInterface
#define EOS_Platform_GetUserInfoInterface                     ::GamingServices::GEOSApi.Platform_GetUserInfoInterface
#define EOS_Platform_Release                                  ::GamingServices::GEOSApi.Platform_Release
#define EOS_Platform_Tick                                     ::GamingServices::GEOSApi.Platform_Tick
#define EOS_PlayerDataStorage_DeleteFile                      ::GamingServices::GEOSApi.PlayerDataStorage_DeleteFile
#define EOS_PlayerDataStorage_ReadFile                        ::GamingServices::GEOSApi.PlayerDataStorage_ReadFile
#define EOS_PlayerDataStorage_WriteFile                       ::GamingServices::GEOSApi.PlayerDataStorage_WriteFile
#define EOS_Platform_GetPresenceInterface                     ::GamingServices::GEOSApi.Platform_GetPresenceInterface
#define EOS_Presence_CopyPresence                             ::GamingServices::GEOSApi.Presence_CopyPresence
#define EOS_Presence_Info_Release                             ::GamingServices::GEOSApi.Presence_Info_Release
#define EOS_Presence_QueryPresence                            ::GamingServices::GEOSApi.Presence_QueryPresence
#define EOS_ProductUserId_FromString                          ::GamingServices::GEOSApi.ProductUserId_FromString
#define EOS_ProductUserId_IsValid                             ::GamingServices::GEOSApi.ProductUserId_IsValid
#define EOS_ProductUserId_ToString                            ::GamingServices::GEOSApi.ProductUserId_ToString
#define EOS_Shutdown                                          ::GamingServices::GEOSApi.Shutdown
#define EOS_Stats_CopyStatByIndex                             ::GamingServices::GEOSApi.Stats_CopyStatByIndex
#define EOS_Stats_GetStatsCount                               ::GamingServices::GEOSApi.Stats_GetStatsCount
#define EOS_Stats_IngestStat                                  ::GamingServices::GEOSApi.Stats_IngestStat
#define EOS_Stats_QueryStats                                  ::GamingServices::GEOSApi.Stats_QueryStats
#define EOS_Stats_Stat_Release                                ::GamingServices::GEOSApi.Stats_Stat_Release
#define EOS_UserInfo_BestDisplayName_Release                  ::GamingServices::GEOSApi.UserInfo_BestDisplayName_Release
#define EOS_UserInfo_CopyBestDisplayName                      ::GamingServices::GEOSApi.UserInfo_CopyBestDisplayName
#define EOS_UserInfo_CopyBestDisplayNameWithPlatform          ::GamingServices::GEOSApi.UserInfo_CopyBestDisplayNameWithPlatform
#define EOS_UserInfo_QueryUserInfo                            ::GamingServices::GEOSApi.UserInfo_QueryUserInfo

#endif // GS_WITH_EOS
