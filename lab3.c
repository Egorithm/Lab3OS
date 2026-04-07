#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

struct MarkerParams {
    int id;
    int* arr;
    int size;
    CRITICAL_SECTION* cs;
    HANDLE start_event;
    HANDLE continue_event;
    HANDLE terminate_event;
    HANDLE blocked_event;
};

DWORD WINAPI MarkerThread(LPVOID lpParam) {
    MarkerParams* p = (MarkerParams*)lpParam;
    WaitForSingleObject(p->start_event, INFINITE);
    srand(p->id);
    int markedCount = 0;

    while (true) {
        int r = rand();
        int index = r % p->size;

        EnterCriticalSection(p->cs);
        if (p->arr[index] == 0) {
            Sleep(5);
            p->arr[index] = p->id;
            Sleep(5);
            markedCount++;
            LeaveCriticalSection(p->cs);
        }
        else {
            LeaveCriticalSection(p->cs);
            printf("Marker %d: marked %d elements, can't mark index %d\n",
                p->id, markedCount, index);
            SetEvent(p->blocked_event);
            HANDLE events[2] = { p->continue_event, p->terminate_event };
            DWORD res = WaitForMultipleObjects(2, events, FALSE, INFINITE);
            if (res == WAIT_OBJECT_0) {
                continue;
            }
            else if (res == WAIT_OBJECT_0 + 1) {
                EnterCriticalSection(p->cs);
                for (int i = 0; i < p->size; ++i) {
                    if (p->arr[i] == p->id) {
                        p->arr[i] = 0;
                    }
                }
                LeaveCriticalSection(p->cs);
                return 0;
            }
        }
    }
    return 0;
}

int main() {
    int N, K;
    printf("Enter array size: ");
    scanf("%d", &N);
    int* array = new int[N]();

    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);

    printf("Enter number of marker threads: ");
    scanf("%d", &K);

    HANDLE start_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    HANDLE continue_event = CreateEvent(NULL, TRUE, FALSE, NULL);

    HANDLE* threads = new HANDLE[K];
    HANDLE* blocked_events = new HANDLE[K];
    HANDLE* terminate_events = new HANDLE[K];
    MarkerParams* params = new MarkerParams[K];

    for (int i = 0; i < K; ++i) {
        blocked_events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        terminate_events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);

        params[i].id = i + 1;
        params[i].arr = array;
        params[i].size = N;
        params[i].cs = &cs;
        params[i].start_event = start_event;
        params[i].continue_event = continue_event;
        params[i].terminate_event = terminate_events[i];
        params[i].blocked_event = blocked_events[i];

        threads[i] = CreateThread(NULL, 0, MarkerThread, &params[i], 0, NULL);
    }

    SetEvent(start_event);

    bool* active = new bool[K];
    for (int i = 0; i < K; ++i) active[i] = true;
    int activeCount = K;

    while (activeCount > 0) {
        HANDLE* waitEvents = new HANDLE[activeCount];
        int idx = 0;
        for (int i = 0; i < K; ++i) {
            if (active[i]) {
                waitEvents[idx++] = blocked_events[i];
            }
        }
        WaitForMultipleObjects(activeCount, waitEvents, TRUE, INFINITE);
        delete[] waitEvents;

        printf("\nCurrent array: ");
        EnterCriticalSection(&cs);
        for (int i = 0; i < N; ++i) {
            printf("%d ", array[i]);
        }
        LeaveCriticalSection(&cs);
        printf("\n");

        int termId;
        do {
            printf("Enter marker number to terminate (1..%d): ", K);
            scanf("%d", &termId);
        } while (termId < 1 || termId > K || !active[termId - 1]);
        int termIdx = termId - 1;

        SetEvent(terminate_events[termIdx]);
        WaitForSingleObject(threads[termIdx], INFINITE);
        CloseHandle(threads[termIdx]);

        printf("Array after termination: ");
        EnterCriticalSection(&cs);
        for (int i = 0; i < N; ++i) {
            printf("%d ", array[i]);
        }
        LeaveCriticalSection(&cs);
        printf("\n");

        active[termIdx] = false;
        activeCount--;

        CloseHandle(blocked_events[termIdx]);
        CloseHandle(terminate_events[termIdx]);

        if (activeCount > 0) {
            SetEvent(continue_event);
            ResetEvent(continue_event);
        }
    }

    printf("All marker threads finished.\n");

    CloseHandle(start_event);
    CloseHandle(continue_event);
    DeleteCriticalSection(&cs);
    delete[] array;
    delete[] threads;
    delete[] blocked_events;
    delete[] terminate_events;
    delete[] params;
    delete[] active;

    return 0;
}