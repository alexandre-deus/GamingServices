#ifdef GS_WITH_STEAM

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

		// FindLeaderboard (not FindOrCreate): a leaderboard that was not pre-defined must fail the
		// write, matching the EOS contract (EOS rejects an unknown leaderboard definition). Using
		// FindOrCreate would silently create the board and report success for a bogus id.
		SteamAPICall_t CallHandle = SteamUserStats->FindLeaderboard(LeaderboardIdUTF8);
		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: FindLeaderboard (for write) issued, "
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

	void FSteamLeaderboards::QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 Offset,
												  TFunction<void(const FLeaderboardResult&)> Callback)
	{
		ISteamUserStats* SteamUserStats = ::SteamUserStats();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamUserStats,
			   TEXT("SteamworksGamingService: QueryLeaderboardPage called when "
					"service not ready"));

		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: Querying leaderboard page: %s "
					"(Offset: %d, Limit: %d)"),
			   *LeaderboardId, Offset, Limit);

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
			[this, LeaderboardId, Limit, Offset, Callback](const LeaderboardFindResult_t& Find,
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
							"Leaderboard=%llu, Offset=%d, Limit=%d"),
					   (uint64)Find.m_hSteamLeaderboard, Offset, Limit);
				HandleDownloadLeaderboardEntries(Find.m_hSteamLeaderboard, LeaderboardId, Limit, Offset,
												 Callback);
			});
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Leaderboard query initiated for: %s"), *LeaderboardId);
	}

	void FSteamLeaderboards::QueryLeaderboardUserRank(const FString& LeaderboardId,
													  TFunction<void(const FLeaderboardResult&)> Callback)
	{
		ISteamUserStats* SteamUserStats = ::SteamUserStats();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamUserStats,
			   TEXT("SteamworksGamingService: QueryLeaderboardUserRank called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Querying own rank on leaderboard: %s"), *LeaderboardId);

		FTCHARToUTF8 UTF8String(*LeaderboardId);
		SteamAPICall_t CallHandle = SteamUserStats->FindLeaderboard(UTF8String.Get());

		if (CallHandle == k_uAPICallInvalid)
		{
			if (Callback)
			{
				Callback(FLeaderboardResult(false, LeaderboardId, TArray<FLeaderboardEntry>(), 0, -1, 0));
			}
			return;
		}

		Core.GetCallResults().Add<LeaderboardFindResult_t>(
			CallHandle,
			[this, LeaderboardId, Callback](const LeaderboardFindResult_t& Find, bool bIOFailure)
			{
				if (bIOFailure || !Find.m_bLeaderboardFound)
				{
					if (Callback)
					{
						Callback(FLeaderboardResult(false, LeaderboardId, TArray<FLeaderboardEntry>(), 0, -1, 0));
					}
					return;
				}
				HandleDownloadUserRank(Find.m_hSteamLeaderboard, LeaderboardId, Callback);
			});
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
															  int32 Offset, TFunction<void(const FLeaderboardResult&)> Callback)
	{
		ISteamUserStats* SteamUserStats = ::SteamUserStats();
		ISteamFriends* SteamFriends = ::SteamFriends();

		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: DownloadLeaderboardEntries begin "
					"(Leaderboard=%llu, Offset=%d, Limit=%d)"),
			   (uint64)Leaderboard, Offset, Limit);

		// The global entry count is the full board size (the EOS contract's TotalEntries). It is NOT
		// the number of rows this page downloads.
		const int32 TotalEntries = SteamUserStats->GetLeaderboardEntryCount(Leaderboard);

		// Offset is a 0-based start index; Steam's global range is 1-based inclusive. Map offset N ->
		// ranks [N+1 .. N+Limit] so offset 0 yields the first Limit entries.
		const int32 RangeStart = Offset + 1;
		const int32 RangeEnd = Offset + Limit;
		SteamAPICall_t DownloadHandle = SteamUserStats->DownloadLeaderboardEntries(
			Leaderboard, k_ELeaderboardDataRequestGlobal, RangeStart, RangeEnd);

		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: DownloadLeaderboardEntries issued, "
					"CallHandle=%llu"),
			   DownloadHandle);

		Core.GetCallResults().Add<LeaderboardScoresDownloaded_t>(
			DownloadHandle,
			[SteamUserStats, SteamFriends, LeaderboardId, Limit, Offset, TotalEntries, Callback](const LeaderboardScoresDownloaded_t& Dl,
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

					Entries.Add(GameEntry);
				}

				// TotalEntries is the global board size; NextOffset is the next 0-based start index, or
				// -1 once the end of the board is reached — matching the EOS contract.
				FLeaderboardResult Result(true, LeaderboardId, Entries, TotalEntries);
				const int32 NextIndex = Offset + Entries.Num();
				Result.NextOffset = (Entries.Num() > 0 && NextIndex < TotalEntries) ? NextIndex : -1;

				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: Completed - Total=%d, Returned=%d, NextOffset=%d"),
					   TotalEntries, Entries.Num(), Result.NextOffset);

				if (Callback)
				{
					Callback(Result);
				}
			});
	}

	void FSteamLeaderboards::HandleDownloadUserRank(uint64 Leaderboard, const FString& LeaderboardId,
												   TFunction<void(const FLeaderboardResult&)> Callback)
	{
		ISteamUserStats* SteamUserStats = ::SteamUserStats();

		// AroundUser range [0,0] returns exactly the caller's own entry — this locates the user on a
		// board too large to page to. UserRank stays -1 when the user has no entry on the board.
		const int32 TotalEntries = SteamUserStats->GetLeaderboardEntryCount(Leaderboard);
		SteamAPICall_t DownloadHandle = SteamUserStats->DownloadLeaderboardEntries(
			Leaderboard, k_ELeaderboardDataRequestGlobalAroundUser, 0, 0);

		Core.GetCallResults().Add<LeaderboardScoresDownloaded_t>(
			DownloadHandle,
			[SteamUserStats, LeaderboardId, TotalEntries, Callback](const LeaderboardScoresDownloaded_t& Dl,
																	bool bDownloadFailure)
			{
				FLeaderboardResult Result(!bDownloadFailure, LeaderboardId, TArray<FLeaderboardEntry>(), TotalEntries);
				if (!bDownloadFailure && Dl.m_cEntryCount > 0)
				{
					LeaderboardEntry_t Entry;
					if (SteamUserStats->GetDownloadedLeaderboardEntry(Dl.m_hSteamLeaderboardEntries, 0, &Entry, nullptr, 0))
					{
						Result.UserRank = Entry.m_nGlobalRank;
						Result.UserScore = Entry.m_nScore;
					}
				}
				UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Caller rank=%d score=%d"),
					   Result.UserRank, Result.UserScore);
				if (Callback)
				{
					Callback(Result);
				}
			});
	}
}

#endif // GS_WITH_STEAM
