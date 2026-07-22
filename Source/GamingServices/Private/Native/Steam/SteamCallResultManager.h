#pragma once

#ifdef USE_STEAMWORKS

#include "CoreMinimal.h"

#include "steam/steam_api.h"

namespace GamingServices
{
	/**
	 * Internal Steam CallResults pump. Tracks pending SteamAPICall_t handles and dispatches
	 * their typed results on Pump(). Self-contained — resolves ISteamUtils via the global
	 * SteamUtils() accessor, so it needs no initialization beyond construction.
	 */
	struct FSteamCallResultManager
	{
		TMap<SteamAPICall_t, TFunction<void(SteamAPICall_t, bool)>> Entries;
		TArray<TPair<SteamAPICall_t, TFunction<void(SteamAPICall_t, bool)>>> PendingEntries;
		bool bIsPumping = false;

		template <typename T>
		void Add(SteamAPICall_t Handle, TFunction<void(const T&, bool)> OnComplete)
		{
			UE_LOG(LogTemp, Log,
				   TEXT("SteamworksGamingService: CallResults.Add - Registering "
						"callback (Handle=%llu, CallbackId=%d)"),
				   (uint64)Handle, (int32)T::k_iCallback);
			auto Wrapped = [OnComplete](SteamAPICall_t H, bool bIOFailure)
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
			};
			if (bIsPumping)
			{
				UE_LOG(LogTemp, Verbose,
					   TEXT("SteamworksGamingService: CallResults.Add - Deferring add "
							"while pumping (Handle=%llu)"),
					   (uint64)Handle);
				PendingEntries.Add(
					TPair<SteamAPICall_t, TFunction<void(SteamAPICall_t, bool)>>(Handle, MoveTemp(Wrapped)));
			}
			else
			{
				Entries.Add(Handle, MoveTemp(Wrapped));
			}
		}

		void Pump()
		{
			TArray<SteamAPICall_t> CompletedCalls;

			for (auto It = Entries.CreateIterator(); It; ++It)
			{
				bool bFailed = false;
				bool bCompleted = SteamUtils()->IsAPICallCompleted(It.Key(), &bFailed);
				if (bCompleted)
				{
					CompletedCalls.Add(It.Key());
				}
			}

			for (SteamAPICall_t CallHandle : CompletedCalls)
			{
				if (TFunction<void(SteamAPICall_t, bool)>* Callback = Entries.Find(CallHandle))
				{
					bool bFailed = false;
					SteamUtils()->IsAPICallCompleted(CallHandle, &bFailed);
					(*Callback)(CallHandle, bFailed);
					Entries.Remove(CallHandle);
				}
			}
		}
	};
}

#endif // USE_STEAMWORKS
