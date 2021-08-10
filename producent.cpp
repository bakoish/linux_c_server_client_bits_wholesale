#define _GNU_SOURCE
#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string>
#include<sys/ioctl.h>
#include <signal.h>
#include <poll.h>
#include <list>
#include <iostream>
#include <sys/timerfd.h>
#include <fcntl.h>
#define MAX_CLIENTS 1024

using namespace std;

typedef struct Client {
    int fd;
    struct sockaddr_in addr;
}Client;

typedef struct Base{
    struct pollfd fds[MAX_CLIENTS];
    struct sockaddr_in clients_addr[MAX_CLIENTS];
    int clients_bites_send[MAX_CLIENTS];
    list<Client>allclients;
}Base;


int set_arguments(int argc , char *argv[], string* addrPortOut, string* portPortOut, float* tempoOut);
void production_bits(int fd,float * tempo);
void distribution_bits(int fd,string * addrIN,string * portIN);
Base * base_init();
int create_socket(Base * base,string * addrIN,string * portIN);
int fd_timer_init(Base * base);
int accept_clients(Base * base,int * server);
int client_disconected(Base * base,int * reserved, int i,int * n_conected);
int cleanup_base(Base * base, int * nfds);
int add_new_clients(Base * base,int bytes_avalible,int reserved,int nfds, int * n_connected);

//reports
void report_disconnect(struct sockaddr_in client,int * reserved);
void report_5sec(int fd,int n_connected, int * bytes_last_ioctl);

int main(int argc , char *argv[]) {

    if (argc < 2) {
        printf("error: missing command line arguments\n");
        return 1;
    }

    string addr;
    string port;
    float tempo = 0;

    if(!set_arguments(argc,argv,&addr,&port,&tempo)) return 0;

    int fd[2];
    int pipe_result=pipe(fd);
    if(pipe_result!=0) {
        printf("pipe fail\n");
        return 0;
    }
    pid_t pid = fork();
    if(pid==-1) {
        printf("fork fail\n");
        exit(EXIT_FAILURE);
    }
    if(pid==0) { //POTOMEK - DYSTRYBUJCA PAKIETOW
        distribution_bits(fd[0],&addr,&port);
    }
    else { //RODZIC - PRODUKCJA PAKIETOW
        printf("-START PROGRAMU-\n\n");
        production_bits(fd[1],&tempo);
    }

    return 1;
}

int set_arguments(int argc , char *argv[], string* addrPortOut, string* portPortOut, float* tempoOut){

    float tempo;
    char *pvalue = NULL;
    char *port = NULL;
    int c;

    opterr = 0;

    while ((c = getopt(argc, argv, "p:c:")) != -1) {
        switch (c) {
            case 'p':
                pvalue = optarg;
                break;

            case '?':
                if (optopt == 'c')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                return 0;
            default:
                abort();
        }
    }

    for (int index = optind; index < argc; index++) {
        port = argv[index];
        port[index + 1] = NULL;
        //printf ("Non-option argument %s\n", argv[index]);
    }


    long double long_double = strtold(pvalue, NULL);
    tempo = strtof(pvalue, NULL);
    if (long_double == tempo) {
        printf("Przekazales tempo = %.2f \n", tempo);
    } else {
        printf("Brak tempa! error\n");
        return 0;
    }

    string addrPort;
    string portPort;
    bool goodAddrPort = 0;
    int i = 0;
    if(port != NULL) {
        while (port[i] != NULL) {
            if (port[i++] == '[') {
                while (port[i] != ':' && port[i] != NULL) {
                    addrPort += port[i];
                    i++;
                }
                if (port[i] == NULL) break;
                else if (port[++i] == ']') {
                    i++;
                    while (port[i] != NULL) {
                        portPort += port[i];
                        i++;
                    }
                    goodAddrPort = 1;
                    break;
                }
            }
            i++;
        }
    }
    if (goodAddrPort == 1) {
        printf("Przekazales <addr> jako: %s , port: %s \n", addrPort.c_str(), portPort.c_str());
    } else {
        printf("Brak <addr> - ustawiam localhost: 127.0.0.1 port 8888\n");
        addrPort = "127.0.0.1";
        portPort = "8888";
    }

    *addrPortOut = addrPort;
    *portPortOut = portPort;
    *tempoOut = tempo;

    return 1;

};
void production_bits(int fd,float * tempo){
    char x = 65; //A
    char block[640] = {0};

    struct timespec sleep;
    sleep.tv_sec = (long)(*tempo);
    sleep.tv_nsec = (long)((*tempo - sleep.tv_sec)*(1e+9));

    while(1){
        if(x==91) x=97; //91 97
        if(x==123) x=65; //123

        memset(block,x,sizeof(block));
        write(fd,block,sizeof(block));
        nanosleep(&sleep,NULL);
        x++;

    }
}
void distribution_bits(int fd,string * addrIN,string * portIN){

    Base* base = base_init();
    int server = create_socket(base,addrIN,portIN);
    if(!server){
        printf("Could not create_socket()");
        exit(EXIT_FAILURE);
    }
    int n_clients = 0;
    int n_conected = 0;
    int nfds = 2;
    int reserved = 0;
    bool sb_disconected = 0;
    int bytes_send = 0;
    int bytes_avalible = 0;
    int bytes_last_ioctl = 0;
    int fdtimer = fd_timer_init(base);
    char buff[1664]={0};

    while(1){
     ioctl(fd,FIONREAD,&bytes_avalible);
     int status = poll(base->fds,nfds,500);
     if(status == -1){
         printf("Could not poll()");
         exit(EXIT_FAILURE);
     }
     if(status > 0){
         n_clients = nfds;
         for(int i=0;i<n_clients;i++){
             if(base->fds[i].revents != 0) { //readable
                 if((base->fds[i].revents & POLLIN) && base->fds[i].fd == fdtimer){ //budzik
                     read(fdtimer,NULL,8); // zerowanie
                     report_5sec(fd,n_conected,&bytes_last_ioctl);
                 }
                 if(base->fds[i].fd == server){
                    n_conected = accept_clients(base,&server);
                 }
                 if(base->fds[i].revents & POLLOUT){
                     if(base->clients_bites_send[i] == 0){
                        reserved = 13312; //13kb
                     }
                     read(fd,buff,sizeof(buff));
                     if(recv(base->fds[i].fd,NULL,1,MSG_PEEK|MSG_DONTWAIT) == 0){
                         sb_disconected = client_disconected(base,&reserved,i,&n_conected);
                     }
                     else {
                       bytes_send = send(base->fds[i].fd,buff,sizeof(buff),MSG_NOSIGNAL);
                       if(bytes_send >= 0){
                           base->clients_bites_send[i] += bytes_send;
                           reserved -= bytes_send;
                       }
                       else {
                           sb_disconected = client_disconected(base,&reserved,i,&n_conected);
                       }

                     }

                 }
                 if(base->clients_bites_send[i] > 13311) {
                     sb_disconected = client_disconected(base,&reserved,i,&n_conected);
                 }

             }

         }
         if(sb_disconected){
             sb_disconected = cleanup_base(base,&nfds);
         }

     }
     nfds = add_new_clients(base,bytes_avalible,reserved,nfds,&n_conected);
    }
}
Base * base_init(){
Base *x = new Base;
memset(x->clients_addr,0,sizeof(struct sockaddr_in)*MAX_CLIENTS);
memset(x->fds,0,sizeof(struct pollfd)*MAX_CLIENTS);
memset(x->clients_bites_send,0,sizeof(int)*MAX_CLIENTS);
return x;
};
int create_socket(Base * base,string * addrIN,string * portIN){

    int socket_desc;
    socket_desc = socket(AF_INET , SOCK_STREAM | SOCK_NONBLOCK , 0); //nonblock
    if (socket_desc == -1)
    {
        printf("Could not create socket");
        return 0;
    }
    int enable = 1;
    if(setsockopt(socket_desc,SOL_SOCKET,SO_REUSEADDR,&enable,sizeof(int)) <0){
        printf("Could not setsockopt");
        return 0;
    }

    struct sockaddr_in server;
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_port = htons( atoi(portIN->c_str()) ); //8888

    const char * Host = addrIN->c_str();   //"127.0.0.1";
    int R = inet_aton(Host,&server.sin_addr);
    if( ! R ) {
        fprintf(stderr,"niepoprawny adres: %s\n",Host);
        exit(0);
    }
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) )
    {
        puts("bind failed");
        return 0;
    }
    //puts("bind done");
    if(listen(socket_desc , 100)){
        puts("listen failed");
        return 0;
    }
    base->fds[0].fd = socket_desc;
    base->fds[0].events = POLLIN;
    return socket_desc;
}
int fd_timer_init(Base * base){
    int x = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
    if(x == -1){
        printf("Could not timerfd_create()");
        return 0;
    }
    struct itimerspec ts;
    ts.it_value.tv_sec = 5;
    ts.it_value.tv_nsec = 0;
    ts.it_interval.tv_sec = 5;
    ts.it_interval.tv_nsec = 0;
    timerfd_settime(x,0,&ts,NULL);
    base->fds[1].fd = x;
    base->fds[1].events = POLLIN;
    return x;
}
int accept_clients(Base * base,int * server){
    int connected = 0;
    int c = sizeof(struct sockaddr_in);
    struct sockaddr_in client_addr;
    int client_fd;
    while((client_fd = accept(*server, (struct sockaddr *)&client_addr, (socklen_t*)&c)) != -1){

        auto * x = new Client;
        x->fd = client_fd;
        x->addr = client_addr;
        base->allclients.push_back(*x);
        //base->vector_size = base->allclients.size();
        connected++;
    }
    return connected;
}
int client_disconected(Base * base,int * reserved, int i,int * n_conected){
    shutdown(base->fds[i].fd,SHUT_RDWR);
    close(base->fds[i].fd);
    base->fds[i].fd = -1;

    (*reserved) -= base->clients_bites_send[i];
    (*n_conected)--;
    report_disconnect(base->clients_addr[i],reserved);
    return 1;
}
void report_disconnect(struct sockaddr_in client,int * reserved){
    struct timespec ts;
    if(clock_gettime(CLOCK_REALTIME,&ts) <0){
        printf("Could not timespec() in disconnect");
        exit(EXIT_FAILURE);
    }
    printf("SB disconnected: TS: %ld,%ld\n",ts.tv_sec,ts.tv_nsec);
    printf("Addr: %s (port %d)\n",inet_ntoa(client.sin_addr),ntohs(client.sin_port));
    printf("Wasted bytes: %d\n\n",(*reserved));
}
void report_5sec(int fd,int n_connected, int * bytes_last_ioctl){
    struct timespec ts;
    if(clock_gettime(CLOCK_REALTIME,&ts) <0){
        printf("Could not timespec() in 5sec");
        exit(EXIT_FAILURE);
    }
    printf("Report(5s): TS: %ld,%ld\n",ts.tv_sec,ts.tv_nsec);
    printf("Connected: %d clients\n",n_connected);
    int bytes_in = 0;
    ioctl(fd,FIONREAD,&bytes_in);
    float procent = (bytes_in*100/(fcntl(fd,F_GETPIPE_SZ)));
    printf("We have: %d bytes in (%.2f)%%\n",bytes_in,procent);
    printf("Material flow: %d\n\n",bytes_in-(*bytes_last_ioctl));
    *bytes_last_ioctl = bytes_in;

}
int cleanup_base(Base * base, int * nfds){
    for(int i=0;i<*nfds;i++){
        if(base->fds[i].fd == -1){//przesuniecie na miejsce usunietego
            for(int j=i;j<*nfds;j++){
                base->fds[j].fd = base->fds[j+1].fd;
                base->clients_addr[j] = base->clients_addr[j+1];
                base->clients_bites_send[j] = base->clients_bites_send[j+1];
            }//usuniecie jednego klienta
            i--;
            *nfds--;
        }

    }
    return 0;

}
int add_new_clients(Base * base,int bytes_avalible,int reserved,int nfds, int * n_connected){
    int n_can_add = bytes_avalible - reserved / 13312 ;

    for(int i=0;i<n_can_add;i++) {
        if(nfds > MAX_CLIENTS) break;

        if (base->allclients.size() > 0) {
            auto x = base->allclients.front();
            base->allclients.pop_front();

            if(recv(x.fd,NULL,1,MSG_PEEK|MSG_DONTWAIT) == 0){
                report_disconnect(x.addr,0);
                continue;
            }
            base->clients_addr[nfds] = x.addr;
            base->fds[nfds].fd = x.fd;
            base->fds[nfds].events = POLLOUT;
            nfds++;
        }
        else break;
    }
    *n_connected = base->allclients.size() + nfds - 2;
    return nfds;
}