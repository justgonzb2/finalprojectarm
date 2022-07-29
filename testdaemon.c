#include <stdio.h>
#include <sys/types.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>

#define OK              0
#define NO_SETSID       1
#define SIGTERM         2
#define SIGHUP          3
#define NO_FORK         4
#define ERR_CHDIR       5
#define ERR_WTF         9
#define INIT_ERR        10
#define REQ_ERR         11
#define NO_FILE         12
#define DAEMON_NAME     "Justins Daemon"

static const char* STATE_URL = "http://54.183.247.219:8080/state";
static const char* TEMP_URL = "http://54.183.247.219:8080/temp";
static const char* REPORT_URL = "http://54.183.247.219:8080/update";
static const char* TEMP_FILENAME = "/tmp/temp";
static const char* STATE_FILENAME = "/tmp/status";
static const char*  WORKING_DIR   = "/";

 struct memory {
   char *response;
   size_t size;
 };
 
 struct memory chunk = {0};

static void _signal_handler(const int signal) {
    switch (signal) {
        case SIGTERM:
            syslog(LOG_INFO, "received SIGTERM, exiting.");
            closelog();
            exit(OK);
            break;
        case SIGHUP:
            break;
        default:
            syslog(LOG_INFO, "received unhandled signal");
    }
}

static size_t call_back(void *data, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *)userp;

    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if(ptr == NULL) {
        return 0;
    }

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}

static char* sendpost(char *url, char *message, bool verb) {
    CURL *curl = curl_easy_init();
    if (curl) {
        CURLcode res;
        FILE* outputFile = fopen("garbage.txt", "wb");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, outputFile);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message);
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            return REQ_ERR;
        }

        curl_easy_cleanup(curl);
    } else {
        return NULL;
    }
    return chunk.response;
}

static char* sendget(char *url, char *message, bool verb) {
    CURL *curl = curl_easy_init();
    if (curl) {
        CURLcode res;
        FILE* outputFile = fopen("garbage.txt", "wb");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, outputFile);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, call_back);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            return REQ_ERR;
        }

        curl_easy_cleanup(curl);
    } else {
        return NULL;
    }
    return chunk.response;
}

static void _exit_process(int err) {
  syslog(LOG_INFO, "error so exit process");
  closelog(); 
  exit(err);
}

static void _handle_fork(const pid_t pid) {
  // For some reason, we were unable to fork.
  if (pid < 0) {
    _exit_process(NO_FORK);
  }

  // Fork was successful so exit the parent process.
  if (pid > 0) {
    exit(OK);
  }
}

static void daemonize(void) {
  // Fork from the parent process.
  pid_t pid = fork();

  // Open syslog with the specified logmask.
  openlog(DAEMON_NAME, LOG_PID | LOG_NDELAY | LOG_NOWAIT, LOG_DAEMON);

  // Handle the results of the fork.
  _handle_fork(pid);

  // Now become the session leader.
  if (setsid() < -1) {
    _exit_process(NO_SETSID);
  }

  // Set our custom signal handling.
  signal(SIGTERM, _signal_handler);
  signal(SIGHUP, _signal_handler);

  // New file persmissions on this process, they need to be permissive.
  //umask(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  //umask(666);

  // Change to the working directory.
  chdir(WORKING_DIR);

  // Closing file descriptors (STDIN, STDOUT, etc.).
  for (long x = sysconf(_SC_OPEN_MAX); x>=0; x--) {
    close(x);
  }
}

static void getcurrenttemp(void) {
    char *buffer = NULL;
    size_t size = 0;

    FILE *fp = fopen(TEMP_FILENAME, "r");
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);
    buffer = malloc((size + 1) * sizeof(*buffer)); 
    fread(buffer, size, 1, fp);
    buffer[size] = '\0';
    sendpost(REPORT_URL, buffer, true);
}

static int updatestate(char *state) {
    FILE *fp = fopen(STATE_FILENAME, "w");
    if (fp == NULL) {
        printf("unable to open file for writing\n");
        return ERR_WTF;
    }
    fputs(state, fp);
    fclose(fp);
    return OK;
}

static void getstatusfromaws(void) {
    char *state = sendget(STATE_URL, NULL, false);
    if (strcmp(state, "true") == 0) {
        updatestate("ON");
    } else if (strcmp(state, "false") == 0) {
        updatestate("OFF");
    }

    chunk.response = NULL;
    chunk.size = NULL;
}

int main(int argc, char **argv) {

    int err;
    syslog(LOG_INFO, DAEMON_NAME);
    daemonize();
    while (true) {
        getcurrenttemp();
        getstatusfromaws();        
        sleep(5);
    }   

    return ERR_WTF;
}