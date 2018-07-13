/*
 * Example client for gserver 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <time.h>

#define CMD_INVALID 0
#define CMD_UNKNOWN 1
#define CMD_VERSION 2
#define CMD_BUILD 3
#define CMD_NETCONFIG 4
#define CMD_DOWNLOAD 5
#define CMD_UPGRADE 6
#define CMD_REBOOT 7

void sig_handler(int sig);
void add_signal_handlers();
int open_socket(const char *ip, int port);
void usage(char *argv_0);
void parse_args(int argc, char **argv);
void run_command(int sock);
void run_download_command(int sock);
int msleep(int ms);

#define MAX_NETCONFIG_STR 124
#define MAX_UPGRADE_FILE_PATH 508

// globals
volatile sig_atomic_t shutdown_signal = 0;
int cmd;
char netconfig_str[MAX_NETCONFIG_STR + 4];
char upgrade_file[MAX_UPGRADE_FILE_PATH + 4];
char server_ip[32];
int server_port;

void usage(char *argv_0)
{
    printf("\nUsage: %s -s <server> -p <port> -<command> [command_args]\n", argv_0);
    printf("  -s          server ip address, default 192.168.10.210\n");
    printf("  -p          server port, default 1234\n");

    printf("\nOnly one of the following commands may be specified:\n\n");

    printf("  -v          version: gserver version on BBG\n");
    printf("  -b          build: BBG build tag and date\n");
    printf("  -u          upgrade: run upgrade script\n");
    printf("  -r          reboot: reboot BBG system\n");
    printf("  -n <config> netconfig: set network configuration\n");
    printf("              <config> is either 'dhcp' or a list of colon\n");
    printf("              delimited fields in this order\n");
    printf("              ip:netmask:gateway:nameserver1:nameserver2\n");
    printf("              Only the first arg, 'dhcp' or ip address is required.\n");
    printf("  -d <file>   download: download a rootfs tarball, should be xz compressed\n");
    printf("  -h          Show this help\n");

    printf("\nExamples:\n\n");
    printf("  %s -v\n", argv_0);
    printf("  %s -n dhcp\n", argv_0);
    printf("  %s -n 192.168.10.210:255.255.255.0:192.168.10.1:8.8.8.8\n", argv_0);
    printf("  %s -d gamry-prod-rootfs.tar.xz\n", argv_0);

    exit(1);
}

void parse_args(int argc, char **argv)
{
    int opt, len;

    cmd = CMD_UNKNOWN;
    strcpy(server_ip, "192.168.10.210");
    server_port = 1234;
 
    while ((opt = getopt(argc, argv, "s:p:d:n:vburh")) != -1) {
        switch (opt) {
        case 's':
            if (strlen(optarg) < 8 || strlen(optarg) > 15) {
                printf("Invalid server ip: %s\n", optarg);
                exit(1);
            }

            strcpy(server_ip, optarg);
            break;

        case 'p':
            server_port = strtoul(optarg, NULL, 0);

            if (server_port < 1 || server_port > 65535) {
                printf("Invalid server port: %s\n", optarg);
                exit(1);
            }

            break;

        case 'd':
            len = strlen(optarg);

            if (len < 4 || len > MAX_UPGRADE_FILE_PATH) {
                printf("Invalid download image filename: %s", optarg);
                exit(1);
            }
          
            if (optarg[len - 3] != '.' || optarg[len - 2] != 'x' || optarg[len - 1] != 'z') {
                printf("Download filename does not end in .xz: %s\n", optarg);
                exit(1);
            }
 
            if (cmd != CMD_UNKNOWN) {
                printf("Only one command may be specified\n");
                exit(1);
            }
 
            strcpy(upgrade_file, optarg);
            cmd = CMD_DOWNLOAD;
            break;
            
        case 'n':
            if (strlen(optarg) < 4 || strlen(optarg) > MAX_NETCONFIG_STR) {
                printf("Invalid netconfig str\n");
                exit(1);
            }

            if (cmd != CMD_UNKNOWN) {
                printf("Only one command may be specified\n");
                exit(1);
            }

            strcpy(netconfig_str, optarg);
            cmd = CMD_NETCONFIG;
            break;
 
        case 'v':
            if (cmd != CMD_UNKNOWN) {
                printf("Only one option command be specified\n");
                exit(1);
            }

            cmd = CMD_VERSION;
            break;

        case 'b':
            if (cmd != CMD_UNKNOWN) {
                printf("Only one command may be specified\n");
                exit(1);
            }

            cmd = CMD_BUILD;
            break;

        case 'u':
            if (cmd != CMD_UNKNOWN) {
                printf("Only one command may be specified\n");
                exit(1);
            }

            cmd = CMD_UPGRADE;
            break;

        case 'r':
            if (cmd != CMD_UNKNOWN) {
                printf("Only one option command be specified\n");
                exit(1);
            }

            cmd = CMD_REBOOT;
            break;

        case 'h':
        default:
            usage(argv[0]);
            break;
        }
    } 

    if (cmd == CMD_UNKNOWN)
        usage(argv[0]);
}


int main(int argc, char **argv)
{
    int sock;

    parse_args(argc, argv);

    add_signal_handlers();

    sock = open_socket(server_ip, server_port);
	
    if (sock < 0)
        exit(1);

    if (cmd == CMD_DOWNLOAD)
        run_download_command(sock);
    else
        run_command(sock);

    close(sock);

    return 0;
}

#define MAX_RESPONSE 508
void read_response(int sock)
{
    char rx[MAX_RESPONSE + 4];
    int len, pos;

    memset(rx, 0, sizeof(rx));
    pos = 0;

    while (1) {
        msleep(100);

        len = read(sock, rx + pos, MAX_RESPONSE - pos);

        if (len == 0)
            break;

        if (len < 1) { 
            perror("read");
            break;
        }

        pos += len;

        if (pos >= MAX_RESPONSE) {
            printf("Overflow of response buffer\n");
            break;
        }
    }

    if (strlen(rx) > 0)
        printf("%s", rx);
}

void run_command(int sock)
{
    char tx[256];
    
    switch (cmd) {
    case CMD_VERSION:
        strcpy(tx, "version\n");
        break;

    case CMD_BUILD:
        strcpy(tx, "build\n");
        break;

    case CMD_UPGRADE:
        strcpy(tx, "upgrade\n");
        break;

    case CMD_REBOOT:
        strcpy(tx, "reboot\n");
        break;

    case CMD_NETCONFIG:
        strcpy(tx, "netconfig\n");
        strcat(tx, netconfig_str);
        strcat(tx, "\n");
        break;

    case CMD_DOWNLOAD:
        printf("Shouldn't be here\n");
        return;
   
    default:
        printf("Unknown command: %d\n", cmd);
        return;
    }

    if (write(sock, tx, strlen(tx)) < 0) {
        perror("write"); 
        return;
    }

    read_response(sock);
}

void run_download_command(int sock)
{
    int len, pos, size;
    char rx[32];
    char *binbuff = NULL;

    FILE *fh = fopen(upgrade_file, "rb");

    if (!fh) {
        perror("fopen");
        goto download_done;
    }

    if (fseek(fh, 0, SEEK_END)) {
        perror("fseek");
        goto download_done;
    }

    size = ftell(fh);

    if (size < 1 || size > (64 * 1024 * 1024)) {
        printf("upgrade file size is suspect: %d\n", size);
        goto download_done;
    }

    rewind(fh);  

    binbuff = (char *) malloc(size);   

    if (!binbuff) {
        perror("malloc");
        goto download_done;
    }

    if (fread(binbuff, 1, size, fh) != size) {
        printf("Failed to slurp upgrade file\n");
        goto download_done;
    }
    
    sprintf(rx, "download\n%d\n", size);

    len = strlen(rx);

    if (write(sock, rx, len) < 0) {
        perror("write");
        goto download_done;
    }

    pos = 0;

    while (pos < size) {
        len = write(sock, binbuff + pos, size - pos);

        if (len < 0) {
            perror("write");
            goto download_done;
        }

        pos += len;
    }

    read_response(sock);

download_done:

    if (fh)
        fclose(fh);

    if (binbuff)
        free(binbuff);
}

void sig_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
        shutdown_signal = 1;
}
 
void add_signal_handlers()
{
    struct sigaction sia;

    memset(&sia, 0, sizeof(sia));

    sia.sa_handler = sig_handler;

    if (sigaction(SIGINT, &sia, NULL) < 0) {
        perror("sigaction");
        exit(1);
    } 
    else if (sigaction(SIGTERM, &sia, NULL) < 0) {
        perror("sigaction");
        exit(1);
    } 
}

int open_socket(const char *ip, int port)
{
    int sock;
    struct sockaddr_in addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }
    
    return sock;
}

int msleep(int ms)
{
    struct timespec ts;

    if (ms < 1)
            return -1;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = 1000000 * (ms % 1000);

    return nanosleep(&ts, NULL);
}
