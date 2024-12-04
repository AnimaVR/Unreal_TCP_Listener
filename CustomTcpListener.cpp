#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/Runnable.h"
#include "Async/Async.h"
#include "CustomTcpListener.generated.h"

// Delegate to broadcast the received string to Blueprints
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStringReceived, const FString&, ReceivedString);

UCLASS()
class MAGEGAME_API ACustomTcpListener : public AActor
{
    GENERATED_BODY()

public:
    ACustomTcpListener();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Start the server
    void StartTcpServer();

public:
    // Expose the received string to Blueprints
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnStringReceived OnStringReceived;

private:
    FSocket* ServerSocket;

    // Runnable class for managing the server thread
    class FServerRunnable : public FRunnable
    {
    public:
        FServerRunnable(FSocket* InServerSocket)
            : ServerSocket(InServerSocket), bShouldRun(true) {
        }

        virtual ~FServerRunnable() = default;

        virtual bool Init() override { return true; }

        virtual uint32 Run() override;

        virtual void Stop() override { bShouldRun = false; }

        // Check if the thread should keep running
        bool IsRunning() const { return bShouldRun; }

        // Stop the thread
        void SetShouldRun(bool bRun) { bShouldRun = bRun; }

        // Delegate to broadcast the received string
        DECLARE_EVENT_OneParam(FServerRunnable, FOnStringReceivedEvent, const FString&)
        FOnStringReceivedEvent& OnStringReceived() { return StringReceivedEvent; }

    private:
        void ListenForConnections();

        FSocket* ServerSocket;
        bool bShouldRun;
        FOnStringReceivedEvent StringReceivedEvent;
    };

    FServerRunnable* ServerRunnable;
    FRunnableThread* ServerThread;

    void HandleReceivedString(const FString& ReceivedString);
};
