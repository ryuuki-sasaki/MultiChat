//----------------------------------------------------------------------
// ex10 TCP GUI ���l�� �`���b�g
//		�T�[�o
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// #include
//----------------------------------------------------------------------
#define	WIN32_LEAN_AND_MEAN			// �d����`�h�~
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
#define MAXCLIENT			2		// �ő�N���C�A���g���F10 
#define MSGBUFFSIZE			256		// �`���b�g�̃��b�Z�[�W�̍ő咷 
#define MSGTYPE_CHAT		1		// ���b�Z�[�W�^�C�v�F�`���b�g 
#define MSGTYPE_LOGOUT		2		// ���b�Z�[�W�^�C�v�F���O�A�E�g 
#define MSGTYPE_ALLLOGOUT	3		// ���b�Z�[�W�^�C�v�F�S�����O�A�E�g 
#define MSGTYPE_LOGIN		4		// ���b�Z�[�W�^�C�v�F���O�C��
	
#define ACCEPT_ERROR		0		// accept()�G���[ 
#define ACCEPT_OK			1		// ���� 
#define ACCEPT_OVER			2		// �ڑ����I�[�o�[
#define ACCEPT_NOTYET		3		// �ڑ��v�����܂����ĂȂ�

#define	TIMERID				1000	// �^�C�}�[�pID
#define	DEFAULTPORT			8000	// �f�t�H���g�̃|�[�g�ԍ�
#define	SENDMSGSIZE			1024	// ���M������T�C�Y
#define	DISPMSGSIZE			1024*10	// �\��������T�C�Y
#define	CRLF				"\r\n"	// ���s�R�[�h

#define	STATE_INIT			0		// ��ԁF�������
#define	STATE_WAITCONNECT	1		// ��ԁF�ڑ��v���҂�
#define	STATE_CHATTING		2		// ��ԁF�`���b�g��

//----------------------------------------------------------------------
//�\���̒�`
//----------------------------------------------------------------------
// �N���C�A���g�Ǘ��p�\���� 
struct tagClientInfo { 
	int sock;						// �\�P�b�g 
	char name[16];					// �n���h���l�[�� 
	struct sockaddr_in addr;		// �N���C�A���g�̃\�P�b�g�A�h���X 
	BOOL enabled;					// �L���t���O 
}; 

// ����M�f�[�^�p�\���� 
struct tagChatMsg 
{ 
	int type;						// ���b�Z�[�W�^�C�v 0:�`���b�g, 1:���O�A�E�g, 2:�Q�� 
	char msg[MSGBUFFSIZE];			// ���M������ 
};


//----------------------------------------------------------------------
//�O���|�o���ϐ�
//----------------------------------------------------------------------
struct tagClientInfo clients[MAXCLIENT];	//�Ȃ��ł����N���C�A���g���i�[����z��
struct tagChatMsg recvMsg;

int listenSock = NULL;						//���X���\�P�b�g
struct sockaddr_in bindAddr;				//�o�C���h�p�A�h���X
int sockaddrLen = sizeof(bindAddr);			// �A�h���X�̃T�C�Y
unsigned long ul = 1;						//�m���u���b�L���O�ɂ���Ƃ��g�����
//bool shutdownClientArray[MAXCLIENT];		
char dispMsg[DISPMSGSIZE];					// �\�����b�Z�[�W
int	state;									// ���
int recvIndex = 0;							//�ǂ̃N���C�A���g�����M���Ă�����
int port;									//�|�[�g�ԍ�
int notAcceptSock;							//�ڑ��Ɏ��s�����N���C�A���g�̃\�P�b�g

HWND	hPortEdit;
HWND	hDispEdit;
HWND	hInputEdit;
HWND	hListenBtn;
HWND	hShutdownBtn;
HWND	hSendBtn;
HWND	hClientListBox;						


//----------------------------------------------------------------------
//�v���g�^�C�v
//----------------------------------------------------------------------
BOOL CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);			// �_�C�A���O�v���V�[�W��
BOOL InitWinSock();									// winsock(windows���\�P�b�g�ŒʐM�����邽�߂̋K����܂Ƃ߂�����)												// WinSock����������
BOOL ExitWinSock();									// WinSock�������
void InitDialog(HWND hDlg);							// �e�R���g���[���̃E�B���h�E�n���h���L����������
int  Accept();										// �ڑ��v����t
BOOL SendToAll(struct tagChatMsg sendMsg);			// �S���ɑ��M
BOOL Send(int sock, struct tagChatMsg sendMsg);		// ����̑���ɑ��M
BOOL Listen();										// ���X���\�P�b�g�쐬�`���X����Ԃ�
BOOL Shutdown(int sock);							// ����̃N���C�A���g�Ƃ̒ʐM��؂�
BOOL ShutdownAll();									// �S�ẴN���C�A���g�Ƃ̒ʐM��؂�
BOOL Recv();										// ��M����
void AddDispMsg(char* addMsg);						// �\�����b�Z�[�W�ǉ�
void ShowDispMsg();									// ���b�Z�[�W�\��
void UpdateListBox();								// ���X�g�{�b�N�X�X�V
void AddListBox(struct tagClientInfo client);		// ���X�g�{�b�N�X�ɒǉ�
void CtrlDlgState();								// �e�R���g���[���̏�Ԑݒ�
void SetBindAddr(u_short port);						// ���[�U�����͂����|�[�g�ԍ������Ƃ�bindAddr�ݒ�
void Close();										// �ؒf���\�P�b�g�j��

//----------------------------------------------------------------------
// WinMain
//----------------------------------------------------------------------
INT WINAPI WinMain(HINSTANCE hCurInst, HINSTANCE hPrevInst, LPSTR lpsCmdLine, int nCmdShow)
{
	// �_�C�A���O�\��
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
		InitWinSock();					// WinSock�̏�����
		InitDialog(hDlg);				// �_�C�A���O������
		CtrlDlgState();					// �R���g���[���̏�Ԑݒ�
	
		for (int i = 0; i < MAXCLIENT; i++)
		{
			clients[i].enabled = FALSE;
		}
		AddDispMsg("����������");

		return	TRUE;

	case	WM_TIMER:
		switch (Accept())
		{
			tagChatMsg sendMsg;		// ���M���b�Z�[�W

		case ACCEPT_OK:
			sendMsg.type = MSGTYPE_LOGIN;
			wsprintf(sendMsg.msg, "�ڑ����܂�");
			Send(clients[recvIndex].sock, sendMsg);
			return TRUE;
		case ACCEPT_ERROR:
			return TRUE;
		case ACCEPT_OVER:
			sendMsg.type = MSGTYPE_LOGOUT;
			wsprintf(sendMsg.msg, "����I�[�o�[�ł�");
			Send(notAcceptSock, sendMsg);
			//�ؒf&�\�P�b�g�j��
			Shutdown(notAcceptSock);
			return TRUE;
		case ACCEPT_NOTYET:
			break;
		}

		//��M
		if (state == STATE_CHATTING)
		{
			if (Recv())
			{
				tagChatMsg sendMsg;		// ���M���b�Z�[�W

				// ��M
				switch (ntohl(recvMsg.type))
				{
				case MSGTYPE_LOGIN:		//�n���h���l�[���𑗂��Ă���
					//clients[].name��recvMsg.msg���R�s�[
					strcpy(clients[recvIndex].name, recvMsg.msg);
					//�S�N���C�A���g�Ɂu�Q�Q���񂪓������܂����v���b�Z�[�W���M
					wsprintf(sendMsg.msg, "%s���񂪓������܂���", recvMsg.msg);
					AddDispMsg(sendMsg.msg);
					sendMsg.type = MSGTYPE_CHAT;
					SendToAll(sendMsg);

					//�N���C�A���g���X�g�̍X�V
					UpdateListBox();
					break;

				case MSGTYPE_LOGOUT:		//�N���C�A���g��[�ގ�]��[x]�{�^��������
					//�S�N���C�A���g�Ɂu�Q�Q���񂪑ގ����܂����v���b�Z�[�W���M
					wsprintf(sendMsg.msg, "%s���񂪑ގ����܂���", clients[recvIndex].name);
					AddDispMsg(sendMsg.msg);
					sendMsg.type = MSGTYPE_LOGOUT;
					SendToAll(sendMsg);

					//�ΏۃN���C�A���g�Ƃ̃R�l�N�V�����ؒf���\�P�b�g�j��
					Shutdown(clients[recvIndex].sock);

					//�N���C�A���g�z�񂩂�ΏۃN���C�A���g�폜�i�L���t���OOFF�Ɂj
					clients[recvIndex].enabled = FALSE;

					//�N���C�A���g���X�g�̍X�V
					UpdateListBox();
					break;

				case MSGTYPE_CHAT:		//�`���b�g�̃��b�Z�[�W
					//���b�Z�[�W���`�u�Q�Q����̔����F�Q�Q�Q�Q�v	
					wsprintf(sendMsg.msg, "%s����̔����F%s", clients[recvIndex].name, recvMsg.msg);
					AddDispMsg(sendMsg.msg);

					//�S�N���C�A���g�Ƀ��b�Z�[�W���M
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
			// �~�{�^��
		case	IDCANCEL:
			if (state == STATE_CHATTING)
			{
				tagChatMsg sendMsg;		// ���M���b�Z�[�W
				// ����ɐؒf�p���b�Z�[�W����	
				sendMsg.type = MSGTYPE_ALLLOGOUT;
				wsprintf(sendMsg.msg, "�T�[�o���I�����܂�");
				SendToAll(sendMsg);						// ���M	...�G���[�����ȗ�
				Close();								//���X���\�P�b�g�j��
				CtrlDlgState();							// �R���g���[���̏�Ԑݒ�
				KillTimer(hDlg, TIMERID);				// �^�C�}�[�X�g�b�v
				ShutdownAll();							// �ؒf����	
			}

			ExitWinSock();								// WinSock�I������
			EndDialog(hDlg, IDCANCEL);					// �_�C�A���O����
			return	TRUE;

			// [�ڑ���t]�{�^��
		case	IDC_LISTENBTN:
			// ���͂��Ă����|�[�g�ԍ��擾
			port = GetDlgItemInt(hDlg, IDC_PORTEDIT, NULL, FALSE);
			// ���X���\�P�b�g�쐬�C�o�C���h�C���X����Ԃ�
			if (Listen())
			{
				state = STATE_CHATTING;
				CtrlDlgState();							// �R���g���[���̏�Ԑݒ�
				SetTimer(hDlg, TIMERID, 100, NULL);		// �^�C�}�[����
				AddDispMsg("�ڑ��v���҂�");
			}
			else
			{
				AddDispMsg("Error:Listen()");
			}
			return	TRUE;

			// [�ؒf]�{�^��
		case	IDC_SHUTDOWNBTN:
			if (state == STATE_CHATTING)
			{
				tagChatMsg sendMsg;		// ���M���b�Z�[�W
				// ����ɐؒf�p���b�Z�[�W����
				sendMsg.type = MSGTYPE_ALLLOGOUT;
				wsprintf(sendMsg.msg, "�T�[�o���I�����܂�");
				SendToAll(sendMsg);							// ���M	...�G���[�����ȗ�
				Close();									//���X���\�P�b�g�j��
				CtrlDlgState();								// �R���g���[���̏�Ԑݒ�
				KillTimer(hDlg, TIMERID);					// �^�C�}�[�X�g�b�v
				ShutdownAll();								// �ؒf����
				return	TRUE;
			}

			// [���M]�{�^��
		case	IDC_SENDBTN:
			if (state == STATE_CHATTING) {
				tagChatMsg sendMsg;		// ���M���b�Z�[�W
				//������擾
				GetWindowText(hInputEdit, sendMsg.msg, sizeof(sendMsg.msg));
				sendMsg.type = MSGTYPE_CHAT;
				//�S�N���C�A���g�ɑ��M
				if (SendToAll(sendMsg))
				{
					//���M����
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
// SetBindAddr
//		�|�[�g�ԍ�����bindAddr�Z�b�g
//		<����>
//			port:�|�[�g�ԍ�
//		<�߂�l>
//			�Ȃ�
//----------------------------------------------------------------------
void	SetBindAddr(u_short port)
{
	memset(&bindAddr, 0, sizeof(bindAddr));			// 0�N���A

	bindAddr.sin_family = AF_INET;					// TCP/IP
	bindAddr.sin_port = htons(port);				// �|�[�g�ԍ�
	bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);	// IP�A�h���X(�ǂ̃A�h���X����̐ڑ��ł��󂯓����)
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
	strcat_s(dispMsg, addMsg);	
	strcat_s(dispMsg, CRLF);

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
	SendMessage(hDispEdit, EM_LINESCROLL, 0, Edit_GetLineCount(hDispEdit));
}

//------------------------------------------------------------ 
// Accept() 
//		�N���C�A���g����̐ڑ��v����t 
//		 <����> 
//		 �Ȃ� 
//		 <�ߒl>
//		 ���� �FACCEPT_OK (1) 
//		 �G���[ �FACCEPT_ERROR(0) 
//		 �ڑ����I�[�o�[ �FACCEPT_OVER(2)
//		 �ڑ��v���Ȃ� �FACCEPT_NOTYET(3)
//------------------------------------------------------------ 
int Accept() 
{ 
	int sock;

	// accept()���� 
	sock = accept(listenSock, (struct sockaddr *)&bindAddr, &sockaddrLen);
	if (sock < 0)
	{
		if (WSAGetLastError() == WSAEWOULDBLOCK)
			// �ڑ��v�����Ȃ���� ACCEPT_NOTYET�Ԃ� 
			return ACCEPT_NOTYET;
		else	
			// �G���[�Ȃ�ACCEPT_ERROR�Ԃ� 
			return ACCEPT_ERROR;
	}
	
	// �ڑ��v������������clients[]���烋�[�v���Ė��ڑ��̗v�f�T�� 
	for (int i = 0; i < MAXCLIENT; i++)
	{
		// clients[i].enabled��false�̂������ 
		if (!clients[i].enabled)
		{
			// accept()�������ʃZ�b�g 
			clients[i].sock = sock;
			//�m���u���b�L���O�ݒ�
			ioctlsocket(clients[i].sock, FIONBIO, &ul);
			clients[i].enabled = TRUE;
			memcpy(&clients[i].addr, &bindAddr, sizeof(bindAddr));
			recvIndex = i;
			// ACCEPT_OK�Ԃ�
			return ACCEPT_OK;
		}		
	}

	//�ڑ����I�[�o�[
	notAcceptSock = sock;
	return ACCEPT_OVER; 
}

//------------------------------------------------------------ 
// Send() 
//		�N���C�A���g�ւ̃f�[�^���M 
//		<����> 
//		sendMsg�F���M�f�[�^ 
//		<�߂�l> 
//		�����FTRUE 
//		���s�FFALSE 
//------------------------------------------------------------ 
BOOL Send(int sock, tagChatMsg sendMsg)
{
	//�o�C�g�I�[�_�[�ϊ�
	sendMsg.type = htonl(sendMsg.type);

	//�����Ȃ�TRUE,�G���[�Ȃ�FALSE��Ԃ�
	return 	(send(sock, (char *)&sendMsg/*send��char�ŃL���X�g����Α����*/,
				sizeof(sendMsg), 0) == sizeof(sendMsg));
}

//------------------------------------------------------------ 
// SendToAll() 
//		�S�ẴN���C�A���g�ւ̃f�[�^���M 
//		<����> 
//		sendMsg�F���M�f�[�^ 
//		<�߂�l> 
//		�����FTRUE 
//		���s�FFALSE 
//------------------------------------------------------------ 
BOOL SendToAll(struct tagChatMsg sendMsg)
{
	BOOL allSuccess = TRUE; // �����t���O 
	// �N���C�A���g�Ǘ��p�z�񃋁[�v 
	for (int i = 0; i < MAXCLIENT; i++)
	{
		// enabled��TRUE�Ȃ�
		if (clients[i].enabled)
		{
			// ���M �i����̑���ɑ��M�\���̎g�p)
			if (!Send(clients[i].sock, sendMsg))
			{
				// �G���[�Ȃ�allSuccess��FALSE�� 
				allSuccess = FALSE;
			}
		}
	}
	// 1�N���C�A���g�ł����M�G���[���� FALSE�C �S�N���C�A���g�ɑ��MOK TRUE �Ԃ� 
	return allSuccess;
}

//------------------------------------------------------------ 
// Listen() 
//		���X���\�P�b�g�̍쐬�`���X����� 
//		<����> 
//		�Ȃ� 
//		<�߂�l> 
//		�����FTRUE 
//		FALSE�FTRUE 
//------------------------------------------------------------
BOOL Listen()
{
	//���X���\�P�b�g�쐬
	if ((listenSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	{
		return FALSE;
	}

	//�m���u���b�L���O���[�h��
	ioctlsocket(listenSock, FIONBIO, &ul);

	// ���͂��Ă����|�[�g�ԍ��擾����bindAddr�ݒ�
	SetBindAddr(port);

	//�o�C���h
	if (bind(listenSock, (struct sockaddr *)&bindAddr, sizeof(bindAddr)) < 0)
		return FALSE;

	//���X���� (2�Ԗڂ̈������L���[�i�҂��s��j�̃T�C�Y(�ő�ڑ��������Ȃ��m��))
	if (listen(listenSock, MAXCLIENT) < 0)
		return FALSE;

	return TRUE;
}

//------------------------------------------------------------ 
// Recv() 
//		��M���� 
//		<����> 
//		�Ȃ� 
//		<�߂�l> 
//		�����FTRUE 
//		FALSE�FTRUE 
//------------------------------------------------------------
BOOL Recv()
{
	//�N���C�A���g�z�񕪃��[�v
	for (int i = 0; i < MAXCLIENT; i++)
	{
		//�ڑ����Ȃ�
		if (clients[i].enabled)
		{
			memset(&recvMsg, 0, sizeof(recvMsg));	// 0�N���A

			if ((recv(clients[i].sock, (char *)&recvMsg, sizeof(recvMsg), 0)) < 0)
			{
				// ����M�ȊO�Ȃ�G���[ 
				if( WSAGetLastError() != WSAEWOULDBLOCK )
				{ 
					//allSuccess = FALSE; 
					return FALSE;
				}
			}
			//��M�ɐ���������
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
//		�N���C�A���g�Ƃ̃R�l�N�V�����ؒf 
//		 <����> 
//		 sock�F�ؒf����\�P�b�g 
//		 <�߂�l>
//		 �����FTRUE 
//		 ���s�FFALSE 
//------------------------------------------------------------
BOOL Shutdown(int sock) { 
	// �ؒf 
	shutdown( sock, 0x02 ); 

	// �\�P�b�g�̔j�� 
	closesocket( sock ); 

	return FALSE; 
}

//------------------------------------------------------------ 
// ShutdownAll() 
//		�ڑ����̑S�N���C�A���g�Ƃ̃R�l�N�V�����ؒf 
//		<����> 
//		�Ȃ� 
//		<�߂�l> 
//		�����FTRUE 
//		���s�FFALSE 
//------------------------------------------------------------
BOOL ShutdownAll()
{
	//�S�N���C�A���g�����[�v
	for (int i = 0; i < MAXCLIENT; i++)
	{
		if (clients[i].enabled)
		{
			//�ؒf&�\�P�b�g�j��
			Shutdown(clients[i].sock);

			//�L���t���O��FALSE��
			clients[i].enabled = FALSE;
		}
	}
	return TRUE;
}	

//----------------------------------------------------------------------
// Close()
//		�\�P�b�g�j��
//		<����> 
//		�Ȃ� 
//		<�߂�l> 
//		�Ȃ�
//----------------------------------------------------y------------------
void	Close()
{
	// ���X���\�P�b�g�����ς݂Ȃ�j��
	if (listenSock != NULL)
	{
		closesocket(listenSock);
		listenSock = NULL;
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
	hDispEdit = GetDlgItem(hDlg, IDC_DISPMSGEDIT);				// ���b�Z�[�W�\����
	hInputEdit = GetDlgItem(hDlg, IDC_INPUTMSGEDIT);			// ���M���b�Z�[�W���͗�

	hListenBtn = GetDlgItem(hDlg, IDC_LISTENBTN);				// �ڑ���t�{�^��
	hShutdownBtn = GetDlgItem(hDlg, IDC_SHUTDOWNBTN);			// �ؒf�{�^��
	hSendBtn = GetDlgItem(hDlg, IDC_SENDBTN);					// ���M�{�^��
	hClientListBox = GetDlgItem(hDlg, IDC_LISTBOX);

	// �f�t�H���g�|�[�g�ԍ��Z�b�g
	SetDlgItemInt(hDlg, IDC_PORTEDIT, DEFAULTPORT, FALSE);
}

//----------------------------------------------------------------------
// UpdateListBox
//		���X�g�{�b�N�X�X�V
//		<����>
//			�Ȃ�
//		<�߂�l>
//			�Ȃ�
//----------------------------------------------------------------------
void UpdateListBox()
{
	//AddListBox��LB_ADDSTRING�͏㏑���ł͂Ȃ��ǉ��Ȃ̂ŁC���̂܂܂����Ƃǂ�ǂ�ꗗ�������Ă����Ă��܂��B
	//�Ȃ̂ŁC���X�g�{�b�N�X�����Z�b�g(�S����)���Ă���CAddListBox()���Ăяo���B
	//���X�g�{�b�N�X���Z�b�g
	SendMessage(hClientListBox, LB_RESETCONTENT, NULL, NULL);

	//�ڑ��N���C�A���g�����X�g�{�b�N�X�ɒǉ�
	for (int i = 0; i < MAXCLIENT; i++)
	{
		// enabled��TRUE�Ȃ�
		if (clients[i].enabled)
		{
			AddListBox(clients[i]);
		}
	}
}

//----------------------------------------------------------------------
// CtrlDlgState
//		�e�R���g���[���̏�Ԑݒ�
//		<����>
//			client:�N���C�A���g���
//		<�߂�l>
//			�Ȃ�
//----------------------------------------------------------------------
void AddListBox(struct tagClientInfo client)
{
	char	str[MSGBUFFSIZE];	

	//���X�g�{�b�N�X�ɒǉ����镶����̐��� IP�A�h���X:�n���h���l�[��
	wsprintf(str, "%s:%s", inet_ntoa(client.addr.sin_addr), client.name);

	//���X�g�{�b�N�X�ɒǉ�
	SendMessage(hClientListBox, LB_ADDSTRING, NULL, (LPARAM)str);
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
	EnableWindow(hInputEdit, (state == STATE_CHATTING));		// ���M���b�Z�[�W���͗�	�F�ڑ����L��

	// Button
	EnableWindow(hListenBtn, (state == STATE_INIT));			// �ڑ���t�{�^��		�F���ڑ����L��
	EnableWindow(hShutdownBtn, (state == STATE_CHATTING));		// �ؒf�{�^��			�F�ڑ����L��
	EnableWindow(hSendBtn, (state == STATE_CHATTING));			// ���M�{�^��			�F�ڑ����L��
}

