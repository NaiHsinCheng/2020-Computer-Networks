#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <random>

#define D_RTT                15
#define D_THRESHOLD            65536
#define D_MSS                1024
#define D_BUFFER_SIZE            524288

using namespace std;
int num;            //紀錄request個數
string flag[10];    //記錄request
string name[10];    //紀錄request的參數
int indexx = 0;

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

class Client;
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
        Packet(PacketCmd command, Client client, const char *);
        PacketCmd parser();
        string getName();
};
class Client{
    public:
        int MSS;
        int RTT;
        int THRESHOLD;
        int BufferSize;

        int fd;
        struct sockaddr_in srcSocket;
        struct sockaddr_in destSocket;
        int seqNum;
        int ackNum;
        int rwnd;

        char fileBuffer[D_BUFFER_SIZE];
        bool delay;
        vector<SACKblock> sackList;
        bool sackoption=false;
        Client(){
            MSS = D_MSS;
            RTT = D_RTT;
            THRESHOLD = D_THRESHOLD;
            BufferSize=D_BUFFER_SIZE;
            memset(fileBuffer, 0, BufferSize);
        }
        void createSocket(const char *srcIP, int srcPort);
        void printInfo();
        void threeWayhandshake();
        void connect(const char*,int);
        void send(Packet packet,bool printornot=true);
        Packet read(bool printornot=true);
        void updateNumber(const Packet packet);
        bool setTimeout(int readFD, int msec);
        void recvResponse();

};

Packet::Packet(PacketCmd command, Client client, const char *dat = NULL){
    flagACK = false;
    flagSYN = false;
    flagFIN = false;
    checksum = 0;
    memset(data, 0, sizeof(data));
    srcPort = client.srcSocket.sin_port;
    destPort = client.destSocket.sin_port;
    seqNum = client.seqNum;
    ackNum = client.ackNum;
    rwnd = client.rwnd;
    switch(command){
        case PacketCmd_ACK:
            flagACK = true;
            if(dat != NULL){
                int dataSize = ((int)strlen(dat) > client.MSS)? client.MSS : strlen(dat);
                strncpy(this->data, dat, dataSize);
                checksum = dataSize;
            }
            else
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
            if(dat != NULL){
                int dataSize = ((int)strlen(dat) > client.MSS)? client.MSS : strlen(dat);
                strncpy(this->data, dat, dataSize);
                checksum = dataSize;
            }
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
        case PacketCmd_ACK:return "packet(ACK)";
        case PacketCmd_SYN:return "packet(SYN)";
        case PacketCmd_SYNACK:return "packet(SYNACK)";
        case PacketCmd_FIN:return "packet(FIN)";
    }
    return "packet";
}
void Client::createSocket(const char *srcIP, int srcPort){
    fd = socket(AF_INET,SOCK_DGRAM,0);
    if (fd < 0) {perror("socket error\n");}
    srcSocket.sin_family = AF_INET;
    srcSocket.sin_port = srcPort;
    srcSocket.sin_addr.s_addr = inet_addr(srcIP);
    if (bind(fd, (struct sockaddr *)&srcSocket, sizeof(srcSocket)) < 0){perror("bind error\n");}
}
void Client::printInfo(){
    cout<<"\n==============================\n";
    cout<<"Client's ip is "<<inet_ntoa(srcSocket.sin_addr)<<endl;
    cout<<"Client is listening on port "<<srcSocket.sin_port<<endl;
    cout<<"==============================\n";
}
void Client::threeWayhandshake(){
    cout<<"\n=====start the three-way handshake=====\n";
    bool end = false;
    seqNum = rand()%10001;
    Packet t = Packet(PacketCmd::PacketCmd_SYN, *this);
    t.sack.kind = 4;
    send(t);
    while(!end){
        Packet recv = read();
        destSocket.sin_port = recv.srcPort;
        switch(recv.parser()){
            case PacketCmd_SYNACK:{
                updateNumber(recv);
                if(recv.sack.kind == 4)   sackoption = true;
                string argument = to_string(num);
                for(int i = 0; i < num; i++){
                    argument += ":" + flag[i] + ":" + name[i];
                }
                send(Packet(PacketCmd::PacketCmd_ACK, *this, argument.c_str()));
                end = true;
                break;
            }
        }
    }
    cout<<"=====complete the three-way handshake=====\n";
}
void Client::connect(const char* destIP,int destPort){
    memset((char*) &destSocket, 0, sizeof(destSocket) );//初始化
    destSocket.sin_family = AF_INET;
    destSocket.sin_addr.s_addr = inet_addr(destIP);
    destSocket.sin_port = destPort;
}
void Client::send(Packet packet,bool printornot){
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
        //case PacketCmd_Data:break;
    }
    
}
Packet Client::read(bool printornot){
    Packet pkg;
    socklen_t pkg_l = sizeof(destSocket);

    recvfrom(fd, &pkg, sizeof(Packet), 0, (struct sockaddr*)&destSocket, &pkg_l);
    switch(pkg.parser()){
        case PacketCmd_SYN:
        case PacketCmd_SYNACK:
        case PacketCmd_FIN:
        {
            if(printornot)
                cout<<"Received a " << pkg.getName() << " from " << inet_ntoa(destSocket.sin_addr)<<":"<<destSocket.sin_port<<endl;
            break;
        }
        case PacketCmd_ACK:{
            cout<<"Received a " << pkg.getName() << " from " << inet_ntoa(destSocket.sin_addr)<<":"<<destSocket.sin_port<<endl;
            break;
        }
        case PacketCmd_Data:{
            cout<<"\t\t" << "Receive a packet (" << "seq_num = " << pkg.seqNum << ", " << "ack_num = " << pkg.ackNum << ")"<<endl;
            break;
        }
    }
    return pkg;
}
void Client::updateNumber(const Packet packet){
    if(seqNum>0) seqNum=packet.ackNum;
    else seqNum=rand()%10001;
    ackNum = packet.seqNum + packet.checksum;
}

void Client::recvResponse(){
    if(flag[indexx] == "-f"){
        cout << "Receive a file from " << inet_ntoa(destSocket.sin_addr) << endl;
        string new_name = name[indexx] + ".mp4";
        FILE *file=fopen(new_name.c_str(),"w");
        bool end=false,first=true;
        int cnt=0;
        Packet recv,prev;

        while(!end){
            bool isTimeout=setTimeout(fd,500000);//listen for 500ms
            if(isTimeout){
                    Packet t=Packet(PacketCmd::PacketCmd_ACK,*this);
                    send(t,false);
                    continue;
            }
            //usleep(100000);
            prev=recv;
            recv=read(false);
            if(recv.seqNum!=ackNum && !first){
                if(sackoption && prev.seqNum+1024 !=recv.seqNum){
                cnt++;
                SACKblock s;
                s.left=prev.seqNum+1024;s.right=recv.seqNum-1;
                sackList.push_back(s);        //client record loss packet
                Packet t=Packet(PacketCmd::PacketCmd_ACK,*this);
                t.sack.kind=5;        //SACK set
                t.sack.blockList[0].left=prev.seqNum+1024;
                t.sack.blockList[0].right=recv.seqNum-1;
                t.sack.length+=sizeof(SACKblock);
                if(cnt<4)send(t,false);
                continue;
                }
                cnt++;
                Packet t = Packet(PacketCmd::PacketCmd_ACK,*this);
                if(cnt<4)send(t,false);
                continue;
            }
            if(sackoption && sackList.size()!=0 && sackList[0].left==recv.seqNum)
                sackList.erase(sackList.begin());
            cnt=0;
            first=false;
            seqNum++;
            ackNum=recv.seqNum+recv.checksum;
            switch(recv.parser()){
                case PacketCmd_Data:{
                    fwrite(recv.data,sizeof(char),recv.checksum,file);
                    Packet t = Packet(PacketCmd::PacketCmd_ACK,*this);
                    send(t,false);
                    //cout<<"here?\n";
                    break;
                }
                case PacketCmd_ACK:{
                    end=true;
                    break;
                }
            }
            
        }
        fclose(file);
        
    }else if(flag[indexx] == "-c"){
        
        cout << "Receive a answer from " << inet_ntoa(destSocket.sin_addr) << endl;
        Packet recv = read();
        cout << "The answer is " << recv.data << endl;
        
    }else if(flag[indexx] == "-d"){
        
        cout << "Receive a dns response from " << inet_ntoa(destSocket.sin_addr) << endl;
        Packet recv = read();
        cout << "The IP of " << name[indexx] << " is " << recv.data << endl;
        
    }else{
        cout << "Flag error."  << endl;
        return;
    }
    
}
bool Client::setTimeout(int readFD, int msec)
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
        case -1:{perror("select");    break;    }
        case 0:{isTimeout = true;    break;    }
        default:{isTimeout = false;    break;    }
    }
    return isTimeout;
}
int main(int argc, char *argv[])
{
    srand(time(NULL));
    
    //argv[1]: flag
    //argv[2]: argument
    //argv[3]: flag
    //argv[4]: argument
    //argv[5]: ....

    //確認參數數量大於二個
    if(argc < 3)  cout << "At least two argurment." << endl;
    if(argc % 2 == 0)  cout << "Number of argurments is error." << endl;
    
    num = argc / 2;
    for(int i = 0;i < num; i++){
        flag[i] = argv[1 + 2*i];
        name[i] = argv[2 + 2*i];
    }

    Client client;
    

    string clientIP = "192.168.0.1";
    int clientPort = 400;
    //clientPort = atoi(argv[1]);
    client.createSocket(clientIP.c_str(),clientPort);

    while(1)
    {
        client.printInfo();
        
        string serverIP;
        int serverPort;
        cout << "Please Input Node [IP] [Port] you want to connect to: \n";
        cin >> serverIP >> serverPort;
        client.connect(serverIP.c_str(),serverPort);
        
        client.threeWayhandshake();
        for(indexx = 0;indexx < num; indexx++)
            client.recvResponse();
        client.destSocket.sin_port=serverPort;

    }
    return 0;

}
















