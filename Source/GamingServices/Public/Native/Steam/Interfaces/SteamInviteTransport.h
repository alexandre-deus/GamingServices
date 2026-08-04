#pragma once

#ifdef GS_WITH_STEAM

#include "CoreMinimal.h"
#include "Native/Interfaces/IInviteTransport.h"
#include "Templates/PimplPtr.h"

namespace GamingServices
{
	class FSteamPlatformCore;

	/**
	 * Steam as the invite channel for a session that lives on another backend.
	 *
	 * Uses Steam's rich-presence "connect" key rather than Steam lobbies: publishing it makes "Join Game"
	 * appear beside the player in their friends list, and it can carry an arbitrary string — here, an EOS
	 * lobby id. No Steam lobby is created, so there is no second session to keep in sync with the real one.
	 *
	 * Accepting an invite comes back as GameRichPresenceJoinRequested_t while the game is running, or as
	 * a "+connect <payload>" launch argument when it was not. Both land on OnJoinRequested; the cold-start
	 * payload is held until a listener binds, since the parse happens long before the game gets that far.
	 *
	 * The Steam SDK and its callbacks live in a private FImpl so this header stays SDK-free.
	 */
	class FSteamInviteTransport final : public IInviteTransport
	{
	public:
		explicit FSteamInviteTransport(FSteamPlatformCore& InCore);
		virtual ~FSteamInviteTransport() override;

		virtual bool IsAvailable() const override;

		virtual void SetJoinInfo(const FString& JoinPayload) override;
		virtual void ClearJoinInfo() override;
		virtual void ShowInviteDialog(TFunction<void(const FGamingServiceResult&)> Callback) override;

		/** Delivers the "+connect" payload a launch-from-invite left queued, if there is one. */
		virtual void FlushPendingJoin() override;

		/**
		 * The payload last published by SetJoinInfo, or empty when nothing is joinable.
		 *
		 * Exists so FSteamFriends can send a targeted invite for the same session the overlay dialog would
		 * offer, instead of keeping a second copy of "what is currently joinable" that could drift.
		 */
		const FString& GetJoinPayload() const;

	private:
		struct FImpl;

		FSteamPlatformCore& Core;
		TPimplPtr<FImpl> Impl;
	};
}

#endif // GS_WITH_STEAM
