#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>


// 全域資料結構
typedef struct global_data
{
    int data; // 共用變數
    atomic_flag m; // semaphore的鎖 spinlock
} global_data;

// 宣告全域變數
global_data gdata; 


//  自製 semaphore (spinlock)

/* 嘗試取得鎖 */
void semwait()
{
    
    /*
    
    atomic_flag_test_and_set() 會將 flag (m) 設為 1，並回傳舊值。

    m = 0 -> atomic_flag_test_and_set() 回傳 0 -> thread 取得鎖 並將 m = 1
    m = 1 -> atomic_flag_test_and_set() 回傳 1 -> thread 無法取得鎖 會一直在 while中一直busy waiting
    
    */
    while (atomic_flag_test_and_set(&gdata.m));  // spin
}


/* 釋放鎖 */
void semsignal()
{
    // 將鎖釋放， m = 0
    atomic_flag_clear(&gdata.m);
}


//  Thread 1: 做 100000 次減法
void *subchild()
{
    for (int i = 0; i < 100000; i++)
    {
        semwait(); // 嘗試取得鎖 如果鎖被占用就一直等待

        gdata.data--;
        
        semsignal(); // 釋放鎖
        //printf("subchild = %d\n", gdata.data);
    }

    // 結束執行緒
    pthread_exit(NULL);
}


//  Thread 2: 做 100000 次加法
void *addchild()
{
    for (int i = 0; i < 100000; i++)
    {
        semwait(); // 嘗試取得鎖 如果鎖被占用就一直等待

        gdata.data++;
        
        semsignal(); // 釋放鎖

        //printf("addchild = %d\n", gdata.data);
    }

    // 結束執行緒
    pthread_exit(NULL);
}

int main()
{
    pthread_t t1, t2;

    // 初始化共用變數
    gdata.data = 0;

    // 初始化 spinlock ，將 m 設為 0 (unlocked)
    atomic_flag_clear(&gdata.m);

    pthread_create(&t1, NULL, subchild, NULL);
    pthread_create(&t2, NULL, addchild, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Final data = %d\n", gdata.data);

    return 0;
}
