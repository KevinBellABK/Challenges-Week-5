#include <enet/enet.h>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>

using namespace std;

mutex Mutex;
ENetHost* NetHost = nullptr;
ENetPeer* Peer = nullptr;
bool IsServer = false;
thread* PacketThread = nullptr;
thread* UserPacketSender = nullptr;
bool gameStart = false;
int randNum = 0;
int currGuess = INT_MIN;
bool waitingForGuessCheck = false;
bool guessFound = false;
bool ngPrinted = false;
bool isFirstRun = true;

void BroadcastBool(bool newBool, string boolName);

enum PacketHeaderTypes
{
    PHT_Invalid = 0,
    PHT_Number,
    PHT_Message,
    PHT_Bool,
    PHT_Result
};

struct GamePacket
{
    GamePacket() {}
    PacketHeaderTypes Type = PHT_Invalid;
};

struct NumberPacket : public GamePacket
{
    int m_num;

    NumberPacket(int num)
    {
        m_num = num;
        Type = PHT_Number;
    }
};

struct MessagePacket : public GamePacket
{
    MessagePacket(string message)
    {
        m_message = message;
        Type = PHT_Message;
    }

    string m_message;
};

struct ResultPacket : public GamePacket
{
    int m_guess;
    string m_result;

    ResultPacket(int guess)
    {
        if (guess == randNum)
        {
            BroadcastBool(true, "GuessFound");
            m_result = ": Correct!";
            guessFound = true;
        }
        else if (guess < randNum)
        {
            m_result = ": Go Higher!";
        }
        else
        {
            m_result = ": Go Lower!";
        }
        m_guess = guess;
        Type = PHT_Result;
    }
};

struct BoolPacket : public GamePacket
{
    bool m_bool;
    string m_boolName;

    BoolPacket(bool newBool, string boolName)
    {
        m_bool = newBool;
        m_boolName = boolName;
        Type = PHT_Bool;
    }
};

//can pass in a peer connection if wanting to limit
bool CreateServer()
{
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 1234;
    NetHost = enet_host_create(&address /* the address to bind the server host to */,
        32      /* allow up to 32 clients and/or outgoing connections */,
        2      /* allow up to 2 channels to be used, 0 and 1 */,
        0      /* assume any amount of incoming bandwidth */,
        0      /* assume any amount of outgoing bandwidth */);

    return NetHost != nullptr;
}

bool CreateClient()
{
    NetHost = enet_host_create(NULL /* create a client host */,
        1 /* only allow 1 outgoing connection */,
        2 /* allow up 2 channels to be used, 0 and 1 */,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of outgoing bandwidth */);

    return NetHost != nullptr;
}

bool AttemptConnectToServer()
{
    ENetAddress address;
    /* Connect to some.server.net:1234. */
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 1234;
    /* Initiate the connection, allocating the two channels 0 and 1. */
    Peer = enet_host_connect(NetHost, &address, 2, 0);
    return Peer != nullptr;
}

void BroadcastMessage(string message)
{
    MessagePacket* messageToSend = new MessagePacket(message);
    ENetPacket* packet = enet_packet_create(messageToSend,
        sizeof(MessagePacket),
        ENET_PACKET_FLAG_RELIABLE);

    // One could also broadcast the packet by
    enet_host_broadcast(NetHost, 0, packet);
    //enet_peer_send(event.peer, 0, packet);

    // One could just use enet_host_service() instead. 
    //enet_host_service();
    enet_host_flush(NetHost);
    delete messageToSend;
}


void BroadcastBool(bool newBool, string boolName)
{
    BoolPacket* boolToSend = new BoolPacket(newBool, boolName);
    ENetPacket* packet = enet_packet_create(boolToSend,
        sizeof(BoolPacket),
        ENET_PACKET_FLAG_RELIABLE);

    /* One could also broadcast the packet by         */
    enet_host_broadcast(NetHost, 0, packet);
    //enet_peer_send(event.peer, 0, packet);

    /* One could just use enet_host_service() instead. */
    //enet_host_service();
    enet_host_flush(NetHost);
    delete boolToSend;
}

void BroadcastGuess(int num)
{
    NumberPacket* numToSend = new NumberPacket(num);
    ENetPacket* packet = enet_packet_create(numToSend,
        sizeof(NumberPacket),
        ENET_PACKET_FLAG_RELIABLE);

    /* One could also broadcast the packet by         */
    enet_host_broadcast(NetHost, 0, packet);
    //enet_peer_send(event.peer, 0, packet);

    /* One could just use enet_host_service() instead. */
    //enet_host_service();
    enet_host_flush(NetHost);
    delete numToSend;
}

void BroadcastResult(int guess)
{
    ResultPacket* resultToSend = new ResultPacket(guess);
    ENetPacket* packet = enet_packet_create(resultToSend,
        sizeof(ResultPacket),
        ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(NetHost, 0, packet);
    enet_host_flush(NetHost);
    delete resultToSend;
}

void SendAMessage(string message, ENetPeer* peer)
{
    MessagePacket* messageToSend = new MessagePacket(message);
    ENetPacket* packet = enet_packet_create(messageToSend,
        sizeof(MessagePacket),
        ENET_PACKET_FLAG_RELIABLE);

    //enet_host_broadcast(NetHost, 0, packet);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(NetHost);
    delete messageToSend;
}

void clearLine()
{
    cout << "\r                \r";
    ngPrinted = false;
}

void printNewGuess()
{
    lock_guard<mutex> Guard(Mutex);
    if (!ngPrinted)
    {
        cout << (isFirstRun ? "New Guess: " : "\nNew Guess: ");
        isFirstRun = false;
        ngPrinted = true;
    }
}

void HandleReceivePacket(const ENetEvent& event)
{
    GamePacket* RecGamePacket = (GamePacket*)(event.packet->data);
    if (RecGamePacket)
    {
        if (IsServer)
        {
            if (gameStart)
            {
                cout << "Received Game Packet " << endl;

                switch (RecGamePacket->Type)
                {
                case PHT_Number:
                    NumberPacket* NewGuess = (NumberPacket*)(RecGamePacket);
                    BroadcastResult(NewGuess->m_num);
                    break;
                }
            }
            else
            {
                SendAMessage("game != started", event.peer);
            }
        }
        else
        {
            switch (RecGamePacket->Type)
            {
            case PHT_Message:
            {
                MessagePacket* newMessage = (MessagePacket*)(RecGamePacket);
                clearLine();
                cout << newMessage->m_message;
                printNewGuess();
                break;
            }
            case PHT_Bool:
            {
                BoolPacket* newBool = (BoolPacket*)(RecGamePacket);
                if (newBool->m_boolName._Equal("GameStart"))
                    gameStart = newBool->m_bool;
                else if (newBool->m_boolName._Equal("GuessFound"))
                    guessFound = newBool->m_bool;
                break;
            }
            case PHT_Result:
                ResultPacket* newResult = (ResultPacket*)(RecGamePacket);
                clearLine();
                cout << newResult->m_guess << newResult->m_result;
                if (!guessFound)
                {
                    if (currGuess == newResult->m_guess)
                    {
                        waitingForGuessCheck = false;
                        currGuess = INT_MIN;
                    }
                    printNewGuess();
                }
            }
        }
    }
    else
    {
        cout << "Invalid Packet" << endl;
    }

    /* Clean up the packet now that we're done using it. */
    enet_packet_destroy(event.packet);
    {
        enet_host_flush(NetHost);
    }
}

void ServerProcessPackets()
{
    while (!guessFound)
    {
        ENetEvent event;
        while (enet_host_service(NetHost, &event, 1000) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                cout << "A new client connected from "
                    << event.peer->address.host
                    << ":" << event.peer->address.port
                    << endl;
                /* Store any relevant client information here. */
                event.peer->data = (void*)("Client information");
                if (NetHost->connectedPeers == 2)
                {
                    gameStart = true;
                    srand(time(0));
                    randNum = rand() % 100;
                    BroadcastBool(true, "GameStart");
                    BroadcastMessage("GAME START!");
                    cout << "Random Number: " << randNum << endl;
                }

                break;
            case ENET_EVENT_TYPE_RECEIVE:
                HandleReceivePacket(event);
                if (guessFound)
                    Sleep(500);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                cout << (char*)event.peer->data << " disconnected." << endl;
                /* Reset the peer's client information. */
                event.peer->data = NULL;
                //notify remaining player that the game is done due to player leaving
            }
        }
    }
}

void ClientProcessPackets()
{
    while (!guessFound)
    {
        ENetEvent event;
        /* Wait up to 1000 milliseconds for an event. */
        while (enet_host_service(NetHost, &event, 1000) > 0)
        {
            switch (event.type)
            {
            case  ENET_EVENT_TYPE_CONNECT:
                clearLine();
                cout << "Connection succeeded";
                printNewGuess();
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                HandleReceivePacket(event);
                break;
            }
        }
    }
}

void ClientPacketSender()
{
    while (!guessFound)
    {
        if (!waitingForGuessCheck)
        {
            printNewGuess();
            int guess;
            cin >> guess;
            currGuess = guess;
            BroadcastGuess(guess);
            ngPrinted = false;
            waitingForGuessCheck = true;
        }
    }
}

int main(int argc, char** argv)
{
    if (enet_initialize() != 0)
    {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        cout << "An error occurred while initializing ENet." << endl;
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    cout << "1) Create Server " << endl;
    cout << "2) Create Client " << endl;
    int UserInput;
    cin >> UserInput;
    if (UserInput == 1)
    {
        if (!CreateServer())
        {
            fprintf(stderr,
                "An error occurred while trying to create an ENet server.\n");
            exit(EXIT_FAILURE);
        }

        IsServer = true;
        cout << "waiting for players to join..." << endl;
        PacketThread = new thread(ServerProcessPackets);
    }
    else if (UserInput == 2)
    {
        if (!CreateClient())
        {
            fprintf(stderr,
                "An error occurred while trying to create an ENet client host.\n");
            exit(EXIT_FAILURE);
        }

        ENetEvent event;
        if (!AttemptConnectToServer())
        {
            fprintf(stderr,
                "No available peers for initiating an ENet connection.\n");
            exit(EXIT_FAILURE);
        }
        
        UserPacketSender = new thread(ClientPacketSender);
        //UserPacketSender->detach();
        PacketThread = new thread(ClientProcessPackets);

        //handle possible connection failure
        {
            //enet_peer_reset(Peer);
            //cout << "Connection to 127.0.0.1:1234 failed." << endl;
        }
    }
    else
    {
        cout << "Invalid Input" << endl;
    }

    if (PacketThread)
        PacketThread->join();
    if (UserPacketSender)
        UserPacketSender->join();

    delete PacketThread;
    delete UserPacketSender;

    if (NetHost != nullptr)
    {
        enet_host_destroy(NetHost);
    }
    if (!IsServer)
    {
        cout << "Enter any key to exit...\n";
        cin.ignore();
    }
    return EXIT_SUCCESS;
}