#ifdef GS_WITH_STEAM

#include "Native/Steam/Interfaces/SteamUser.h"
#include "Native/Steam/SteamPlatformCore.h"

#include "Engine/Texture2D.h"
#include "PixelFormat.h"
#include "UObject/StrongObjectPtr.h"

#include "steam/steam_api.h"

namespace GamingServices
{
	struct FSteamUser::FImpl
	{
		FSteamUser& Owner;

		TMap<uint64, TStrongObjectPtr<UTexture2D>> AvatarCache;
		TSet<uint64> RequestedAvatars;

		// Display-name resolves waiting on Steam to download the persona (keyed by SteamID64).
		TMap<uint64, TArray<TFunction<void(const FResolveDisplayNameResult&)>>> PendingNameResolves;

		CCallback<FImpl, AvatarImageLoaded_t> m_CallbackAvatarImageLoaded;
		CCallback<FImpl, PersonaStateChange_t> m_CallbackPersonaStateChange;

		explicit FImpl(FSteamUser& InOwner)
			: Owner(InOwner)
			, m_CallbackAvatarImageLoaded(this, &FImpl::OnAvatarImageLoaded)
			, m_CallbackPersonaStateChange(this, &FImpl::OnPersonaStateChange)
		{
		}

		void OnAvatarImageLoaded(AvatarImageLoaded_t* pParam)
		{
			const uint64 SteamID64 = pParam->m_steamID.ConvertToUint64();
			// Only react to users we care about: ones we've explicitly requested (BP-driven) or
			// ones we've already cached. Filters out unrelated avatars Steam happens to load
			// (e.g. friend list portraits).
			if (!RequestedAvatars.Contains(SteamID64)
				&& !AvatarCache.Contains(SteamID64))
			{
				return;
			}
			BuildAvatarFromHandle(SteamID64, pParam->m_iImage);

			// Fire OnAvatarReady only here — this is the genuine async completion. The other
			// caller of BuildAvatarFromHandle is GetAvatarForSteamID, which is a synchronous
			// "try now" path where the caller already gets the texture as a return value;
			// firing a ready event from that path would be redundant (and re-entrant from the
			// caller's perspective).
			if (Owner.OnAvatarReady && AvatarCache.Contains(SteamID64))
			{
				Owner.OnAvatarReady(FString::Printf(TEXT("%llu"), SteamID64));
			}
		}

		// Steam does not guarantee AvatarImageLoaded_t for every avatar we display - for
		// non-friend lobby members the bytes often arrive via persona delivery instead, or
		// are already client-cached so no image-load event fires. Re-query the handle on a
		// persona change for users we've requested/shown so the avatar still resolves.
		void OnPersonaStateChange(PersonaStateChange_t* pParam)
		{
			if (!SteamFriends() || !pParam)
			{
				return;
			}

			const uint64 SteamID64 = pParam->m_ulSteamID;

			// A pending display-name resolve for this user can complete now that the persona arrived.
			if ((pParam->m_nChangeFlags & k_EPersonaChangeName) && PendingNameResolves.Contains(SteamID64))
			{
				FlushPendingNameResolves(SteamID64);
			}

			if (!RequestedAvatars.Contains(SteamID64) && !AvatarCache.Contains(SteamID64))
			{
				return;
			}

			const bool bAvatarChanged = (pParam->m_nChangeFlags & k_EPersonaChangeAvatar) != 0;
			const bool bNeedAvatar = !AvatarCache.Contains(SteamID64);
			if (!bAvatarChanged && !bNeedAvatar)
			{
				return;
			}

			const int AvatarHandle = SteamFriends()->GetLargeFriendAvatar(CSteamID(SteamID64));
			if (AvatarHandle <= 0)
			{
				// Still not materialized; a later AvatarImageLoaded_t or persona change retries.
				return;
			}

			BuildAvatarFromHandle(SteamID64, AvatarHandle);
			if (Owner.OnAvatarReady && AvatarCache.Contains(SteamID64))
			{
				Owner.OnAvatarReady(FString::Printf(TEXT("%llu"), SteamID64));
			}
		}

		// Fire every queued resolve for a user with the persona name Steam now has cached. Always
		// yields a non-empty value: the id string is the fallback when the name is blank.
		void FlushPendingNameResolves(uint64 SteamID64)
		{
			TArray<TFunction<void(const FResolveDisplayNameResult&)>> Callbacks;
			PendingNameResolves.RemoveAndCopyValue(SteamID64, Callbacks);

			const FString UserId = FString::Printf(TEXT("%llu"), SteamID64);
			const FString Name = SteamFriends()
				                     ? UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(CSteamID(SteamID64)))
				                     : FString();
			const FResolveDisplayNameResult Result = (Name.IsEmpty() || Name == TEXT("[unknown]"))
				                                          ? FResolveDisplayNameResult::Fallback(UserId)
				                                          : FResolveDisplayNameResult::Resolved(UserId, Name);
			for (const auto& Callback : Callbacks)
			{
				Callback(Result);
			}
		}

		UTexture2D* GetAvatarForSteamID(const CSteamID& SteamID)
		{
			if (!SteamFriends() || !SteamUtils())
			{
				return nullptr;
			}

			const uint64 SteamID64 = SteamID.ConvertToUint64();
			if (TStrongObjectPtr<UTexture2D>* Existing = AvatarCache.Find(SteamID64))
			{
				if (Existing->IsValid())
				{
					return Existing->Get();
				}
			}

			// Make sure persona/avatar info is queued for download for non-friend users.
			// Returns false if the data is already available, true if a request was started.
			SteamFriends()->RequestUserInformation(SteamID, /*bRequireNameOnly=*/false);

			const int AvatarHandle = SteamFriends()->GetLargeFriendAvatar(SteamID);
			if (AvatarHandle == 0)
			{
				// Not loaded yet — Steam will fire AvatarImageLoaded_t when ready.
				RequestedAvatars.Add(SteamID64);
				return nullptr;
			}
			if (AvatarHandle < 0)
			{
				// User has no avatar set.
				return nullptr;
			}

			BuildAvatarFromHandle(SteamID64, AvatarHandle);
			if (TStrongObjectPtr<UTexture2D>* Built = AvatarCache.Find(SteamID64))
			{
				return Built->Get();
			}
			return nullptr;
		}

		void BuildAvatarFromHandle(uint64 SteamID64, int AvatarHandle)
		{
			if (AvatarHandle <= 0 || !SteamUtils())
			{
				return;
			}

			uint32 Width = 0;
			uint32 Height = 0;
			if (!SteamUtils()->GetImageSize(AvatarHandle, &Width, &Height) || Width == 0 || Height == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Failed to get avatar image size for %llu"), SteamID64);
				return;
			}

			const int32 BufferSize = static_cast<int32>(Width * Height * 4);
			TArray<uint8> RGBA;
			RGBA.SetNumUninitialized(BufferSize);
			if (!SteamUtils()->GetImageRGBA(AvatarHandle, RGBA.GetData(), BufferSize))
			{
				UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Failed to read avatar pixel data for %llu"), SteamID64);
				return;
			}

			// Steam returns pixels as RGBA; UTexture2D B8G8R8A8 expects BGRA — swap channels.
			for (int32 i = 0; i + 3 < RGBA.Num(); i += 4)
			{
				Swap(RGBA[i + 0], RGBA[i + 2]);
			}

			UTexture2D* Texture = UTexture2D::CreateTransient(static_cast<int32>(Width), static_cast<int32>(Height), PF_B8G8R8A8);
			if (!Texture)
			{
				UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: CreateTransient avatar texture failed for %llu"), SteamID64);
				return;
			}
			Texture->SRGB = true;

			FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
			void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(TextureData, RGBA.GetData(), RGBA.Num());
			Mip.BulkData.Unlock();
			Texture->UpdateResource();

			AvatarCache.Emplace(SteamID64, TStrongObjectPtr<UTexture2D>(Texture));
			RequestedAvatars.Remove(SteamID64);
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Avatar loaded for %llu (%ux%u)"), SteamID64, Width, Height);
		}

		// Eagerly fetch a lobby member's user info + avatar so Steam knows we care about them.
		// Without this, on a fresh Steam restart the local client has nothing cached for non-friend
		// lobby members and Steam never fires AvatarImageLoaded_t for them (we never asked).
		// Call when a member is detected via lobby snapshot or LobbyChatUpdate "entered".
		void EnsureAvatarForMember(const CSteamID& SteamID)
		{
			if (!SteamFriends() || !SteamID.IsValid())
			{
				return;
			}

			const uint64 SteamID64 = SteamID.ConvertToUint64();
			if (AvatarCache.Contains(SteamID64))
			{
				return;
			}

			// Tell Steam to download persona/avatar info for this user. No-op if already cached.
			SteamFriends()->RequestUserInformation(SteamID, /*bRequireNameOnly=*/false);

			const int AvatarHandle = SteamFriends()->GetLargeFriendAvatar(SteamID);
			if (AvatarHandle == 0)
			{
				// Not loaded yet — register interest so OnAvatarImageLoaded's gate passes when Steam delivers.
				RequestedAvatars.Add(SteamID64);
				return;
			}
			if (AvatarHandle < 0)
			{
				// User has no avatar set — nothing to fetch.
				return;
			}

			// Already loaded by Steam. Build the texture now and fire OnAvatarReady so any
			// listener that registered before the eager fetch can still pick it up — the
			// synchronous GetAvatarForSteamID path skips the event, but here there's no caller
			// holding the return value, so we must broadcast.
			BuildAvatarFromHandle(SteamID64, AvatarHandle);
			if (Owner.OnAvatarReady && AvatarCache.Contains(SteamID64))
			{
				Owner.OnAvatarReady(FString::Printf(TEXT("%llu"), SteamID64));
			}
		}
	};

	FSteamUser::FSteamUser(FSteamPlatformCore& InCore)
		: Core(InCore)
		, Impl(MakePimpl<FImpl>(*this))
	{
	}

	void FSteamUser::Login(const FGamingServiceLoginParams& Params, TFunction<void(const FGamingServiceResult&)> Callback)
	{
		// Steam logs in implicitly when the client is running; nothing to do.
		if (Callback)
		{
			Callback(FGamingServiceResult(true));
		}
	}

	bool FSteamUser::IsLoggedIn() const
	{
		return Core.IsLoggedIn();
	}

	bool FSteamUser::NeedsLogin() const
	{
		return Core.NeedsLogin();
	}

	FString FSteamUser::GetUserId() const
	{
		return Core.GetUserId();
	}

	FString FSteamUser::GetDisplayName() const
	{
		return Core.GetDisplayName();
	}

	void FSteamUser::ResolveDisplayName(const FString& UserId,
	                                   TFunction<void(const FResolveDisplayNameResult&)> Callback)
	{
		const uint64 SteamID64 = FCString::Strtoui64(*UserId, nullptr, 10);
		if (SteamID64 == 0 || !SteamFriends())
		{
			Callback(FResolveDisplayNameResult::Fallback(UserId));
			return;
		}

		const CSteamID SteamID(SteamID64);

		// RequestUserInformation returns false when the persona is already cached (resolve now),
		// true when Steam started a download (resolve on the persona-state-change callback).
		const bool bDownloading = SteamFriends()->RequestUserInformation(SteamID, /*bRequireNameOnly=*/true);
		if (!bDownloading)
		{
			const FString Name = UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(SteamID));
			Callback((Name.IsEmpty() || Name == TEXT("[unknown]"))
				         ? FResolveDisplayNameResult::Fallback(UserId)
				         : FResolveDisplayNameResult::Resolved(UserId, Name));
			return;
		}

		Impl->PendingNameResolves.FindOrAdd(SteamID64).Add(MoveTemp(Callback));
	}

	UTexture2D* FSteamUser::GetAvatar() const
	{
		if (!Core.IsLoggedIn() || !SteamUser())
		{
			return nullptr;
		}
		// Avatar fetch mutates the avatar cache; const here mirrors the legacy const accessor.
		return Impl->GetAvatarForSteamID(SteamUser()->GetSteamID());
	}

	UTexture2D* FSteamUser::GetAvatarByUserId(const FString& InUserId) const
	{
		const uint64 SteamID64 = FCString::Strtoui64(*InUserId, nullptr, 10);
		if (SteamID64 == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: GetAvatarByUserId called with invalid UserId '%s'"), *InUserId);
			return nullptr;
		}
		return Impl->GetAvatarForSteamID(CSteamID(SteamID64));
	}

	void FSteamUser::EnsureAvatarForMember(uint64 SteamID64)
	{
		Impl->EnsureAvatarForMember(CSteamID(SteamID64));
	}
}

#endif // GS_WITH_STEAM
