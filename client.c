/* client.c */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define LINE_SIZE 256
#define NAME_SIZE 128
#define MAX_SIZE 99999


int main(int argc, char *argv[])
{
    int sockfd, byte;
    struct hostent *he;
    struct sockaddr_in their_addr;
    char input[LINE_SIZE], output[MAX_SIZE];
    char c;

    if (argc != 3)
    {
        fprintf(stderr,"usage: ./client hostname PortNumber\n");
        exit(1);
    }

    int port = atoi(argv[2]);

    if ((he = gethostbyname(argv[1])) == NULL)
    {
        herror("gethostbyname error!");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error!");
        exit(1);
    }

    their_addr.sin_family = AF_INET;
    their_addr.sin_port = htons(port);
    their_addr.sin_addr = *((struct in_addr *) he->h_addr);
    //bzero(&(their_addr.sin_zero), 8);

    if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("connect error!");
        exit(1);
    }

    while(1)
    {
        do
        {
            printf("\nEnter 's' or 'S' to search food.\n");
            printf("Enter 'a' or 'A' to add new food item.\n");
            printf("Enter 'q' or 'Q' to quit.\n\n");
            printf("Please enter a valid character: ");
            c = tolower(getchar());
            getchar();
        }
        while(c != 's' && c != 'q' && c != 'a');

        memset(input, 0, sizeof(input));
        memset(output, 0, sizeof(output));

        if (c == 'q')
        {
            break;
        }
        else if(c == 's')
        {
            printf("\nEnter the food name to search for: ");
            gets(input);
            fflush(stdin);
            printf("\n");
        }
        else if(c == 'a')
        {
            char name[NAME_SIZE], measure[NAME_SIZE], weight[NAME_SIZE],
                 kCal[NAME_SIZE], fat[NAME_SIZE], carbo[NAME_SIZE], protein[NAME_SIZE];

            printf("Please enter the food name: ");
            gets(name);
            fflush(stdin);

            printf("Please enter the measure: ");
            gets(measure);
            fflush(stdin);

            printf("Please enter the weight (g): ");
            gets(weight);
            fflush(stdin);

            printf("Please enter the kCal: ");
            gets(kCal);
            fflush(stdin);

            printf("Please enter the fat (g): ");
            gets(fat);
            fflush(stdin);

            printf("Please enter the carbo (g): ");
            gets(carbo);
            fflush(stdin);

            printf("Please enter the protein (g): ");
            gets(protein);
            fflush(stdin);

            strcpy(input, "Add new item:");
            sprintf(input, "%s%s,%s,%s,%s,%s,%s,%s", input, name,
                    measure, weight, kCal, fat, carbo, protein);
        }

        if(send(sockfd, &input, strlen(input), 0) == -1)
        {
            printf("send message failed.");
        }

        if((byte = recv(sockfd, &output, MAX_SIZE, 0)) == -1)
        {
            perror("receive error!");
            exit(1);
        }
        else if (byte == 0)
        {
            printf("connection terminated.\n");
            break;
        }

        puts(output);
    }

    close(sockfd);

    return 0;
}
