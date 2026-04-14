// Copyright Mindera. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPAddress.h"

#include "steam/steam_api.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"

#define MINDERA_STEAM_URL_PREFIX TEXT("steam.")

/** Protocol type name for Mindera Steam P2P networking. */
static const FName MINDERA_STEAM_P2P_PROTOCOL(TEXT("MinderaSteamP2P"));

/**
 * Internet address implementation wrapping SteamNetworkingIdentity.
 * Supports both P2P (SteamID-based) and IP-based addressing via the
 * newer ISteamNetworkingSockets API.
 */
class FInternetAddrMindera : public FInternetAddr
{
private:
	SteamNetworkingIdentity Addr;
	int32 P2PVirtualPort;

public:
	FInternetAddrMindera()
		: P2PVirtualPort(0)
	{
		Addr.Clear();
	}

	explicit FInternetAddrMindera(const SteamNetworkingIdentity& InIdentity)
		: Addr(InIdentity)
		, P2PVirtualPort(0)
	{
	}

	explicit FInternetAddrMindera(const SteamNetworkingIPAddr& InIPAddr)
		: P2PVirtualPort(0)
	{
		Addr.SetIPAddr(InIPAddr);
	}

	explicit FInternetAddrMindera(const CSteamID& InSteamID)
		: P2PVirtualPort(0)
	{
		Addr.SetSteamID(InSteamID);
	}

	// ------- Accessors -------

	const SteamNetworkingIdentity& GetIdentity() const { return Addr; }

	CSteamID GetSteamID() const { return Addr.GetSteamID(); }
	uint64 GetSteamID64() const { return Addr.GetSteamID64(); }

	void SetSteamID(CSteamID InSteamID) { Addr.SetSteamID(InSteamID); }
	void SetSteamID64(uint64 InSteamID64) { Addr.SetSteamID64(InSteamID64); }

	void SetIdentity(const SteamNetworkingIdentity& InIdentity) { Addr = InIdentity; }

	bool IsSteamID() const { return Addr.m_eType == k_ESteamNetworkingIdentityType_SteamID; }
	bool IsIPAddress() const { return Addr.m_eType == k_ESteamNetworkingIdentityType_IPAddress && Addr.GetIPAddr() != nullptr; }

	// ------- FInternetAddr interface -------

	virtual TArray<uint8> GetRawIp() const override;
	virtual void SetRawIp(const TArray<uint8>& RawAddr) override;

	virtual void SetIp(uint32 InAddr) override;
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) override;
	virtual void GetIp(uint32& OutAddr) const override;

	virtual void SetPort(int32 InPort) override;
	virtual int32 GetPort() const override;

	virtual void SetPlatformPort(int32 InPort) override { P2PVirtualPort = static_cast<int16>(InPort); }
	virtual int32 GetPlatformPort() const override { return static_cast<int32>(P2PVirtualPort); }

	virtual void SetAnyAddress() override;
	virtual void SetBroadcastAddress() override { /* Not supported */ }
	virtual void SetLoopbackAddress() override { Addr.SetLocalHost(); }

	virtual FString ToString(bool bAppendPort) const override;

	virtual bool operator==(const FInternetAddr& Other) const override;
	virtual uint32 GetTypeHash() const override;

	virtual FName GetProtocolType() const override { return MINDERA_STEAM_P2P_PROTOCOL; }
	virtual bool IsValid() const override { return !Addr.IsInvalid(); }
	virtual TSharedRef<FInternetAddr> Clone() const override;

	// Conversion operators
	operator const SteamNetworkingIdentity() const { return Addr; }
	operator const SteamNetworkingIPAddr() const;
};
