//----------------------------------------------------------------------
// ex10 TCP GUI ���l�� �`���b�g
//		�N���C�A���g
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// #include
//----------------------------------------------------------------------
#define	WIN32_LEAN_AND_MEAN		// �d����`�h�~
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
#define MSGBUFFSIZE			256			// �`���b�g�̃��b�Z�[�W�̍ő咷 
#define MSGTYPE_CHAT		1			// ���b�Z�[�W�^�C�v�F�`���b�g 
#define MSGTYPE_LOGOUT		2			// ���b�Z�[�W�^�C�v�F���O�A�E�g 
#define MSGTYPE_ALLLOGOUT	3			// ���b�Z�[�W�^�C�v�F�S�����O�A�E�g 
#define MSGTYPE_LOGIN		4			// ���b�Z�[�W�^�C�v�F���O�C��

#define	SENDMSGSIZE			1024		// ���M������T�C�Y
#define	DISPMSGSIZE			1024*10		// �\��������T�C�Y
#define	CRLF				"\r\n"		// ���s�R�[�h
#define DEFAULTNAME			"DEFAULT"	// �f�t�H���g�n���h���l�[��
#define	DEFAULTPORT			8000		// �f�t�H���g�̃|�[�g�ԍ�
#define	TIMERID				1000		// �^�C�}�[�pID

#define	STATE_INIT			0			// ��ԁF�������
#define	STATE_WAITCONNECT	1			// ��ԁF�ڑ��v���҂�
#define	STATE_CHATTING		2			// ��ԁF�`���b�g��

#define NAMESIZE			32

//----------------------------------------------------------------------
// ����M�f�[�^�p�\���� 
//----------------------------------------------------------------------
struct tagChatMsg
{
	int type;				// ���b�Z�[�W�^�C�v 0:�`���b�g, 1:���O�A�E�g, 2:�Q�� 
	char msg[MSGBUFFSIZE];  // ���M������ 
};

//----------------------------------------------------------------------
// �O���[�o���ϐ�
//----------------------------------------------------------------------
//�\�P�b�g
int sock = NULL;
//�ڑ���̃T�[�o�̃��X���\�P�b�g�̃A�h���X 
struct sockaddr_in serverAddr;		
//�m���u���b�L���O���[�h�ɂ���Ƃ��g����B 
unsigned long ul = 1;

struct tagChatMsg	recvMsg;			// ��M���b�Z�[�W
int	state;								// ���
char dispMsg[DISPMSGSIZE];				// �\�����b�Z�[�W
char Name[NAMESIZE];					// �N���C�A���g��
char ipAddr[64];						// ip�A�h���X
int port;								// �|�[�g�ԍ�

HWND	hPortEdit;
HWND	hIpAddrEdit;
HWND	hDispEdit;
HWND	hInputEdit;
HWND	hConnectBtn;
HWND	hShutdownBtn;
HWND	hSendBtn;
HWND    hNameBtn;

//----------------------------------------------------------------------
// �v���g�^�C�v
//----------------------------------------------------------------------
BOOL CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);			// �_�C�A���O�v���V�[�W��
BOOL Connect();																// �ڑ��v��
BOOL Recv();																// ��M����
void InitDialog(HWND hDlg);													// �e�R���g���[���̃E�B���h�E�n���h���L����������
void CtrlDlgState();														// �e�R���g���[���̏�Ԑݒ�
BOOL InitWinSock();															// WinSock����������
BOOL ExitWinSock();															// WinSock�������
void SetServerAddr(u_short port, char* ipAddr);								// ���[�U�����͂����|�[�g�ԍ������Ƃ�serverAddr�ݒ�
BOOL Connect();																// �\�P�b�g�̍쐬�`�ڑ��v���o��
void Close();																// �ؒf���\�P�b�g�j��
int	 Recv();																// ��M����
int  Send(struct tagChatMsg sendMsg);										// ���M����
void AddDispMsg(char* addMsg);												// �\�����b�Z�[�W�ǉ�
void ShowDispMsg();															// ���b�Z�[�W�\��
BOOL ShutDown(int sock);													// �ؒf			

//----------------------------------------------------------------------
// WinMain
//----------------------------------------------------------------------
INT WINAPI WinMain(HINSTANCE hCurInst, HINSTANCE hPrevInst, LPSTR lpsCmdLine, int nCmdShow)
{
	// �_�C�A���O�\��
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
		InitWinSock();					// WinSock�̏�����
		InitDialog(hDlg);				// �_�C�A���O������
		state = STATE_INIT;
		CtrlDlgState();					// �R���g���[���̏�Ԑݒ�
		AddDispMsg("����������");

		return	TRUE;

	case	WM_TIMER:
		if (state == STATE_CHATTING)
		{
			if (Recv())
			{
				tagChatMsg		sendMsg;		// ���M���b�Z�[�W

				switch (ntohl(recvMsg.type))
				{
				case MSGTYPE_LOGIN:
					//�T�[�o�[����accept()OK
					//�`���b�g�Ƀ��O�C���ł���
					//�n���h���l�[���𑗐M
					//�n���h���l�[�����M
					sendMsg.type = MSGTYPE_LOGIN;
					GetDlgItemText(hDlg, IDC_NAME, sendMsg.msg, sizeof(sendMsg.msg));
					Send(sendMsg);
					break;

				case MSGTYPE_CHAT: 
					//���b�Z�[�W�\������msg�Z�b�g
					AddDispMsg(recvMsg.msg);
					break;

				case MSGTYPE_LOGOUT:
					//�T�[�o���I��or�N���C�A���g���z��
					//���b�Z�[�W�\������msg�Z�b�g
					AddDispMsg(recvMsg.msg);
					break;

				case MSGTYPE_ALLLOGOUT:
					//�T�[�o���I��or�N���C�A���g���z��
					//���b�Z�[�W�\������msg�Z�b�g
					AddDispMsg(recvMsg.msg);
					//�ؒf����&�\�P�b�g�j��
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
			// �~�{�^��
		case	IDCANCEL:
			if (state == STATE_CHATTING)
			{
				tagChatMsg	sendMsg;					// ���M���b�Z�[�W
				sendMsg.type = MSGTYPE_LOGOUT;
				Send(sendMsg);							// ���M	...�G���[�����ȗ�
			}
			Close();									// �ؒf����
			KillTimer(hDlg, TIMERID);					// �^�C�}�[�X�g�b�v
			ExitWinSock();								// WinSock�I������
			EndDialog(hDlg, IDCANCEL);					// �_�C�A���O����

			return	TRUE;

			// [�ڑ��v��]�{�^��
		case	IDC_CONNECTBTN:
			GetWindowText(hIpAddrEdit, ipAddr, sizeof(ipAddr));
			// ���͂��Ă����|�[�g�ԍ��擾
			port = GetDlgItemInt(hDlg, IDC_PORTEDIT, NULL, FALSE);
			// �\�P�b�g�쐬�`�ڑ��v��
			if (Connect())
			{
				state = STATE_CHATTING;
				CtrlDlgState();							// �R���g���[���̏�Ԑݒ�
				SetTimer(hDlg, TIMERID, 100, NULL);		// �^�C�}�[����
				AddDispMsg("�ڑ�OK");			
			}
			else
			{
				AddDispMsg("Error:Connect()");
			}

			return	TRUE;

			// [�ؒf]�{�^��
		case	IDC_SHUTDOWNBTN:
			if (state == STATE_CHATTING) {
				tagChatMsg	sendMsg;					// ���M���b�Z�[�W
				sendMsg.type = MSGTYPE_LOGOUT;
				Send(sendMsg);							// ���M	...�G���[�����ȗ�
			}
			Close();									// �ؒf����
			CtrlDlgState();								// �R���g���[���̏�Ԑݒ�
			KillTimer(hDlg, TIMERID);					// �^�C�}�[�X�g�b�v

			return	TRUE;

			// [���M]�{�^��
		case	IDC_SENDBTN:
			if (state == STATE_CHATTING) {
				tagChatMsg	sendMsg;					// ���M���b�Z�[�W
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
//		WinSock�̏�����
//		<����>
//			�Ȃ�
//		<�߂�l>
//			����:TRUE
//			���s:FALSE
//----------------------------------------------------------------------
BOOL	InitWinSock()
{
	WSADATA	wsaData;
	return	(WSAStartup(MAKEWORD(2, 2), &wsaData));
}

//----------------------------------------------------------------------
// ExitWinSock
//		WinSock�̏I������
//		<����>
//			�Ȃ�
//		<�߂�l>
//			����:TRUE
//			���s:FALSE
//----------------------------------------------------------------------
BOOL	ExitWinSock()
{
	return	(WSACleanup());
}

//----------------------------------------------------------------------
// SetServerAddr
//		Ip�A�h���X�C�|�[�g�ԍ�����serverAddr�Z�b�g
//		<����>
//			port:�|�[�g�ԍ�
//			ipAddr:IP�A�h���X
//		<�߂�l>
//			�Ȃ�
//----------------------------------------------------------------------
void	SetServerAddr(u_short port, char* ipAddr)
{
	memset(&serverAddr, 0, sizeof(serverAddr));			// 0�N���A

	serverAddr.sin_family = AF_INET;					// TCP/IP
	serverAddr.sin_port = htons(port);					// �|�[�g�ԍ�
	serverAddr.sin_addr.s_addr = inet_addr(ipAddr);		// IP�A�h���X
}

//------------------------------------------------------------ 
// Connect() 
//		�\�P�b�g�쐬�`�T�[�o�ւ̐ڑ��v�� 
//		<����> 
//		�Ȃ� 
//		<�߂�l> 
//		�����FTRUE 
//		���s�FFALSE 
//------------------------------------------------------------ 
BOOL Connect() { 
	// �\�P�b�g�쐬
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		return FALSE;

	SetServerAddr(port, ipAddr);

	// �m���u���b�L���O���[�h��
	ioctlsocket(sock, FIONBIO, &ul);

	// �T�[�o�ɐڑ��v���o��
	if ((connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) < 0)
	{
		// WSAEWOULDBLOCK�ȊO�̃G���[�Ȃ� 
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			// �G���[���� 
			return FALSE;
	}
 
	// TRUE�Ԃ� 
	return TRUE;
}

//------------------------------------------------------------ 
// Recv() 
//		�T�[�o����̃f�[�^��M 
//		<����> 
//		�Ȃ� 
//		<�߂�l> 
//		�����FTRUE 
//		���s�FFALSE 
//------------------------------------------------------------ 
BOOL Recv( ) {
	//struct tagChatMsg recvMsg;	//��M�f�[�^�̊i�[�̈�

	memset(&recvMsg, 0, sizeof(recvMsg));			// 0�N���A

	// recv() 
	if ((recv(sock, (char *)&recvMsg, sizeof(recvMsg), 0)) < 0)
	{
		// �����Ȃ�TRUE�C �G���[�Ȃ�FALSE�Ԃ�
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			return FALSE;
	}
	return TRUE;
}

//------------------------------------------------------------ 
// Send() 
//		�T�[�o�ւ̃f�[�^���M 
//		<����> 
//		sendMsg�F���M�f�[�^ 
//		<�߂�l> 
//		�����FTRUE 
//		���s�FFALSE 
//------------------------------------------------------------ 
BOOL Send( struct tagChatMsg sendMsg ) { 
	//�o�C�g�I�[�_�[�ϊ�
	sendMsg.type = htonl(sendMsg.type);

	// send() 
	// �����Ȃ�TRUE�C �G���[�Ȃ�FALSE�Ԃ� 
	return ((send(sock, (char *)&sendMsg, sizeof(sendMsg), 0)) == sizeof(sendMsg));
}

//----------------------------------------------------------------------
// Close
//		�ؒf�������\�P�b�g�j��
//		<����>
//			�Ȃ�
//		<�߂�l>
//			�Ȃ�
//----------------------------------------------------------------------
void	Close()
{
	// �ڑ����Ȃ�ؒf
	if (state == STATE_CHATTING)
		shutdown(sock, 0x02);

	// �ʐM���\�P�b�g�����ς݂Ȃ�j��
	if (sock != NULL)
	{
		closesocket(sock);
		sock = NULL;
	}

	// ���
	state = STATE_INIT;
}


//----------------------------------------------------------------------
// InitDialog
//		�_�C�A���O��̊e�R���g���[���̃E�B���h�E�n���h���擾��������
//		<����>
//			�Ȃ�
//		<�߂�l>
//			�Ȃ�
//----------------------------------------------------------------------
void InitDialog(HWND hDlg)
{
	// ���
	state = STATE_INIT;

	// �R���g���[���̃E�B���h�E�n���h���擾
	hPortEdit = GetDlgItem(hDlg, IDC_PORTEDIT);					// �|�[�g�ԍ����͗�
	hIpAddrEdit = GetDlgItem(hDlg, IDC_IP);						// IP�A�h���X���͗�
	hDispEdit = GetDlgItem(hDlg, IDC_DISPMSGEDIT);				// ���b�Z�[�W�\����
	hInputEdit = GetDlgItem(hDlg, IDC_INPUTMSGEDIT);			// ���M���b�Z�[�W���͗�

	hConnectBtn = GetDlgItem(hDlg, IDC_CONNECTBTN);				// �ڑ��v���{�^��
	hShutdownBtn = GetDlgItem(hDlg, IDC_SHUTDOWNBTN);			// �ؒf�{�^��
	hSendBtn = GetDlgItem(hDlg, IDC_SENDBTN);					// ���M�{�^��
	hNameBtn = GetDlgItem(hDlg, IDC_NAME);

	SetDlgItemText(hDlg, IDC_NAME, DEFAULTNAME);
	// �f�t�H���g�|�[�g�ԍ��Z�b�g
	SetDlgItemInt(hDlg, IDC_PORTEDIT, DEFAULTPORT, FALSE);
	//IP�����Z�b�g
	SetDlgItemTextA(hDlg, IDC_IP, "127.0.0.1");
}

//----------------------------------------------------------------------
// CtrlDlgState
//		�e�R���g���[���̏�Ԑݒ�
//		<����>
//			�Ȃ�
//		<�߂�l>
//			�Ȃ�
//----------------------------------------------------------------------
void CtrlDlgState()
{
	// Edit Control
	EnableWindow(hPortEdit, (state == STATE_INIT));				// �|�[�g�ԍ����͗�		�F���ڑ����L��
	EnableWindow(hIpAddrEdit, (state == STATE_INIT));			// IP�A�h���X���͗�		�F���ڑ����L��
	EnableWindow(hInputEdit, (state == STATE_CHATTING));		// ���M���b�Z�[�W���͗�	�F�ڑ����L��
	EnableWindow(hNameBtn, (state == STATE_INIT));

	// Button
	EnableWindow(hConnectBtn, (state == STATE_INIT));			// �ڑ��v���{�^��		�F���ڑ����L��
	EnableWindow(hShutdownBtn, (state == STATE_CHATTING));		// �ؒf�{�^��			�F�ڑ����L��
	EnableWindow(hSendBtn, (state == STATE_CHATTING));			// ���M�{�^��			�F�ڑ����L��
}

//----------------------------------------------------------------------
// AddDispMsg
//		�\�����b�Z�[�W�ǉ�
//		<����>
//			addMsg:�ǉ����郁�b�Z�[�W
//		<�߂�l>
//			�Ȃ�
//----------------------------------------------------------------------
void AddDispMsg(char* addMsg)
{
	int newLen = strlen(dispMsg) + strlen(addMsg) + strlen(CRLF);

	//���݂̕����̒�����DISPMSGSIZE�𒴂��Ă���Β��ߕ��폜(�Â��ق�����)
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
//		���b�Z�[�W�\��
//		<����>
//			�Ȃ�
//		<�߂�l>
//			�Ȃ�
//----------------------------------------------------------------------
void ShowDispMsg()
{
	SetWindowText(hDispEdit, dispMsg);		// �\��������Z�b�g
	SendMessage(hDispEdit, EM_LINESCROLL, 0, Edit_GetLineCount(hDispEdit));		//�X�N���[���o�[�X�V
}


//----------------------------------------------------------------------
// Shutdown
//		�N���C�A���g�Ƃ̃R�l�N�V�����ؒf
//		<����>
//			�ؒf����\�P�b�g
//		<�߂�l>
//			����:TRUE�@
//			���s:FALSE
//----------------------------------------------------------------------
BOOL Shutdown(int sock)
{
	//�R�l�N�V�����ؒf
	shutdown(sock, 0x02);
	//�\�P�b�g�j��
	closesocket(sock);

	return TRUE;
}


