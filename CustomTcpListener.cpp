#include "CustomTcpListener.h"

ACustomTcpListener::ACustomTcpListener()
{
    PrimaryActorTick.bCanEverTick = false;
    ServerSocket = nullptr;
    ServerRunnable = nullptr;
    ServerThread = nullptr;
}

void ACustomTcpListener::BeginPlay()
{
    Super::BeginPlay();
    StartTcpServer();
}

void ACustomTcpListener::StartTcpServer()
{
    // Create and bind the server socket
    ServerSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("TcpListener"), false);
    TSharedRef<FInternetAddr> ListenAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

    bool bIsValid;
    ListenAddress->SetIp(TEXT("127.0.0.1"), bIsValid);
    ListenAddress->SetPort(7777);

    if (!bIsValid || !ServerSocket->Bind(*ListenAddress) || !ServerSocket->Listen(1))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to bind or listen on the server socket."));
        return;
    }

    // Create and start the thread using FRunnableThread
    ServerRunnable = new FServerRunnable(ServerSocket);
    ServerRunnable->OnStringReceived().AddUObject(this, &ACustomTcpListener::HandleReceivedString);
    ServerThread = FRunnableThread::Create(ServerRunnable, TEXT("TcpServerThread"));
    UE_LOG(LogTemp, Log, TEXT("Tcp server thread started successfully."));
}

void ACustomTcpListener::HandleReceivedString(const FString& ReceivedString)
{
    UE_LOG(LogTemp, Log, TEXT("Received string: %s"), *ReceivedString);
    OnStringReceived.Broadcast(ReceivedString);
}

void ACustomTcpListener::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UE_LOG(LogTemp, Log, TEXT("Shutting down Tcp listener..."));

    // Signal the thread to stop
    if (ServerRunnable)
    {
        ServerRunnable->SetShouldRun(false); // Signal the runnable to stop
    }

    // Wait for the thread to terminate safely
    if (ServerThread)
    {
        UE_LOG(LogTemp, Log, TEXT("Waiting for server thread to complete..."));
        ServerThread->WaitForCompletion(); // Ensure the thread finishes cleanly
        delete ServerThread; // Free the thread memory
        ServerThread = nullptr;
    }

    // Clean up the runnable
    if (ServerRunnable)
    {
        delete ServerRunnable;
        ServerRunnable = nullptr;
    }

    // Clean up the server socket
    if (ServerSocket)
    {
        UE_LOG(LogTemp, Log, TEXT("Closing server socket..."));
        ServerSocket->Close(); // Gracefully close the socket
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ServerSocket); // Destroy the socket
        ServerSocket = nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("Tcp listener shut down successfully."));
    Super::EndPlay(EndPlayReason);
}

uint32 ACustomTcpListener::FServerRunnable::Run()
{
    ListenForConnections();
    return 0;
}

void ACustomTcpListener::FServerRunnable::ListenForConnections()
{
    UE_LOG(LogTemp, Log, TEXT("Server is listening on 127.0.0.1:7777"));

    while (bShouldRun)
    {
        bool bHasPendingConnection = false;

        if (ServerSocket->WaitForPendingConnection(bHasPendingConnection, FTimespan::FromSeconds(1.0)))
        {
            if (bHasPendingConnection)
            {
                // Accept incoming connections
                TSharedRef<FInternetAddr> ClientAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
                FSocket* ClientSocket = ServerSocket->Accept(*ClientAddress, TEXT("Accepted Connection"));

                if (ClientSocket)
                {
                    TArray<uint8> DataBuffer;
                    uint8 TempBuffer[1024];
                    int32 BytesRead;

                    // Read data from the client
                    while (ClientSocket->Recv(TempBuffer, sizeof(TempBuffer), BytesRead))
                    {
                        DataBuffer.Append(TempBuffer, BytesRead);
                    }

                    // Ensure null termination
                    if (DataBuffer.Num() > 0)
                    {
                        DataBuffer.Add(0); // Append a null terminator
                    }

                    // Convert received data to FString
                    FString ReceivedString = FString(UTF8_TO_TCHAR(DataBuffer.GetData()));

                    // Trim any whitespace or extra characters
                    ReceivedString = ReceivedString.TrimStartAndEnd();

                    // Pass the string to the game thread
                    AsyncTask(ENamedThreads::GameThread, [this, ReceivedString]()
                        {
                            StringReceivedEvent.Broadcast(ReceivedString);
                        });

                    // Close the client socket
                    ClientSocket->Close();
                    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
                }
            }
        }
        else
        {
            // An error occurred, or the socket is closed; exit the loop
            break;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Server stopped listening."));
}
