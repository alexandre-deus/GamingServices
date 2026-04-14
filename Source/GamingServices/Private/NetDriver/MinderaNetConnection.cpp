// Copyright Mindera. All Rights Reserved.

#include "NetDriver/MinderaNetConnection.h"
#include "NetDriver/MinderaNetDriver.h"
#include "NetDriver/MinderaInternetAddr.h"
#include "NetDriver/MinderaSocketSubsystem.h"

#include "PacketHandler.h"

#include "steam/steam_api.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MinderaNetConnection)

DEFINE_LOG_CATEGORY_STATIC(LogMinderaNetConn, Log, All);

// ---------------------------------------------------------------------------
UMinderaNetConnection::UMinderaNetConnection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UE_LOG(LogMinderaNetConn, Verbose, TEXT("[Conn] Constructor"));
}

// ---------------------------------------------------------------------------
ISteamNetworkingSockets* UMinderaNetConnection::GetSteamSocketsInterface() const
{
	if (IsRunningDedicatedServer() && SteamGameServerNetworkingSockets())
	{
		UE_LOG(LogMinderaNetConn, VeryVerbose, TEXT("[Conn] GetSteamSocketsInterface: using GameServer interface"));
		return SteamGameServerNetworkingSockets();
	}
	UE_LOG(LogMinderaNetConn, VeryVerbose, TEXT("[Conn] GetSteamSocketsInterface: using Client interface"));
	return SteamNetworkingSockets();
}

// ---------------------------------------------------------------------------
void UMinderaNetConnection::InitLocalConnection(UNetDriver* InDriver, FSocket* InSocket,
	const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	UMinderaNetDriver* MinderaDriver = Cast<UMinderaNetDriver>(InDriver);
	bIsPassthrough = MinderaDriver ? MinderaDriver->bIsPassthrough : true;

	UE_LOG(LogMinderaNetConn, Log, TEXT("[Conn] InitLocalConnection: bIsPassthrough=%d, URL.Host='%s', State=%d, MaxPacket=%d, Overhead=%d"),
		(int32)bIsPassthrough, *InURL.Host, (int32)InState, InMaxPacket, InPacketOverhead);

	if (!bIsPassthrough)
	{
		DisableAddressResolution();
	}

	Super::InitLocalConnection(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);
	UE_LOG(LogMinderaNetConn, Verbose, TEXT("[Conn] InitLocalConnection: DONE, RemoteAddr=%s"),
		RemoteAddr.IsValid() ? *RemoteAddr->ToString(true) : TEXT("null"));
}

// ---------------------------------------------------------------------------
void UMinderaNetConnection::InitRemoteConnection(UNetDriver* InDriver, FSocket* InSocket,
	const FURL& InURL, const FInternetAddr& InRemoteAddr, EConnectionState InState,
	int32 InMaxPacket, int32 InPacketOverhead)
{
	UMinderaNetDriver* MinderaDriver = Cast<UMinderaNetDriver>(InDriver);
	bIsPassthrough = MinderaDriver ? MinderaDriver->bIsPassthrough : true;

	UE_LOG(LogMinderaNetConn, Log, TEXT("[Conn] InitRemoteConnection: bIsPassthrough=%d, URL.Host='%s', RemoteAddr=%s, State=%d"),
		(int32)bIsPassthrough, *InURL.Host, *InRemoteAddr.ToString(true), (int32)InState);

	if (!bIsPassthrough)
	{
		DisableAddressResolution();
	}

	Super::InitRemoteConnection(InDriver, InSocket, InURL, InRemoteAddr, InState, InMaxPacket, InPacketOverhead);

	// For Steam connections, look up the pending Steam handle from the driver
	if (!bIsPassthrough && MinderaDriver)
	{
		const FInternetAddrMindera* SteamAddr = static_cast<const FInternetAddrMindera*>(&InRemoteAddr);
		SteamNetworkingIdentity Identity;
		Identity.SetSteamID64(SteamAddr->GetSteamID64());

		SteamConnectionHandle = MinderaDriver->FindSteamHandleForIdentity(Identity);
		MinderaDriver->PendingSteamConnections.Remove(SteamConnectionHandle);

		UE_LOG(LogMinderaNetConn, Log, TEXT("[Conn] InitRemoteConnection: resolved Steam handle=%u for SteamID %llu"),
			SteamConnectionHandle, SteamAddr->GetSteamID64());
	}

	UE_LOG(LogMinderaNetConn, Verbose, TEXT("[Conn] InitRemoteConnection: DONE, RemoteAddr=%s"),
		RemoteAddr.IsValid() ? *RemoteAddr->ToString(true) : TEXT("null"));
}

// ---------------------------------------------------------------------------
void UMinderaNetConnection::LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	if (bIsPassthrough)
	{
		UE_LOG(LogMinderaNetConn, VeryVerbose, TEXT("[Conn] LowLevelSend: passthrough, delegating to Super (%d bits)"), CountBits);
		Super::LowLevelSend(Data, CountBits, Traits);
		return;
	}

	const uint8* DataToSend = reinterpret_cast<uint8*>(Data);

	// Process through packet handler — stamps session/connection IDs, encryption, etc.
	// UIpConnection::LowLevelSend does this internally; since we bypass Super we must do it here.
	if (Handler.IsValid() && !Handler->GetRawSend())
	{
		const ProcessedPacket ProcessedData = Handler->Outgoing(reinterpret_cast<uint8*>(Data), CountBits, Traits);
		if (ProcessedData.bError)
		{
			UE_LOG(LogMinderaNetConn, Warning, TEXT("[Conn] LowLevelSend: PacketHandler Outgoing error"));
			return;
		}
		DataToSend = ProcessedData.Data;
		CountBits = ProcessedData.CountBits;
	}

	ISteamNetworkingSockets* Sockets = GetSteamSocketsInterface();
	if (!Sockets || SteamConnectionHandle == k_HSteamNetConnection_Invalid)
	{
		UE_LOG(LogMinderaNetConn, VeryVerbose, TEXT("[Conn] LowLevelSend: skipped (Sockets=%p, handle=%u)"),
			Sockets, SteamConnectionHandle);
		return;
	}

	const int32 DataLen = FMath::DivideAndRoundUp(CountBits, 8);
	if (DataLen <= 0) return;

	UE_LOG(LogMinderaNetConn, VeryVerbose, TEXT("[Conn] LowLevelSend: sending %d bits (%d bytes) on handle=%u"),
		CountBits, DataLen, SteamConnectionHandle);

	EResult Result = Sockets->SendMessageToConnection(
		SteamConnectionHandle,
		DataToSend,
		static_cast<uint32>(DataLen),
		k_nSteamNetworkingSend_UnreliableNoNagle,
		nullptr);

	if (Result != k_EResultOK)
	{
		UE_LOG(LogMinderaNetConn, Warning, TEXT("[Conn] LowLevelSend: SendMessageToConnection returned %d (handle=%u, %d bytes)"),
			(int32)Result, SteamConnectionHandle, DataLen);
	}
}

// ---------------------------------------------------------------------------
void UMinderaNetConnection::CleanUp()
{
	UE_LOG(LogMinderaNetConn, Log, TEXT("[Conn] CleanUp: bIsPassthrough=%d, handle=%u"), (int32)bIsPassthrough, SteamConnectionHandle);

	// Official pattern: let the parent flush/close UE-level state first,
	// then tear down the Steam transport (mirrors USteamNetConnection::CleanUp).
	UE_LOG(LogMinderaNetConn, Verbose, TEXT("[Conn] CleanUp: calling Super::CleanUp"));
	Super::CleanUp();

	ISteamNetworkingSockets* Sockets = GetSteamSocketsInterface();
	if (Sockets && SteamConnectionHandle != k_HSteamNetConnection_Invalid && !bIsPassthrough)
	{
		UE_LOG(LogMinderaNetConn, Log, TEXT("[Conn] CleanUp: closing Steam connection handle=%u"), SteamConnectionHandle);
		Sockets->CloseConnection(SteamConnectionHandle, 0, "UE connection cleanup", false);
		SteamConnectionHandle = k_HSteamNetConnection_Invalid;
	}
}