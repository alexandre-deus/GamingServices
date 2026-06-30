#ifdef USE_STEAMWORKS

#include "Native/Steam/Interfaces/SteamLeaderboards.h"
#include "Native/Steam/SteamPlatformCore.h"
#include "SteamCallResultManager.h"

#include "steam/steam_api.h"

namespace GamingServices
{
	void FSteamLeaderboards::WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
												   TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamUserStats* SteamUserStats = ::SteamUserStats();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamUserStats,
			   TEXT("SteamworksGamingService: WriteLeaderboardScore called when "
					"service not ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Writing leaderboard score: %d to %s"), Score,
			   *LeaderboardId);

		FTCHARToUTF8 UTF8String(*LeaderboardId);
		const char* LeaderboardIdUTF8 = UTF8String.Get();

		SteamAPICall_t CallHandle = SteamUserStats->FindOrCreateLeaderboard(
			LeaderboardIdUTF8, k_ELeaderboardSortMethodDescending, k_ELeaderboardDisplayTypeNumeric);
		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: FindOrCreateLeaderboard issued, "
					"CallHandle=%llu"),
			   CallHandle);

		if (CallHandle == k_uAPICallInvalid)
		{
			UE_LOG(LogTemp, Error,
				   TEXT("SteamworksGamingService: Failed to create API call for "
						"leaderboard: %s"),
				   *LeaderboardId);
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		Core.GetCallResults().Add<LeaderboardFindResult_t>(
			CallHandle,
			[this, Score, Callback](const LeaderboardFindResult_t& Find, bool bIOFailure)
			{
				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: CallResult - WriteLeaderboardScore "
							"FindLeaderboard callback (Found=%d, IOFail=%d, Handle=%llu)"),
					   (int32)Find.m_bLeaderboardFound, (int32)bIOFailure, (uint64)Find.m_hSteamLeaderboard);
				if (bIOFailure || !Find.m_bLeaderboardFound)
				{
					if (Callback)
					{
						Callback(FGamingServiceResult(false));
					}
					return;
				}
				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: Proceeding to upload score to "
							"Leaderboard=%llu"),
					   (uint64)Find.m_hSteamLeaderboard);
				HandleUploadLeaderboardScore(Find.m_hSteamLeaderboard, Score, Callback);
			});
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Leaderboard write initiated for: %s"), *LeaderboardId);
	}

	void FSteamLeaderboards::QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
												  TFunction<void(const FLeaderboardResult&)> Callback)
	{
		ISteamUserStats* SteamUserStats = ::SteamUserStats();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamUserStats,
			   TEXT("SteamworksGamingService: QueryLeaderboardPage called when "
					"service not ready"));

		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: Querying leaderboard page: %s "
					"(Start: %d, Limit: %d)"),
			   *LeaderboardId, ContinuationToken, Limit);

		FTCHARToUTF8 UTF8String(*LeaderboardId);
		const char* LeaderboardIdUTF8 = UTF8String.Get();

		SteamAPICall_t CallHandle = SteamUserStats->FindLeaderboard(LeaderboardIdUTF8);
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: FindLeaderboard issued, CallHandle=%llu"), CallHandle);

		if (CallHandle == k_uAPICallInvalid)
		{
			UE_LOG(LogTemp, Error,
				   TEXT("SteamworksGamingService: Failed to create API call for "
						"leaderboard query: %s"),
				   *LeaderboardId);
			if (Callback)
			{
				Callback(FLeaderboardResult(false, LeaderboardId, TArray<FLeaderboardEntry>(), 0, -1, 0));
			}
			return;
		}

		Core.GetCallResults().Add<LeaderboardFindResult_t>(
			CallHandle,
			[this, LeaderboardId, Limit, ContinuationToken, Callback](const LeaderboardFindResult_t& Find,
																	  bool bIOFailure)
			{
				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: CallResult - QueryLeaderboardPage "
							"FindLeaderboard callback (Found=%d, IOFail=%d, Handle=%llu)"),
					   (int32)Find.m_bLeaderboardFound, (int32)bIOFailure, (uint64)Find.m_hSteamLeaderboard);
				if (bIOFailure || !Find.m_bLeaderboardFound)
				{
					if (Callback)
					{
						Callback(FLeaderboardResult(false, LeaderboardId, TArray<FLeaderboardEntry>(), 0, -1, 0));
					}
					return;
				}
				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: Proceeding to download entries for "
							"Leaderboard=%llu, Start=%d, Limit=%d"),
					   (uint64)Find.m_hSteamLeaderboard, ContinuationToken, Limit);
				HandleDownloadLeaderboardEntries(Find.m_hSteamLeaderboard, LeaderboardId, Limit, ContinuationToken,
												 Callback);
			});
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Leaderboard query initiated for: %s"), *LeaderboardId);
	}

	void FSteamLeaderboards::HandleUploadLeaderboardScore(uint64 Leaderboard, int32 Score,
														  TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamUserStats* SteamUserStats = ::SteamUserStats();

		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: UploadLeaderboardScore begin "
					"(Leaderboard=%llu, Score=%d)"),
			   (uint64)Leaderboard, Score);
		SteamAPICall_t UploadHandle = SteamUserStats->UploadLeaderboardScore(
			Leaderboard, k_ELeaderboardUploadScoreMethodKeepBest, Score, nullptr, 0);
		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: UploadLeaderboardScore issued, "
					"CallHandle=%llu"),
			   UploadHandle);
		Core.GetCallResults().Add<LeaderboardScoreUploaded_t>(
			UploadHandle,
			[Callback](const LeaderboardScoreUploaded_t& Up, bool bUploadFailure)
			{
				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: CallResult - "
							"UploadLeaderboardScore callback (SuccessFlag=%d, "
							"Score=%d, NewRank=%d, IOFail=%d)"),
					   (int32)Up.m_bSuccess, (int32)Up.m_nScore, (int32)Up.m_nGlobalRankNew, (int32)bUploadFailure);
				if (Callback)
				{
					Callback(FGamingServiceResult(!bUploadFailure && Up.m_bSuccess));
				}
			});
	}

	void FSteamLeaderboards::HandleDownloadLeaderboardEntries(uint64 Leaderboard, const FString& LeaderboardId, int32 Limit,
															  int32 ContinuationToken, TFunction<void(const FLeaderboardResult&)> Callback)
	{
		ISteamUserStats* SteamUserStats = ::SteamUserStats();
		ISteamFriends* SteamFriends = ::SteamFriends();

		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: DownloadLeaderboardEntries begin "
					"(Leaderboard=%llu, Start=%d, Limit=%d)"),
			   (uint64)Leaderboard, ContinuationToken, Limit);

		SteamAPICall_t DownloadHandle = SteamUserStats->DownloadLeaderboardEntries(
			Leaderboard, k_ELeaderboardDataRequestGlobal, ContinuationToken, ContinuationToken + Limit);

		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: DownloadLeaderboardEntries issued, "
					"CallHandle=%llu"),
			   DownloadHandle);

		Core.GetCallResults().Add<LeaderboardScoresDownloaded_t>(
			DownloadHandle,
			[this, SteamUserStats, SteamFriends, LeaderboardId, Limit, ContinuationToken, Callback](const LeaderboardScoresDownloaded_t& Dl,
																	  bool bDownloadFailure)
			{
				if (bDownloadFailure)
				{
					if (Callback)
					{
						Callback(FLeaderboardResult(false, LeaderboardId, TArray<FLeaderboardEntry>(), 0, -1, 0));
					}
					return;
				}

				TArray<FLeaderboardEntry> Entries;
				int32 EntryCount = FMath::Min((int32)Dl.m_cEntryCount, Limit);

				for (int32 i = 0; i < EntryCount; ++i)
				{
					LeaderboardEntry_t Entry;
					if (!SteamUserStats->GetDownloadedLeaderboardEntry(Dl.m_hSteamLeaderboardEntries, i, &Entry,
																	   nullptr, 0))
					{
						UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Failed to get entry %d"), i);
						continue;
					}

					FLeaderboardEntry GameEntry;
					GameEntry.UserId = FString::Printf(TEXT("%llu"), Entry.m_steamIDUser.ConvertToUint64());
					GameEntry.Score = Entry.m_nScore;
					GameEntry.Rank = Entry.m_nGlobalRank;

					if (SteamFriends)
					{
						const char* Persona = SteamFriends->GetFriendPersonaName(Entry.m_steamIDUser);
						GameEntry.DisplayName = Persona ? UTF8_TO_TCHAR(Persona) : TEXT("Unknown");
					}
					else
					{
						GameEntry.DisplayName = TEXT("Unknown");
					}

					UE_LOG(LogTemp, Log,
						   TEXT("SteamworksGamingService: Entry %d: %s (%s) Score=%d "
								"Rank=%d"),
						   i, *GameEntry.DisplayName, *GameEntry.UserId, GameEntry.Score, GameEntry.Rank);

					Entries.Add(GameEntry);
				}

				FLeaderboardResult Result(true, LeaderboardId, Entries, Dl.m_cEntryCount);
				if (Entries.Num() > 0)
				{
					Result.ContinuationToken = ContinuationToken + Entries.Num() + 1;
				}
				else
				{
					Result.ContinuationToken = -1;
				}

				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: Completed - Total=%d, "
							"Returned=%d, NextToken=%d"),
					   (int32)Dl.m_cEntryCount, Entries.Num(), Result.ContinuationToken);

				if (Callback)
				{
					Callback(Result);
				}
			});
	}
}

#endif // USE_STEAMWORKS
