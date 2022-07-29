// for standard gcc: make -f make-gcc
// for arm: make -f make-arm
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
#define ERR_SETSID      1
#define SIGTERM         2
#define SIGHUP          3
#define ERR_FORK        4
#define ERR_CHDIR       5
#define ERR_WTF         9
#define INIT_ERR        10
#define REQ_ERR         11
#define NO_FILE         12
#define DAEMON_NAME     "Justins Daemon"

// params, move to config file
static const char* STATE_URL = "http://54.183.247.219:8080/state";
static const char* TEMP_URL = "http://54.183.247.219:8080/temp";
static const char* REPORT_URL = "http://54.183.247.219:8080/report";
static const char* TEMP_FILENAME = "/tmp/temp";
static const char* STATE_FILENAME = "/tmp/status";

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
        FILE* outputFile = fopen("curloutput.txt", "wb");
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
        FILE* outputFile = fopen("curloutput.txt", "wb");
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

static int daemonize(void) {
    openlog(DAEMON_NAME, LOG_PID | LOG_NDELAY | LOG_NOWAIT, LOG_DAEMON);

    syslog(LOG_INFO, DAEMON_NAME);

    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, strerror(pid));
        return ERR_FORK;
    }

    if (pid > 0) {
        return OK;
    }

    if (setsid() < -1) {
        syslog(LOG_ERR, strerror(pid));
        return ERR_SETSID;
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    umask(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (chdir("/") < 0) {
        syslog(LOG_ERR, strerror(pid));
        return ERR_CHDIR;
    }

    signal(SIGTERM, _signal_handler);
    signal(SIGHUP, _signal_handler);

    return OK;
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

static int startprogram(void) {
    // run the code until kill
    while (true) {
        getcurrenttemp();
        getstatusfromaws();        
        sleep(5);
    }
    
    return ERR_WTF;
}

int main(int argc, char **argv) {

    int err;
    syslog(LOG_INFO, DAEMON_NAME);
    err = daemonize();
    if (err != OK) {
        return ERR_WTF;
    }
    err = startprogram();
    if (err != OK) {
        return ERR_WTF;
    }

    return ERR_WTF;
}