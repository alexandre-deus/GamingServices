// Copyright Mindera. All Rights Reserved.

#include "NetDriver/MinderaInternetAddr.h"

#include "NetDriver/MinderaSocketSubsystem.h"
#include "Native/Interfaces/IP2PTransport.h"

// Fully SDK-free: the address is just a peer-id string + channel. No backend guard needed.
// The "steam."/"eos." URL prefix is a backend detail owned by the active transport, so we ask it
// rather than hard-coding per-backend constants here.

TArray<uint8> FInternetAddrMindera::GetRawIp() const
{
	// Serialise the peer id as its UTF-8 bytes (used by the engine for address comparison / storage).
	TArray<uint8> Raw;
	const FTCHARToUTF8 Utf8(*PeerId);
	Raw.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	return Raw;
}

void FInternetAddrMindera::SetRawIp(const TArray<uint8>& RawAddr)
{
	if (RawAddr.Num() == 0)
	{
		PeerId.Empty();
		return;
	}
	PeerId = FString(FUTF8ToTCHAR(reinterpret_cast<const ANSICHAR*>(RawAddr.GetData()), RawAddr.Num()).Get(), RawAddr.Num());
	// Trim any embedded terminators just in case.
	int32 NullIdx;
	if (PeerId.FindChar(TEXT('\0'), NullIdx))
	{
		PeerId.LeftInline(NullIdx);
	}
}

void FInternetAddrMindera::SetIp(const TCHAR* InAddr, bool& bIsValid)
{
	FString InAddrStr(InAddr);
	if (IP2PTransport* Transport = FMinderaSocketSubsystem::GetTransport())
	{
		InAddrStr.RemoveFromStart(Transport->GetUrlPrefix());
	}

	// Optional ":channel" suffix.
	FString IdPart, ChannelPart;
	if (InAddrStr.Split(TEXT(":"), &IdPart, &ChannelPart, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		PeerId = IdPart;
		P2PVirtualPort = FCString::Atoi(*ChannelPart);
	}
	else
	{
		PeerId = InAddrStr;
	}
	bIsValid = !PeerId.IsEmpty();
}

FString FInternetAddrMindera::ToString(bool bAppendPort) const
{
	if (PeerId.IsEmpty())
	{
		return TEXT("INVALID");
	}
	return bAppendPort ? FString::Printf(TEXT("%s:%d"), *PeerId, P2PVirtualPort) : PeerId;
}

bool FInternetAddrMindera::operator==(const FInternetAddr& Other) const
{
	const FInternetAddrMindera& O = static_cast<const FInternetAddrMindera&>(Other);
	return PeerId == O.PeerId && P2PVirtualPort == O.P2PVirtualPort;
}

uint32 FInternetAddrMindera::GetTypeHash() const
{
	// Hash the peer id via FCrc: the member name shadows the global GetTypeHash(FString) overload here.
	return HashCombine(FCrc::StrCrc32(*PeerId), static_cast<uint32>(P2PVirtualPort));
}

TSharedRef<FInternetAddr> FInternetAddrMindera::Clone() const
{
	TSharedRef<FInternetAddrMindera> NewAddress = MakeShareable(new FInternetAddrMindera());
	NewAddress->PeerId = PeerId;
	NewAddress->P2PVirtualPort = P2PVirtualPort;
	return NewAddress;
}
