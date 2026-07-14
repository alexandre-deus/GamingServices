// Copyright Mindera. All Rights Reserved.

#include "NetDriver/MinderaSocketSubsystem.h"
#include "NetDriver/MinderaSocket.h"
#include "Native/Interfaces/IP2PTransport.h"
#include "GamingServices.h"
#include "Native/IGamingService.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMinderaSocketSub, Log, All);

FMinderaSocketSubsystem* FMinderaSocketSubsystem::SocketSingleton = nullptr;

FMinderaSocketSubsystem::FMinderaSocketSubsystem()
	: FTSTickerObjectBase(0.0f)
{
}

FMinderaSocketSubsystem* FMinderaSocketSubsystem::Create()
{
	if (SocketSingleton == nullptr)
	{
		SocketSingleton = new FMinderaSocketSubsystem();
	}
	return SocketSingleton;
}

void FMinderaSocketSubsystem::Destroy()
{
	if (SocketSingleton != nullptr)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = nullptr;
	}
}

IP2PTransport* FMinderaSocketSubsystem::GetTransport()
{
	if (FGamingServicesModule* Module = FModuleManager::GetModulePtr<FGamingServicesModule>(TEXT("GamingServices")))
	{
		return Module->GetService().GetP2PTransport();
	}
	return nullptr;
}

bool FMinderaSocketSubsystem::Init(FString& Error)
{
	return true;
}

void FMinderaSocketSubsystem::Shutdown()
{
}

FSocket* FMinderaSocketSubsystem::CreateSocket(const FName& SocketType, const FString& SocketDescription, const FName& ProtocolType)
{
	IP2PTransport* Transport = GetTransport();
	if (!Transport)
	{
		UE_LOG(LogMinderaSocketSub, Warning, TEXT("[SubSys] CreateSocket: no P2P transport available (not logged in?)"));
		return nullptr;
	}
	return new FMinderaSocket(Transport, SOCKTYPE_Datagram, SocketDescription, ProtocolType);
}

void FMinderaSocketSubsystem::DestroySocket(FSocket* Socket)
{
	delete Socket;
}

FAddressInfoResult FMinderaSocketSubsystem::GetAddressInfo(const TCHAR* HostName, const TCHAR* ServiceName,
	EAddressInfoFlags QueryFlags, const FName ProtocolTypeName, ESocketType SocketType)
{
	FAddressInfoResult Result(HostName, ServiceName);
	if (HostName != nullptr)
	{
		TSharedRef<FInternetAddrMindera> Addr = MakeShareable(new FInternetAddrMindera());
		bool bValid = false;
		Addr->SetIp(HostName, bValid);
		if (bValid)
		{
			Result.ReturnCode = SE_NO_ERROR;
			Result.Results.Add(FAddressInfoResultData(Addr, 0, MINDERA_P2P_PROTOCOL, SOCKTYPE_Datagram));
		}
		else
		{
			Result.ReturnCode = SE_HOST_NOT_FOUND;
		}
	}
	return Result;
}

TSharedPtr<FInternetAddr> FMinderaSocketSubsystem::GetAddressFromString(const FString& InAddress)
{
	TSharedRef<FInternetAddrMindera> Addr = MakeShareable(new FInternetAddrMindera());
	bool bValid = false;
	Addr->SetIp(*InAddress, bValid);
	return Addr;
}

bool FMinderaSocketSubsystem::GetHostName(FString& HostName)
{
	if (IP2PTransport* Transport = GetTransport())
	{
		HostName = Transport->GetLocalPeerId();
		return true;
	}
	return false;
}

TSharedRef<FInternetAddr> FMinderaSocketSubsystem::CreateInternetAddr()
{
	return MakeShareable(new FInternetAddrMindera());
}

bool FMinderaSocketSubsystem::GetLocalAdapterAddresses(TArray<TSharedPtr<FInternetAddr>>& OutAddresses)
{
	if (IP2PTransport* Transport = GetTransport())
	{
		TSharedRef<FInternetAddrMindera> Addr = MakeShareable(new FInternetAddrMindera());
		Addr->SetPeerId(Transport->GetLocalPeerId());
		OutAddresses.Add(Addr);
		return true;
	}
	return false;
}

TArray<TSharedRef<FInternetAddr>> FMinderaSocketSubsystem::GetLocalBindAddresses()
{
	TArray<TSharedRef<FInternetAddr>> Addresses;
	if (IP2PTransport* Transport = GetTransport())
	{
		TSharedRef<FInternetAddrMindera> Addr = MakeShareable(new FInternetAddrMindera());
		Addr->SetPeerId(Transport->GetLocalPeerId());
		Addresses.Add(Addr);
	}
	return Addresses;
}

bool FMinderaSocketSubsystem::Tick(float DeltaTime)
{
	// Pump the transport so incoming connections are accepted even before a netdriver ticks.
	if (IP2PTransport* Transport = GetTransport())
	{
		Transport->Tick();
	}
	return true;
}
