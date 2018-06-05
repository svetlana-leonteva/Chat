#ifndef SERV_H
#define	SERV_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <time.h>
#include <map>
#include <vector>

using namespace std;

const int MSG_LENGTH=1024;
const int NICK_LENGTH=50;
const int PORT=8082;

enum MSG_TYPES {NORMAL_CHAT_MSG=0, NICK_REGISTRATION_REQUEST, NICK_REGISTRATION_SUCCESS, NICK_REGISTRATION_FAIL};

struct ClientData
{
	bool isRegistered_;
	char clientNickName_[NICK_LENGTH];
};

struct SenderClientMsg
{
	char clientNickName_[NICK_LENGTH];
	char message_[MSG_LENGTH];
	unsigned short int msgType_;
};

struct RecieverClientMsg
{
	char message_[MSG_LENGTH+NICK_LENGTH];	
	time_t msgServerRecvTime_;
	unsigned short int msgType_;
	RecieverClientMsg(time_t recvTime)
		{msgServerRecvTime_=recvTime;}
		
	RecieverClientMsg(char *senderNickName, char *senderMsg, time_t recvTime)
	{
		strcpy(message_, senderNickName);
		strcat(message_, ": ");
		strcat(message_, senderMsg);
		msgServerRecvTime_=recvTime;
	}
};

	/*Описывает чат-сервер*/
class Server
{
	int Listener; // сокет для отлова подключений
	sockaddr_in ServAddr; // данные о сетевом соединении (адресе) сервера
	unsigned int Port; 	// порт сервера
	fd_set socketsRead; // набор из дескрипторов для Select
        map <int, ClientData> Clients; 	// набор данных о клиентах (сокет, данные)
	map <int, ClientData>::iterator Iterator; // итератор для набора данных клиентов
	
	vector <RecieverClientMsg*> ClientMess; 		// все сообщения от клиентов
	vector <RecieverClientMsg*> recMess; 	// сообщения, полученные за одно чтение сокетов клиентов
	vector <RecieverClientMsg*>::iterator cMsgIter; 	// итератор для сообщений
	
	time_t serverLocalTime_; // текущее время на сервере

public:
	Server()
	{	
		Port=PORT;
		Listener=socket(AF_INET, SOCK_STREAM, 0);
                if (Listener < 0) 
                {	
                        perror("error socket"); 
                        exit(1);
                }
                fcntl(Listener, F_SETFL, O_NONBLOCK);
                ServAddr.sin_family=AF_INET;
                ServAddr.sin_port=htons(Port);
                ServAddr.sin_addr.s_addr=INADDR_ANY;
                if (bind(Listener , (sockaddr*) &ServAddr, sizeof(ServAddr)) < 0)
                {
                        perror("error bind");
                        exit(2);
                }
                listen(Listener, 10);
	};

private:	
	inline void NewClient();
	inline void readClients();
	inline void clientConnectionDrop();
	bool registerNick(char *regClientNickName);
	inline void SendMes();
	bool serverCycleIter();
	
public:
	void BeginChat()
        {
            while (true){
                serverLocalTime_=time(NULL);
                FD_ZERO(&socketsRead);
                FD_SET(Listener, &socketsRead);
                for (Iterator = Clients.begin();
                         Iterator != Clients.end();
                         Iterator++)
                        FD_SET(Iterator->first, &socketsRead);
                int Sock=Listener;
                if (!Clients.empty())
                {
                        Iterator = Clients.end();
                        Sock=max(Listener, (--Iterator)->first);
                }
                if (select(Sock+1, &socketsRead, NULL, NULL, NULL) <= 0)
                {	// если произошла ошибка при выборе события (сокета)
                        perror("select");
                        exit(3);
                }
                if (FD_ISSET(Listener, &socketsRead))
                        NewClient(); // новые клиенты
                readClients(); // читаем клиентские сокеты
                SendMes(); // рассылаем клиентам принятые сообщения
            }
        }
};

#endif	/* SERV_H */