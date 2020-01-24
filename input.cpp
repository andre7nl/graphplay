
#include <input.h>

#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <memory.h>
#include <iostream>

// Need to link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 1024 * 6
#define DEFAULT_PORT "13000"

point graph[NSIZE];
unsigned __int32 t_start = 0;
unsigned __int32 timerange;
unsigned __int32 time_per_frame;
unsigned __int32 time_frames;
int glwait;
int n = 0;
Frames frames(4);
int fstart; // hold start for graph event

int dataLoopTCP(sendFunc);
int dataLoopSimulatie(sendFunc);
//int (*dataLoop_ptr)(sendFunc) = &dataLoopSimulatie;
int (*dataLoop_ptr)(sendFunc) = &dataLoopTCP;

void time_calculate(int pos, char *timebuf)
{
    unsigned __int32 lvalue;
    memcpy(&lvalue, timebuf, 4);
    if (t_start == 0)
        t_start = lvalue;
    if (lvalue < t_start)
    {
        time_per_frame = 1000000000 + lvalue - t_start;
        //printf("%I32u %I32u\n", lvalue, 1000000000 + lvalue - t_start);
    }
    else
    {
        time_per_frame = lvalue - t_start;
        //printf("%I32u %I32u\n", lvalue, lvalue - t_start);
    }
    time_frames += time_per_frame;
    t_start = lvalue;
    if (pos == 0)//NSIZE)
    {
        timerange = time_frames / 1000000;
        time_frames = 0;
    }
}

int dataReceived(char *recvbuf, int buflen, sendFunc sendData)
{
    unsigned int value;
    unsigned int offset = 0;
    while (offset <= buflen - 6)
    {
        while (glwait == 1)
            ;
        memcpy(&value, recvbuf + offset + 4, 2);
        graph[n].x = n;
        graph[n].y = value;
        offset += 6;
        n += 1;
        if (frames.frame_complete(&n, &fstart))
        {
            time_calculate(n, recvbuf + offset - 6);
            glwait = 1;
            sendData(&fstart, &frames.size);
            while (glwait == 1)
                ;
        }
    }
    return 1;
}

int dataLoopTCP(sendFunc sendData)
{
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    int iSendResult;
    int iReceiveResult;
    char recvbuf[DEFAULT_BUFLEN];
    //int recvbuflen = 6 * fsize;
    //int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0)
    {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET)
    {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR)
    {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // Accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET)
    {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // No longer need server socket
    closesocket(ListenSocket);

    // Receive until the peer shuts down the connection
    do
    {

        iResult = recv(ClientSocket, recvbuf, frames.recvbuflen, MSG_WAITALL);
        if (iResult > 0)
        {
            iReceiveResult = dataReceived(recvbuf, iResult, sendData);
        }
        else if (iResult == 0)
            printf("Connection closing...\n");
        else
        {
            printf("recv failed with error: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            return 1;
        }

    } while (iResult > 0);

    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR)
    {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}

float start_shift = 0.4;
int dataLoopSimulatie(sendFunc sendData)
{
    unsigned int value;
    unsigned __int32 lvalue;
    float freq = 1.0, shift = 0.5;
    char recvbuf[6 * NSIZE]; // 2 = frames
    int size = frames.size;
    int recvbuflen = 6 * frames.size;
    unsigned int offset = 0;
    int iReceiveResult;
    LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
    LARGE_INTEGER Frequency;

    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&StartingTime);
    for (size_t j = 0; j < 1000; j++)
    {
        /* code */
        for (int i = 0; i < NSIZE; i++)
        {
            QueryPerformanceCounter(&EndingTime);
            ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
            ElapsedMicroseconds.QuadPart *= 1000000;
            ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
            lvalue = (unsigned __int32)(ElapsedMicroseconds.QuadPart * 1000);
            //lvalue = GetTickCount() * 1000000;
            for (size_t i = 0; i < 1500000; i++) // 10000 -> 22ms (+ 16ms)
            {
                //GetTickCount();
            }

            memcpy(recvbuf + offset, &lvalue, 4);
            float x = 2 * ((float)(i)) / NSIZE - 1.0;
            value = (unsigned int)(512 * sin(start_shift + 2 * freq * x * 3.1416 + shift) + 512);
            memcpy(recvbuf + offset + 4, &value, 2);
            offset += 6;
            if ((i % size) == size - 1)
            {
                iReceiveResult = dataReceived(recvbuf, recvbuflen, sendData);
                offset = 0;
            };
        }
        //Sleep(100);
        start_shift += shift;
        // iReceiveResult = dataReceived(recvbuf, recvbuflen, sendData);
        // offset = 0;
    }
    return 1;
}
// int dataLoopSimulatie(sendFunc sendData)
// {
//     unsigned int value;
//     unsigned __int32 lvalue;
//     float start_shift = 0.4, freq = 1.0, shift = 0.1;
//     for (size_t j = 0; j < 1000; j++)
//     {
//         /* code */
//         for (int i = 0; i < NSIZE; i++)
//         {
//             while (glwait == 1)
//                 ;
//             graph[i].x = (float)i;
//             float x = 2 * ((float)(i)) / NSIZE - 1.0;
//             graph[i].y = (float)(512 * sin(start_shift + 2 * freq * x * 3.1416 + shift) + 512); // / (1.0 + x * x);
//         }
//         Sleep(20);
//         lvalue = GetTickCount();
//         if (t_start == 0)
//             t_start = lvalue;
//         if (lvalue < t_start)
//         {
//             timerange = 1000000000 + lvalue - t_start;
//             //printf("%I32u %I32u\n", lvalue, 1000000000 + lvalue - t_start);
//         }
//         else
//         {
//             timerange = lvalue - t_start;
//             //printf("%I32u %I32u\n", lvalue, lvalue - t_start);
//         }

//         t_start = lvalue;
//         start_shift += shift;
//         glwait = 1;
//         sendData();
//         while (glwait == 1)
//             ;
//     }
//     return 1;
// }
// int start_shift=0.1,freq=5
// void dataLoopSimulatie(sendFunc sendData)
// {
//     float shift = 0.4;
//     struct timespec tsx;
//     long diff, nanostart;

//     timespec_get(&ts, TIME_UTC);
//     printf("Start time: %lli %09ld UTC\n", ts.tv_sec, ts.tv_nsec);
//     for (size_t j = 0; j < 1000; j++)
//     {
//         /* code */
//         long t =0;
//         for (int i = 0; i < NSIZE; i++)
//         {
//             float x = 2 * ((float)(i)) / NSIZE - 1.0;
//             graph[i].x = x;
//             graph[i].y = sin(start_shift + 2 *freq * graph[i].x * M_PI + shift); // / (1.0 + x * x);
//             for (size_t i = 0; i < 750; i++)
//             {
//                 t = sqrt(x);
//             }

//             //_sleep(1);
//         }

//          sendData();

//          start_shift += shift;
//         _sleep(2);
//     }
//     timespec_get(&ts, TIME_UTC);
//     printf("End time: %lli %09ld UTC\n", ts.tv_sec, ts.tv_nsec);
// }
