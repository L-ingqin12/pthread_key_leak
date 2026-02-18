// leak.c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void do_something(void) {
    pthread_key_t key;
    pthread_key_create(&key, free);   // 每次调用都创建新key（错误！）
    int *data = malloc(sizeof(int));
    *data = 42;
    pthread_setspecific(key, data);
    printf("[leak] 创建了一个新key，数据已设置\n");
}