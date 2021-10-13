#include <iostream>
#include <cstdlib>
#include <sstream>
#include <string>
#include <cstring>
#include <cmath>
#include <ctime>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <random>
#include <arpa/inet.h>

#define D_RTT				15
#define D_THRESHOLD			65536
#define D_MSS				1024
#define D_BUFFER_SIZE			524288


using namespace std;
pid_t pid;

typedef enum {
	TcpState_None,					
	TcpState_SlowStart,			
	TcpState_CongestionAvoid,		
	TcpState_FastRecover			
} TcpState;
	
typedef enum {
	PacketCmd_Data,				
	PacketCmd_ACK,			
	PacketCmd_SYN,				
	PacketCmd_SYNACK,			
	PacketCmd_FIN			
} PacketCmd;

class Server;
class Packet;

struct SACKblock{
	int left;
	int right;
};
struct SACKheader{
	unsigned char kind=0;
	unsigned char length=2;
	SACKblock blockList[3];
};

class Packet 
{
		public:
		short int srcPort;      			
		short int destPort;					
		
		
		int seqNum;       					
		int ackNum;       					
		short head_len:4,not_use:6;	
	
		bool flagURG;
		bool flagACK;						
		bool flagPSH;
		bool flagRST;
		bool flagSYN;					
		bool flagFIN;					
				
	
		short int rwnd;            			
		short int checksum;					
		short urg_ptr;
		int datasize;
		SACKheader sack;
		char data[D_MSS];   		
		

		Packet(){
			srcPort = 0;
			destPort = 0;
			seqNum = 0;
			ackNum = 0;
			flagACK = false;
			flagSYN = false;
			flagFIN = false;
			rwnd = 0;
			checksum = 0;
			memset(data, 0, sizeof(data));
		}
		Packet(PacketCmd command, Server server);
		PacketCmd parser();
		string getName();	
};

class Server {
	public:
		int MSS;
		int RTT;
		int THRESHOLD;
		int BufferSize;
		

		Packet lastPacket;				
		int fd;					
		struct sockaddr_in srcSocket;			
		struct sockaddr_in destSocket;		
		struct sockaddr_in tempSocket;
		
		int seqNum;
		int ackNum;
		int rwnd;
		int cwnd;					
		char fileBuffer[D_BUFFER_SIZE];		
		bool delay;					
		int duplicateACKcount;				
		TcpState state;
        int reqNum;         //request總數
        string flag[10];                    //client要求的flag
		string name[10];					//request的參數
		vector<SACKblock> sackList;
		bool sackoption=false;
	
		Server(){
			MSS = D_MSS;
			RTT = D_RTT;
			THRESHOLD = D_THRESHOLD;
			BufferSize=D_BUFFER_SIZE;
			memset(fileBuffer,0,BufferSize);
		}
		
		void createSocket(const char *srcIP, int srcPort);
		void printInfo();
		void threeWayhandshake(int tmp_p);
		void send(Packet packet,bool printornot=true);
		Packet read(bool printornot=true);
		void updateNumber(const Packet packet);
		
		void reset();
		void transfer(int i);
        void calculate(int i);
        void to_postfix(string infix, char postfix[][10], char stack[][10], int *top);
        int priority(char* a, char* b);
        char* top_data(char stack[][10], int top);
        int IsEmpty(int top);
        char* pop(char stack[][10], int *top);
        void push(char *item, char stack[][10], int *top);
        int IsDigit(char* c);
        void dns(int i);
		bool setTimeout(int readFD,int msec);
};
Packet::Packet(PacketCmd command, Server server){
	flagACK = false;
	flagSYN = false;
	flagFIN = false;
	checksum = 0;
	memset(data, 0, 1024);
	srcPort = server.srcSocket.sin_port;
	destPort = server.destSocket.sin_port;
	seqNum = server.seqNum;
	ackNum = server.ackNum;
	rwnd = server.rwnd;
	switch(command){
		case PacketCmd_ACK:				
			flagACK = true;
			checksum = 1;
			break;
		case PacketCmd_SYN:				
			flagSYN = true;
			checksum = 1;
			break;
		case PacketCmd_SYNACK:			
			flagACK = true;
			flagSYN = true;
			checksum = 1;
			break;
		case PacketCmd_FIN:
			flagFIN = true;
			checksum = 1;
			break;
		case PacketCmd_Data:{				
			break;
		}
	}
}
PacketCmd Packet::parser(){
	if(flagACK && flagSYN) return PacketCmd_SYNACK;
	else if(flagACK) return PacketCmd_ACK;
	else if(flagSYN) return PacketCmd_SYN;
	else if(flagFIN) return PacketCmd_FIN;
	else return PacketCmd_Data;
}
string Packet::getName(){
	switch(this->parser()){
		case PacketCmd_Data:return "packet";			
		case PacketCmd_ACK:return "packet(ACK)";			
		case PacketCmd_SYN:return "packet(SYN)";		
		case PacketCmd_SYNACK:return "packet(SYNACK)";		
		case PacketCmd_FIN:return "packet(FIN)";
	}
	return "";
}
void Server::createSocket(const char *srcIP, int srcPort){
	fd = socket(AF_INET,SOCK_DGRAM,0);
	if (fd < 0) {perror("socket error\n");}
	memset((char*) &srcSocket, 0, sizeof(srcSocket) );
	srcSocket.sin_family = AF_INET;
	srcSocket.sin_port = srcPort; 
	srcSocket.sin_addr.s_addr = inet_addr(srcIP); 

	//inet_addr()的功能是將一個點分十進制的IP轉換成一個長整數型數（u_long類型）
	if (bind(fd, (struct sockaddr *)&srcSocket, sizeof(srcSocket)) < 0){perror("bind error\n");} 
}
void Server::printInfo(){
	cout<<"\n==============================\n";
	cout<<"Server's ip is "<<inet_ntoa(srcSocket.sin_addr)<<endl;
	cout<<"Server is listening on port "<<srcSocket.sin_port<<endl;
	cout<<"==============================\n";
	cout<<"listening......\n";
}
Packet Server::read(bool printornot){
	Packet pkg;
	socklen_t pkg_l = sizeof(destSocket);
	usleep((this->RTT>>1)*1000);
	recvfrom(fd, &pkg, sizeof(Packet), 0, (struct sockaddr*)&destSocket, &pkg_l);
	
	switch(pkg.parser()){
		case PacketCmd_ACK:
		case PacketCmd_SYNACK:
		case PacketCmd_FIN:
		{
			if(printornot)
			cout<<"Received a " << pkg.getName() << " from " << inet_ntoa(destSocket.sin_addr)<<":"<<destSocket.sin_port<<endl;
		}
		case PacketCmd_Data:{
			if(pkg.checksum==0)cout<<"Packet loss!! ACK = "<<pkg.ackNum<<endl;
			else cout<<"\t\t" << "Receive a packet (" << "seq_num = " << pkg.seqNum << ", " << "ack_num = " << pkg.ackNum << ")"<<endl;
			break;
		}
	}
	return pkg;
}
void Server::updateNumber(const Packet packet){
	if(seqNum>0)
		seqNum=packet.ackNum;
	else
		seqNum=rand()%10001;
	ackNum = packet.seqNum + packet.checksum;
}

void Server::send(Packet packet,bool printornot){
	sendto(fd,&packet,sizeof(Packet),0,(struct sockaddr*)&destSocket, sizeof(destSocket));
	switch(packet.parser()){
		case PacketCmd_SYN:
		case PacketCmd_ACK:
		case PacketCmd_SYNACK:
		case PacketCmd_FIN:
		{
			if(printornot)
			cout<<"Send a " << packet.getName() << " to " << inet_ntoa(destSocket.sin_addr)<<":"<<destSocket.sin_port<<endl;
			break;
		}
		
	}
	
}
void Server::threeWayhandshake(int tmp_p){
	Packet recv = read();
    
	struct sockaddr_in ori = srcSocket;
	int ori_fd = fd;
	int sd = socket(AF_INET,SOCK_DGRAM,0);
	if (sd < 0) { perror("socket error\n"); }
    
	memset((char*) &tempSocket, 0, sizeof(tempSocket) );
	tempSocket.sin_family = AF_INET;
	tempSocket.sin_port = tmp_p; 
	tempSocket.sin_addr.s_addr = inet_addr("192.168.0.1");
    
	if (bind(sd, (struct sockaddr *)&tempSocket, sizeof(tempSocket)) < 0){ perror("bind error\n"); }
	int reuseAddr = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr) );
	bool end = false;
    
    //建立子執行緒
	pid = fork();
	if(pid == 0){
		cout<<"\n=====start the three-way handshake=====\n";
		srcSocket = tempSocket;
		fd = sd;
		cout  << "Server's ip is " << inet_ntoa(srcSocket.sin_addr) << endl;
		cout << "Server is listening on port " << srcSocket.sin_port << endl;
		while(!end){		
			switch(recv.parser()){
				case PacketCmd_SYN:{
					updateNumber(recv);
					if(recv.sack.kind == 4)  sackoption = true;
					Packet t = Packet(PacketCmd::PacketCmd_SYNACK,*this);
					t.sack.kind=4;
					send(t);
					break;
				}
				case PacketCmd_ACK:{
					updateNumber(recv);
                    string t, temp = recv.data;
                    int p = 0,  tern = 0;
                    stringstream ss(temp);      //接收flag和參數
                    while(getline(ss, t, ':')){
                        if(p == 0)  reqNum = stoi(t);
                        else  if(p % 2 != 0){
                            flag[tern] = t;
                        }
                        else{
                            name[tern] = t;
                            tern ++;
                        }
                        p++;
                    }
					end = true;
					break;
				}
			}
			if(!end)recv=read();
		}
		cout<<"=====complete the three-way handshake=====\n";
        for(int j = 0; j < reqNum; j++){
            if(flag[j] == "-f")
                transfer(j);
            else if(flag[j] == "-c")
                calculate(j);
            else if(flag[j] == "-d")
                dns(j);
            else
                cout << "Flag Error" << endl;
        }
	}
	srcSocket=ori;
	fd=ori_fd;
}
void Server::transfer(int j){
	long cBytes;
	FILE *input;
    name[j] = name[j] + ".mp4";
	if((input = fopen(name[j].c_str(),"r")) == NULL){ cout << "open error\n"; return; }


	int wanttoloss = 10240, wanttoloss2 = 12288, wanttoloss3 = 14336;

	//初始化
	state = TcpState::TcpState_SlowStart;
	int base = 1,len, len_cnt = 0;
	bool end = false, dup = false, ploss = true, dont = false, ploss2 = true, ploss3 = true;
	Packet recv,prev;
	cwnd = 1;
	seqNum = base;
	rwnd = BufferSize;
	duplicateACKcount = 0;

	cout<<"======slow start=========\n";
	while(1){
	cout<<"Sack list:\n";if(sackList.size()==0)cout<<"None\n";
	else{	for(int i=0;i<sackList.size();i++){cout<<"left = "<<sackList[i].left<<" ,right = "<<sackList[i].right<<endl;}}
		cout<<"cwnd =  "<< cwnd << ", " <<"rwnd = "<< rwnd << ", "<<"threshold = " << THRESHOLD<<endl;
		if(state==TcpState::TcpState_SlowStart){							//slow start
			for(int j=0;j<cwnd;j++){
				Packet t=Packet(PacketCmd::PacketCmd_Data,*this);
				len=fread(t.data,sizeof(char),MSS,input);
				t.checksum=len;
				if(len_cnt==wanttoloss && ploss){ploss=false;cout<<"***Data loss at byte : "<<wanttoloss<<endl;}
				else if(len_cnt==wanttoloss2 && ploss2){ploss2=false;cout<<"***Data loss at byte : "<<wanttoloss2<<endl;} 					//generate loss
				else if(len_cnt==wanttoloss3 && ploss3){ploss3=false;cout<<"***Data loss at byte : "<<wanttoloss3<<endl;} 					//generate loss
				else {
					send(t);
					cout<<"\t\tSend a packet at : "<<len<<" byte\n";
					if(sackoption && sackList.size()!=0 && t.seqNum==sackList[0].left)sackList.erase(sackList.begin());
				}
				base+=len;
				rwnd-=len;
				prev=recv;
				bool isTimeout=setTimeout(fd,500000);						//listen for 500ms
				if(isTimeout){									//stay slow start
					dont=true;
					THRESHOLD=cwnd*MSS/2;
					cwnd=1;
					duplicateACKcount=0;
					seqNum=base;
					ackNum++;
					if(len<MSS){end=true;break;}
					len_cnt+=len;
					cout<<"timeout!!\n";
					cout<<"========slow start======\n";
					break;

				}
				else{
					recv=read(false);
					if(sackoption && recv.sack.length>2){
						bool exist=false;			
						SACKblock s;
						s.left=recv.sack.blockList[0].left;
						s.right=recv.sack.blockList[0].right;
						for(int j=0;j<sackList.size();j++){
							if(sackList[j].left==s.left){
								exist=true;break;
							}
						}
						if(!exist)sackList.push_back(s);
						//cout<<"***Data loss at byte : "<<s.left<<endl;
					}
					if(prev.ackNum==recv.ackNum){
						duplicateACKcount++;
						//cout<<"dup++\n";
					}
					if(duplicateACKcount>=3){						//enter fast recovery
						dont=true;
						dup=true;
						end=false;
						duplicateACKcount=0;
						THRESHOLD=cwnd*MSS/2;
						if(THRESHOLD==0)THRESHOLD=1;
						cwnd=THRESHOLD/MSS+3;
						if(cwnd==0)cwnd=1;
						rwnd=BufferSize;
						if(len==1024){
							base-=(len*4);
							fseek(input,-4*len,SEEK_CUR);
							seqNum=base;
							ackNum-=4;
							len_cnt-=(len*4);
						}
						else{
							base-=(len+1024*3);
							fseek(input,-1*(len+3*1024),SEEK_CUR);
							seqNum=base;
							ackNum-=4;
							len_cnt-=(len+1024*3);
						}
						cout<<"Receive 3 duplicate ack!!\n";
						cout<<"======fast retransmit=====\n";
						cout<<"======fast recovery=====\n";
						state=TcpState::TcpState_FastRecover;
						break;
					}
					else{
						seqNum=base;
						ackNum++;
						if(len<MSS){end=true;break;}
						len_cnt+=len;
					}
				}
			}
			if(!end && !dont)cwnd*=2;
			if(cwnd*MSS>=THRESHOLD  && !end && !dont){
				cout<<"=====congetstion avoidance======\n";
				state=TcpState::TcpState_CongestionAvoid;
			}
		}
		else if(state==TcpState::TcpState_CongestionAvoid){						//congestion avoidance
			for(int j=0;j<cwnd;j++){
				Packet t=Packet(PacketCmd::PacketCmd_Data,*this);
				len=fread(t.data,sizeof(char),MSS,input);
				t.checksum=len;
				if(len_cnt==wanttoloss && ploss){ploss=false;cout<<"***Data loss at byte : "<<wanttoloss<<endl;}
				else if(len_cnt==wanttoloss2 && ploss2){ploss2=false;cout<<"***Data loss at byte : "<<wanttoloss2<<endl;} 					//generate loss
				else if(len_cnt==wanttoloss3 && ploss3){ploss3=false;cout<<"***Data loss at byte : "<<wanttoloss3<<endl;} 					//generate loss
				else {
					send(t);
					cout<<"\t\tSend a packet at : "<<len<<" byte\n";
					if(sackoption && sackList.size()!=0 &&t.seqNum==sackList[0].left)sackList.erase(sackList.begin());
				}
				base+=len;
				rwnd-=len;
				prev=recv;
				bool isTimeout=setTimeout(fd,500000);						//listen for 500ms
				if(isTimeout){									//timeout!! go to slow start
					dont=true;
					THRESHOLD=cwnd*MSS/2;
					cwnd=1;
					duplicateACKcount=0;
					seqNum=base;
					ackNum++;
					if(len<MSS){end=true;break;}
					len_cnt+=len;
					cout<<"timeout!!\n";
					cout<<"========slow start======\n";

				}
				else{
					recv=read(false);
					if(sackoption && recv.sack.length>2){
						bool exist=false;			
						SACKblock s;
						s.left=recv.sack.blockList[0].left;
						s.right=recv.sack.blockList[0].right;
						for(int j=0;j<sackList.size();j++){
							if(sackList[j].left==s.left){
								exist=true;break;
							}
						}
						if(!exist)sackList.push_back(s);
						//cout<<"***Data loss at byte : "<<s.left<<endl;
					}
					if(prev.ackNum==recv.ackNum){
						duplicateACKcount++;
						//cout<<"dup++\n";
					}
					if(duplicateACKcount>=3){						//go to fast recovery
						dont=true;
						dup=true;
						end=false;
						duplicateACKcount=0;
						THRESHOLD=cwnd*MSS/2;
						if(THRESHOLD==0)THRESHOLD=1;
						cwnd=THRESHOLD/MSS+3;
						if(cwnd==0)cwnd=1;
						rwnd=BufferSize;
						if(len==1024){
							base-=(len*4);
							fseek(input,-4*len,SEEK_CUR);
							seqNum=base;
							ackNum-=4;
							len_cnt-=(len*4);
						}
						else{
							base-=(len+1024*3);
							fseek(input,-1*(len+3*1024),SEEK_CUR);
							seqNum=base;
							ackNum-=4;
							len_cnt-=(len+1024*3);
						}
						cout<<"Receive 3 duplicate ack!!\n";
						cout<<"======fast retransmit=====\n";
						cout<<"======fast recovery=====\n";
						state=TcpState::TcpState_FastRecover;
						break;
					}
					else{
						seqNum=base;
						ackNum++;
						if(len<MSS){end=true;break;}
						len_cnt+=len;
					}
				}
			}
			if(!end && !dont)cwnd++;
		}
		else if(state==TcpState::TcpState_FastRecover){												//fast recovery
			for(int j=0;j<cwnd;j++){
				Packet t=Packet(PacketCmd::PacketCmd_Data,*this);
				len=fread(t.data,sizeof(char),MSS,input);
				t.checksum=len;
				if(len_cnt==wanttoloss && ploss){ploss=false;cout<<"***Data loss at byte : "<<wanttoloss<<endl;}
				else if(len_cnt==wanttoloss2 && ploss2){ploss2=false;cout<<"***Data loss at byte : "<<wanttoloss2<<endl;} 					//generate loss
				else if(len_cnt==wanttoloss3 && ploss3){ploss3=false;cout<<"***Data loss at byte : "<<wanttoloss3<<endl;} 					//generate loss
				else {
					send(t);
					cout<<"\t\tSend a packet at : "<<len<<" byte\n";
					if(sackoption && sackList.size()!=0 &&t.seqNum==sackList[0].left)sackList.erase(sackList.begin());
				}
				base+=len;
				rwnd-=len;
				prev=recv;
				bool isTimeout=setTimeout(fd,500000);						//listen for 500ms
				if(isTimeout){									//timeout!! to slow start
					dont=true;
					THRESHOLD=cwnd*MSS/2;
					cwnd=1;
					duplicateACKcount=0;
					seqNum=base;
					ackNum++;
					if(len<MSS){end=true;break;}
					len_cnt+=len;
					cout<<"timeout!!\n";
					cout<<"======slow start=====\n";
					state=TcpState::TcpState_SlowStart;
					break;
					//continue;

				}
				else{
					recv=read(false);
					if(sackoption && recv.sack.length>2){
						bool exist=false;			
						SACKblock s;
						s.left=recv.sack.blockList[0].left;
						s.right=recv.sack.blockList[0].right;
						for(int j=0;j<sackList.size();j++){
							if(sackList[j].left==s.left){
								exist=true;break;
							}
						}
						if(!exist)sackList.push_back(s);
						//cout<<"***Data loss at byte : "<<s.left<<endl;
					}
					if(prev.ackNum==recv.ackNum){}
					else{									//new ack go to congestion avoid
						cwnd=THRESHOLD/MSS;
						duplicateACKcount=0;
						dont=true;
						seqNum=base;
						ackNum++;
						state=TcpState::TcpState_CongestionAvoid;
						cout<<"=====congestion avoidance======\n";
						break;
					}			
						seqNum=base;
						ackNum++;
						if(len<MSS){end=true;break;}
						len_cnt+=len;
					
				}
				
			}
			if(!end && !dont)cwnd*=2;
		}
		
		dont=false;
		if(end && !dup)break;
		dup=false;
	}
	Packet t = Packet(PacketCmd::PacketCmd_ACK, *this );
	send(t);
	fclose(input);
}
//判斷是否為數字
int Server::IsDigit(char* c){
    if(c[0] >= '0' && c[0] <= '9')
        return 1;
    else
        return 0;
}
//將資料item放入stack
void Server::push(char *item, char stack[][10], int *top){
    if (*top >= 100-1)  cout << "stackFull" << endl;
    *top = *top + 1;
    strcpy(stack[*top], item);
}
//取出stack頂端的資料
char* Server::pop(char stack[][10], int *top){
    if(*top == -1)  cout << "stackEmpty" << endl;
    int temp = *top;
    *top = *top - 1;
    return stack[temp];
}
//判斷stack是否為空
int Server::IsEmpty(int top){
    return (top < 0) ? 1 : 0;
}
//回傳頂端資料，並非取出
char* Server::top_data(char stack[][10], int top){
    return stack[top];
}
//回傳運算子c的優先順序
int Server::priority(char* a, char* b){
    char op[6] = {'(', '+', '-', '*', '/', '^'};
    int op_priority[6] = {0, 1, 1, 2, 2, 3};
    int i, a_p = -1, b_p = -1;
    for( i = 0; i < 6; i++){
        if (op[i] == a[0])
            a_p = op_priority[i];
        if (op[i] == b[0])
            b_p = op_priority[i];
    }
    if(a_p >= b_p)
        return 1;
    else
        return 0;
}
//把算式轉成後序式
void Server::to_postfix(string infix, char postfix[][10], char stack[][10], int *top){
    
    int i = 0, j = -1;
    char *y, x[10] = {};
    int number  = 0, check = 0;

    while(infix[i] != '\0'){
        x[0] = infix[i++];
        switch(x[0]){
            case '(':   push(x, stack, top);
                        check = 0;
                        break;
            case ')':   strcpy(y, pop(stack, top));
                        while(!IsEmpty(*top) && (strcmp(y, "(") != 0))
                            strcpy(postfix[++j], x);
                        check = 0;
                        break;
            case '+':
            case '-':
            case '*':
            case '/':
            case '^':   if(!IsEmpty(*top)){
                            strcpy(y, top_data(stack, *top));
                    
                            while(!IsEmpty(*top) && priority(y, x)){
                                strcpy(postfix[++j], pop(stack, top));
                                strcpy(y, top_data(stack, *top));
                            }
                        }
                        push(x, stack, top);
                        check = 0;
                        break;
            default:    //二位數以上的處理
                        if(check != 0){
                            strcat(postfix[j], x);
                        }else{
                            strcpy(postfix[++j], x);
                        }
                        check++;
        }
    }
    while(! IsEmpty(*top)){
        strcpy(postfix[++j], pop(stack, top));
    }
    postfix[++j][0] = '\0';
}
void Server::calculate(int j){
    cout << "Start to calculate the equation " << name[j] << endl;
    int i = 0, top = -1, point = 0;
    //為了可以處理二位數以上與小數點的計算 設為二維陣列
    char stack[100][10], postfix[100][10] = {};     //[Num_string][one_string_len]
    
    //將算式從infix轉成postfix
    to_postfix(name[j], postfix, stack, &top);

    //轉成postfix後 再用stack 一個一個pop出來計算
    while(postfix[point][0] != '\0'){
        while(IsDigit(postfix[point])){
            push(postfix[point++], stack, &top);
        }
        double a = atof(pop(stack, &top)), b = atof(pop(stack, &top)), c = 0;
        switch(postfix[point][0]){
            case '+':   c = b + a;
                        break;
            case '-':   c = b - a;
                        break;
            case '*':   c = b * a;
                        break;
            case '/':   c = b / a;
                        break;
            case '^':   c = pow(b, a);
                        break;
        }
        char result[10];
        gcvt(c, 7, result);
        push(result , stack, &top);
        point++;
    }
    char *result = pop(stack, &top);
    printf("[Ans] %s\n", result);
    Packet t = Packet(PacketCmd::PacketCmd_Data, *this );
    strcpy(t.data, result);
    send(t);
    
}
void Server::dns(int j){
    //接收DNS request 並轉傳給DNS server 以獲得IP位置
    cout << "Start to response DNS request " << name[j] << endl;
    struct hostent* host = gethostbyname(name[j].c_str());
    if(host == NULL)  cout << "Get Host Error!" << endl;
    
    
    //獲取第一個IP位址 回傳給client
    Packet t = Packet(PacketCmd::PacketCmd_Data, *this );
    strcpy(t.data, inet_ntoa(*(struct in_addr*)host->h_addr_list[0]));
    cout << "[IP] " << t.data << endl;
    send(t);
    
}
void Server::reset(){
	cwnd = 1;
	rwnd = BufferSize;
	THRESHOLD = D_THRESHOLD;
	duplicateACKcount = 0;
	state = TcpState::TcpState_None;
}
bool Server::setTimeout(int readFD, int msec)
{
	fd_set fdReadSet;
	struct timeval timer;	
	bool isTimeout = false;
	FD_ZERO(&fdReadSet);
	FD_SET(readFD, &fdReadSet);
	timer.tv_sec = 0;
	timer.tv_usec = msec;
	const int MaxFd = readFD + 1;
	switch(select(MaxFd, &fdReadSet, NULL, NULL, &timer) ) 
	{
		case -1:{perror("select");	break;	}
		case 0:{isTimeout = true;	break;	}
		default:{isTimeout = false;	break;	}
	}
	return isTimeout;
}

int main(int argc, char *argv[])
{
	
	Server server;
	string serverIP = "192.168.1.112";
	int serverPort  = 100;

	server.createSocket(serverIP.c_str(),serverPort);
	int tmp_p=server.srcSocket.sin_port;

    while(1){
		
        server.printInfo();
		tmp_p++;
		server.threeWayhandshake(tmp_p);
		server.reset();
		
	}
    
	return 0;
}








