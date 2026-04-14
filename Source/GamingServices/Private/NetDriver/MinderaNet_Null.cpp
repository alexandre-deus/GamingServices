#if !defined(USE_STEAMWORKS) && !defined(USE_EOS)

#include "NetDriver/MinderaNetDriver.h"
#include "NetDriver/MinderaNetConnection.h"
#include "SocketSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogMinderaNet, Log, All);

// Empty backend implementations
struct UMinderaNetConnection::FImpl {};
struct UMinderaNetDriver::FImpl {};

// ============================================================================
// UMinderaNetConnection — Null implementation
// ============================================================================

UMinderaNetConnection::~UMinderaNetConnection() = default;

void UMinderaNetConnection::InitLocalConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL,
                                                EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	Super::InitLocalConnection(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);
}

void UMinderaNetConnection::InitRemoteConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL,
                                                  const FInternetAddr& InRemoteAddr, EConnectionState InState,
                                                  int32 InMaxPacket, int32 InPacketOverhead)
{
	Super::InitRemoteConnection(InDriver, InSocket, InURL, InRemoteAddr, InState, InMaxPacket, InPacketOverhead);
}

void UMinderaNetConnection::LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	// No backend available
}

FString UMinderaNetConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	return TEXT("null");
}

FString UMinderaNetConnection::LowLevelDescribe()
{
	return TEXT("MinderaNetConnection (null backend)");
}

void UMinderaNetConnection::CleanUp()
{
	Super::CleanUp();
}

// ============================================================================
// UMinderaNetDriver — Null implementation
// ============================================================================

UMinderaNetDriver::~UMinderaNetDriver() = default;

bool UMinderaNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL,
                                 bool bReuseAddressAndPort, FString& Error)
{
	Error = TEXT("MinderaNetDriver: No networking backend available");
	UE_LOG(LogMinderaNet, Warning, TEXT("%s"), *Error);
	return false;
}

bool UMinderaNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	Error = TEXT("MinderaNetDriver: No networking backend available");
	return false;
}

bool UMinderaNetDriver::InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort,
                                   FString& Error)
{
	Error = TEXT("MinderaNetDriver: No networking backend available");
	return false;
}

void UMinderaNetDriver::TickDispatch(float DeltaTime)
{
	Super::TickDispatch(DeltaTime);
}

void UMinderaNetDriver::Shutdown()
{
	Super::Shutdown();
}

bool UMinderaNetDriver::IsNetResourceValid()
{
	return false;
}

ISocketSubsystem* UMinderaNetDriver::GetSocketSubsystem()
{
	return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
}

#endif // !USE_STEAMWORKS && !USE_EOS
