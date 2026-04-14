// Copyright Mindera. All Rights Reserved.

#include "NetDriver/MinderaSocketSubsystem.h"
#include "NetDriver/MinderaSocket.h"
#include "SocketSubsystemModule.h"

#include "steam/isteamnetworkingutils.h"

DEFINE_LOG_CATEGORY_STATIC(LogMinderaSocketSub, Log, All);

FMinderaSocketSubsystem* FMinderaSocketSubsystem::SocketSingleton = nullptr;

FMinderaSocketSubsystem::FMinderaSocketSubsystem()
	: LastSocketError(0)
{
}

FMinderaSocketSubsystem* FMinderaSocketSubsystem::Create()
{
	if (SocketSingleton == nullptr)
	{
		UE_LOG(LogMinderaSocketSub, Log, TEXT("[SubSys] Create: creating new singleton"));
		SocketSingleton = new FMinderaSocketSubsystem();
	}
	else
	{
		UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] Create: returning existing singleton"));
	}
	return SocketSingleton;
}

void FMinderaSocketSubsystem::Destroy()
{
	UE_LOG(LogMinderaSocketSub, Log, TEXT("[SubSys] Destroy: singleton=%p"), SocketSingleton);
	if (SocketSingleton != nullptr)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = nullptr;
		UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] Destroy: DONE"));
	}
}

bool FMinderaSocketSubsystem::Init(FString& Error)
{
	UE_LOG(LogMinderaSocketSub, Log, TEXT("[SubSys] Init: starting"));

	// Match the official pattern: Init always succeeds.
	// ISteamNetworkingSockets may not be available yet at module startup time
	// (Steam API initializes later). The interface is resolved lazily when needed.
	ISteamNetworkingSockets* SocketInterface = GetSteamSocketsInterface();
	if (!SocketInterface)
	{
		UE_LOG(LogMinderaSocketSub, Log, TEXT("[SubSys] Init: ISteamNetworkingSockets not yet available — will resolve lazily"));
	}

	// Initialize relay network access for P2P if ready
	if (SteamNetworkingUtils())
	{
		UE_LOG(LogMinderaSocketSub, Log, TEXT("[SubSys] Init: initializing relay network access"));
		SteamNetworkingUtils()->InitRelayNetworkAccess();
	}

	UE_LOG(LogMinderaSocketSub, Log, TEXT("[SubSys] Init: SUCCESS"));
	return true;
}

void FMinderaSocketSubsystem::Shutdown()
{
	UE_LOG(LogMinderaSocketSub, Log, TEXT("[SubSys] Shutdown: shutting down"));
}

ISteamNetworkingSockets* FMinderaSocketSubsystem::GetSteamSocketsInterface()
{
	if (IsRunningDedicatedServer() && SteamGameServerNetworkingSockets())
	{
		UE_LOG(LogMinderaSocketSub, VeryVerbose, TEXT("[SubSys] GetSteamSocketsInterface: using GameServer"));
		return SteamGameServerNetworkingSockets();
	}
	ISteamNetworkingSockets* Iface = SteamNetworkingSockets();
	UE_LOG(LogMinderaSocketSub, VeryVerbose, TEXT("[SubSys] GetSteamSocketsInterface: using Client (ptr=%p)"), Iface);
	return Iface;
}

FSocket* FMinderaSocketSubsystem::CreateSocket(const FName& SocketType, const FString& SocketDescription, const FName& ProtocolType)
{
	FName ProtocolToUse = ProtocolType;
	if (ProtocolToUse.IsNone())
	{
		ProtocolToUse = MINDERA_STEAM_P2P_PROTOCOL;
	}

	UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] CreateSocket: type='%s', desc='%s', protocol='%s'"),
		*SocketType.ToString(), *SocketDescription, *ProtocolToUse.ToString());

	return new FMinderaSocket(SOCKTYPE_Streaming, SocketDescription, ProtocolToUse);
}

void FMinderaSocketSubsystem::DestroySocket(FSocket* Socket)
{
	UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] DestroySocket: Socket=%p"), Socket);
	if (Socket)
	{
		Socket->Close();
		delete Socket;
	}
}

FAddressInfoResult FMinderaSocketSubsystem::GetAddressInfo(const TCHAR* HostName, const TCHAR* ServiceName,
	EAddressInfoFlags QueryFlags, const FName ProtocolTypeName, ESocketType SocketType)
{
	UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] GetAddressInfo: HostName='%s', ServiceName='%s', Flags=%d"),
		HostName ? HostName : TEXT("null"), ServiceName ? ServiceName : TEXT("null"), (int32)QueryFlags);

	FAddressInfoResult ResultData(HostName, ServiceName);

	if (HostName == nullptr && ServiceName == nullptr)
	{
		ResultData.ReturnCode = SE_EINVAL;
		return ResultData;
	}

	TSharedPtr<FInternetAddr> SerializedAddr;

	// Handle binding / no hostname
	if (HostName == nullptr || EnumHasAnyFlags(QueryFlags, EAddressInfoFlags::BindableAddress))
	{
		TArray<TSharedPtr<FInternetAddr>> Addrs;
		GetLocalAdapterAddresses(Addrs);
		if (Addrs.Num() > 0)
		{
			SerializedAddr = Addrs[0];
		}
	}
	else
	{
		SerializedAddr = GetAddressFromString(HostName);
	}

	if (SerializedAddr.IsValid())
	{
		ResultData.ReturnCode = SE_NO_ERROR;
		UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] GetAddressInfo: resolved to %s"), *SerializedAddr->ToString(true));

		// Apply port
		if (ServiceName != nullptr && FCString::IsNumeric(ServiceName))
		{
			SerializedAddr->SetPort(FCString::Atoi(ServiceName));
		}

		ResultData.Results.Add(FAddressInfoResultData(SerializedAddr.ToSharedRef(), 0,
			SerializedAddr->GetProtocolType(), SOCKTYPE_Streaming));
	}
	else
	{
		// Fall through to platform subsystem for non-Steam addresses,
		// mirroring the official FSocketSubsystemSteam::GetAddressInfo.
		UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] GetAddressInfo: falling through to platform subsystem"));
		return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetAddressInfo(HostName, ServiceName, QueryFlags, ProtocolTypeName, SocketType);
	}

	return ResultData;
}

TSharedPtr<FInternetAddr> FMinderaSocketSubsystem::GetAddressFromString(const FString& InAddress)
{
	UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] GetAddressFromString: '%s'"), *InAddress);

	TSharedRef<FInternetAddrMindera> NewAddr = MakeShareable(new FInternetAddrMindera());

	if (InAddress.IsEmpty())
	{
		NewAddr->SetAnyAddress();
		return NewAddr;
	}

	bool bIsValid = false;
	NewAddr->SetIp(*InAddress, bIsValid);
	if (!bIsValid)
	{
		// Fall through to platform subsystem for non-Steam addresses (e.g. plain IP),
		// mirroring the official FSocketSubsystemSteam::GetAddressFromString.
		UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] GetAddressFromString: '%s' not a Steam addr, falling through to platform"), *InAddress);
		return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetAddressFromString(InAddress);
	}

	UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] GetAddressFromString: resolved '%s' -> %s"), *InAddress, *NewAddr->ToString(true));
	return NewAddr;
}

bool FMinderaSocketSubsystem::GetHostName(FString& HostName)
{
	return false;
}

TSharedRef<FInternetAddr> FMinderaSocketSubsystem::CreateInternetAddr()
{
	UE_LOG(LogMinderaSocketSub, VeryVerbose, TEXT("[SubSys] CreateInternetAddr"));
	return MakeShareable(new FInternetAddrMindera());
}

bool FMinderaSocketSubsystem::GetLocalAdapterAddresses(TArray<TSharedPtr<FInternetAddr>>& OutAddresses)
{
	UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] GetLocalAdapterAddresses"));

	ISteamNetworkingSockets* SocketInterface = GetSteamSocketsInterface();
	if (SocketInterface)
	{
		SteamNetworkingIdentity Identity;
		if (SocketInterface->GetIdentity(&Identity))
		{
			TSharedPtr<FInternetAddrMindera> IdentityAddr = MakeShareable(new FInternetAddrMindera(Identity));
			UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] GetLocalAdapterAddresses: Steam identity=%s"), *IdentityAddr->ToString(true));
			OutAddresses.Add(IdentityAddr);
		}
		else
		{
			UE_LOG(LogMinderaSocketSub, Warning, TEXT("[SubSys] GetLocalAdapterAddresses: GetIdentity failed"));
		}
	}
	else
	{
		UE_LOG(LogMinderaSocketSub, Warning, TEXT("[SubSys] GetLocalAdapterAddresses: no Steam socket interface"));
	}

	// Always provide an any address as fallback
	TSharedPtr<FInternetAddrMindera> AnyAddr = MakeShareable(new FInternetAddrMindera());
	AnyAddr->SetAnyAddress();
	OutAddresses.Add(AnyAddr);
	UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] GetLocalAdapterAddresses: total %d addresses"), OutAddresses.Num());

	return true;
}

TArray<TSharedRef<FInternetAddr>> FMinderaSocketSubsystem::GetLocalBindAddresses()
{
	UE_LOG(LogMinderaSocketSub, Verbose, TEXT("[SubSys] GetLocalBindAddresses"));
	TArray<TSharedRef<FInternetAddr>> OutAddresses;
	TArray<TSharedPtr<FInternetAddr>> AdapterAddresses;
	GetLocalAdapterAddresses(AdapterAddresses);
	for (const auto& Addr : AdapterAddresses)
	{
		OutAddresses.Add(Addr.ToSharedRef());
	}
	return OutAddresses;
}

bool FMinderaSocketSubsystem::Tick(float DeltaTime)
{
	return true;
}