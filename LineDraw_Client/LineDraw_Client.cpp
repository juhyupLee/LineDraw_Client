// LineDraw_Client.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//
#pragma comment(lib,"ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <WS2tcpip.h>
#include "framework.h"
#include "LineDraw_Client.h"
#include <windowsx.h>
#include <iostream>
#include "RingBuffer.h"
#include "SocketLog.h"

#define MAX_LOADSTRING 100
#define WM_NETWORK (WM_USER+1)
#define SERVER_PORT 25000
#define SERVER_IP L"127.0.0.1"


// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);



struct Header
{
    unsigned short len;
};
struct DrawPacket
{
    int startX;
    int startY;
    int endX;
    int endY;
};

//-----------------------------------------------------------------------
// Juhyup Code
int g_PrevX = 0;
int g_PrevY = 0;
int g_bDraw = false;

SOCKET g_Socket;
bool g_bConnected = false;

RingBuffer g_RecvRingBuffer;
RingBuffer g_SendRingBuffer;

HWND g_hWnd;

void Network_Init(HWND hWnd);
void SelectProcess(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void SendPacket(Header* header, char* payload, int payloadSize);
void SendEvent();
void RecvEvent();
void DrawLine(int startX, int startY, int endX, int endY);

//-----------------------------------------------------------------------
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 여기에 코드를 입력합니다.

    // 전역 문자열을 초기화합니다.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_LINEDRAWCLIENT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 애플리케이션 초기화를 수행합니다:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LINEDRAWCLIENT));

    MSG msg;

    Network_Init(g_hWnd);

    // 기본 메시지 루프입니다:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}



//
//  함수: MyRegisterClass()
//
//  용도: 창 클래스를 등록합니다.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LINEDRAWCLIENT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_LINEDRAWCLIENT);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   함수: InitInstance(HINSTANCE, int)
//
//   용도: 인스턴스 핸들을 저장하고 주 창을 만듭니다.
//
//   주석:
//
//        이 함수를 통해 인스턴스 핸들을 전역 변수에 저장하고
//        주 프로그램 창을 만든 다음 표시합니다.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   
   if (!hWnd)
   {
      return FALSE;
   }
   g_hWnd = hWnd;

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  함수: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  용도: 주 창의 메시지를 처리합니다.
//
//  WM_COMMAND  - 애플리케이션 메뉴를 처리합니다.
//  WM_PAINT    - 주 창을 그립니다.
//  WM_DESTROY  - 종료 메시지를 게시하고 반환합니다.
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 메뉴 선택을 구문 분석합니다:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: 여기에 hdc를 사용하는 그리기 코드를 추가합니다...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_MOUSEMOVE:
    {
        if (g_bDraw && g_bConnected)
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            Header header;
            DrawPacket drawPacket; 

            header.len = sizeof(DrawPacket);
            drawPacket.endX = xPos;
            drawPacket.endY = yPos;
            drawPacket.startX = g_PrevX;
            drawPacket.startY = g_PrevY;
           
            SendPacket(&header, (char*)&drawPacket, sizeof(drawPacket));
        }
        break;
    }
    case WM_LBUTTONDOWN:
        g_PrevX = GET_X_LPARAM(lParam);
        g_PrevY = GET_Y_LPARAM(lParam);
        g_bDraw = true;
        break;
    case WM_LBUTTONUP:
        g_bDraw = false;
        break;

    case WM_NETWORK:
        SelectProcess(hWnd, message, wParam, lParam);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 정보 대화 상자의 메시지 처리기입니다.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;

    }
    return (INT_PTR)FALSE;
}

void Network_Init(HWND hWnd)
{
    WSAData wsaData;
    SOCKADDR_IN serverAddr;

    if (0 != WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        ERROR_LOG(L"WSAStart up() error", hWnd);
    }
    g_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_Socket == INVALID_SOCKET)
    {
        ERROR_LOG(L"socket() error", hWnd);
    }

    if (0 != WSAAsyncSelect(g_Socket, hWnd, WM_NETWORK, FD_CONNECT | FD_CLOSE | FD_READ | FD_WRITE))
    {
        ERROR_LOG(L"WSAAsyncSelect() error", hWnd);
    }

    memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    InetPton(AF_INET, SERVER_IP, &serverAddr.sin_addr.S_un.S_addr);

    if (0!= connect(g_Socket, (sockaddr*)&serverAddr, sizeof(SOCKADDR_IN)))
    { 
       ERROR_LOG(L"connect() error", g_hWnd);   
    }
}

void SelectProcess(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (WSAGETSELECTERROR(lParam))
    {
        ERROR_LOG_SELECT(L"SelectProcess() error", hWnd, WSAGETSELECTERROR(lParam));
        closesocket(wParam);
        return;
    }

    switch (WSAGETSELECTEVENT(lParam))
    {
    case FD_CONNECT:
        g_bConnected = true;
        break;
    case FD_READ:
        RecvEvent();
        break;
    case FD_WRITE:
        SendEvent();
        break;
    case FD_CLOSE:
        closesocket(g_Socket);
        break;
    }
}

void SendPacket(Header* header, char* payload, int payloadSize)
{
    int enqueueRtn = g_SendRingBuffer.Enqueue((char*)header, sizeof(Header));
    if (enqueueRtn != sizeof(Header))
        ERROR_LOG(L"SendRingbuffer() error", g_hWnd);

    enqueueRtn = g_SendRingBuffer.Enqueue(payload, payloadSize);
    if (enqueueRtn != payloadSize)
        ERROR_LOG(L"SendRingbuffer() error", g_hWnd);

    //---------------------------------------------
    // 여기서 Send Event는 송신링버퍼에서 데이터를 디큐해 진짜 send하는 코드임.
    //---------------------------------------------
    SendEvent();
}

void RecvEvent()
{
    //----------------------------------------------------
    // 수신링버퍼에 recv한 데이터를 저장함 
    //----------------------------------------------------
    char buffer[100];
    int recvRtn = recv(g_Socket, buffer, 100, 0);
   // int recvRtn = recv(g_Socket, g_RecvRingBuffer.GetRearBufferPtr(), g_RecvRingBuffer.GetDirectEnqueueSize(), 0);
    g_RecvRingBuffer.Enqueue(buffer, recvRtn);
    //g_RecvRingBuffer.MoveRear(recvRtn); //수동으로 Rear 포인터 옮겨주기

    if (recvRtn <= 0)
    {
        //WOULDBLOCK을 제외한 오류면, closesocket
        if (WSAGetLastError() != WSAEWOULDBLOCK)
        {
            ERROR_LOG(L"recvError()",g_hWnd);
            closesocket(g_Socket);
            return;
        }
    }
    //----------------------------------------------------
    // 링버퍼를 통해서, 수신된 패킷이 완성됬는지 확인해야함.
    //----------------------------------------------------

    if (g_RecvRingBuffer.GetUsedSize() < sizeof(Header))
        return;

    Header peekHeader;
    int peekRtn = g_RecvRingBuffer.Peek((char*)&peekHeader, sizeof(Header));

    if (g_RecvRingBuffer.GetUsedSize() < sizeof(Header) + peekHeader.len)
        return;

    g_RecvRingBuffer.MoveFront(2);

    DrawPacket drawPacket;

    //----------------------------------------------------
    // 패킷+헤더만큼 사이즈가있는걸 확인했다면, 헤더는 필요없으니 디큐하거나 MoveFront를하고, 
    // 서버에서 보내온 데이터를 패킷에 담은 뒤, DrawLine함수를 실행한다.
    //----------------------------------------------------
    int deqRtn = g_RecvRingBuffer.Dequeue((char*)&drawPacket, sizeof(DrawPacket));

    DrawLine(drawPacket.startX, drawPacket.startY, drawPacket.endX, drawPacket.endY);
}

void SendEvent()
{
    //---------------------------------------------
    // 송신링버퍼에있는거를 send해야된다
    // 그런데 또 send 에러가 날 수도 있는 상황이다.
    // 그래서 peek을해서 보내보고, 
    // 에러가없다면 그만큼 front를 땡겨와야된다.
    //---------------------------------------------
    char buffer[100];

    while (g_SendRingBuffer.GetUsedSize() != 0)
    {
        int retPeek = g_SendRingBuffer.Peek(buffer, g_SendRingBuffer.GetUsedSize());
        int sendRtn = send(g_Socket, buffer, retPeek, 0);
        g_SendRingBuffer.MoveFront(sendRtn);
        if (sendRtn < 0)
        {
            ERROR_LOG(L"sendError()", g_hWnd);
        }
    }

}

void DrawLine(int startX, int startY, int endX, int endY)
{
    HDC hdc = GetDC(g_hWnd);
    MoveToEx(hdc, startX, startY, NULL);
    LineTo(hdc, endX, endY);
    g_PrevX = endX;
    g_PrevY = endY;
    ReleaseDC(g_hWnd, hdc);
}
