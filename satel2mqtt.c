/*
    Satel CA10 alarm events to MQTT messages parser.
    
    Copyright (c) 2020 Wojciech Sawasciuk <voyo@conserit.pl>
    Distributed under the MIT license.

    compile as:  gcc -o satel2mqtt satel2mqtt.c -lmosquitto
*/

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <getopt.h> 
#include <sys/types.h>
#include <regex.h>
#include <mosquitto.h>

char mqtt_host[200]  = "localhost";
int  mqtt_port = 1883;
char topic[200] = "domoticz/in";
char device[200] = "/dev/ttyUSB0";
char DEBUG = 0;

/* here for domoticz, but can be easily altered for any other system */
#define MSG_TEMPLATE  "{\"idx\":%d, \"nvalue\":%d, \"svalue\":\"%d\"}"

struct mosquitto *mosq = NULL;

int set_interface_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag |= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    cfsetspeed (&tty, (speed_t)speed);

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

char action (char *line)
{
 if (strcmp(line, "NARUSZENIE CZUJNIKA . . . . . . . ") == 0) 
 {
   return(1);
   } 
   else if (strcmp(line, "KONIEC NARUSZENIA CZUJNIKA  . . . ") == 0)
   {
   return(0);
   }
   else if (strcmp(line, "!!!     ALARM Z CZUJNIKA      !!! ") == 0)
   {
   return(1);
   }
   else return(3);
}


/* dictionary to map Satel inputs to Domoticz idx
  can be implemented as XML maybe ?  */
int idx(uint s)
{
switch (s) {
 case 1: return(55); // PIR przedpokoj
 case 2: return(54); // PIR gabinet
 case 3: return(49); // PIR pokoj dzieci
 case 4: return(51); // okno lazienka, pokoj dzieci
 case 5: return(48); // PIR lazienka
 case 6: return(46); // PIR salon
 case 7: return(50); // okno salon, balkon
 case 8: return(0); //
 case 9: return(47); // dzwi wejsciowe
 case 10: return(0); //
 }
}

int parse_line(char *line) 
{
 regex_t    preg;                                                            
// static const char       *pattern = "((0?[1-9]|[12][0-9]|3[01])\\/(0?[1-9]|[1[012])\\/(\\d{4}))(\\s{3})(((0[0-9]|1[0-9]|2[0-3]|[0-9]):([0-5][0-9])))(\\s-\\s)(.*)(WE.[0-9][0-9])$";
 static const char *pattern = "(\\s-\\s)(.*)(WE.[0-9][0-9])$";
 int        rc;  
 regmatch_t pmatch[1];
 size_t maxGroups = 4;
 regmatch_t groupArray[maxGroups];
 char       buffer[100];
 char 	group[maxGroups][100]; 
 char 	msg[100];
 
/* Compile regular expression */
 if (rc = regcomp(&preg, pattern, REG_EXTENDED))
         {
          regerror(rc, &preg, buffer, 100);
          printf("regcomp() failed with '%s'\n", buffer);
          exit(1);
         }
 
 if (regexec(&preg, line, maxGroups, groupArray, 0) == 0) 
    {
      unsigned int g = 0;

      for (g = 0; g < maxGroups; g++)
        {
          if (groupArray[g].rm_so == (size_t)-1)
            break;  // No more groups
          char sourceCopy[strlen(line) + 1];
          strcpy(sourceCopy, line);
          sourceCopy[groupArray[g].rm_eo] = 0;
          strcpy((char *) &group[g],sourceCopy + groupArray[g].rm_so);
        }
    }
    
 regfree(&preg);

 char input = (atoi(&group[3][2])*10) + atoi(&group[3][3]) ; 
  
 sprintf(msg, MSG_TEMPLATE,idx(input),action(group[2]),action(group[2])); 
 if (DEBUG)
        printf("publishing MQTT host: %s,  topic: %s , msg: %s\n",mqtt_host,topic,msg); 

  return mosquitto_publish(mosq, NULL, topic, strlen(msg), msg, 0, 0);
}

void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str) {
	/* Print all log messages regardless of level. */
  switch(level){
    //case MOSQ_LOG_DEBUG:
    //case MOSQ_LOG_INFO:
    //case MOSQ_LOG_NOTICE:
    case MOSQ_LOG_WARNING:
    case MOSQ_LOG_ERR: {
      printf("%i:%s\n", level, str);
    }
  }	
}

void mqtt_setup() {
  int keepalive = 60;
  bool clean_session = true;
  
  mosquitto_lib_init();
  mosq = mosquitto_new(NULL, clean_session, NULL);
  if(!mosq){
		fprintf(stderr, "Error: Out of memory.\n");
		exit(1);
	}
  
  mosquitto_log_callback_set(mosq, mosq_log_callback);
  
  if(mosquitto_connect(mosq, mqtt_host, mqtt_port, keepalive)){
		fprintf(stderr, "Unable to connect.\n");
		exit(1);
	}
  int loop = mosquitto_loop_start(mosq);
  if(loop != MOSQ_ERR_SUCCESS){
    fprintf(stderr, "Unable to start loop: %i\n", loop);
    exit(1);
  }
}


int usage() {
  printf("\
  Usage: satel2mqtt [-h] [-v] [-d] -H MQTT_HOST [-p MQTT_PORT] [-D tty_device] [-t MQTT_TOPIC]\n\
  \n\
  -h : print help and usage\n\
  -v : enable debug messages\n\
  -d : fork to background\n\
  -H : mqtt host to connect to. Defaults to localhost.\n\
  -p : network port to connect to. Defaults to 1883.\n\
  -D : tty device to listen to Satel logs. Defaults to /dev/ttyUSB0\n\
  -t : mqtt topic to publish to.\n\
  \n\
  See http://mosquitto.org/ for more information.\n\
  ");
}


int main (int argc, char **argv) {
   int fd;
   int c;
   char daemon;
   pid_t pid;

/* 
extern int getopt_long (int ___argc, char *__getopt_argv_const *___argv,
                        const char *__shortopts,
                        const struct option *__longopts, int *__longind)
       __THROW __nonnull ((2, 3));
*/

while ((c = getopt (argc, argv, "hvdH:D:p:t:")) != -1)
    switch (c)
      {
      case 'h':
         usage();
         exit(1); 
      case 'v':
        DEBUG = 1;
        break;
      case 'd':
        daemon = 1;
        break;        
      case 'H':
        strcpy(mqtt_host,optarg);
        break;
      case 'D':
        strcpy(device,optarg);
        break;        
      case 'p':
        mqtt_port = atoi(optarg);
        break; 
      case 't':
        strcpy(topic,optarg);
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

printf("daemon %d, host %s, port %d, topic %s, debug %d, device %s \n",daemon,mqtt_host,mqtt_port,topic,DEBUG,device);
//exit(1);

   if (daemon) {
        pid = fork();

        if (pid < 0) {
            /* fork failed, probably due to resource limits */
            perror("fork()");
            return errno;
        } else if (pid > 0) {
            /* fork successful! Tell us the forked processes's pid and exit */
            printf("Forked master process: %d\n", pid);
            return 0;
        }
   }


   fd = open(device, O_RDWR | O_NOCTTY);// | O_SYNC);
   
   char buffer[255];       /* Input buffer */
   char *bufptr;           /* Current char in buffer */
   int  nbytes;            /* Number of bytes read */
  
   
   mqtt_setup();
  
   set_interface_attribs(fd, B2400);

   
   while(1){
   memset(buffer, 0, 255); //clear buffer
   bufptr = buffer;
/* main loop */
   while ((nbytes = read(fd, bufptr, buffer + sizeof(buffer) - bufptr - 1)) > 0) {
                bufptr += nbytes;
                if (bufptr[-1] == '\r') {
                    bufptr[-1] = '\0';
                    if (DEBUG) printf("Satel: %s\n",buffer);
                    parse_line(buffer); /* pare message end send to MQTT */
                    break;
                }
            }
   }
   close(fd);
}
