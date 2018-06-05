#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syslog.h>

#include "Client.h"

using namespace std;

void Client::initConnection(int argc, char** argv)
{
        //создаем сокет
	sockaddr_in addr;
	sock=socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		perror("Error Socket");
		exit(1);
	}
		addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
                addr.sin_family=AF_INET;
		addr.sin_port=htons(PORT);
	//коннект. Если ошибка,то сигналим.
	if (connect (sock, (sockaddr*)&addr, sizeof(addr)) < 0)
	{	
		perror("Error Connect");
		exit(2);
	}
}

//регистрация ника
void Client::Registration()
{
		printf("Enter nick: ");
                //вводим ник
		fgets(nick_, NICK_LENGTH, stdin);
                //защита т дурака
		while (strlen(nick_) == 1 )
		{
			printf("Nickname is empty!\nEnter a nickname: ");
			fgets(nick_, NICK_LENGTH, stdin);
		}
            
		nick_[strlen(nick_)-1]='\0';	
        
        fd_set descrReadSet_;
	FD_ZERO(&descrReadSet_);
	FD_SET(sock, &descrReadSet_);

        //отправляем данные на сервер
	SendMsg newMsg(nick_, NICK_REGISTRATION_REQUEST);
	send (sock, (void*)&newMsg, sizeof(SendMsg), 0);
}

void Client::BeginChat()
{
	int flags(fcntl(STDIN_FILENO, F_GETFL, 0));
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);		
	fcntl(sock, F_SETFL, O_NONBLOCK);
	
	while (true){
                FD_ZERO(&descrReadSet_);
                FD_SET(STDIN_FILENO, &descrReadSet_);
                FD_SET(sock, &descrReadSet_);

                if (select(sock+1, &descrReadSet_, NULL, NULL, NULL) <= 0)
                {	
                        perror("Error Select");
                        exit(3);
                }
                if (FD_ISSET(sock, &descrReadSet_))
                {
                    //прием сообшения
                        RecievMsg recvMsg;
                        if (recv(sock, (void*)&recvMsg, sizeof(RecievMsg), 0) > 0) 
                                recvMsg.printMsg();
                }

                if (FD_ISSET(STDIN_FILENO, &descrReadSet_));
                {
                    //отправка
                    printf("-->");
                        if (fgets_unlocked(outMsg_, MSG_LENGTH, stdin ))
                        {
                                SendMsg newMsg(outMsg_, NORMAL_CHAT_MSG);
                                send (sock, (void*)&newMsg, sizeof(SendMsg), 0);
                        }
                }
        }
}

int main (int argc, char** argv)
{
	Client client(argc, argv);
	client.Registration();
	client.BeginChat();
}

