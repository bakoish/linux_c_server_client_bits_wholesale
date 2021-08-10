#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string>
#include<fcntl.h>

using namespace std;

int set_arguments(int argc , char *argv[], string* addrPortOut, string* portPortOut, float* tempoOut , float* capacity,float* degradation);
int create_socket();
struct sockaddr_in register_sock(string* addrPortOut, string* portPortOut);
void set_time(struct timespec *ts);
struct timespec timespec_diff(struct timespec *start, struct timespec *stop);

int main(int argc , char *argv[]) {

    if (argc < 2) {
        printf("error: missing command line arguments\n");
        return 0;
    }

    float pace;
    float capacity;
    float degradation;
    string addr;
    string port;
    if(!set_arguments(argc,argv,&addr,&port,&pace,&capacity,&degradation)) return 0;
    int sock = create_socket();
    struct sockaddr_in server = register_sock(&addr,&port);

    struct timespec time_connected,time_reciv_1,time_disconnect,time_wait,dif;
    int storage =capacity*30720; //30kib
    int recived = 0;
    char buff[13312] = {0};
    bool first_bytes = 1;

    time_wait.tv_sec = (long)(1664/pace);
    time_wait.tv_nsec = (long)(((1664/pace) - time_wait.tv_sec)*(1e+9));


    while(1){
        if((storage - recived) < 13312) break;
        sock = create_socket();

        int proba = 11;
        while(--proba){
            if(connect(sock,(struct sockaddr *)&server,sizeof(server)) != -1 ) break;
        }
        if(proba) {
            printf("Cant connect - error\n");
            return 0;
        }
        if(first_bytes == 1) {
            set_time(&time_connected);
        }
        recived=0;
        while (recived < 1664){
            recived +=recv(sock,buff+recived,1664,0);
            if(first_bytes == 1){
                set_time(&time_reciv_1);
                first_bytes = 0;
            }
            nanosleep(&time_wait,NULL);
        }
        shutdown(sock,SHUT_RDWR);
        set_time(&time_disconnect);

        dif = timespec_diff(&time_connected,&time_disconnect);
        storage+=recived - ((dif.tv_sec+dif.tv_nsec/(1e+9))*degradation*819); //degradacja
    }

    close(sock);
    if(clock_gettime(CLOCK_REALTIME,&time_wait) <0){
        printf("Could not timespec() in end");
        exit(EXIT_FAILURE);
    }
    printf("End: TS: %ld,%ld\n",time_wait.tv_sec,time_wait.tv_nsec);
    printf("Addr: %s (port %d)\n",inet_ntoa(server.sin_addr),ntohs(server.sin_port));
    dif = timespec_diff(&time_connected,&time_reciv_1);
    printf("Delay 0->1 pack: %ld,%ld\n",dif.tv_sec,dif.tv_nsec);
    dif = timespec_diff(&time_reciv_1,&time_disconnect);
    printf("Delay 1->disc pack: %ld,%ld\n",dif.tv_sec,dif.tv_nsec);

    return 0;
}

int set_arguments(int argc , char *argv[], string* addrPortOut, string* portPortOut, float* tempoOut , float* capacity,float* degradation) {

    float tempo;
    float degradacja;
    float pojemnosc;
    char *pvalue = NULL;
    char *port = NULL;
    char *cvalue = NULL;
    char *dvalue = NULL;

    int c;
    opterr = 0;

    while ((c = getopt (argc, argv, "c:p:d:")) != -1)
        switch (c)
        {
            case 'p':
                pvalue = optarg;
                break;

            case 'c':
                cvalue = optarg;
                break;

            case 'd':
                dvalue = optarg;
                break;

            case '?':
                if (optopt == 'c')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr,
                             "Unknown option character `\\x%x'.\n",
                             optopt);
                return 1;
            default:
                abort ();
        }
    for (int index = optind; index < argc; index++) {
        port = argv[index];
        port[index+1] = NULL;
        //printf ("Non-option argument %s\n", argv[index]);
    }

    long double long_double = strtold(cvalue,NULL);
    pojemnosc = strtof(cvalue,NULL);
    if(long_double == pojemnosc){
        printf("Przekazales pojemnosc = %.2f \n",pojemnosc);
    }
    else {
        printf("Brak pojemnosci! error\n");
        return 0;
    }


    long_double = strtold(pvalue,NULL);
    tempo = strtof(pvalue,NULL);
    if(long_double == tempo){
        printf("Przekazales tempo = %.2f \n",tempo);
    }
    else {
        printf("Brak tempa! error\n");
        return 0;
    }

    long_double = strtold(dvalue,NULL);
    degradacja = strtof(dvalue,NULL);
    if(long_double == degradacja){
        printf("Przekazales degradacje = %.2f \n",degradacja);
    }
    else {
        printf("Brak degradacji! error\n");
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

    if(goodAddrPort == 1){
        printf ("Przekazales <addr> jako: %s , port: %s \n", addrPort.c_str(),portPort.c_str());
    }
    else {
        printf ("Brak <addr> - ustawiam localhost: 127.0.0.1 port 8888\n");
        addrPort = "127.0.0.1";
        portPort = "8888";
    }

    *addrPortOut = addrPort;
    *portPortOut = portPort;
    *tempoOut = tempo;
    *capacity = pojemnosc;
    *degradation = degradacja;
    return 1;
}
int create_socket() {

    int sock_fd = socket(AF_INET,SOCK_STREAM,0);
    if( sock_fd == -1 ) {
        printf("Could not create socket");
        return 0;
    }

    int enable = 1;
    if(setsockopt(sock_fd,SOL_SOCKET,SO_REUSEADDR,&enable,sizeof(int)) <0){
        printf("Could not setsockopt");
        return 0;
    }

    return  sock_fd;

}
struct sockaddr_in register_sock(string* addrPortOut, string* portPortOut){

    struct sockaddr_in server;
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_port = htons( atoi(addrPortOut->c_str()) ); //8888

    const char * Host = portPortOut->c_str();   //"127.0.0.1";
    int R = inet_aton(Host,&server.sin_addr);
    if( ! R ) {
        fprintf(stderr,"niepoprawny adres: %s\n",Host);
        exit(0);
    }
    return server;

}
void set_time(struct timespec *ts){
    if(clock_gettime(CLOCK_MONOTONIC,ts) <0){
        printf("Could not timespec() in set_time");
        exit(EXIT_FAILURE);
    }
}
struct timespec timespec_diff(struct timespec *start, struct timespec *stop)
{
    struct timespec result;
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result.tv_sec = stop->tv_sec - start->tv_sec - 1;
        result.tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result.tv_sec = stop->tv_sec - start->tv_sec;
        result.tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
    return result;
}