//----------------------------------------------------------------------
// ex10 TCP GUI 多人数 チャット
//		クライアント
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// #include
//----------------------------------------------------------------------
#define	WIN32_LEAN_AND_MEAN		// 重複定義防止
#define _CRT_SECURE_NO_WARNINGS 1;
#include <Windows.h>
#include <WindowsX.h>
#include <WinSock2.h>
#include <string.h>
#include "resource.h"

//----------------------------------------------------------------------
// lib
//----------------------------------------------------------------------
#pragma comment( lib, "ws2_32.lib" )

//----------------------------------------------------------------------
// define
//----------------------------------------------------------------------
#define MSGBUFFSIZE			256			// チャットのメッセージの最大長 
#define MSGTYPE_CHAT		1			// メッセージタイプ：チャット 
#define MSGTYPE_LOGOUT		2			// メッセージタイプ：ログアウト 
#define MSGTYPE_ALLLOGOUT	3			// メッセージタイプ：全員ログアウト 
#define MSGTYPE_LOGIN		4			// メッセージタイプ：ログイン

#define	SENDMSGSIZE			1024		// 送信文字列サイズ
#define	DISPMSGSIZE			1024*10		// 表示文字列サイズ
#define	CRLF				"\r\n"		// 改行コード
#define DEFAULTNAME			"DEFAULT"	// デフォルトハンドルネーム
#define	DEFAULTPORT			8000		// デフォルトのポート番号
#define	TIMERID				1000		// タイマー用ID

#define	STATE_INIT			0			// 状態：初期状態
#define	STATE_WAITCONNECT	1			// 状態：接続要求待ち
#define	STATE_CHATTING		2			// 状態：チャット中

#define NAMESIZE			32

//----------------------------------------------------------------------
// 送受信データ用構造体 
//----------------------------------------------------------------------
struct tagChatMsg
{
	int type;				// メッセージタイプ 0:チャット, 1:ログアウト, 2:参加 
	char msg[MSGBUFFSIZE];  // 送信文字列 
};

//----------------------------------------------------------------------
// グローバル変数
//----------------------------------------------------------------------
//ソケット
int sock = NULL;
//接続先のサーバのリスンソケットのアドレス 
struct sockaddr_in serverAddr;		
//ノンブロッキングモードにするとき使うやつ。 
unsigned long ul = 1;

struct tagChatMsg	recvMsg;			// 受信メッセージ
int	state;								// 状態
char dispMsg[DISPMSGSIZE];				// 表示メッセージ
char Name[NAMESIZE];					// クライアント名
char ipAddr[64];						// ipアドレス
int port;								// ポート番号

HWND	hPortEdit;
HWND	hIpAddrEdit;
HWND	hDispEdit;
HWND	hInputEdit;
HWND	hConnectBtn;
HWND	hShutdownBtn;
HWND	hSendBtn;
HWND    hNameBtn;

//----------------------------------------------------------------------
// プロトタイプ
//----------------------------------------------------------------------
BOOL CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);			// ダイアログプロシージャ
BOOL Connect();																// 接続要求
BOOL Recv();																// 受信処理
void InitDialog(HWND hDlg);													// 各コントロールのウィンドウハンドル記憶＆初期化
void CtrlDlgState();														// 各コントロールの状態設定
BOOL InitWinSock();															// WinSock初期化処理
BOOL ExitWinSock();															// WinSock解放処理
void SetServerAddr(u_short port, char* ipAddr);								// ユーザが入力したポート番号をもとにserverAddr設定
BOOL Connect();																// ソケットの作成～接続要求出し
void Close();																// 切断＆ソケット破棄
int	 Recv();																// 受信処理
int  Send(struct tagChatMsg sendMsg);										// 送信処理
void AddDispMsg(char* addMsg);												// 表示メッセージ追加
void ShowDispMsg();															// メッセージ表示
BOOL ShutDown(int sock);													// 切断			

//----------------------------------------------------------------------
// WinMain
//----------------------------------------------------------------------
INT WINAPI WinMain(HINSTANCE hCurInst, HINSTANCE hPrevInst, LPSTR lpsCmdLine, int nCmdShow)
{
	// ダイアログ表示
	DialogBox(hCurInst, MAKEINTRESOURCE(IDD_CLIENTDLG), NULL, (DLGPROC)DlgProc);
	return	0;
}

//----------------------------------------------------------------------
// DlgProc
//----------------------------------------------------------------------
BOOL CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
	case	WM_INITDIALOG:
		InitWinSock();					// WinSockの初期化
		InitDialog(hDlg);				// ダイアログ初期化
		state = STATE_INIT;
		CtrlDlgState();					// コントロールの状態設定
		AddDispMsg("初期化完了");

		return	TRUE;

	case	WM_TIMER:
		if (state == STATE_CHATTING)
		{
			if (Recv())
			{
				tagChatMsg		sendMsg;		// 送信メッセージ

				switch (ntohl(recvMsg.type))
				{
				case MSGTYPE_LOGIN:
					//サーバー側でaccept()OK
					//チャットにログインできた
					//ハンドルネームを送信
					//ハンドルネーム送信
					sendMsg.type = MSGTYPE_LOGIN;
					GetDlgItemText(hDlg, IDC_NAME, sendMsg.msg, sizeof(sendMsg.msg));
					Send(sendMsg);
					break;

				case MSGTYPE_CHAT: 
					//メッセージ表示欄にmsgセット
					AddDispMsg(recvMsg.msg);
					break;

				case MSGTYPE_LOGOUT:
					//サーバが終了orクライアント数越え
					//メッセージ表示欄にmsgセット
					AddDispMsg(recvMsg.msg);
					break;

				case MSGTYPE_ALLLOGOUT:
					//サーバが終了orクライアント数越え
					//メッセージ表示欄にmsgセット
					AddDispMsg(recvMsg.msg);
					//切断処理&ソケット破棄
					Close();
					break;

				default:
					break;
				}
				break;
			}
		}

		return	TRUE;

	case	WM_COMMAND:
		switch (LOWORD(wp))
		{
			// ×ボタン
		case	IDCANCEL:
			if (state == STATE_CHATTING)
			{
				tagChatMsg	sendMsg;					// 送信メッセージ
				sendMsg.type = MSGTYPE_LOGOUT;
				Send(sendMsg);							// 送信	...エラー処理省略
			}
			Close();									// 切断処理
			KillTimer(hDlg, TIMERID);					// タイマーストップ
			ExitWinSock();								// WinSock終了処理
			EndDialog(hDlg, IDCANCEL);					// ダイアログ閉じる

			return	TRUE;

			// [接続要求]ボタン
		case	IDC_CONNECTBTN:
			GetWindowText(hIpAddrEdit, ipAddr, sizeof(ipAddr));
			// 入力してきたポート番号取得
			port = GetDlgItemInt(hDlg, IDC_PORTEDIT, NULL, FALSE);
			// ソケット作成～接続要求
			if (Connect())
			{
				state = STATE_CHATTING;
				CtrlDlgState();							// コントロールの状態設定
				SetTimer(hDlg, TIMERID, 100, NULL);		// タイマー発動
				AddDispMsg("接続OK");			
			}
			else
			{
				AddDispMsg("Error:Connect()");
			}

			return	TRUE;

			// [切断]ボタン
		case	IDC_SHUTDOWNBTN:
			if (state == STATE_CHATTING) {
				tagChatMsg	sendMsg;					// 送信メッセージ
				sendMsg.type = MSGTYPE_LOGOUT;
				Send(sendMsg);							// 送信	...エラー処理省略
			}
			Close();									// 切断処理
			CtrlDlgState();								// コントロールの状態設定
			KillTimer(hDlg, TIMERID);					// タイマーストップ

			return	TRUE;

			// [送信]ボタン
		case	IDC_SENDBTN:
			if (state == STATE_CHATTING) {
				tagChatMsg	sendMsg;					// 送信メッセージ
				GetWindowText(hInputEdit, sendMsg.msg, sizeof(sendMsg.msg));
				SetWindowText(hInputEdit, "");
				sendMsg.type = MSGTYPE_CHAT;
				Send(sendMsg);

				return	TRUE;
			}
		}
		return	FALSE;
	}
	return	FALSE;
}

//----------------------------------------------------------------------
// InitWinSock
//		WinSockの初期化
//		<引数>
//			なし
//		<戻り値>
//			成功:TRUE
//			失敗:FALSE
//----------------------------------------------------------------------
BOOL	InitWinSock()
{
	WSADATA	wsaData;
	return	(WSAStartup(MAKEWORD(2, 2), &wsaData));
}

//----------------------------------------------------------------------
// ExitWinSock
//		WinSockの終了処理
//		<引数>
//			なし
//		<戻り値>
//			成功:TRUE
//			失敗:FALSE
//----------------------------------------------------------------------
BOOL	ExitWinSock()
{
	return	(WSACleanup());
}

//----------------------------------------------------------------------
// SetServerAddr
//		Ipアドレス，ポート番号からserverAddrセット
//		<引数>
//			port:ポート番号
//			ipAddr:IPアドレス
//		<戻り値>
//			なし
//----------------------------------------------------------------------
void	SetServerAddr(u_short port, char* ipAddr)
{
	memset(&serverAddr, 0, sizeof(serverAddr));			// 0クリア

	serverAddr.sin_family = AF_INET;					// TCP/IP
	serverAddr.sin_port = htons(port);					// ポート番号
	serverAddr.sin_addr.s_addr = inet_addr(ipAddr);		// IPアドレス
}

//------------------------------------------------------------ 
// Connect() 
//		ソケット作成～サーバへの接続要求 
//		<引数> 
//		なし 
//		<戻り値> 
//		成功：TRUE 
//		失敗：FALSE 
//------------------------------------------------------------ 
BOOL Connect() { 
	// ソケット作成
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		return FALSE;

	SetServerAddr(port, ipAddr);

	// ノンブロッキングモードへ
	ioctlsocket(sock, FIONBIO, &ul);

	// サーバに接続要求出す
	if ((connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) < 0)
	{
		// WSAEWOULDBLOCK以外のエラーなら 
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			// エラー処理 
			return FALSE;
	}
 
	// TRUE返す 
	return TRUE;
}

//------------------------------------------------------------ 
// Recv() 
//		サーバからのデータ受信 
//		<引数> 
//		なし 
//		<戻り値> 
//		成功：TRUE 
//		失敗：FALSE 
//------------------------------------------------------------ 
BOOL Recv( ) {
	//struct tagChatMsg recvMsg;	//受信データの格納領域

	memset(&recvMsg, 0, sizeof(recvMsg));			// 0クリア

	// recv() 
	if ((recv(sock, (char *)&recvMsg, sizeof(recvMsg), 0)) < 0)
	{
		// 成功ならTRUE， エラーならFALSE返す
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			return FALSE;
	}
	return TRUE;
}

//------------------------------------------------------------ 
// Send() 
//		サーバへのデータ送信 
//		<引数> 
//		sendMsg：送信データ 
//		<戻り値> 
//		成功：TRUE 
//		失敗：FALSE 
//------------------------------------------------------------ 
BOOL Send( struct tagChatMsg sendMsg ) { 
	//バイトオーダー変換
	sendMsg.type = htonl(sendMsg.type);

	// send() 
	// 成功ならTRUE， エラーならFALSE返す 
	return ((send(sock, (char *)&sendMsg, sizeof(sendMsg), 0)) == sizeof(sendMsg));
}

//----------------------------------------------------------------------
// Close
//		切断処理＆ソケット破棄
//		<引数>
//			なし
//		<戻り値>
//			なし
//----------------------------------------------------------------------
void	Close()
{
	// 接続中なら切断
	if (state == STATE_CHATTING)
		shutdown(sock, 0x02);

	// 通信中ソケット生成済みなら破棄
	if (sock != NULL)
	{
		closesocket(sock);
		sock = NULL;
	}

	// 状態
	state = STATE_INIT;
}


//----------------------------------------------------------------------
// InitDialog
//		ダイアログ上の各コントロールのウィンドウハンドル取得＆初期化
//		<引数>
//			なし
//		<戻り値>
//			なし
//----------------------------------------------------------------------
void InitDialog(HWND hDlg)
{
	// 状態
	state = STATE_INIT;

	// コントロールのウィンドウハンドル取得
	hPortEdit = GetDlgItem(hDlg, IDC_PORTEDIT);					// ポート番号入力欄
	hIpAddrEdit = GetDlgItem(hDlg, IDC_IP);						// IPアドレス入力欄
	hDispEdit = GetDlgItem(hDlg, IDC_DISPMSGEDIT);				// メッセージ表示欄
	hInputEdit = GetDlgItem(hDlg, IDC_INPUTMSGEDIT);			// 送信メッセージ入力欄

	hConnectBtn = GetDlgItem(hDlg, IDC_CONNECTBTN);				// 接続要求ボタン
	hShutdownBtn = GetDlgItem(hDlg, IDC_SHUTDOWNBTN);			// 切断ボタン
	hSendBtn = GetDlgItem(hDlg, IDC_SENDBTN);					// 送信ボタン
	hNameBtn = GetDlgItem(hDlg, IDC_NAME);

	SetDlgItemText(hDlg, IDC_NAME, DEFAULTNAME);
	// デフォルトポート番号セット
	SetDlgItemInt(hDlg, IDC_PORTEDIT, DEFAULTPORT, FALSE);
	//IP初期セット
	SetDlgItemTextA(hDlg, IDC_IP, "127.0.0.1");
}

//----------------------------------------------------------------------
// CtrlDlgState
//		各コントロールの状態設定
//		<引数>
//			なし
//		<戻り値>
//			なし
//----------------------------------------------------------------------
void CtrlDlgState()
{
	// Edit Control
	EnableWindow(hPortEdit, (state == STATE_INIT));				// ポート番号入力欄		：未接続時有効
	EnableWindow(hIpAddrEdit, (state == STATE_INIT));			// IPアドレス入力欄		：未接続時有効
	EnableWindow(hInputEdit, (state == STATE_CHATTING));		// 送信メッセージ入力欄	：接続時有効
	EnableWindow(hNameBtn, (state == STATE_INIT));

	// Button
	EnableWindow(hConnectBtn, (state == STATE_INIT));			// 接続要求ボタン		：未接続時有効
	EnableWindow(hShutdownBtn, (state == STATE_CHATTING));		// 切断ボタン			：接続時有効
	EnableWindow(hSendBtn, (state == STATE_CHATTING));			// 送信ボタン			：接続時有効
}

//----------------------------------------------------------------------
// AddDispMsg
//		表示メッセージ追加
//		<引数>
//			addMsg:追加するメッセージ
//		<戻り値>
//			なし
//----------------------------------------------------------------------
void AddDispMsg(char* addMsg)
{
	int newLen = strlen(dispMsg) + strlen(addMsg) + strlen(CRLF);

	//現在の文字の長さがDISPMSGSIZEを超えていれば超過分削除(古いほうから)
	if (newLen > DISPMSGSIZE)
	{
		strcpy(dispMsg, &dispMsg[newLen - DISPMSGSIZE]);
	}
	strcat(dispMsg, addMsg);
	strcat(dispMsg, CRLF);

	ShowDispMsg();
}

//----------------------------------------------------------------------
// ShowDispMsg
//		メッセージ表示
//		<引数>
//			なし
//		<戻り値>
//			なし
//----------------------------------------------------------------------
void ShowDispMsg()
{
	SetWindowText(hDispEdit, dispMsg);		// 表示文字列セット
	SendMessage(hDispEdit, EM_LINESCROLL, 0, Edit_GetLineCount(hDispEdit));		//スクロールバー更新
}


//----------------------------------------------------------------------
// Shutdown
//		クライアントとのコネクション切断
//		<引数>
//			切断するソケット
//		<戻り値>
//			成功:TRUE　
//			失敗:FALSE
//----------------------------------------------------------------------
BOOL Shutdown(int sock)
{
	//コネクション切断
	shutdown(sock, 0x02);
	//ソケット破棄
	closesocket(sock);

	return TRUE;
}


