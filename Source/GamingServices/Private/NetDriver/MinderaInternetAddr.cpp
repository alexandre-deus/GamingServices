// Copyright Mindera. All Rights Reserved.

#ifdef USE_STEAMWORKS

#include "NetDriver/MinderaInternetAddr.h"

#include "steam/isteamnetworkingutils.h"

// ---------------------------------------------------------------------------
// GetRawIp / SetRawIp
// ---------------------------------------------------------------------------
TArray<uint8> FInternetAddrMindera::GetRawIp() const
{
	TArray<uint8> RawAddressArray;
	if (Addr.m_eType == k_ESteamNetworkingIdentityType_SteamID)
	{
		uint64 SteamIDNum = Addr.GetSteamID64();
		const uint8* Walk = reinterpret_cast<const uint8*>(&SteamIDNum);
		for (int32 i = 0; i < (int32)sizeof(uint64); ++i)
		{
			RawAddressArray.Add(Walk[i]);
		}
#if PLATFORM_LITTLE_ENDIAN
		Algo::Reverse(RawAddressArray);
#endif
		RawAddressArray.EmplaceAt(0, k_ESteamNetworkingIdentityType_SteamID);
	}
	else if (IsIPAddress())
	{
		const SteamNetworkingIPAddr* RawSteamIP = Addr.GetIPAddr();
		RawAddressArray.Add(k_ESteamNetworkingIdentityType_IPAddress);
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(RawSteamIP->m_ipv6); ++i)
		{
			RawAddressArray.Add(RawSteamIP->m_ipv6[i]);
		}
	}
	return RawAddressArray;
}

void FInternetAddrMindera::SetRawIp(const TArray<uint8>& RawAddr)
{
	if (RawAddr.Num() <= 1) return;

	Addr.Clear();
	const uint8 ArrayType = RawAddr[0];

	if (ArrayType == k_ESteamNetworkingIdentityType_SteamID)
	{
		TArray<uint8> WorkingArray = RawAddr;
		WorkingArray.RemoveAt(0);
#if PLATFORM_LITTLE_ENDIAN
		Algo::Reverse(WorkingArray);
#endif
		uint64 NewSteamId = 0;
		for (int32 i = 0; i < WorkingArray.Num(); ++i)
		{
			NewSteamId |= static_cast<uint64>(WorkingArray[i]) << (i * 8);
		}
		Addr.SetSteamID64(NewSteamId);
	}
	else if (ArrayType == k_ESteamNetworkingIdentityType_IPAddress)
	{
		SteamNetworkingIPAddr NewAddr;
		NewAddr.Clear();
		for (int32 i = 1; i < RawAddr.Num() && (i - 1) < (int32)UE_ARRAY_COUNT(NewAddr.m_ipv6); ++i)
		{
			NewAddr.m_ipv6[i - 1] = RawAddr[i];
		}
		Addr.SetIPAddr(NewAddr);
	}
}

// ---------------------------------------------------------------------------
// SetIp / GetIp
// ---------------------------------------------------------------------------
void FInternetAddrMindera::SetIp(uint32 InAddr)
{
	// Not used for Steam networking
}

void FInternetAddrMindera::SetIp(const TCHAR* InAddr, bool& bIsValid)
{
	FString InAddrStr(InAddr);

	// Check for SteamID format: "steam.XXXXX" or pure numeric
	if (InAddrStr.StartsWith(MINDERA_STEAM_URL_PREFIX) || InAddrStr.IsNumeric())
	{
		InAddrStr.RemoveFromStart(MINDERA_STEAM_URL_PREFIX);

		FString SteamIPStr, SteamChannelStr;
		if (InAddrStr.Split(TEXT(":"), &SteamIPStr, &SteamChannelStr, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			const uint64 Id = FCString::Atoi64(*SteamIPStr);
			if (Id != 0)
			{
				Addr.SetSteamID64(Id);
				const int32 Channel = FCString::Atoi(*SteamChannelStr);
				P2PVirtualPort = Channel;
				bIsValid = true;
			}
			else
			{
				bIsValid = false;
			}
		}
		else
		{
			const uint64 Id = FCString::Atoi64(*InAddrStr);
			if (Id != 0)
			{
				Addr.SetSteamID64(Id);
				bIsValid = true;
			}
			else
			{
				bIsValid = false;
			}
		}
	}
	else if (SteamNetworkingUtils())
	{
		SteamNetworkingIPAddr NewAddress;
		bIsValid = NewAddress.ParseString(TCHAR_TO_ANSI(InAddr));
		Addr.SetIPAddr(NewAddress);
	}
	else
	{
		bIsValid = false;
	}
}

void FInternetAddrMindera::GetIp(uint32& OutAddr) const
{
	OutAddr = 0;
	if (IsIPAddress())
	{
		OutAddr = Addr.GetIPAddr()->GetIPv4();
	}
}

// ---------------------------------------------------------------------------
// SetPort / GetPort
// ---------------------------------------------------------------------------
void FInternetAddrMindera::SetPort(int32 InPort)
{
	if (IsSteamID())
	{
		P2PVirtualPort = InPort;
	}
	else if (IsIPAddress())
	{
		SteamNetworkingIPAddr NewAddrInfo;
		const SteamNetworkingIPAddr* Internal = Addr.GetIPAddr();
		FMemory::Memcpy(NewAddrInfo, *Internal);
		NewAddrInfo.m_port = static_cast<uint16>(InPort);
		Addr.SetIPAddr(NewAddrInfo);
	}
}

int32 FInternetAddrMindera::GetPort() const
{
	if (IsSteamID())
	{
		return P2PVirtualPort;
	}
	if (const SteamNetworkingIPAddr* Internal = Addr.GetIPAddr())
	{
		return Internal->m_port;
	}
	return 0;
}

// ---------------------------------------------------------------------------
// SetAnyAddress
// ---------------------------------------------------------------------------
void FInternetAddrMindera::SetAnyAddress()
{
	Addr.Clear();
	SteamNetworkingIPAddr NewAddress;
	NewAddress.Clear();
	Addr.SetIPAddr(NewAddress);
}

// ---------------------------------------------------------------------------
// ToString
// ---------------------------------------------------------------------------
FString FInternetAddrMindera::ToString(bool bAppendPort) const
{
	if (Addr.IsInvalid())
	{
		return TEXT("INVALID");
	}

	if (IsSteamID())
	{
		FString Result = FString::Printf(TEXT("%llu"), Addr.GetSteamID64());
		if (bAppendPort)
		{
			Result += FString::Printf(TEXT(":%d"), P2PVirtualPort);
		}
		return Result;
	}

	if (IsIPAddress())
	{
		const SteamNetworkingIPAddr* Internal = Addr.GetIPAddr();
		if (Internal && SteamNetworkingUtils())
		{
			ANSICHAR StrBuffer[SteamNetworkingIPAddr::k_cchMaxString];
			FMemory::Memzero(StrBuffer);
			Internal->ToString(StrBuffer, SteamNetworkingIPAddr::k_cchMaxString, bAppendPort);
			return FString(ANSI_TO_TCHAR(StrBuffer));
		}
	}

	return TEXT("UNKNOWN");
}

// ---------------------------------------------------------------------------
// operator== / GetTypeHash
// ---------------------------------------------------------------------------
bool FInternetAddrMindera::operator==(const FInternetAddr& Other) const
{
	const FInternetAddrMindera& SteamOther = static_cast<const FInternetAddrMindera&>(Other);
	return Addr == SteamOther.Addr && P2PVirtualPort == SteamOther.P2PVirtualPort;
}

uint32 FInternetAddrMindera::GetTypeHash() const
{
	if (IsSteamID())
	{
		return HashCombine(::GetTypeHash(Addr.GetSteamID64()), ::GetTypeHash(P2PVirtualPort));
	}
	if (IsIPAddress())
	{
		const SteamNetworkingIPAddr* IPAddr = Addr.GetIPAddr();
		uint32 Hash = 0;
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(IPAddr->m_ipv6); ++i)
		{
			Hash = HashCombine(Hash, ::GetTypeHash(IPAddr->m_ipv6[i]));
		}
		Hash = HashCombine(Hash, ::GetTypeHash(IPAddr->m_port));
		return Hash;
	}
	return 0;
}

// ---------------------------------------------------------------------------
// Clone
// ---------------------------------------------------------------------------
TSharedRef<FInternetAddr> FInternetAddrMindera::Clone() const
{
	TSharedRef<FInternetAddrMindera> NewAddress = MakeShareable(new FInternetAddrMindera());
	NewAddress->Addr = Addr;
	NewAddress->P2PVirtualPort = P2PVirtualPort;
	return NewAddress;
}

// ---------------------------------------------------------------------------
// Conversion operator: SteamNetworkingIPAddr
// ---------------------------------------------------------------------------
FInternetAddrMindera::operator const SteamNetworkingIPAddr() const
{
	const SteamNetworkingIPAddr* IPAddr = Addr.GetIPAddr();
	if (IPAddr == nullptr)
	{
		SteamNetworkingIPAddr EmptyAddr;
		EmptyAddr.Clear();
		return EmptyAddr;
	}
	return *IPAddr;
}

#endif