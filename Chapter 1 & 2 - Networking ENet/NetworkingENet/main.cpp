#include <enet/enet.h>
#include <thread>
#include <iostream>
#include <cstring>
#include <Windows.h>
using namespace std;

ENetAddress address;
ENetHost* server = nullptr;
ENetHost* client = nullptr;
string leaveCheck = " ";
char name[27];

bool CreateServer()
{
    /* Bind the server to the default localhost.     */
    /* A specific host address can be specified by   */
    /* enet_address_set_host (& address, "x.x.x.x"); */
    address.host = ENET_HOST_ANY;
    /* Bind the server to port 1234. */
    address.port = 1234;
    server = enet_host_create(&address /* the address to bind the server host to */,
        32      /* allow up to 32 clients and/or outgoing connections */,
        2      /* allow up to 2 channels to be used, 0 and 1 */,
        0      /* assume any amount of incoming bandwidth */,
        0      /* assume any amount of outgoing bandwidth */);

    return server != nullptr;
}

bool CreateClient()
{
    client = enet_host_create(NULL /* create a client host */,
        1 /* only allow 1 outgoing connection */,
        2 /* allow up 2 channels to be used, 0 and 1 */,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of outgoing bandwidth */);

    return client != nullptr;
}

void UserLoop()
{

    cout << "Username: ";
    cin.ignore();
    cin.getline(name, 27);
    strcat_s(name, ": ");

    while (leaveCheck != "$leave")
    {
        char fullMessage[400] = "";
        strcat_s(fullMessage, name);
        char message[300];
        cout << name;
        cin.getline(message, 300);
        strcat_s(fullMessage, message);
        leaveCheck = message;
        if (leaveCheck == "$leave")
            break;
        ENetPacket* packet = enet_packet_create(fullMessage,
            400,
            ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(client, 0, packet);
        enet_host_flush(client);
    }
}

bool fromThisClient(char myName[27], char* theirName)
{
    for (int i = 0; i < 27; i++)
    {
        if (myName[i] != theirName[i])
            return false;
        else if (myName[i] == ':')
            break;
    }
    return true;
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
                "An error occurred while trying to create an ENet server host.\n");
            exit(EXIT_FAILURE);
        }
        ENetEvent event;

        while (true)
        {
            /* Wait up to 1000 milliseconds for an event. */
            while (enet_host_service(server, &event, 1000) > 0)
            {
                switch (event.type)
                {
                case ENET_EVENT_TYPE_CONNECT:
                    cout << "A new client connected from "
                        << event.peer->address.host
                        << ":" << event.peer->address.port
                        << endl;
                    /* Store any relevant client information here. */
                    event.peer->data = (void*)event.peer->address.port;
                    {
                        break;  
                    }
                case ENET_EVENT_TYPE_RECEIVE:;
                    cout << "\r" << event.packet->data << endl;
                    /* Clean up the packet now that we're done using it. */
                    //enet_packet_destroy(event.packet);
                    {
                        enet_host_broadcast(server, 0, event.packet);
                        enet_host_flush(server);
                        break;
                    }
                }
            }
        }
    }
    else if (UserInput == 2)
    {
        if (!CreateClient())
        {
            fprintf(stderr,
                "An error occurred while trying to create an ENet client host.\n");
            exit(EXIT_FAILURE);
        }

        ENetAddress address;
        ENetEvent event;
        ENetPeer* peer;
        /* Connect to some.server.net:1234. */
        enet_address_set_host(&address, "127.0.0.1");
        address.port = 1234;
        /* Initiate the connection, allocating the two channels 0 and 1. */
        peer = enet_host_connect(client, &address, 2, 0);
        if (peer == NULL)
        {
            fprintf(stderr,
                "No available peers for initiating an ENet connection.\n");
            exit(EXIT_FAILURE);
        }
        
        /* Wait up to 5 seconds for the connection attempt to succeed. */
        if (enet_host_service(client, &event, 5000) > 0 &&
            event.type == ENET_EVENT_TYPE_CONNECT)
        {
            cout << "Connection to 127.0.0.1:1234 succeeded." << endl;
        }
        else
        {
            /* Either the 5 seconds are up or a disconnect event was */
            /* received. Reset the peer in the event the 5 seconds   */
            /* had run out without any significant event.            */
            enet_peer_reset(peer);
            cout << "Connection to 127.0.0.1:1234 failed." << endl;
        }
        thread UL = thread(UserLoop);
        UL.detach();
        while (leaveCheck != "$leave")
        {
            /* Wait up to 1000 milliseconds for an event. */
            while (enet_host_service(client, &event, 1000) > 0)
            {
                switch (event.type)
                {
                case ENET_EVENT_TYPE_RECEIVE:
                    if (!fromThisClient(name, (char*)event.packet->data))
                    {
                        cout << "\r                     ";
                        cout << "\r" << event.packet->data << endl;
                        /* Clean up the packet now that we're done using it. */
                        enet_packet_destroy(event.packet);
                        cout << name;
                    }
                }
            }
        }
    }
    else
    {
        cout << "Invalid Input" << endl;
    }

    if (server != nullptr)
    {
        enet_host_destroy(server);
    }

    if (client != nullptr)
    {
        enet_host_destroy(client);
    }


    return EXIT_SUCCESS;
}