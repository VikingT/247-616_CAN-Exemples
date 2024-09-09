#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <signal.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#define CAN_ID 274
#define BUFFER_SIZE 128

int sock;
int pipe_fd[2];

void setup_can_socket() {
    struct sockaddr_can addr;
    struct ifreq ifr;

    sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    strcpy(ifr.ifr_name, "vcan0"); // Utiliser vcan0 pour le test
    ioctl(sock, SIOCGIFINDEX, &ifr);

    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }
}

void send_can_message() {
    struct can_frame frame;
    frame.can_id = CAN_ID;
    frame.can_dlc = 8; // Longueur des données
    memset(frame.data, 0, sizeof(frame.data)); // Remplir avec des données si nécessaire

    printf("Envoi du message CAN avec ID %d\n", CAN_ID);
    if (write(sock, &frame, sizeof(frame)) != sizeof(frame)) {
        perror("Echec d'envoi du message");
    }
}

void receive_can_message() {
    struct can_frame frame;
    while (1) {
        int nbytes = read(sock, &frame, sizeof(frame));
        if (nbytes > 0) {
            printf("Message reçu avec ID %d: ", frame.can_id);
            for (int i = 0; i < frame.can_dlc; i++) {
                printf("%02X ", frame.data[i]);
            }
            printf("\n");
            // Transfert au processus père
            write(pipe_fd[1], &frame, sizeof(frame));
        }
    }
}

void handle_received_message() {
    struct can_frame frame;
    while (1) {
        int nbytes = read(pipe_fd[0], &frame, sizeof(frame));
        if (nbytes > 0) {
            printf("Message transféré par le fils avec ID %d: ", frame.can_id);
            for (int i = 0; i < frame.can_dlc; i++) {
                printf("%02X ", frame.data[i]);
            }
            printf("\n");
        }
    }
}

void sigint_handler(int signo) {
    close(sock);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    exit(0);
}

int main() {
    setup_can_socket();

    // Création du pipe
    if (pipe(pipe_fd) == -1) {
        perror("Echec de création du pipe");
        exit(1);
    }

    // Gestion du signal SIGINT
    signal(SIGINT, sigint_handler);

    pid_t pid = fork();
    
    if (pid == 0) {
        // Processus fils : réception des messages
        receive_can_message();
    } else if (pid > 0) {
        // Processus père : envoi des messages et réception des messages du fils
        int choice;
        // Lancer le thread pour gérer les messages reçus du fils
        if (fork() == 0) {
            handle_received_message();
        }

        while (1) {
            printf("1. Envoyer un message CAN\n");
            printf("2. Quitter\n");
            printf("Choisissez une option: ");
            scanf("%d", &choice);

            if (choice == 1) {
                send_can_message();
            } else if (choice == 2) {
                kill(pid, SIGINT);
                break;
            }
        }
    } else {
        perror("Fork échoué");
        exit(1);
    }

    close(sock);
    return 0;
}