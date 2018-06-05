#ifndef CLIENT_H
#define	CLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

using namespace std;

const int MSG_LENGTH=1024; //длина сообшения
const int NICK_LENGTH=50; //длина ника
const int PORT=8082; //порт

//переменные для обмена сообщениями
enum MSG_TYPES {NORMAL_CHAT_MSG=0, NICK_REGISTRATION_REQUEST, NICK_REGISTRATION_SUCCESS, NICK_REGISTRATION_FAIL};

struct SendMsg
{
	char clientNickName_[NICK_LENGTH];
	char message_[MSG_LENGTH];
	unsigned short int msgType_;
	SendMsg(char* data, unsigned short int msgType)
	{
		msgType_=msgType; //тип посылки
		strcpy(clientNickName_, "");
                // если регистрация ника
		if (msgType_== NICK_REGISTRATION_REQUEST)
			strcpy(clientNickName_, data);
                //если отправка сообщения
		if (msgType_== NORMAL_CHAT_MSG)
			strcpy(message_, data);
	}
};

struct RecievMsg
{
	char message_[MSG_LENGTH+NICK_LENGTH];	
	time_t msgServerRecvTime_;
	unsigned short int msgType_;
        //принимаем сообшение
	void printMsg()
		{printf("\n%s%s\n", asctime(localtime(&msgServerRecvTime_)), message_);}
};

class Client
{
	int sock;
	fd_set descrReadSet_;
	char nick_[NICK_LENGTH];
	char outMsg_[MSG_LENGTH];
	
public:
	Client(int argc, char** argv)
	{
            //инициируем коннект
            initConnection(argc, argv);
	}

private:
	void initConnection(int argc, char** argv);

public:
	void Registration();
	void BeginChat();
};

#endif	/* CLIENT_H */

