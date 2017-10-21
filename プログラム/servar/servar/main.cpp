//----------------------------------------------------------------------
// ex10 TCP GUI 多人数 チャット
//		サーバ
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// #include
//----------------------------------------------------------------------
#define	WIN32_LEAN_AND_MEAN			// 重複定義防止
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
#define MAXCLIENT			2		// 最大クライアント数：10 
#define MSGBUFFSIZE			256		// チャットのメッセージの最大長 
#define MSGTYPE_CHAT		1		// メッセージタイプ：チャット 
#define MSGTYPE_LOGOUT		2		// メッセージタイプ：ログアウト 
#define MSGTYPE_ALLLOGOUT	3		// メッセージタイプ：全員ログアウト 
#define MSGTYPE_LOGIN		4		// メッセージタイプ：ログイン
	
#define ACCEPT_ERROR		0		// accept()エラー 
#define ACCEPT_OK			1		// 成功 
#define ACCEPT_OVER			2		// 接続数オーバー
#define ACCEPT_NOTYET		3		// 接続要求がまだきてない

#define	TIMERID				1000	// タイマー用ID
#define	DEFAULTPORT			8000	// デフォルトのポート番号
#define	SENDMSGSIZE			1024	// 送信文字列サイズ
#define	DISPMSGSIZE			1024*10	// 表示文字列サイズ
#define	CRLF				"\r\n"	// 改行コード

#define	STATE_INIT			0		// 状態：初期状態
#define	STATE_WAITCONNECT	1		// 状態：接続要求待ち
#define	STATE_CHATTING		2		// 状態：チャット中

//----------------------------------------------------------------------
//構造体定義
//----------------------------------------------------------------------
// クライアント管理用構造体 
struct tagClientInfo { 
	int sock;						// ソケット 
	char name[16];					// ハンドルネーム 
	struct sockaddr_in addr;		// クライアントのソケットアドレス 
	BOOL enabled;					// 有効フラグ 
}; 

// 送受信データ用構造体 
struct tagChatMsg 
{ 
	int type;						// メッセージタイプ 0:チャット, 1:ログアウト, 2:参加 
	char msg[MSGBUFFSIZE];			// 送信文字列 
};


//----------------------------------------------------------------------
//グロ－バル変数
//----------------------------------------------------------------------
struct tagClientInfo clients[MAXCLIENT];	//つないできたクライアントを格納する配列
struct tagChatMsg recvMsg;

int listenSock = NULL;						//リスンソケット
struct sockaddr_in bindAddr;				//バインド用アドレス
int sockaddrLen = sizeof(bindAddr);			// アドレスのサイズ
unsigned long ul = 1;						//ノンブロッキングにするとき使うやつ
//bool shutdownClientArray[MAXCLIENT];		
char dispMsg[DISPMSGSIZE];					// 表示メッセージ
int	state;									// 状態
int recvIndex = 0;							//どのクライアントが送信してきたか
int port;									//ポート番号
int notAcceptSock;							//接続に失敗したクライアントのソケット

HWND	hPortEdit;
HWND	hDispEdit;
HWND	hInputEdit;
HWND	hListenBtn;
HWND	hShutdownBtn;
HWND	hSendBtn;
HWND	hClientListBox;						


//----------------------------------------------------------------------
//プロトタイプ
//----------------------------------------------------------------------
BOOL CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);			// ダイアログプロシージャ
BOOL InitWinSock();									// winsock(windowsがソケットで通信をするための規約をまとめたもの)												// WinSock初期化処理
BOOL ExitWinSock();									// WinSock解放処理
void InitDialog(HWND hDlg);							// 各コントロールのウィンドウハンドル記憶＆初期化
int  Accept();										// 接続要求受付
BOOL SendToAll(struct tagChatMsg sendMsg);			// 全員に送信
BOOL Send(int sock, struct tagChatMsg sendMsg);		// 特定の相手に送信
BOOL Listen();										// リスンソケット作成～リスン状態へ
BOOL Shutdown(int sock);							// 特定のクライアントとの通信を切る
BOOL ShutdownAll();									// 全てのクライアントとの通信を切る
BOOL Recv();										// 受信処理
void AddDispMsg(char* addMsg);						// 表示メッセージ追加
void ShowDispMsg();									// メッセージ表示
void UpdateListBox();								// リストボックス更新
void AddListBox(struct tagClientInfo client);		// リストボックスに追加
void CtrlDlgState();								// 各コントロールの状態設定
void SetBindAddr(u_short port);						// ユーザが入力したポート番号をもとにbindAddr設定
void Close();										// 切断＆ソケット破棄

//----------------------------------------------------------------------
// WinMain
//----------------------------------------------------------------------
INT WINAPI WinMain(HINSTANCE hCurInst, HINSTANCE hPrevInst, LPSTR lpsCmdLine, int nCmdShow)
{
	// ダイアログ表示
	DialogBox(hCurInst, MAKEINTRESOURCE(IDD_SERVERDLG), NULL, (DLGPROC)DlgProc);
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
		CtrlDlgState();					// コントロールの状態設定
	
		for (int i = 0; i < MAXCLIENT; i++)
		{
			clients[i].enabled = FALSE;
		}
		AddDispMsg("初期化完了");

		return	TRUE;

	case	WM_TIMER:
		switch (Accept())
		{
			tagChatMsg sendMsg;		// 送信メッセージ

		case ACCEPT_OK:
			sendMsg.type = MSGTYPE_LOGIN;
			wsprintf(sendMsg.msg, "接続します");
			Send(clients[recvIndex].sock, sendMsg);
			return TRUE;
		case ACCEPT_ERROR:
			return TRUE;
		case ACCEPT_OVER:
			sendMsg.type = MSGTYPE_LOGOUT;
			wsprintf(sendMsg.msg, "定員オーバーです");
			Send(notAcceptSock, sendMsg);
			//切断&ソケット破棄
			Shutdown(notAcceptSock);
			return TRUE;
		case ACCEPT_NOTYET:
			break;
		}

		//受信
		if (state == STATE_CHATTING)
		{
			if (Recv())
			{
				tagChatMsg sendMsg;		// 送信メッセージ

				// 受信
				switch (ntohl(recvMsg.type))
				{
				case MSGTYPE_LOGIN:		//ハンドルネームを送ってきた
					//clients[].nameにrecvMsg.msgをコピー
					strcpy(clients[recvIndex].name, recvMsg.msg);
					//全クライアントに「＿＿さんが入室しました」メッセージ送信
					wsprintf(sendMsg.msg, "%sさんが入室しました", recvMsg.msg);
					AddDispMsg(sendMsg.msg);
					sendMsg.type = MSGTYPE_CHAT;
					SendToAll(sendMsg);

					//クライアントリストの更新
					UpdateListBox();
					break;

				case MSGTYPE_LOGOUT:		//クライアントが[退室]か[x]ボタン押した
					//全クライアントに「＿＿さんが退室しました」メッセージ送信
					wsprintf(sendMsg.msg, "%sさんが退室しました", clients[recvIndex].name);
					AddDispMsg(sendMsg.msg);
					sendMsg.type = MSGTYPE_LOGOUT;
					SendToAll(sendMsg);

					//対象クライアントとのコネクション切断＆ソケット破棄
					Shutdown(clients[recvIndex].sock);

					//クライアント配列から対象クライアント削除（有効フラグOFFに）
					clients[recvIndex].enabled = FALSE;

					//クライアントリストの更新
					UpdateListBox();
					break;

				case MSGTYPE_CHAT:		//チャットのメッセージ
					//メッセージ整形「＿＿さんの発言：＿＿＿＿」	
					wsprintf(sendMsg.msg, "%sさんの発言：%s", clients[recvIndex].name, recvMsg.msg);
					AddDispMsg(sendMsg.msg);

					//全クライアントにメッセージ送信
					sendMsg.type = MSGTYPE_CHAT;
					SendToAll(sendMsg);
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
				tagChatMsg sendMsg;		// 送信メッセージ
				// 相手に切断用メッセージ生成	
				sendMsg.type = MSGTYPE_ALLLOGOUT;
				wsprintf(sendMsg.msg, "サーバを終了します");
				SendToAll(sendMsg);						// 送信	...エラー処理省略
				Close();								//リスンソケット破棄
				CtrlDlgState();							// コントロールの状態設定
				KillTimer(hDlg, TIMERID);				// タイマーストップ
				ShutdownAll();							// 切断処理	
			}

			ExitWinSock();								// WinSock終了処理
			EndDialog(hDlg, IDCANCEL);					// ダイアログ閉じる
			return	TRUE;

			// [接続受付]ボタン
		case	IDC_LISTENBTN:
			// 入力してきたポート番号取得
			port = GetDlgItemInt(hDlg, IDC_PORTEDIT, NULL, FALSE);
			// リスンソケット作成，バインド，リスン状態へ
			if (Listen())
			{
				state = STATE_CHATTING;
				CtrlDlgState();							// コントロールの状態設定
				SetTimer(hDlg, TIMERID, 100, NULL);		// タイマー発動
				AddDispMsg("接続要求待ち");
			}
			else
			{
				AddDispMsg("Error:Listen()");
			}
			return	TRUE;

			// [切断]ボタン
		case	IDC_SHUTDOWNBTN:
			if (state == STATE_CHATTING)
			{
				tagChatMsg sendMsg;		// 送信メッセージ
				// 相手に切断用メッセージ生成
				sendMsg.type = MSGTYPE_ALLLOGOUT;
				wsprintf(sendMsg.msg, "サーバを終了します");
				SendToAll(sendMsg);							// 送信	...エラー処理省略
				Close();									//リスンソケット破棄
				CtrlDlgState();								// コントロールの状態設定
				KillTimer(hDlg, TIMERID);					// タイマーストップ
				ShutdownAll();								// 切断処理
				return	TRUE;
			}

			// [送信]ボタン
		case	IDC_SENDBTN:
			if (state == STATE_CHATTING) {
				tagChatMsg sendMsg;		// 送信メッセージ
				//文字列取得
				GetWindowText(hInputEdit, sendMsg.msg, sizeof(sendMsg.msg));
				sendMsg.type = MSGTYPE_CHAT;
				//全クライアントに送信
				if (SendToAll(sendMsg))
				{
					//送信成功
					AddDispMsg(sendMsg.msg);
					SetWindowText(hInputEdit, "");
				}
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
// SetBindAddr
//		ポート番号からbindAddrセット
//		<引数>
//			port:ポート番号
//		<戻り値>
//			なし
//----------------------------------------------------------------------
void	SetBindAddr(u_short port)
{
	memset(&bindAddr, 0, sizeof(bindAddr));			// 0クリア

	bindAddr.sin_family = AF_INET;					// TCP/IP
	bindAddr.sin_port = htons(port);				// ポート番号
	bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);	// IPアドレス(どのアドレスからの接続でも受け入れる)
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
	strcat_s(dispMsg, addMsg);	
	strcat_s(dispMsg, CRLF);

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
	SendMessage(hDispEdit, EM_LINESCROLL, 0, Edit_GetLineCount(hDispEdit));
}

//------------------------------------------------------------ 
// Accept() 
//		クライアントからの接続要求受付 
//		 <引数> 
//		 なし 
//		 <戻値>
//		 成功 ：ACCEPT_OK (1) 
//		 エラー ：ACCEPT_ERROR(0) 
//		 接続数オーバー ：ACCEPT_OVER(2)
//		 接続要求なし ：ACCEPT_NOTYET(3)
//------------------------------------------------------------ 
int Accept() 
{ 
	int sock;

	// accept()する 
	sock = accept(listenSock, (struct sockaddr *)&bindAddr, &sockaddrLen);
	if (sock < 0)
	{
		if (WSAGetLastError() == WSAEWOULDBLOCK)
			// 接続要求がなければ ACCEPT_NOTYET返す 
			return ACCEPT_NOTYET;
		else	
			// エラーならACCEPT_ERROR返す 
			return ACCEPT_ERROR;
	}
	
	// 接続要求があったらclients[]からループして未接続の要素探す 
	for (int i = 0; i < MAXCLIENT; i++)
	{
		// clients[i].enabledがfalseのやつ見つけて 
		if (!clients[i].enabled)
		{
			// accept()した結果セット 
			clients[i].sock = sock;
			//ノンブロッキング設定
			ioctlsocket(clients[i].sock, FIONBIO, &ul);
			clients[i].enabled = TRUE;
			memcpy(&clients[i].addr, &bindAddr, sizeof(bindAddr));
			recvIndex = i;
			// ACCEPT_OK返す
			return ACCEPT_OK;
		}		
	}

	//接続数オーバー
	notAcceptSock = sock;
	return ACCEPT_OVER; 
}

//------------------------------------------------------------ 
// Send() 
//		クライアントへのデータ送信 
//		<引数> 
//		sendMsg：送信データ 
//		<戻り値> 
//		成功：TRUE 
//		失敗：FALSE 
//------------------------------------------------------------ 
BOOL Send(int sock, tagChatMsg sendMsg)
{
	//バイトオーダー変換
	sendMsg.type = htonl(sendMsg.type);

	//成功ならTRUE,エラーならFALSEを返す
	return 	(send(sock, (char *)&sendMsg/*sendはcharでキャストすれば送れる*/,
				sizeof(sendMsg), 0) == sizeof(sendMsg));
}

//------------------------------------------------------------ 
// SendToAll() 
//		全てのクライアントへのデータ送信 
//		<引数> 
//		sendMsg：送信データ 
//		<戻り値> 
//		成功：TRUE 
//		失敗：FALSE 
//------------------------------------------------------------ 
BOOL SendToAll(struct tagChatMsg sendMsg)
{
	BOOL allSuccess = TRUE; // 成功フラグ 
	// クライアント管理用配列ループ 
	for (int i = 0; i < MAXCLIENT; i++)
	{
		// enabledがTRUEなら
		if (clients[i].enabled)
		{
			// 送信 （特定の相手に送信構造体使用)
			if (!Send(clients[i].sock, sendMsg))
			{
				// エラーならallSuccessをFALSEに 
				allSuccess = FALSE;
			}
		}
	}
	// 1クライアントでも送信エラーあり FALSE， 全クライアントに送信OK TRUE 返す 
	return allSuccess;
}

//------------------------------------------------------------ 
// Listen() 
//		リスンソケットの作成～リスン状態 
//		<引数> 
//		なし 
//		<戻り値> 
//		成功：TRUE 
//		FALSE：TRUE 
//------------------------------------------------------------
BOOL Listen()
{
	//リスンソケット作成
	if ((listenSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	{
		return FALSE;
	}

	//ノンブロッキングモードへ
	ioctlsocket(listenSock, FIONBIO, &ul);

	// 入力してきたポート番号取得しつつbindAddr設定
	SetBindAddr(port);

	//バインド
	if (bind(listenSock, (struct sockaddr *)&bindAddr, sizeof(bindAddr)) < 0)
		return FALSE;

	//リスンへ (2番目の引数がキュー（待ち行列）のサイズ(最大接続数だけ席を確保))
	if (listen(listenSock, MAXCLIENT) < 0)
		return FALSE;

	return TRUE;
}

//------------------------------------------------------------ 
// Recv() 
//		受信処理 
//		<引数> 
//		なし 
//		<戻り値> 
//		成功：TRUE 
//		FALSE：TRUE 
//------------------------------------------------------------
BOOL Recv()
{
	//クライアント配列分ループ
	for (int i = 0; i < MAXCLIENT; i++)
	{
		//接続中なら
		if (clients[i].enabled)
		{
			memset(&recvMsg, 0, sizeof(recvMsg));	// 0クリア

			if ((recv(clients[i].sock, (char *)&recvMsg, sizeof(recvMsg), 0)) < 0)
			{
				// 未受信以外ならエラー 
				if( WSAGetLastError() != WSAEWOULDBLOCK )
				{ 
					//allSuccess = FALSE; 
					return FALSE;
				}
			}
			//受信に成功したら
			else
			{
				recvIndex = i;
				return TRUE;
			}
		}
	}

	return FALSE;
}

//------------------------------------------------------------ 
// Shutdown() 
//		クライアントとのコネクション切断 
//		 <引数> 
//		 sock：切断するソケット 
//		 <戻り値>
//		 成功：TRUE 
//		 失敗：FALSE 
//------------------------------------------------------------
BOOL Shutdown(int sock) { 
	// 切断 
	shutdown( sock, 0x02 ); 

	// ソケットの破棄 
	closesocket( sock ); 

	return FALSE; 
}

//------------------------------------------------------------ 
// ShutdownAll() 
//		接続中の全クライアントとのコネクション切断 
//		<引数> 
//		なし 
//		<戻り値> 
//		成功：TRUE 
//		失敗：FALSE 
//------------------------------------------------------------
BOOL ShutdownAll()
{
	//全クライアント分ループ
	for (int i = 0; i < MAXCLIENT; i++)
	{
		if (clients[i].enabled)
		{
			//切断&ソケット破棄
			Shutdown(clients[i].sock);

			//有効フラグをFALSEに
			clients[i].enabled = FALSE;
		}
	}
	return TRUE;
}	

//----------------------------------------------------------------------
// Close()
//		ソケット破棄
//		<引数> 
//		なし 
//		<戻り値> 
//		なし
//----------------------------------------------------y------------------
void	Close()
{
	// リスンソケット生成済みなら破棄
	if (listenSock != NULL)
	{
		closesocket(listenSock);
		listenSock = NULL;
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
	hDispEdit = GetDlgItem(hDlg, IDC_DISPMSGEDIT);				// メッセージ表示欄
	hInputEdit = GetDlgItem(hDlg, IDC_INPUTMSGEDIT);			// 送信メッセージ入力欄

	hListenBtn = GetDlgItem(hDlg, IDC_LISTENBTN);				// 接続受付ボタン
	hShutdownBtn = GetDlgItem(hDlg, IDC_SHUTDOWNBTN);			// 切断ボタン
	hSendBtn = GetDlgItem(hDlg, IDC_SENDBTN);					// 送信ボタン
	hClientListBox = GetDlgItem(hDlg, IDC_LISTBOX);

	// デフォルトポート番号セット
	SetDlgItemInt(hDlg, IDC_PORTEDIT, DEFAULTPORT, FALSE);
}

//----------------------------------------------------------------------
// UpdateListBox
//		リストボックス更新
//		<引数>
//			なし
//		<戻り値>
//			なし
//----------------------------------------------------------------------
void UpdateListBox()
{
	//AddListBoxのLB_ADDSTRINGは上書きではなく追加なので，このままいくとどんどん一覧が増えていってしまう。
	//なので，リストボックスをリセット(全消去)してから，AddListBox()を呼び出す。
	//リストボックスリセット
	SendMessage(hClientListBox, LB_RESETCONTENT, NULL, NULL);

	//接続クライアントをリストボックスに追加
	for (int i = 0; i < MAXCLIENT; i++)
	{
		// enabledがTRUEなら
		if (clients[i].enabled)
		{
			AddListBox(clients[i]);
		}
	}
}

//----------------------------------------------------------------------
// CtrlDlgState
//		各コントロールの状態設定
//		<引数>
//			client:クライアント情報
//		<戻り値>
//			なし
//----------------------------------------------------------------------
void AddListBox(struct tagClientInfo client)
{
	char	str[MSGBUFFSIZE];	

	//リストボックスに追加する文字列の生成 IPアドレス:ハンドルネーム
	wsprintf(str, "%s:%s", inet_ntoa(client.addr.sin_addr), client.name);

	//リストボックスに追加
	SendMessage(hClientListBox, LB_ADDSTRING, NULL, (LPARAM)str);
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
	EnableWindow(hInputEdit, (state == STATE_CHATTING));		// 送信メッセージ入力欄	：接続時有効

	// Button
	EnableWindow(hListenBtn, (state == STATE_INIT));			// 接続受付ボタン		：未接続時有効
	EnableWindow(hShutdownBtn, (state == STATE_CHATTING));		// 切断ボタン			：接続時有効
	EnableWindow(hSendBtn, (state == STATE_CHATTING));			// 送信ボタン			：接続時有効
}

