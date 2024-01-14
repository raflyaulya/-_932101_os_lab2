#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

#define PORT 8085
#define BUFFER_SIZE 1024

volatile sig_atomic_t wasSigHup = 0;
 
void sigHupHandler(int signo) {
    wasSigHup = 1;
}

int main() {
    // Регистрация обработчика сигнала SIGHUP
    struct sigaction sa;
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);

    // Блокировка сигнала SIGHUP
    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigemptyset(&origMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);

    // Создание сокета
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Настройка серверного адреса
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    // Привязка сокета к адресу и порту
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        perror("Bind failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Перевод сокета в режим прослушивания
    if (listen(serverSocket, 5) == -1) {  // Максимальная длина очереди - 5
        perror("Listen failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    char buffer[BUFFER_SIZE] = { 0 };
    // Множество дескрипторов файлов для pselect
    int maxFd;
    fd_set fds;
    int AnotherSocket = 0;
    int addressLength = sizeof(serverAddress);
    // Основной цикл
    while (1) {
        // Инициализация множества дескрипторов файлов
        FD_ZERO(&fds);
        FD_SET(serverSocket, &fds);

        if (AnotherSocket > 0)
        {
            FD_SET(AnotherSocket, &fds);
        }
        
        if (AnotherSocket > serverSocket)
        {
            maxFd = AnotherSocket;
        } else {
            maxFd = serverSocket;
        }

        // Вызов pselect
        if (pselect(maxFd + 1, &fds, NULL, NULL, NULL, &origMask) == -1) {
            if (errno == EINTR) {
                if (wasSigHup) {
                    printf("Received SIGHUP signal.\n");
                    wasSigHup = 0;  // Сброс флага
                }
            } else {
                perror("pselect error");
                exit(EXIT_FAILURE);
            }
        }

        if (FD_ISSET(AnotherSocket, &fds) && AnotherSocket > 0) {
            int readBytes = read(AnotherSocket, buffer, 1024);
            if (readBytes > 0) 
            {  
                printf("Received data: %d bytes\n", readBytes);
                printf("Received message from client: %s\n", buffer);
            } else {
                if (readBytes == 0) {
                    printf("Connection closed\n\n");
                    close(AnotherSocket); 
                    AnotherSocket = 0; 
                } else { 
                    perror("read error"); 
                }  
            }
        }

        if (FD_ISSET(serverSocket, &fds)) {
            if ((AnotherSocket = accept(serverSocket, (struct sockaddr*)&serverAddress, (socklen_t*)&addressLength)) < 0) {
                perror("accept error");
                exit(EXIT_FAILURE);
            }

            printf("New connection.\n");


        }

    }

    close(serverSocket);

    return 0;
}