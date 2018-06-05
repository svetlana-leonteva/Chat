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
		strcat(message_, " says: ");
		strcat(message_, senderMsg);
		msgServerRecvTime_=recvTime;
	}
};

	/*Описывает чат-сервер*/
class Server
{
	int connectionsListener_; // сокет для отлова подключений
	sockaddr_in serverAddress_; // данные о сетевом соединении (адресе) сервера
	unsigned int serverPort_; 	// порт сервера
	fd_set socketsReadSet_; // набор из дескрипторов для Select
    map <int, ClientData> clientsDataSet_; 	// набор данных о клиентах (сокет, данные)
	map <int, ClientData>::iterator clientsDataSetIter_; // итератор для набора данных клиентов
	
	int lastMessagesTimeout_; 		// время, за которое показываются последние сообщения для нового клиента
	char timeoutInMinutesStr_[255];	// lastMessagesTimeout_, переведенный в строку 
	
	vector <RecieverClientMsg*> allClientMessages_; 		// все сообщения от клиентов
	vector <RecieverClientMsg*> recievedClientMessages_; 	// сообщения, полученные за одно чтение сокетов клиентов
	vector <RecieverClientMsg*>::iterator clientMsgIter_; 	// итератор для сообщений
	
	time_t serverLocalTime_; // текущее время на сервере

public:
	Server()
	{	// конструктор (читает начальные настройки из конфига)
		loadConfig();
		initServer();
	};

private:	
	void loadConfig();
	void initServer();
	inline void initSocketsFDSet();
	inline void selectSockets();
	inline void recieveNewClientConnection();
	void sendLastMessagesToClient(int clientSocket);
	inline void readClientsSockets();
	inline void clientConnectionDrop();
	bool registerNickName(char *regClientNickName);
	inline void sendMessagesToClients();
	bool serverCycleIter();
	
public:
	void serverCycle()
		{while (serverCycleIter());}
};


void Server::loadConfig()
{
	serverPort_=3000;
	lastMessagesTimeout_=60;
	sprintf(timeoutInMinutesStr_, "%5.2f", (float)lastMessagesTimeout_/60.0);
};

void Server::initServer()
{
	connectionsListener_=socket(AF_INET, SOCK_STREAM, 0);
	if (connectionsListener_ < 0) 
	{	// если произошла ошибка при открытии сокета
		perror("socket"); 
		exit(1);
	}
		// устанавливаем флаг NONBLOCK
	fcntl(connectionsListener_, F_SETFL, O_NONBLOCK);
		// настраиваем параметры соединения 
	serverAddress_.sin_family=AF_INET;
	serverAddress_.sin_port=htons(serverPort_);
	serverAddress_.sin_addr.s_addr=INADDR_ANY;
		// биндим настройки на сокет
	if (bind(connectionsListener_ , (sockaddr*) &serverAddress_, sizeof(serverAddress_)) < 0)
	{	// если произошла ошибка при бинде
		perror("bind");
		exit(2);
	}
		// переводим сокет в пассивный режим
	listen(connectionsListener_, 10);
};

inline void Server::initSocketsFDSet()
{
	FD_ZERO(&socketsReadSet_);
	FD_SET(connectionsListener_, &socketsReadSet_);
	for (clientsDataSetIter_ = clientsDataSet_.begin();
		 clientsDataSetIter_ != clientsDataSet_.end();
		 clientsDataSetIter_++)
		FD_SET(clientsDataSetIter_->first, &socketsReadSet_);
}
		
inline void Server::selectSockets()
{
	int maxSockNum=connectionsListener_;
	if (!clientsDataSet_.empty())
	{
		clientsDataSetIter_ = clientsDataSet_.end();
		maxSockNum=max(connectionsListener_, (--clientsDataSetIter_)->first);
	}
	if (select(maxSockNum+1, &socketsReadSet_, NULL, NULL, NULL) <= 0)
	{	// если произошла ошибка при выборе события (сокета)
		perror("select");
		exit(3);
	}
}

inline void Server::recieveNewClientConnection()
{
	int newSocket=accept(connectionsListener_, NULL, NULL);
	if (newSocket < 0)
	{	// если произошла ошибка при приеме нового соединения
		perror("accept");
		exit(3);
	}
		// устанавливаем флаг NONBLOCK
	fcntl(newSocket, F_SETFL, O_NONBLOCK);
		// добавляем нового клиента
	ClientData newClientData;
	newClientData.isRegistered_=false; // выставляем признак незареганности
	clientsDataSet_.insert(pair<int, ClientData>(newSocket, newClientData));
}
	
void Server::sendLastMessagesToClient(int clientSocket)
{	// отсылает клиенту с заданным сокетом последние сообщения за lastMessagesTimeout_ секунд
	if (!allClientMessages_.empty())
	{
			// подготовливаем сервисное сообщение
		RecieverClientMsg *newServerMsg = new RecieverClientMsg(serverLocalTime_);
		strcpy(newServerMsg->message_, "..:::LAST MESSAGES FOR LAST");
		strcat(newServerMsg->message_, timeoutInMinutesStr_);
		strcat(newServerMsg->message_, " MINUTES:::..");
			// отправляем сервисное сообщение
		send(clientSocket, (void*)newServerMsg, sizeof(RecieverClientMsg), 0);
		
		for (clientMsgIter_ = allClientMessages_.begin();
			 clientMsgIter_ != allClientMessages_.end();
			 clientMsgIter_++) // высылаем последние принятые сообщения
		{
			if ((serverLocalTime_ - (*clientMsgIter_)->msgServerRecvTime_) > lastMessagesTimeout_)
			{	// сообщение выходит за предел таймаута удаляем и переходим к следующему
				allClientMessages_.erase(clientMsgIter_);
				continue;
			}
			send(clientSocket, (void*)(*clientMsgIter_), sizeof(RecieverClientMsg), 0);
		} // endFor
		newServerMsg = new RecieverClientMsg(serverLocalTime_);
		strcpy(newServerMsg->message_, "..:::END OF LAST MESSAGES LIST:::...");
			// отправляем сервисное сообщение
		send(clientSocket, (void*)newServerMsg, sizeof(RecieverClientMsg), 0);
		delete newServerMsg;
	} // endIf	
}

inline void Server::readClientsSockets()
{
	recievedClientMessages_.clear(); // очищаем вектор принятых сообщений		
	for (clientsDataSetIter_ = clientsDataSet_.begin(); 
		 clientsDataSetIter_ != clientsDataSet_.end();
		 clientsDataSetIter_++)
	{	// проверяем каждый клиентский сокет
		if (FD_ISSET(clientsDataSetIter_->first, &socketsReadSet_))
		{	// если поступили данные от клиента - читаем и сохраняем их
			SenderClientMsg* newSenderMsg = new SenderClientMsg;
			int bytes_read = recv(clientsDataSetIter_->first, newSenderMsg, sizeof(SenderClientMsg), MSG_WAITALL);
			if (bytes_read <= 0)
			{	// если соединение разорвано - обрабатываем событие
				clientConnectionDrop();
				if (clientsDataSet_.empty())
					break;
				continue;
			} // endIf (dropped connection)
			
			switch (newSenderMsg->msgType_)
			{	
			case NICK_REGISTRATION_REQUEST:
					// если пришел запрос на регистрацию - пытаемся зарегать ник клиента 
				if (registerNickName(newSenderMsg->clientNickName_))
				{	// если рега прошла успешно
						// подготавливаем ответное сообщение клиенту
					RecieverClientMsg* regMsg = new RecieverClientMsg(serverLocalTime_);
					strcpy(regMsg->message_, "Registration success! Welcome!");
					strcat(regMsg->message_, "\nCurrent number of chatters: ");
					char numOfChattersStr[10];
					sprintf(numOfChattersStr, "%i", clientsDataSet_.size());
					strcat(regMsg->message_, numOfChattersStr); 
					regMsg->msgType_=NICK_REGISTRATION_SUCCESS;
						// отсылаем клиенту сообщение об успешной реге
					send(clientsDataSetIter_->first, (void*)regMsg, sizeof(RecieverClientMsg), 0);
						// отсылаем клиенту последние сообщения 
					sendLastMessagesToClient(clientsDataSetIter_->first);
						// подготавливаем сообщение с приветствием
					RecieverClientMsg* newRecieverMsg = new RecieverClientMsg(serverLocalTime_);
					strcpy(newRecieverMsg->message_, "[SERVER] We have a new member! Greetings to ");
					strcat(newRecieverMsg->message_, newSenderMsg->clientNickName_);
					strcat(newRecieverMsg->message_, "!");
						// добавляем сообщение с приветствием в рассылку
					recievedClientMessages_.push_back(newRecieverMsg);
					delete regMsg;
				}
				else
				{	// если рега обломалась
					RecieverClientMsg* regMsg = new RecieverClientMsg(serverLocalTime_);
					strcpy(regMsg->message_, "Registration failed! This nick is in use!");
					regMsg->msgType_=NICK_REGISTRATION_FAIL;
						// отсылаем сообщение об эпик фейл
					send(clientsDataSetIter_->first, (void*)regMsg, sizeof(RecieverClientMsg), 0);
					delete regMsg;
				}
			break;

			case NORMAL_CHAT_MSG: 
				// если это обычное сообщение - добавляем его в рассылку
				RecieverClientMsg* newRecieverMsg = new RecieverClientMsg(clientsDataSetIter_->second.clientNickName_ ,newSenderMsg->message_, serverLocalTime_);
				recievedClientMessages_.push_back(newRecieverMsg);	
			break;
			} // endSwitch 
		}	// endIf (FD_ISSET)
	} // endFor
	allClientMessages_.insert(allClientMessages_.end(),
							 recievedClientMessages_.begin(), 
							 recievedClientMessages_.end());
}

void Server::clientConnectionDrop()
{
		
	if (clientsDataSetIter_->second.isRegistered_)
	{	// если клиент зарегистрирован - подготавливаем сообщение с прощанием
		RecieverClientMsg* newRecieverMsg = new RecieverClientMsg(serverLocalTime_);
		strcpy(newRecieverMsg->message_, "[SERVER] ");
		strcat(newRecieverMsg->message_, clientsDataSetIter_->second.clientNickName_);
		strcat(newRecieverMsg->message_, " has left us! Bye-bye!");
			// добавляем сообщение с прощанием в рассылку
		recievedClientMessages_.push_back(newRecieverMsg);	
	}
		// удаляем данные о клиенте
	close(clientsDataSetIter_->first);
	map <int, ClientData>::iterator iter=clientsDataSetIter_;
	iter--;
	clientsDataSet_.erase(clientsDataSetIter_);
	clientsDataSetIter_=iter;	
}

bool Server::registerNickName(char *regClientNickName)
{
	map<int, ClientData>::iterator iter;
	for (iter = clientsDataSet_.begin(); iter != clientsDataSet_.end(); iter++)
	{	// ищем клиента с совпадающим ником
		if (strcmp(regClientNickName, iter->second.clientNickName_) == 0)
			return false; // если нашли - отказ в реге
	}
		// если все ок - регаем ник
	strcpy(clientsDataSetIter_->second.clientNickName_, regClientNickName);
	clientsDataSetIter_->second.isRegistered_=true;
	return true;
	
}

inline void Server::sendMessagesToClients()
{
	for (clientsDataSetIter_ = clientsDataSet_.begin(); 
		 clientsDataSetIter_ != clientsDataSet_.end();
		 clientsDataSetIter_++)
	{ // для каждого подключенного клиента
		for (clientMsgIter_ = recievedClientMessages_.begin();
			 clientMsgIter_ != recievedClientMessages_.end();
			 clientMsgIter_++) // высылаем принятые сообщения
		{
			if (clientsDataSetIter_->second.isRegistered_)
				send(clientsDataSetIter_->first, (void*)(*clientMsgIter_), sizeof(RecieverClientMsg), 0);
		}
	} 
}

bool Server::serverCycleIter()
{	// итерация рабочего цикла сервера
	serverLocalTime_=time(NULL);
	initSocketsFDSet();
	selectSockets();
	if (FD_ISSET(connectionsListener_, &socketsReadSet_))
		recieveNewClientConnection(); // принимаем новое подключение
	readClientsSockets(); // читаем клиентские сокеты
	sendMessagesToClients(); // рассылаем клиентам принятые сообщения
	return true;
}






int main(int argc, const char * argv[])
{
	Server serv;			
	serv.serverCycle();
}
