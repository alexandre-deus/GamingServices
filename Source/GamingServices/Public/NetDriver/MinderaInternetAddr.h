// Copyright Mindera. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPAddress.h"

/** Protocol type name for Mindera P2P networking (backend-agnostic). */
static const FName MINDERA_P2P_PROTOCOL(TEXT("MinderaP2P"));

/**
 * Internet address for the Mindera P2P netdriver — a plain platform peer id (SteamID64 / EOS
 * ProductUserId in text form) plus a virtual-port channel. Fully SDK-free: no platform networking
 * type appears here; the transport (IP2PTransport) converts the peer id string to its native identity.
 */
class FInternetAddrMindera : public FInternetAddr
{
private:
	/** Peer id in text form, WITHOUT any "steam."/"eos." URL prefix. */
	FString PeerId;
	/** P2P virtual port / channel. */
	int32 P2PVirtualPort = 0;

public:
	FInternetAddrMindera() = default;

	const FString& GetPeerId() const { return PeerId; }
	void SetPeerId(const FString& InPeerId) { PeerId = InPeerId; }

	// ------- FInternetAddr interface -------

	virtual TArray<uint8> GetRawIp() const override;
	virtual void SetRawIp(const TArray<uint8>& RawAddr) override;

	virtual void SetIp(uint32 InAddr) override {}
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) override;
	virtual void GetIp(uint32& OutAddr) const override { OutAddr = 0; }

	virtual void SetPort(int32 InPort) override { P2PVirtualPort = InPort; }
	virtual int32 GetPort() const override { return P2PVirtualPort; }

	virtual void SetPlatformPort(int32 InPort) override { P2PVirtualPort = InPort; }
	virtual int32 GetPlatformPort() const override { return P2PVirtualPort; }

	virtual void SetAnyAddress() override { PeerId.Empty(); P2PVirtualPort = 0; }
	virtual void SetBroadcastAddress() override { /* Not supported */ }
	virtual void SetLoopbackAddress() override { PeerId.Empty(); }

	virtual FString ToString(bool bAppendPort) const override;

	virtual bool operator==(const FInternetAddr& Other) const override;
	virtual uint32 GetTypeHash() const override;

	virtual FName GetProtocolType() const override { return MINDERA_P2P_PROTOCOL; }
	virtual bool IsValid() const override { return !PeerId.IsEmpty(); }
	virtual TSharedRef<FInternetAddr> Clone() const override;
};
