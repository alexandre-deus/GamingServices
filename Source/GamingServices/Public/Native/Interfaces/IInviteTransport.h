#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"

/**
 * A platform's invite system, used to carry a session id that belongs to a DIFFERENT backend.
 *
 * This exists because invites are social, and the social graph does not necessarily live on the backend
 * running the session. When Steam authenticates and EOS owns the lobby, the player's friends are on
 * Steam while the thing they need to join is an EOS lobby id — so Steam carries the id and EOS resolves
 * it. The transport never interprets the payload; it is opaque text to it.
 *
 * The counterpart on the session side is IMatchmakingService::JoinLobbyById, which turns that text back
 * into a joined lobby.
 */
class GAMINGSERVICES_API IInviteTransport
{
public:
	virtual ~IInviteTransport() = default;

	/** Whether the platform is up and able to carry invites right now. */
	virtual bool IsAvailable() const = 0;

	/**
	 * Publish the current session so friends can be invited to it, and so the platform can offer its own
	 * "join game" affordances. JoinPayload is the opaque string handed back to a joiner later.
	 */
	virtual void SetJoinInfo(const FString& JoinPayload) = 0;

	/** Stop advertising a joinable session. Safe to call when nothing is published. */
	virtual void ClearJoinInfo() = 0;

	/**
	 * Open the platform's own friend picker for the published session. Fails when nothing is published.
	 * Preferred over a custom UI: it is the overlay players already know, and it respects their privacy
	 * and blocklist settings.
	 */
	virtual void ShowInviteDialog(TFunction<void(const FGamingServiceResult&)> Callback) = 0;

	/**
	 * Fires when the local player accepts an invite, carrying the payload the host published.
	 *
	 * Must also deliver invites that arrived before the game was running — accepting an invite while the
	 * game is closed launches it, and that payload has to reach the same sink rather than being lost. So
	 * implementations queue a payload seen at startup and deliver it as soon as this is bound.
	 */
	TFunction<void(const FString& JoinPayload)> OnJoinRequested;

	/**
	 * Deliver a payload that arrived before anyone was listening, if one is held.
	 *
	 * Binding OnJoinRequested is not enough on its own: the sink is bound while the platform layer is
	 * being wired up, which is earlier than the game is ready to act on a join. So delivery is a separate
	 * call the game makes when it can actually handle one. No-op on transports that queue nothing.
	 */
	virtual void FlushPendingJoin() {}
};
