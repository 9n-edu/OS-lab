#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#define SIZE 3  // 定義矩陣大小為 3x3

// 宣告全域變數
int arr[SIZE][SIZE];         // 用於儲存輸入矩陣
int result[SIZE][SIZE] = {0};// 用於儲存矩陣相乘結果
bool isStart = false;        // 控制子執行緒開始執行的旗標

// 定義結構體用於傳遞執行緒 ID
typedef struct my_pid {
    int pid;
} my_pid;

/* don't change */
// 從檔案讀取矩陣資料
void readMatrix()
{
    FILE *fp;
    fp = fopen("number.txt", "r");
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            fscanf(fp, "%d", &arr[i][j]);
        }
    }
    fclose(fp);
}

// 執行矩陣相乘運算
void multipfy()
{
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            for (int k = 0; k < SIZE; k++) {
                result[i][j] += arr[i][k] * arr[k][j];
            }
        }
    }
}

// 計算結果矩陣所有元素的總和
long sum()
{
    long ret = 0;
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            ret += result[i][j];
        }
    }
    return ret;
}

/* don't change */
// 計算兩個時間點之間的時間差
// timespec 結構包含兩個部分：
// - tv_sec：秒數
// - tv_nsec：奈秒數（10^-9 秒）
void diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    // 處理奈秒相減可能出現負數的情況
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        // 如果 end 的奈秒小於 start 的奈秒
        // 需要向秒數借位（1秒 = 1000000000奈秒）
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;  // 秒數減 1
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;  // 借 1 秒補足奈秒
    } else {
        // 一般情況下的直接相減
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    printf("Time = %ld seconds and %ld nanoseconds\n", temp.tv_sec, temp.tv_nsec);
}

/* ✅ 正確取得最後一個 core index */
int getLastCore()
{
    FILE *fp = popen("nproc --all", "r");
    int cpuNum = 0;
    fscanf(fp, "%d", &cpuNum);
    pclose(fp);
    return cpuNum - 1;
}

/*
 * 將執行緒綁定到最後一顆 CPU 核心的函式
 * 參數：
 *   - pid: 指向 my_pid 結構的指標，包含要綁定的執行緒 ID
 * 
 * 運作流程：
 * 1. 取得系統最後一顆 CPU 核心的編號
 * 2. 等待子執行緒設置好自己的 PID（因為執行緒需要先啟動才能取得其 ID）
 * 3. 使用 Linux 的 taskset 命令將指定的執行緒綁定到特定核心
 * 
 * 注意：
 * - 使用 busy-wait (while loop) 等待執行緒 ID 準備就緒
 * - taskset -cp 命令用於在執行時期改變執行緒的 CPU 親和性（affinity）
 * - 此函式為 Linux 專用，依賴 taskset 命令的可用性
 */
void setTaskToCore(my_pid *pid)
{
    int core = getLastCore();
    while (pid->pid == 0); // 等待執行緒的 PID 準備就緒

    char cmd[100];
    sprintf(cmd, "taskset -cp %d %d", core, pid->pid);  // 建構 taskset 命令
    system(cmd);  // 執行命令以設定 CPU 親和性
}

/* child thread */
// 子執行緒的主要函數
void *child(void *arg)
{
    // 配置記憶體用於儲存回傳值
    long *ret = calloc(1, sizeof(long)); //配置一個元素，參數大小為long
    my_pid *pid = (my_pid *)arg;

    // 取得當前執行緒的 ID
    pid->pid = syscall(SYS_gettid);

    // 印出處理程序 ID 和執行緒 ID
    printf("Process Id: %ld\n", syscall(SYS_getpid));
    printf("Thread Id: %d\n", pid->pid);

    // 等待主執行緒發出開始信號
    while (!isStart);
    multipfy();  // 執行矩陣相乘
    *ret = sum();// 計算結果總和

    pthread_exit(ret);
}

int main()
{
    pthread_t thread;        // 宣告執行緒變數
    void *ret;              // 用於儲存執行緒回傳值
    my_pid pid = {0};       // 初始化 pid 結構體
    struct timespec timeStart, timeEnd;  // 用於計時

    // 印出主執行緒的處理程序 ID 和執行緒 ID
    printf("Process Id: %d\n", getpid());
    printf("Thread Id: %ld\n", syscall(SYS_gettid));

    // 讀取矩陣資料
    readMatrix();

    // 建立子執行緒
    pthread_create(&thread, NULL, child, &pid);// 傳遞 pid 結構體給子執行緒

    // 將子執行緒綁定到最後一個 CPU 核心
    // 取得CPU核心數、找到最後一顆核心編號
    setTaskToCore(&pid); 

    sleep(1);  // 等待 1 秒確保設定完成

    // 開始計時並發出開始信號
    isStart = true;
    clock_gettime(CLOCK_REALTIME, &timeStart);

    // 等待子執行緒完成
    pthread_join(thread, &ret);

    // 結束計時
    clock_gettime(CLOCK_REALTIME, &timeEnd);

    // 輸出結果和執行時間
    printf("Sum = %ld\n", *(long *)ret);
    diff(timeStart, timeEnd);
    free(ret);  // 釋放配置的記憶體

    return 0;
}
