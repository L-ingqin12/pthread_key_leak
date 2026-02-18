// clean.c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static pthread_key_t key;
static pthread_once_t once = PTHREAD_ONCE_INIT;

static void dtor(void *ptr) {
    free(ptr);
}

static void create_key(void) {
    pthread_key_create(&key, dtor);
    printf("[clean] 内部key已创建\n");
}

__attribute__((constructor))
static void init(void) {
    pthread_once(&once, create_key);
    printf("[clean] 库已加载\n");
}

__attribute__((destructor))
static void fini(void) {
    pthread_key_delete(key);
    printf("[clean] 库已卸载，key已删除\n");
}

void do_something(void) {
    int *data = pthread_getspecific(key);
    if (data == NULL) {
        data = malloc(sizeof(int));
        *data = 42;
        pthread_setspecific(key, data);
    }
    printf("[clean] 使用已有key，数据值=%d\n", *data);
}