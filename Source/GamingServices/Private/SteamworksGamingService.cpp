#ifdef USE_STEAMWORKS

#include "SteamworksGamingService.h"
#include "Engine/World.h"
#include "HAL/CriticalSection.h"

#include "steam/steam_api.h"

class FSteamworksGamingService::FSteamworksGamingServiceImpl
{
public:
    FSteamworksGamingServiceImpl(FSteamworksGamingService* InOwner)
        : Owner(InOwner)
        , bIsInitialized(false)
        , bIsLoggedIn(false)
    {
    }

    ~FSteamworksGamingServiceImpl()
    {
        ShutdownSteamworks();
    }

    bool Connect(const FSteamworksInitOptions& /*Opts*/)
    {
        UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Starting initialization..."));

        InitializeSteamworks();

        if (bIsInitialized)
        {
            UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Initialization completed successfully"));
            return true;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Initialization failed"));
            return false;
        }
    }

    void Shutdown()
    {
        UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Starting shutdown..."));

        ShutdownSteamworks();

        bIsInitialized = false;
        bIsLoggedIn = false;
        UserId.Empty();
        DisplayName.Empty();

        UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Shutdown completed"));
    }

    void UnlockAchievement(const FString& AchievementId,
                           TFunction<void(const FGamingServiceResult&)> Callback)
    {
        checkf(bIsInitialized && bIsLoggedIn && SteamUserStats, TEXT("SteamworksGamingService: UnlockAchievement called when service not ready"));

        UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Unlocking achievement: %s"), *AchievementId);

        // Convert FString to const char*
        FTCHARToUTF8 UTF8String(*AchievementId);
        const char* AchievementIdUTF8 = UTF8String.Get();

        // Check if achievement exists
        bool bAchievementExists = SteamUserStats->GetAchievement(AchievementIdUTF8, nullptr);
        if (!bAchievementExists)
        {
            UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Achievement does not exist: %s"), *AchievementId);
            if (Callback) { Callback(FGamingServiceResult(false)); }
            return;
        }

        // Check if already unlocked
        bool bAlreadyUnlocked = false;
        SteamUserStats->GetAchievement(AchievementIdUTF8, &bAlreadyUnlocked);
        if (bAlreadyUnlocked)
        {
            UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Achievement already unlocked: %s"), *AchievementId);
            // Should this be true or false?
            if (Callback) { Callback(FGamingServiceResult(false)); }
            return;
        }

        // Unlock achievement
        bool bSuccess = SteamUserStats->SetAchievement(AchievementIdUTF8);

        if (bSuccess)
        {
            // Store achievement for upload
            SteamUserStats->StoreStats();
            UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Achievement unlocked successfully: %s"), *AchievementId);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to unlock achievement: %s"), *AchievementId);
        }

        if (Callback) { Callback(FGamingServiceResult(bSuccess)); }
    }

    void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback)
    {
        checkf(bIsInitialized && bIsLoggedIn && SteamUserStats, TEXT("SteamworksGamingService: QueryAchievements called when service not ready"));

        UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Querying achievements..."));

        TArray<FGameAchievement> Achievements;

        // Get number of achievements
        uint32 AchievementCount = SteamUserStats->GetNumAchievements();

        for (uint32 i = 0; i < AchievementCount; ++i)
        {
            FGameAchievement GameAchievement;

            // Get achievement API name
            const char* AchievementId = SteamUserStats->GetAchievementName(i);
            if (AchievementId)
            {
                GameAchievement.Id = UTF8_TO_TCHAR(AchievementId);

                // Get achievement display name
                const char* AchievementDisplayName = SteamUserStats->GetAchievementDisplayAttribute(AchievementId, "name");
                GameAchievement.DisplayName = AchievementDisplayName ? UTF8_TO_TCHAR(AchievementDisplayName) : GameAchievement.Id;

                // Get achievement description
                const char* Description = SteamUserStats->GetAchievementDisplayAttribute(AchievementId, "desc");
                GameAchievement.Description = Description ? UTF8_TO_TCHAR(Description) : TEXT("");

                // Check if achievement is unlocked
                bool bUnlocked = false;
                SteamUserStats->GetAchievement(AchievementId, &bUnlocked);
                GameAchievement.bIsUnlocked = bUnlocked;

                // Get achievement progress (if applicable)
                float Progress = 0.0f;
                SteamUserStats->GetAchievementAchievedPercent(AchievementId, &Progress);
                GameAchievement.Progress = Progress;

                Achievements.Add(GameAchievement);
            }
        }

        if (Callback) { Callback(FAchievementsQueryResult(true, Achievements)); }

        UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Queried %d achievements"), Achievements.Num());
    }

    void WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
                               TFunction<void(const FGamingServiceResult&)> Callback)
    {
        checkf(bIsInitialized && bIsLoggedIn && SteamUserStats, TEXT("SteamworksGamingService: WriteLeaderboardScore called when service not ready"));

        UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Writing leaderboard score: %d to %s"), Score, *LeaderboardId);

        // Convert FString to const char*
        FTCHARToUTF8 UTF8String(*LeaderboardId);
        const char* LeaderboardIdUTF8 = UTF8String.Get();

        // Find or create leaderboard
        SteamAPICall_t CallHandle = SteamUserStats->FindOrCreateLeaderboard(
            LeaderboardIdUTF8,
            k_ELeaderboardSortMethodDescending,
            k_ELeaderboardDisplayTypeNumeric
        );

        if (CallHandle == k_uAPICallInvalid)
        {
            UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to create API call for leaderboard: %s"), *LeaderboardId);
            if (Callback) { Callback(FGamingServiceResult(false)); }
            return;
        }

        CallResults.Add<LeaderboardFindResult_t>(CallHandle, [this, Score, Callback](const LeaderboardFindResult_t& Find, bool bIOFailure)
        {
            if (bIOFailure || !Find.m_bLeaderboardFound)
            {
                if (Callback) { Callback(FGamingServiceResult(false)); }
                return;
            }
            HandleUploadLeaderboardScore(Find.m_hSteamLeaderboard, Score, Callback);
        });
        UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Leaderboard write initiated for: %s"), *LeaderboardId);
    }

    void QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
                              TFunction<void(const FLeaderboardResult&)> Callback)
    {
        checkf(bIsInitialized && bIsLoggedIn && SteamUserStats, TEXT("SteamworksGamingService: QueryLeaderboardPage called when service not ready"));

        UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Querying leaderboard page: %s (Start: %d, Limit: %d)"), *LeaderboardId, ContinuationToken, Limit);

        // Convert FString to const char*
        FTCHARToUTF8 UTF8String(*LeaderboardId);
        const char* LeaderboardIdUTF8 = UTF8String.Get();

        // Find leaderboard
        SteamAPICall_t CallHandle = SteamUserStats->FindLeaderboard(LeaderboardIdUTF8);

        if (CallHandle == k_uAPICallInvalid)
        {
            UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to create API call for leaderboard query: %s"), *LeaderboardId);
            if (Callback) { Callback(FLeaderboardResult(false, LeaderboardId, TArray<FLeaderboardEntry>(), 0, -1, 0)); }
            return;
        }

        // Register completion using callresult manager (single-level lambda)
        CallResults.Add<LeaderboardFindResult_t>(CallHandle, [this, LeaderboardId, Limit, Callback](const LeaderboardFindResult_t& Find, bool bIOFailure)
        {
            if (bIOFailure || !Find.m_bLeaderboardFound)
            {
                if (Callback) { Callback(FLeaderboardResult(false, LeaderboardId, TArray<FLeaderboardEntry>(), 0, -1, 0)); }
                return;
            }
            HandleDownloadLeaderboardEntries(Find.m_hSteamLeaderboard, LeaderboardId, Limit, ContinuationToken, Callback);
        });
        UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Leaderboard query initiated for: %s"), *LeaderboardId);
    }

    void IngestStat(const FString& StatName, int32 Amount,
                    TFunction<void(const FGamingServiceResult&)> Callback)
    {
        checkf(bIsInitialized && bIsLoggedIn && SteamUserStats, TEXT("SteamworksGamingService: IngestStat called when service not ready"));

        FTCHARToUTF8 StatNameUTF8(*StatName);
        const char* StatId = StatNameUTF8.Get();

        // Steam supports integer and float stats. We'll try int first, then float.
        int32 CurrentInt = 0;
        bool bHasInt = SteamUserStats->GetStat(StatId, &CurrentInt);
        if (bHasInt)
        {
            SteamUserStats->SetStat(StatId, CurrentInt + Amount);
            SteamUserStats->StoreStats();
            if (Callback) { Callback(FGamingServiceResult(true)); }
            return;
        }

        float CurrentFloat = 0.0f;
        bool bHasFloat = SteamUserStats->GetStat(StatId, &CurrentFloat);
        if (bHasFloat)
        {
            SteamUserStats->SetStat(StatId, CurrentFloat + static_cast<float>(Amount));
            SteamUserStats->StoreStats();
            if (Callback) { Callback(FGamingServiceResult(true)); }
            return;
        }

        // If stat doesn't exist yet (not defined in Steam backend), nothing to do.
        UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Stat not found: %s"), *StatName);
        if (Callback) { Callback(FGamingServiceResult(false)); }
    }

    void QueryStat(const FString& StatName,
                   TFunction<void(const FStatQueryResult&)> Callback)
    {
        checkf(bIsInitialized && bIsLoggedIn && SteamUserStats, TEXT("SteamworksGamingService: QueryStat called when service not ready"));
        FTCHARToUTF8 NameUTF8(*StatName);
        const char* StatId = NameUTF8.Get();
        int32 Value = 0;
        if (SteamUserStats->GetStat(StatId, &Value))
        {
            if (Callback) { Callback(FStatQueryResult::Make(StatName, Value)); }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Stat not found: %s"), *StatName);
            FStatQueryResult Result; Result.bSuccess = false; Result.StatName = StatName;
            if (Callback) { Callback(Result); }
        }
    }

    void Tick()
    {
        if (bIsInitialized)
        {
            SteamAPI_RunCallbacks();
            CallResults.Pump();
        }
    }

    // Getters
    bool IsInitialized() const { return bIsInitialized; }
    bool IsConnected() const { return bIsInitialized; } // Steam is connected when initialized
    bool IsLoggedIn() const { return bIsLoggedIn; }
    bool NeedsLogin() const { return false; } // Steam doesn't need separate login
    const FString& GetUserId() const { return UserId; }
    const FString& GetDisplayName() const { return DisplayName; }

    // Additional utility methods
    bool IsSteamRunning() const
    {
        return SteamAPI_IsSteamRunning();
    }

    bool IsSteamOverlayEnabled() const
    {
        return SteamUtils ? SteamUtils->IsOverlayEnabled() : false;
    }

private:
    FSteamworksGamingService* Owner;

    // Internal state
    bool bIsInitialized;
    bool bIsLoggedIn;
    FString UserId;
    FString DisplayName;

    // Steamworks interfaces
    ISteamUserStats* SteamUserStats;
    ISteamUser* SteamUser;
    ISteamUtils* SteamUtils;
    ISteamFriends* SteamFriends;

    // Callback management
    FCriticalSection CallbackCriticalSection;
    struct FCallResultManager
    {
        TMap<SteamAPICall_t, TFunction<void(SteamAPICall_t, bool)>> Entries;

        template<typename T>
        void Add(SteamAPICall_t Handle, TFunction<void(const T&, bool)> OnComplete)
        {
            Entries.Add(Handle, [OnComplete](SteamAPICall_t H, bool bIOFailure)
            {
                T Result{};
                uint32 CubResult = sizeof(T);
                bool bGot = SteamUtils()->GetAPICallResult(H, &Result, CubResult, T::k_iCallback, nullptr);
                if (!bGot)
                {
                    OnComplete(Result, true);
                    return;
                }
                OnComplete(Result, bIOFailure);
            });
        }

        void Pump()
        {
            for (auto It = Entries.CreateIterator(); It; )
            {
                bool bFailed = false;
                bool bCompleted = SteamUtils()->IsAPICallCompleted(It.Key(), &bFailed);
                if (!bCompleted)
                {
                    ++It;
                    continue;
                }
                It.Value()(It.Key(), bFailed);
                It.RemoveCurrent();
            }
        }
    };

    FCallResultManager CallResults;

    void HandleUploadLeaderboardScore(SteamLeaderboard_t Leaderboard, int32 Score,
                                      TFunction<void(const FGamingServiceResult&)> Callback)
    {
        SteamAPICall_t UploadHandle = SteamUserStats->UploadLeaderboardScore(
            Leaderboard,
            k_ELeaderboardUploadScoreMethodKeepBest,
            Score,
            nullptr,
            0
        );
        CallResults.Add<LeaderboardScoreUploaded_t>(UploadHandle, [Callback](const LeaderboardScoreUploaded_t& Up, bool bUploadFailure)
        {
            if (Callback) { Callback(FGamingServiceResult(!bUploadFailure && Up.m_bSuccess)); }
        });
    }

    void HandleDownloadLeaderboardEntries(SteamLeaderboard_t Leaderboard, const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
                                          TFunction<void(const FLeaderboardResult&)> Callback)
    {
        SteamAPICall_t DownloadHandle = SteamUserStats->DownloadLeaderboardEntries(
            Leaderboard,
            k_ELeaderboardDataRequestGlobal,
            ContinuationToken,
            Limit
        );
        CallResults.Add<LeaderboardScoresDownloaded_t>(DownloadHandle, [this, LeaderboardId, Limit, ContinuationToken, Callback](const LeaderboardScoresDownloaded_t& Dl, bool bDownloadFailure)
        {
            FLeaderboardResult Result;
            if (bDownloadFailure)
            {
                Result = FLeaderboardResult(false, LeaderboardId, TArray<FLeaderboardEntry>(), 0, -1, 0);
            }
            else
            {
                TArray<FLeaderboardEntry> Entries;
                int32 EntryCount = FMath::Min((int32)Dl.m_cEntryCount, Limit);
                for (int32 i = 0; i < EntryCount; ++i)
                {
                    LeaderboardEntry_t Entry;
                    if (SteamUserStats->GetDownloadedLeaderboardEntry(Dl.m_hSteamLeaderboardEntries, i, &Entry, nullptr, 0))
                    {
                        FLeaderboardEntry GameEntry;
                        GameEntry.UserId = FString::Printf(TEXT("%llu"), Entry.m_steamIDUser.ConvertToUint64());
                        GameEntry.DisplayName = UTF8_TO_TCHAR(SteamFriends->GetFriendPersonaName(Entry.m_steamIDUser));
                        GameEntry.Score = Entry.m_nScore;
                        GameEntry.Rank = Entry.m_nGlobalRank;
                        Entries.Add(GameEntry);
                    }
                }
                Result = FLeaderboardResult(true, LeaderboardId, Entries, Dl.m_cEntryCount);
                Result.ContinuationToken = ContinuationToken + Entries.Num();
            }
            if (Callback) { Callback(Result); }
        });
    }

    void InitializeSteamworks()
    {
        // Initialize Steamworks API
        if (!SteamAPI_Init())
        {
            UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to initialize Steamworks API"));
            return;
        }

        // Cache Steam interfaces
        SteamUserStats = ::SteamUserStats();
        SteamUser      = ::SteamUser();
        SteamUtils     = ::SteamUtils();
        SteamFriends   = ::SteamFriends();

        // Validate Steam interfaces are accessible
        if (!SteamUserStats || !SteamUser || !SteamUtils || !SteamFriends)
        {
            UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to get Steam interfaces"));
            SteamAPI_Shutdown();
            return;
        }

        // Check if user is logged in
        if (SteamUser->BLoggedOn())
        {
            bIsLoggedIn = true;
            CSteamID SteamID = SteamUser->GetSteamID();
            UserId = FString::Printf(TEXT("%llu"), SteamID.ConvertToUint64());

            // Get display name
            const char* PersonaName = SteamFriends->GetPersonaName();
            DisplayName = PersonaName ? UTF8_TO_TCHAR(PersonaName) : TEXT("Steam User");

            UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: User logged in: %s (ID: %s)"), *DisplayName, *UserId);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: User not logged in to Steam"));
        }

        bIsInitialized = true;
        UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Steamworks initialized successfully"));
    }

    void ShutdownSteamworks()
    {
        if (bIsInitialized)
        {
            FScopeLock Lock(&CallbackCriticalSection);
            SteamAPI_Shutdown();
            UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Steamworks shutdown"));
        }
    }
};

FSteamworksGamingService::FSteamworksGamingService()
{
    Impl = MakeUnique<FSteamworksGamingServiceImpl>(this);
}

FSteamworksGamingService::~FSteamworksGamingService() {}

bool FSteamworksGamingService::Connect(const FGamingServiceConnectParams& Params)
{
    return Impl->Connect(Params.Steamworks);
}

void FSteamworksGamingService::Shutdown()
{
    Impl->Shutdown();
}

void FSteamworksGamingService::Login(const FGamingServiceLoginParams& Params, TFunction<void(const FGamingServiceResult&)> Callback)
{
    // Steamworks handles login automatically during initialization
    // Just return success since we're already logged in if Steam is running
    Callback(FGamingServiceResult(true));
}

void FSteamworksGamingService::UnlockAchievement(const FString& AchievementId,
                                                TFunction<void(const FGamingServiceResult&)> Callback)
{
    Impl->UnlockAchievement(AchievementId, MoveTemp(Callback));
}

void FSteamworksGamingService::QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback)
{
    Impl->QueryAchievements(MoveTemp(Callback));
}

void FSteamworksGamingService::WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
                                                    TFunction<void(const FGamingServiceResult&)> Callback)
{
    Impl->WriteLeaderboardScore(LeaderboardId, Score, MoveTemp(Callback));
}

void FSteamworksGamingService::QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
                                                   TFunction<void(const FLeaderboardResult&)> Callback)
{
    Impl->QueryLeaderboardPage(LeaderboardId, Limit, ContinuationToken, MoveTemp(Callback));
}

void FSteamworksGamingService::IngestStat(const FString& StatName, int32 Amount,
                                         TFunction<void(const FGamingServiceResult&)> Callback)
{
    Impl->IngestStat(StatName, Amount, MoveTemp(Callback));
}

void FSteamworksGamingService::QueryStat(const FString& StatName,
                                        TFunction<void(const FStatQueryResult&)> Callback)
{
    Impl->QueryStat(StatName, MoveTemp(Callback));
}

void FSteamworksGamingService::Tick()
{
    Impl->Tick();
}

bool FSteamworksGamingService::IsInitialized() const
{
    return Impl->IsInitialized();
}

bool FSteamworksGamingService::IsLoggedIn() const
{
    return Impl->IsLoggedIn();
}

bool FSteamworksGamingService::NeedsLogin() const
{
    return Impl->NeedsLogin();
}

FString FSteamworksGamingService::GetUserId() const
{
    return Impl->GetUserId();
}

FString FSteamworksGamingService::GetDisplayName() const
{
    return Impl->GetDisplayName();
}

bool FSteamworksGamingService::IsSteamRunning() const
{
    return Impl->IsSteamRunning();
}

bool FSteamworksGamingService::IsSteamOverlayEnabled() const
{
    return Impl->IsSteamOverlayEnabled();
}

#endif
