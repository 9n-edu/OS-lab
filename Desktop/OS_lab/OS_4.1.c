#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

int data = 0;               // global variable 全域變數

atomic_int turn = 0;        // Peterson's Algorithm
atomic_int flag[2] = {0};  // 指出哪個執行緒想進入 critical section


//  子執行緒：做 100000 次減法
void *subchild()
{
    for (int i = 0; i < 100000; i++)
    {
        flag[0] = 1;    // 告訴系統：我 (thread 0) 想進入 critical section
        turn = 1;      // 讓對方 (thread 1) 先，如果雙方同時想進，就讓 thread1 優先
        
        /* 
           若同時滿足以下兩點，就必須等待：
           (1) 對方也想進入 (flag[1] == 1)
           (2) turn 指向 1，表示我需要讓對方先
        */
        while (flag[1] == 1 && turn == 1);  // busy waiting


        // ===== Critical Section =====
        // 此區域執行時，保證 thread0 與 thread1 不會同時進入
        data--;
        // ============================

        //printf("subchild = %d\n", data);
    
        // 表示我(thread0) 已經離開 critical section
        flag[0] = 0;
    }

    // 結束執行緒
    pthread_exit(NULL);
}



//  子執行緒：做 100000 次加法
void *addchild()
{
    for (int i = 0; i < 100000; i++)
    {

        flag[1] = 1;       // 告訴系統想進入 critical section
        turn = 0;          // 輪到對方先 ，如果雙方同時想進，就讓 thread0 (subchild) 優先

        /*
            若同時滿足以下兩點，就必須等待：
            (1) 對方也想進入 (flag[0] == 1)
            (2) turn 指向 0，表示我需要讓對方先
        */
        while (flag[0] == 1 && turn == 0) 
            ;              // busy waiting

        // ===== Critical Section =====
        data++;
        // ============================

        //printf("addchild = %d\n", data);

        flag[1] = 0;       // 離開 critical section
    }

    // 結束執行緒
    pthread_exit(NULL);
}

int main()
{
    pthread_t t1, t2;

    pthread_create(&t1, NULL, subchild, NULL); // 建立減法執行緒
    pthread_create(&t2, NULL, addchild, NULL); // 建立加法執行緒

    pthread_join(t1, NULL); // 等待減法執行緒結束
    pthread_join(t2, NULL); // 等待加法執行緒結束

    printf("Final data = %d\n", data);


    return 0;
}