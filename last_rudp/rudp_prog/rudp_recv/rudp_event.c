#ifdef HAVE_CONFIG_H
#include "config.h" /* generated by config & autoconf */
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <assert.h>

#include "rudp_event.h"

struct event_data{
    int (*event_func)(int,void*); /*callback_function*/
    enum {EVENT_FD,EVENT_TIME} event_type;/*type of event*/
    int event_fd;/*file descriptor*/
    struct timeval event_time; /*time struct*/
    void *event_arg; /*function_arument*/
    char event_string[32]; /*string argument*/
    struct event_data *next; 
};

static struct event_data *evente = NULL;
static struct event_data *evente_timers = NULL;

int event_timeout(struct timeval t,int (*func)(int,void*),void *arg,char *str){
    struct event_data *event,*event1,**event_prev;

    event = malloc(sizeof(struct event_data));
    if(event == NULL){
        fprintf(stderr,"event_timeout: Error memory allocating");
        return -1;
    }
    memset(event,0,sizeof(struct event_data));
    strcpy(event->event_string,str);
    event->event_func = func;
    event->event_type = EVENT_TIME;
    event->event_arg = arg;
    event->event_time = t;

    event_prev = &evente_timers;
    for(event1=evente_timers;event1;event1=event1->next){
        if(timercmp(&event->event_time,&event1->event_time,<)){
            break;
        }
        event_prev = &event1->next;
    }
    event->next = event1;
    *event_prev = event;
    return 0;
}

static int event_delete(struct event_data **ev_dt,int (*func)(int,void*),void *arg){
    struct event_data *event,**event_prev;

    event_prev = ev_dt;
    for(event = *ev_dt;event;event=event->next){
        if(func == event->event_func && arg == event->event_arg){
            *event_prev = event->next;
            free(event);
            return 0;
        }
        event_prev = &event->next;
    }
    return -1;
}

int event_timeout_delete(int (*fn)(int,void*),void *arg){
    return event_delete(&evente_timers,fn,arg);
}

int event_fd_delete(int (*fn)(int,void*),void *arg){
    return event_delete(&evente,fn,arg);
}

int event_fd(int fd,int (*func)(int,void*),void *arg,char *str){
    struct event_data *event;

    event =(struct event_data*)malloc(sizeof(struct event_data));
    if(event==NULL){
        fprintf(stderr,"event_fd: Error allocating Memory");
        return -1;
    }
    memcpy(event,"0",sizeof(struct event_data));
    strcpy(event->event_string,str);
    event->event_fd = fd;
    event->event_func = func;
    event->event_arg = arg;
    event->event_type = EVENT_FD;
    event->next = evente;
    evente = event;

    return 0;
}

int eventloop(){
    struct event_data *event,*event1;
    fd_set fdset;
    int n;
    struct timeval t,t0;

    while(evente || evente_timers){
        FD_ZERO(&fdset);
        for(event = evente;event;event=event->next){
            if(event->event_type == EVENT_FD){
                FD_SET(event->event_fd,&fdset);
            }
        }

        if(evente_timers){
            gettimeofday(&t0,NULL);
            timersub(&evente_timers->event_time,&t0,&t);
            if(t.tv_sec < 0){
                n = 0;
            }
            else{
                n = select(FD_SETSIZE,&fdset,NULL,NULL,&t);
            }
        }
        else{
            n = select(FD_SETSIZE,&fdset,NULL,NULL,NULL);
        }

        if(n == -1){
            if(errno != EINTR){
                perror("eventloop: select");
            }
        }
        if(n == 0){
            event = evente_timers;
            evente_timers = evente_timers->next;
            #ifdef DEBIG
            fprintf(stderr,"eventloop: timeout : %s[arg: %x]\n",event->event_string,(int)event->event_arg);
            #endif
            /*timeout _callbackをここで起こす*/
            if((*event->event_func)(0,event->event_arg)<0){
                return -1;
            }
            switch(event->event_type){
                case EVENT_TIME:
                free(event);
                break;

                default:
                fprintf(stderr,"eventloop: illegal e_type:%d\n",event->event_type);
            }
            continue;
        }
        event = evente;
        while(event){
            event1 = event->next;
            if(event->event_type == EVENT_FD && FD_ISSET(event->event_fd,&fdset)){
                #ifdef DEBUG
                    fprintf(stderr,"eventloop: socket rcv: %s[fd: %d arg: %x]\n",
                    event->event_string,event->event_fd,(int)event->event_arg);
                #endif
                if((*event->event_func)(event->event_fd,event->event_arg)<0){
                    return -1;
                }
            }
            event = event1;
        }
    }
    #ifdef DEBUG
        fprintf(stderr,"eventloop: returning 0\n");
    #endif
    return 0;
}
