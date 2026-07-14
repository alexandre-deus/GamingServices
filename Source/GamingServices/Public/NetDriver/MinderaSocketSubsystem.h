// Copyright Mindera. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SocketSubsystem.h"
#include "Containers/Ticker.h"

#include "NetDriver/MinderaInternetAddr.h"

class IP2PTransport;

/** Name used to register and retrieve this socket subsystem. */
#define MINDERA_SOCKET_SUBSYSTEM_NAME FName(TEXT("MinderaP2P"))

/**
 * Socket subsystem for the Mindera P2P netdriver. SDK-free: it creates FMinderaSocket instances over
 * the active backend's IP2PTransport (fetched from the live IGamingService), and resolves addresses
 * as peer-id strings. No platform networking SDK appears here.
 */
class FMinderaSocketSubsystem : public ISocketSubsystem, public FTSTickerObjectBase
{
public:
	FMinderaSocketSubsystem();

	// -- Singleton --
	static FMinderaSocketSubsystem* Create();
	static void Destroy();

	// -- ISocketSubsystem interface --
	virtual bool Init(FString& Error) override;
	virtual void Shutdown() override;

	virtual FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, const FName& ProtocolType) override;
	virtual void DestroySocket(FSocket* Socket) override;

	virtual FAddressInfoResult GetAddressInfo(const TCHAR* HostName, const TCHAR* ServiceName = nullptr,
		EAddressInfoFlags QueryFlags = EAddressInfoFlags::Default,
		const FName ProtocolTypeName = NAME_None,
		ESocketType SocketType = ESocketType::SOCKTYPE_Unknown) override;

	virtual TSharedPtr<FInternetAddr> GetAddressFromString(const FString& InAddress) override;
	virtual bool GetHostName(FString& HostName) override;
	virtual TSharedRef<FInternetAddr> CreateInternetAddr() override;

	virtual bool HasNetworkDevice() override { return true; }
	virtual const TCHAR* GetSocketAPIName() const override { return TEXT("MinderaP2PSockets"); }

	virtual ESocketErrors GetLastErrorCode() override { return static_cast<ESocketErrors>(LastSocketError); }
	virtual ESocketErrors TranslateErrorCode(int32 Code) override { return static_cast<ESocketErrors>(Code); }

	virtual bool GetLocalAdapterAddresses(TArray<TSharedPtr<FInternetAddr>>& OutAddresses) override;
	virtual TArray<TSharedRef<FInternetAddr>> GetLocalBindAddresses() override;

	virtual bool IsSocketWaitSupported() const override { return false; }
	virtual bool RequiresChatDataBeSeparate() override { return false; }
	virtual bool RequiresEncryptedPackets() override { return false; }

	// -- FTSTickerObjectBase --
	virtual bool Tick(float DeltaTime) override;

	/** The active backend's P2P transport, from the live IGamingService (null before login / unsupported). */
	static IP2PTransport* GetTransport();

	/** Last error code set by sockets in this subsystem. */
	int32 LastSocketError = 0;

private:
	static FMinderaSocketSubsystem* SocketSingleton;
};
