/* server.c */
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define BACKLOG 10
#define LINE_SIZE 1024
#define NAME_SIZE 128
#define MAX_SIZE 99999
#define TRUE 1
#define FALSE 0
#define MAX_THREAD 10
#define DEFAULT_PORT 12345

typedef struct food food_t;

struct food
{
    char *name;
    char *measure;
    char *weight;
    char *kCal;
    char *fat;
    char *carbo;
    char *protein;
};

typedef struct node node_t;

struct node
{
    food_t *food;
    node_t *next;
};

typedef struct task task_t;

struct task
{
    void *(*function)(void *);
    void *arg;
    task_t *next;
};

typedef struct threadpool threadpool_t;

struct threadpool
{
    int shutdown;
    pthread_mutex_t pool_mutex;
    pthread_cond_t ready;
    int max_thread_num;
    int cur_task_num;
    task_t *tasks;
    pthread_t *threadid;
};


food_t *newitem();
char *result_msg(node_t *head);
char *item_to_str(food_t* food);
node_t *node_add(node_t *head, food_t* food);
node_t *search_result(char* input, node_t* head);
node_t *food_list();
node_t *list_reversal(node_t *head);
node_t *sort_insert(node_t* head, food_t* food);
void item_create_from_str(food_t* food, char* line);
void file_edit(node_t *head);
void delete_char(char *str, const char del);
void free_list(node_t* head);
int str_check(char *a, char *b);
int str_char_num(char *str, const char *del);
int items_count(node_t *head);

threadpool_t *threadpool_init(int thread_num);
void add_task(threadpool_t *pool, void*(*function)(void*), void *arg);
void *threadpool_thread(void *threadpool);
void threadpool_destroy(threadpool_t *pool);
void threadpool_free(threadpool_t *pool);

void signal_handler(int signal);

void *process(void *fd);


node_t *list1 = NULL; // used to store item from file
node_t *list2 = NULL; // used to store item from file and add new item to file
threadpool_t *pool = NULL;
pthread_mutex_t file_mutex;
int newfood = FALSE;


int main(int argc, char *argv[])
{
    int sockfd, port;
    int *new_fd;
    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr;
    socklen_t sin_size;

    signal(SIGINT, signal_handler);

    if (argc == 1)
    {
        port = DEFAULT_PORT;
    }
    else if (argc == 2)
    {
        port = atoi(argv[1]);
    }
    else
    {
        fprintf(stderr, "usage: server port\n");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error!");
        exit(1);
    }

    if (pthread_mutex_init(&file_mutex, NULL) != 0)
    {
        perror("file mutex error!");
        exit(1);
    }

    pool = threadpool_init(MAX_THREAD);
    printf("Threadpool created.\n");

    list1 = food_list();
    list2 = food_list();

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind error!");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listening error!");
        exit(1);
    }

    printf("server starts listening ...\n");

    while(TRUE)
    {
        sin_size = sizeof(struct sockaddr_in);

        new_fd = (int*)malloc(sizeof(int));
        if ((*new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1)
        {
            perror("accept error!");
            continue;
        }

        printf("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));

        add_task(pool, process, (void *)new_fd);
    }

    return 0;
}


void *process(void *fd)
{
    int new_fd = *((int*)fd);
    int byte;
    char input[LINE_SIZE], newname[LINE_SIZE], output[MAX_SIZE];

    printf("client id-%d has connected.\n", new_fd);

    while(TRUE)
    {
        memset(input, 0, sizeof(input));
        memset(output, 0, sizeof(output));

        if((byte = recv(new_fd, &input, LINE_SIZE, 0)) == -1)
        {
            perror("receive error!");
            exit(1);
        }
        else if(byte == 0)
        {
            printf("client id-%d has terminated.\n", new_fd);
            break;
        }
        input[byte] = '\0';

        printf("received: %s. from client id-%d\n", input, new_fd);

        if(strncmp(input, "Add new item:", 13) == 0)
        {
            newfood = TRUE;
            int i;
            int j = 0;
            memset(newname, 0, sizeof(newname));

            for(i = 13; i < byte; i++)
            {
                newname[j] = input[i];
                j++;
            }

            food_t *newfood = newitem();
            item_create_from_str(newfood, newname);
            pthread_mutex_lock(&file_mutex);
            node_t *newitem = sort_insert(list2, newfood);
            list2 = newitem;
            pthread_mutex_unlock(&file_mutex);
            strcpy(output, "\nAdd new item successfully!\n");
        }
        else
        {
            strcpy(output, result_msg(search_result(input, list1)));
        }

        if (send(new_fd, &output, strlen(output), 0) == -1)
        {
            printf("send error!\n");
        }
    }
    free(fd);

    close(new_fd);
}


/* This function used to reversal the list */
node_t *list_reversal(node_t *head)
{
    node_t *mid, *last;
    mid = NULL;
    while(head != NULL)
    {
        last = mid;
        mid = head;
        head = head->next;
        mid->next = last;
    }
    return mid;
}


/* This function used to insert a new node to list in ascending order by ASCII */
node_t *sort_insert(node_t* head, food_t* food)
{
    node_t *p0, *p1, *p2;
    p1 = head;
    p0 = (node_t*)malloc(sizeof(node_t));
    p0->food = food;

    while (strcasecmp(p0->food->name, p1->food->name) > 0 && p1->next != NULL)
    {
        p2 = p1;
        p1 = p1->next;
    }

    if (strcasecmp(p0->food->name, p1->food->name) <= 0)
    {
        if(p1 == head)
        {
            p0->next = p1;
            head = p0;
        }
        else
        {
            p0->next = p1;
            p2->next = p0;
        }
    }
    else
    {
        p1->next = p0;
        p0->next = NULL;
    }

    return head;
}


/* Initialize a new item */
food_t *newitem()
{
    food_t *food = (food_t*)malloc(sizeof(food_t));
    if(food == NULL)
    {
        printf("food malloc fail.");
        return NULL;
    }

    food->name = NULL;
    food->measure = NULL;
    food->weight = NULL;
    food->fat = NULL;
    food->kCal = NULL;
    food->carbo = NULL;
    food->protein = NULL;

    return food;
}


/* This function used to format item in single string */
char *item_to_str(food_t* food)
{
    char *str = (char*)malloc(LINE_SIZE);
    memset(str, 0, LINE_SIZE);

    strcpy(str, food->name);
    sprintf(str, "%s,%s", str, food->measure);
    sprintf(str, "%s,%s", str, food->weight);
    sprintf(str, "%s,%s", str, food->kCal);
    sprintf(str, "%s,%s", str, food->fat);
    sprintf(str, "%s,%s", str, food->carbo);
    sprintf(str, "%s,%s", str, food->protein);

    if(str[strlen(str) - 1] != '\n')
    {
        sprintf(str, "%s\n", str);
    }

    return str;
}


/* This function used to create a new item and assign it value from string */
void item_create_from_str(food_t* food, char* line)
{
    char name[NAME_SIZE];
    char *c;
    int comma_num = 0;

    comma_num = str_char_num(line, ",");

    c = strtok(line, ",");
    strcpy(name, c);

    int i = 1;
    while(comma_num - 5 != i)
    {
        c = strtok(NULL, ",");
        sprintf(name, "%s,%s", name, c);
        i++;
    }

    food->name = malloc(strlen(name) + 1);
    strcpy(food->name, name);

    c = strtok(NULL, ",");
    food->measure = malloc(strlen(c) + 1);
    strcpy(food->measure, c);

    c = strtok(NULL, ",");
    food->weight = malloc(strlen(c) + 1);
    strcpy(food->weight, c);

    c = strtok(NULL, ",");
    food->kCal = malloc(strlen(c) + 1);
    strcpy(food->kCal, c);

    c = strtok(NULL, ",");
    food->fat = malloc(strlen(c) + 1);
    strcpy(food->fat, c);

    c = strtok(NULL, ",");
    food->carbo = malloc(strlen(c) + 1);
    strcpy(food->carbo, c);

    c = strtok(NULL, ",");
    food->protein = malloc(strlen(c) + 1);
    strcpy(food->protein, c);

    c = NULL;
}

/* This function used to add new item in file */
void file_edit(node_t *head)
{
    char line[LINE_SIZE];
    FILE *oldfile, *newfile;

    oldfile = fopen("calories.csv", "r");
    newfile = fopen("temp.csv", "w");

    if(oldfile == NULL)
    {
        printf("open file failed\n");
    }

    if(newfile == NULL)
    {
        printf("open file failed\n");
    }

    while (fgets(line, sizeof(line), oldfile) != NULL)
    {
        if (line[0] == '#')
        {
            fputs(line, newfile);
        }
    }
    fclose(oldfile);

    while(head != NULL)
    {
        fputs(item_to_str(head->food), newfile);
        head = head->next;
    }

    fclose(newfile);

    remove("calories.csv");
    rename("temp.csv", "calories.csv");
    free_list(head);
}


/* This function used to count the number of particular character in string */
int str_char_num(char *str, const char *del)
{
    char *first = NULL;
    char *second = NULL;
    int num = 0;
    first = strstr(str, del);

    while (first != NULL)
    {
        second = first + 1;
        num++;
        first = strstr(second, del);
    }
    return num;
}


/* This function used to campare each character of two string */
int str_check(char *a, char *b)
{
    for (int i = 0; b[i] != '\0'; i++)
    {
        if (b[i] != a[i])
        {
            return FALSE;
        }
    }
    return TRUE;
}


/* This function used to delete the particular characters in string */
void delete_char(char *str, const char del)
{
    int i, j;
    for (i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == del)
        {
            for (j = i; str[j] != '\0'; j++)
            {
                str[j] = str[j + 1];
            }
        }
    }
}


/* This function used to add new node to a list */
node_t *node_add(node_t *head, food_t* food)
{
    node_t *newnode = (node_t*)malloc(sizeof(node_t));

    if (newnode == NULL)
    {
        printf("newnode allocate failed\n");
        return NULL;
    }

    newnode->food = food;
    newnode->next = head;

    return newnode;
}


/* This function used to free a list */
void free_list(node_t* head)
{
    node_t *p;
    while(head)
    {
        p = head->next;
        free(head);
        head = p;
    }
}


/* This function used to count the number of items in list */
int items_count(node_t *head)
{
    int i = 0;
    for ( ; head != NULL; head = head->next)
    {
        i++;
    }

    return i;
}


/* This function used to create a list and stroe items in list from file */
node_t *food_list()
{
    node_t *food_list = NULL;

    char line[LINE_SIZE];

    pthread_mutex_lock(&file_mutex);
    FILE *file = fopen("calories.csv", "r");
    if (file == NULL)
    {
        printf("open file failed\n");
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (line[0] != '#')
        {
            food_t *food = newitem();

            item_create_from_str(food, line);

            node_t *n = node_add(food_list, food);
            food_list = n;
        }
    }
    fclose(file);
    pthread_mutex_unlock(&file_mutex);

    return list_reversal(food_list);
}


/* This function used to search the item in the list by name, and stroe the result in new list */
node_t *search_result(char* input, node_t* head)
{
    int i = 0;
    char newname[NAME_SIZE], name[NAME_SIZE], search[NAME_SIZE];
    node_t *result_list = NULL;

    strcpy(search, input);

    for (int i = 0; search[i] != '\0'; i++)
    {
        search[i] = tolower(search[i]);
    }

    while (head != NULL)
    {
        strcpy(name, head->food->name);

        if (str_char_num(name, ",") > 0)
        {
            delete_char(name, ',');
            strcpy(newname, name);
        }
        else
        {
            strcpy(newname, name);
        }

        for (int i = 0; newname[i] != '\0'; i++)
        {
            newname[i] = tolower(newname[i]);
        }

        if (str_check(newname, search) == TRUE)
        {
            i++;
            node_t *n = node_add(result_list, head->food);
            result_list = n;

            head = head->next;
        }
        else
        {
            head = head->next;
        }
    }

    return list_reversal(result_list);
}


/* This function used to format the result list in string */
char *result_msg(node_t *head)
{
    int num = items_count(head);
    char items[MAX_SIZE];
    char *result = NULL;
    if (num == 0)
    {
        result = "No food item found.\nPlease check your spelling and try again.\n\n";
    }
    else
    {
        sprintf(items, "%d food items found.\n\n", num);
        for( ; head != NULL; head = head->next)
        {
            sprintf(items, "%sFood: %s\nMeasure: %s\nWeight (g): %s\nkCal: %s\nFat (g): %s\nCarbo (g): %s\nProtein (g): %s\n",
                    items, head->food->name, head->food->measure, head->food->weight, head->food->kCal, head->food->fat,
                    head->food->carbo, head->food->protein);
        }
        result = malloc(strlen(items) + 1);
        strcpy(result, items);
    }
    free_list(head);

    return result;
}


/* This function used to initialize a threadpool with the number of threads */
threadpool_t *threadpool_init(int thread_num)
{
    threadpool_t *pool = NULL;
    pool = (threadpool_t*)malloc(sizeof(threadpool_t));
    if (pool == NULL)
    {
        printf("threadpool malloc fail.\n");
        return NULL;
    }

    pool->max_thread_num = thread_num;
    pool->cur_task_num = 0;
    pool->shutdown = FALSE;
    pool->tasks = NULL;
    pool->threadid = (pthread_t*)malloc(sizeof(pthread_t) * thread_num);
    if (pool->threadid == NULL)
    {
        printf("threadid malloc fail.\n");
        return NULL;
    }

    if (pthread_mutex_init(&(pool->pool_mutex), NULL) != 0 ||
            pthread_cond_init(&(pool->ready), NULL) != 0)
    {
        printf("pool mutex or cond init fail.\n");
        return NULL;
    }

    for (int i = 0; i < thread_num; i++)
    {
        if (pthread_create(&(pool->threadid[i]), NULL, threadpool_thread, (void *)pool) != 0)
        {
            printf("thread create fail.\n");
            return NULL;
        }
    }

    return pool;
}


/* This function used to add task to the threadpool */
void add_task(threadpool_t *pool, void*(*function)(void*), void *arg)
{
    assert(pool != NULL);
    assert(function != NULL);
    assert(arg != NULL);

    task_t *newtask = (task_t*)malloc(sizeof(task_t));
    newtask->function = function;
    newtask->arg = arg;
    newtask->next = NULL;

    pthread_mutex_lock(&(pool->pool_mutex));

    if(pool->cur_task_num >= pool->max_thread_num)
    {
        printf("current queue full!");
        pthread_mutex_unlock(&(pool->pool_mutex));
        return;
    }

    task_t *member = pool->tasks;
    if(member != NULL)
    {
        while(member->next != NULL)
            member = member->next;
        member->next = newtask;
    }
    else
    {
        pool->tasks = newtask;
    }

    assert (pool->tasks != NULL);

    pool->cur_task_num++;

    pthread_mutex_unlock(&(pool->pool_mutex));
    pthread_cond_signal(&(pool->ready));
}


void *threadpool_thread(void *threadpool)
{
    threadpool_t *pool = (threadpool_t*)threadpool;

    while(TRUE)
    {
        pthread_mutex_lock(&(pool->pool_mutex));

        if(pool->shutdown == TRUE)
        {
            pthread_mutex_unlock(&(pool->pool_mutex));
            pthread_exit(NULL);
        }

        if(pool->cur_task_num == 0 && pool->shutdown == FALSE)
        {
            pthread_cond_wait(&(pool->ready), &(pool->pool_mutex));
        }

        assert(pool->cur_task_num != 0);
        assert(pool->tasks != NULL);

        pool->cur_task_num--;

        task_t *task = pool->tasks;
        pool->tasks = task->next;

        pthread_mutex_unlock(&(pool->pool_mutex));

        (*(task->function))(task->arg);
        free(task);
        task = NULL;
    }
    return NULL;
}


void threadpool_destroy(threadpool_t *pool)
{
    if (pool == NULL)
    {
        return;
    }

    pool->shutdown = TRUE;

    for(int i = 0; i < pool->max_thread_num; i++)
    {
        pthread_join(pool->threadid[i], NULL);
    }

    threadpool_free(pool);
}


void threadpool_free(threadpool_t *pool)
{
    if(pool == NULL)
    {
        return;
    }

    if(pool->tasks)
    {
        free(pool->tasks);
    }

    if(pool->threadid)
    {
        free(pool->threadid);
        pthread_mutex_destroy(&(pool->pool_mutex));
        pthread_cond_destroy(&(pool->ready));
    }
    free(pool);
    pool = NULL;
}


void signal_handler(int signal)
{
    if(newfood == TRUE)
    {
        file_edit(list2);
    }
    free_list(list1);
    threadpool_destroy(pool);
    pthread_mutex_destroy(&file_mutex);
}
