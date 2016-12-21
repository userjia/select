//
// Created by jp on 12/15/16.
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/param.h>
#include <pthread.h>

#define MAXBUF 1024
#define CLIENTNUM 10
int fd[CLIENTNUM];//for accept client
int sockfd;

void error(char *e){
    syslog(LOG_ERR,"%s",e);
}

void *acceptClient() {
    struct sockaddr_in their_addr;

    //initialize the fd array
    for (int j = 0; j < CLIENTNUM; ++j) {
        fd[j]=-2;
    }

    while (1) {
        printf("\n----wait for new connect\n");
        socklen_t len = sizeof(struct sockaddr);
        int maxfd = 0;
        for (int i = 0; i < CLIENTNUM; ++i) {
            if ((fd[i] = accept(sockfd, (struct sockaddr *) &their_addr, &len)) == -1) {
                error("accept");
                exit(errno);
            } else {
                //log print data
                syslog(LOG_INFO, "server: got %dth connection from %s, port %d, socket %d\n", i,
                       inet_ntoa(their_addr.sin_addr), ntohs(their_addr.sin_port), fd[i]);

                if (fd[i] > maxfd) {
                    maxfd = fd[i];
                }
            }
        }
        //break;
    }

}

int main(int argc, char **argv)
{
    //////////////////init socket
    int new_fd;
    socklen_t len;
    struct sockaddr_in my_addr, their_addr;
    unsigned int myport, lisnum;
    char buf[MAXBUF + 1];

    fd_set rfds;//select set
    struct timeval tv;//for time out
    int retval, maxfd = -1;

    if (argv[2])//set listening port
        myport = atoi(argv[2]);
    else
        myport = 7838;
    if (argv[3])//set listening number
        lisnum = atoi(argv[3]);
    else
        lisnum = 2;

    ///////////init daemon
    int pid;

    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    signal(SIGTSTP,SIG_IGN);
    signal(SIGHUP,SIG_IGN);

    if(pid=fork()) {
        exit(EXIT_SUCCESS);
    } else if(pid<0){
        error("fork");
        exit(EXIT_FAILURE);
    }

    setsid();
    if(pid=fork()) {
        exit(EXIT_SUCCESS);
    } else if(pid<0){
        error("fork");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NOFILE; ++i) {
        close(i);
    }

    open("/dev/null",O_RDONLY);
    open("/dev/null",O_RDWR);

    chdir("/temp");

    umask(0);

    signal(SIGCHLD,SIG_IGN);

    openlog(argv[0],LOG_PID,LOG_LOCAL0);// daemon print in the log file

    ///////////socket create, bind, listen
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        error("socket");
        exit(EXIT_FAILURE);
    }
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = PF_INET;
    my_addr.sin_port = htons(myport);
    if (argv[1])
        my_addr.sin_addr.s_addr = inet_addr(argv[1]);
    else
        my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1) {
        error("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, lisnum) == -1) {
        error("listen");
        exit(EXIT_FAILURE);
    }

    ///////////////////thread for accepting multipul clients
    pthread_t thread_id;
    pthread_create(&thread_id, NULL,(void *)acceptClient,NULL);
    //acceptClient();
    //////////////////select
    while (1){
        FD_ZERO(&rfds); //clear select poor
        int fd_id;

        //add fd to select poor, some fd will be removed after select
        for (int i = 0; i < CLIENTNUM; ++i) {
            if (fd[i]!=-2){
                FD_SET(fd[i], &rfds);
                if (fd[i]>maxfd){
                    maxfd=fd[i];
                }
            } else{
                continue;
            }
        }

        len=0;
        tv.tv_sec = 3;//select timeout set
        tv.tv_usec = 0;

        retval = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (retval == -1)
        {
            error("select");
            exit(EXIT_FAILURE);
        } else if (retval == 0) {
            printf("%s","Time out..\n");
            continue;
        }
        else
        {
            for (int i = 0; i < CLIENTNUM; ++i) {
                if (fd[i]!=-2){
                    if (FD_ISSET(fd[i], &rfds)){
                        new_fd=fd[i];
                        fd_id=i;
                    }
                    else{
                        continue;
                    }
                } else{
                    continue;
                }
            }

            if(FD_ISSET(new_fd,&rfds))
            {
                bzero(buf, MAXBUF + 1);
                len = recv(new_fd, buf, MAXBUF, 0);
                if (len > 0) {
                    //log print data
                    syslog(LOG_INFO, "recv success :'%s',%dbyte recv\n", buf, len);
                }
                else
                {
                    if (len < 0)
                        error("recv failure\n");
                    else
                    {
                        syslog(LOG_INFO,"One client quit\n");
                        close(new_fd);
                        fd[fd_id]=-2;
                        continue;
                    }
                }
            }
        }
    }


}


