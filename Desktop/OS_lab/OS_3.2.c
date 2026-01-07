#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h> // For syscall(SYS_gettid) to get Linux TID
#include <time.h>        // For clock_gettime
#define SIZE 4
#define THREAD 4

int arr[SIZE][SIZE];
int result[SIZE][SIZE] = {0};
bool isStart = false;

/*
 * 中文說明：
 * 這個程式讀取名為 number.txt 的 80x80 矩陣，然後用 4 個 pthread 來
 * 計算矩陣與自己相乘（C = A * A）。每個執行緒會負責一段連續的列 (rows)，
 * 使用 busy-wait 機制等待主執行緒發出開始訊號（isStart = true）。
 * 最後主執行緒會加入所有子執行緒，計算結果總和並顯示所花費的時間。
 */

// Structure to pass thread-specific data (start and end row indices)
// 用於傳遞線程特定資料（起始行和結束行索引）的結構
typedef struct {
    int startRow;
    int endRow;
} thread_data_t;

// thread_data_t: 每個執行緒的工作區間，包含開始列與結束列（startRow 包含，endRow 不包含）

/* don't change */
void readMatrix()
{
    FILE *fp;
    fp = fopen("number.txt", "r");
    if (fp == NULL) {
        perror("Error opening number.txt");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < SIZE; i++)
    {
        for (int j = 0; j < SIZE; j++)
        {
            if (fscanf(fp, "%d", &arr[i][j]) != 1) {
                fprintf(stderr, "Error reading matrix element at (%d, %d)\n", i, j);
                fclose(fp);
                exit(EXIT_FAILURE);
            }
        }
    }
    fclose(fp);
}

// readMatrix(): 從檔案讀入 SIZE x SIZE 的整數矩陣到全域陣列 arr

/* don't change */
long sum()
{
    long ret = 0;
    for (int i = 0; i < SIZE; i++)
    {
        for (int j = 0; j < SIZE; j++)
        {
            ret += result[i][j];
        }
    }
    return ret;
}

// sum(): 將 result 矩陣的所有元素加總並回傳（用來驗證結果）

/* don't change */
void diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0)
    {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    }
    else
    {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    printf("Time = %ld seconds and %ld nanoseconds\n", temp.tv_sec, temp.tv_nsec);
}

// diff(): 計算兩個 timespec 的差值並輸出（秒與奈秒）

// Function to perform matrix multiplication (result = arr * arr) for a range of rows
// Each thread handles a distinct set of 'i' rows.

void multipfy(int startIndex, int endIndex)
{
    for (int i = startIndex; i < endIndex; i++) {
        for (int k = 0; k < SIZE; k++) {
            // [i][k] = sum(A[i][j] * A[j][k]) for j=0 to SIZE-1
            for (int j = 0; j < SIZE-1; j++) {
                result[i][k] = sum(arr[i][j] * arr[j][k]);
            }
        }
    }
}

// multipfy(startIndex, endIndex): 計算由 startIndex 到 endIndex-1 的列，
// 對每個 (i,k) 位置做內積累加。注意：result 為全域陣列，函式會直接修改它。

/*
 * 子執行緒函式：執行部分矩陣相乘運算
 * 矩陣相乘公式：C[i][k] = Σ(A[i][j] * A[j][k])，其中 j 從 0 到 SIZE-1
 *
 * 參數 arg: 指向 thread_data_t 結構的 void 指標，包含：
 * - startRow: 此執行緒負責的起始列
 * - endRow: 此執行緒負責的結束列（不含此列）
 *
 * 每個執行緒負責計算：
 * C[i][k] = Σ(A[i][j] * A[j][k])
 * 其中：
 * - i 的範圍是 [startRow, endRow)
 * - k 的範圍是 [0, SIZE)
 * - j 的範圍是 [0, SIZE)
 */
void *child(void *arg)
{
    // 取得並印出處理程序 ID 與執行緒 ID
    pid_t pid = getpid();
    pid_t tid = syscall(SYS_gettid);
    // 輸出順序會因排程器而與主執行緒交錯
    printf("Process Id: %d\n", pid);
    printf("Thread Id: %d\n", tid);

    // 等待主執行緒設定 isStart 為 true（同步障壁）
    while (!isStart);

    // 將參數轉型回 thread_data_t 指標
    thread_data_t *data = (thread_data_t *)arg;

    // 執行指定範圍的矩陣相乘：C[i][k] = Σ(A[i][j] * A[j][k])
    multipfy(data->startRow, data->endRow);

    pthread_exit(NULL);  // 執行完畢，不需要回傳值
}

/*
 * child(): 執行緒進入點。
 * 1. 列印 PID/TID（可觀察主從執行緒的識別號）
 * 2. 使用 while(!isStart) 作為簡單的 barrier，等主執行緒設定 isStart 為 true
 * 3. 呼叫 multipfy 計算指定的列範圍
 * 注意：busy-wait 會消耗 CPU，較好的方式是使用 mutex/cond 或 barrier，
 * 但此程式故意使用簡單的 busy-wait 實作以符合教學實驗需求。
 */

int main()
{
    pthread_t *threads;
    thread_data_t *data; // Array to hold thread data
    struct timespec timeStart, timeEnd;

    // Print PID and TID for the main thread
    pid_t main_pid = getpid();
    pid_t main_tid = syscall(SYS_gettid);
    printf("Process Id: %d\n", main_pid);
    printf("Thread Id: %d\n", main_tid);

    readMatrix();

    // Allocate memory for threads and thread data
    threads = (pthread_t *)malloc(THREAD * sizeof(pthread_t));
    data = (thread_data_t *)malloc(THREAD * sizeof(thread_data_t));
    if (threads == NULL || data == NULL) {
        perror("Failed to allocate memory");
        return 1;
    }

    // Determine row ranges for load balancing (80/4 = 20 rows per thread)
    int rowsPerThread = SIZE / THREAD;
    int currentStartRow = 0;

    for (int i = 0; i < THREAD; i++)
    {
        data[i].startRow = currentStartRow;
        data[i].endRow = currentStartRow + rowsPerThread;
        currentStartRow = data[i].endRow;

        // pthread_create()
        if (pthread_create(&threads[i], NULL, child, &data[i]) != 0) {
            perror("Error creating thread");
            free(threads);
            free(data);
            return 1;
        }
    }

    sleep(1); // Wait for all child threads to reach the busy-wait loop
   
    // Start the timer and signal the threads to begin multiplication
    clock_gettime(CLOCK_REALTIME, &timeStart);
    isStart = true;

    for (int i = 0; i < THREAD; i++)
    {
        // pthread_join(); - Wait for all threads to finish
        pthread_join(threads[i], NULL);
    }
    clock_gettime(CLOCK_REALTIME, &timeEnd);

    // Clean up
    free(threads);
    free(data);

    printf("Sum = %ld\n", sum()); // verify the answer
    diff(timeStart, timeEnd);     // time spent

    return 0;
}

// main()：程式進入點。說明重點：
// - 分配與建立 THREAD 個 pthread，每個執行緒負責一段連續的列
// - 使用 sleep(1) 確保子執行緒已建立並等待（實驗性質做法）
// - 計時開始後設定 isStart=true，子執行緒開始計算
// - 等待所有子執行緒結束後，列印 result 的總和與耗時