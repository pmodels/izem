/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

/*
 * The original version of this code was contributed by Milind Chabbi
 * based on the work when he was at Rice University. It relies on the
 * HMCS lock description in [1] and the fast path described in [2].
 *
 * [1] Chabbi, Milind, Michael Fagan, and John Mellor-Crummey. "High
 * performance locks for multi-level NUMA systems." In Proceedings of
 * the ACM SIGPLAN Symposium on Principles and Practice of Parallel
 * Programming (PPoPP'15), ACM, 2015.
 *
 * [2] Chabbi, Milind, and John Mellor-Crummey. "Contention-conscious,
 * locality-preserving locks." In Proceedings of the 21st ACM SIGPLAN
 * Symposium on Principles and Practice of Parallel Programming (PPoPP'16,
 * p. 22. ACM, 2016.
 */


#include <algorithm>    // std::sort
#include <vector>    // std::sort

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <assert.h>
#include <sys/time.h>
#include <iostream>
#include <malloc.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>    /* For SYS_xxx definitions */
#include <time.h>
#include <unistd.h>
#if __cplusplus >= 201103L
#include <atomic>
#endif
#include "lock/zm_lock_types.h"

#define DEFAULT_THRESHOLD 256

#define  LOCKED (false)
#define  UNLOCKED (true)

#define WAIT (0xffffffffffffffff)
//#define ACQUIRE_PARENT (0xfffffffffffffffe)
#define COHORT_START (0x1)
#define ABORTED (0xeffffffffffffffe)
#define READY_TO_USE (0xdffffffffffffffd)
#define MOVED_ON (QNode *)(0xdffffffffffffffd)
#define ACQUIRE_PARENT (0xcffffffffffffffc)
#define CANT_WAIT_FOR_NEXT (QNode *)(0x1)



#define ALARM_TIME (3 * 60)

#if defined(__xlC__) || defined (__xlc__)
#include<builtins.h>
static inline int64_t PPCSwap(volatile int64_t * addr, int64_t value) {
 	for(;;){
		const int64_t oldVal = __ldarx(addr);
		if(__stdcx(addr, value)) {
            //__isync();
			return oldVal;
		}
	}
}

static inline bool PPCBoolCompareAndSwap(volatile int64_t * addr, int64_t oldValue, int64_t newValue) {
    for(;;) {
        const int64_t val = __ldarx(addr);
        if (val != oldValue) {
            return false;
        }
        if(__stdcx(addr, newValue)) {
            return true;
        }
    }
}
#define CAS(location, oldValue, newValue) assert(0 && "NYI")
#define SWAP(location, value) PPCSwap((volatile int64_t *)location, (int64_t)value)
#define BOOL_CAS(location, oldValue, newValue) PPCBoolCompareAndSwap((volatile int64_t *)location, (int64_t)oldValue, (int64_t)newValue)
#define ATOMIC_ADD(location, value) __fetch_and_addlp((volatile int64_t *) location, (int64_t) value)
#define FORCE_INS_ORDERING() __isync()
#define COMMIT_ALL_WRITES() __lwsync()
#define GET_TICK(var) __asm__ __volatile__ ("mfspr %0, 268\n\t": "=r" (var): )
#else
// ASSUME __GNUC__
#define CAS(location, oldValue, newValue) __sync_val_compare_and_swap(location, oldValue, newValue)
#define SWAP(location, value) __sync_lock_test_and_set(location, value)
#define BOOL_CAS(location, oldValue, newValue) __sync_bool_compare_and_swap(location, oldValue, newValue)
#define ATOMIC_ADD(location, value) __sync_fetch_and_add((volatile int64_t *) location, (int64_t) value)

#ifdef __PPC__
#define FORCE_INS_ORDERING() __asm__ __volatile__ (" isync\n\t")
#define COMMIT_ALL_WRITES() __asm__ __volatile__ (" lwsync\n\t")
#define GET_TICK(var) __asm__ __volatile__ ("mfspr %0, 268\n\t": "=r" (var): )

#elif defined(__x86_64__)
#define FORCE_INS_ORDERING() do{}while(0)
#define COMMIT_ALL_WRITES() do{}while(0)
#define GET_TICK(var) assert(0 && "NYI")
#else
assert( 0 && "unsupported platform");
#endif

#endif

#define AtomicWrite(loc, value) std::atomic_store_explicit( (volatile std::atomic<uint64_t> *)(loc), (value), std::memory_order_relaxed)

#define AtomicLoad(loc) std::atomic_load_explicit( (const volatile std::atomic<uint64_t> *)(loc), std::memory_order_relaxed)

#define TIME_SPENT(start, end) (end.tv_sec * 1000000 + end.tv_usec - start.tv_sec*1000000 - start.tv_usec)

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE (128)
#endif

//#define VALIDATE
#define CHECK_THREAD_AFFINITY

#ifdef VALIDATE
volatile int var = 0;
#endif

#define handle_error_en(en, msg) \
do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

/* taken from https://computing.llnl.gov/tutorials/pthreads/man/pthread_setaffinity_np.txt */
void SetAffinity(int tid){
#ifdef BLACKLIGHT
    return;
#endif
    int s, j;
    cpu_set_t cpuset;
    pthread_t thread;
    
    thread = pthread_self();
    
    /* Set affinity mask to include CPUs tid */
    
    CPU_ZERO(&cpuset);
    CPU_SET(1*tid, &cpuset);

    s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (s != 0)
        handle_error_en(s, "pthread_setaffinity_np");
#if 0
    /* Check the actual affinity mask assigned to the thread */
    
    s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (s != 0)
        handle_error_en(s, "pthread_getaffinity_np");
    
    printf("Set returned by pthread_getaffinity_np() contained:\n");
    for (j = 0; j < CPU_SETSIZE; j++)
        if (CPU_ISSET(j, &cpuset))
            printf("    %d: CPU %d\n", tid,j);
#endif
}

#if defined(__i386__)

static __inline__ unsigned long long rdtsc(void)
{
    unsigned long long int x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}

#elif defined(__x86_64__)

static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#endif

static inline int64_t GetFastClockTick() {return rdtsc();}

    struct QNode{
        struct QNode * volatile next __attribute__((aligned(CACHE_LINE_SIZE)));
        volatile uint64_t status __attribute__((aligned(CACHE_LINE_SIZE)));
        char buf[CACHE_LINE_SIZE-sizeof(uint64_t)-sizeof(struct QNode *)];
        QNode() : status(WAIT), next(NULL) {}
        
        inline __attribute__((always_inline)) void* operator new(size_t size) {
            void *storage = memalign(CACHE_LINE_SIZE, size);
            if(NULL == storage) {
                throw "allocation fail : no free memory";
            }
            return storage;
        }
        
        inline __attribute__((always_inline)) void Reuse(){
            status = WAIT;
            next = NULL;
            // Updates must happen before swap
            COMMIT_ALL_WRITES();
        }
    }__attribute__((aligned(CACHE_LINE_SIZE)));

    struct HNode{
        int threshold __attribute__((aligned(CACHE_LINE_SIZE)));
        struct HNode * parent __attribute__((aligned(CACHE_LINE_SIZE)));
        struct QNode *  volatile lock __attribute__((aligned(CACHE_LINE_SIZE)));
        struct QNode  node __attribute__((aligned(CACHE_LINE_SIZE)));
        
        inline __attribute__((always_inline)) void* operator new(size_t size) {
            void *storage = memalign(CACHE_LINE_SIZE, size);
            if(NULL == storage) {
                throw "allocation fail : no free memory";
            }
            return storage;
        }
        
        inline __attribute__((always_inline)) bool IsTopLevel() {
            return parent == NULL ? true : false;
        }
        /* TODO: Macro or Template this for fast comprison */
        inline __attribute__((always_inline)) uint64_t GetThreshold()const {
            return threshold;
        }
        
        inline __attribute__((always_inline)) void SetThreshold(uint64_t t) {
            threshold = t;
        }
        
    }__attribute__((aligned(CACHE_LINE_SIZE)));

//    int threshold;
//    int * thresholdAtLevel;

//    inline __attribute__((always_inline)) int GetThresholdAtLevel(int level){
//        return thresholdAtLevel[level];
//    }

    inline __attribute__((always_inline)) static void NormalMCSReleaseWithValue(HNode * L, QNode *I, uint64_t val){
        QNode * succ = I->next;
        if(succ) {
            succ->status = val;
            return;
        }
        if (BOOL_CAS(&(L->lock), I, NULL))
            return;
        while( (succ=I->next) == NULL);
        succ->status = val;
        return;
    }

    template<int level>
    struct HMCSLock{
        inline __attribute__((always_inline)) static void AcquireHelper(HNode * L, QNode *I) {
            // Prepare the node for use.
            I->Reuse();
            QNode * pred = (QNode *) SWAP(&(L->lock), I);
            if(!pred) {
                // I am the first one at this level
                // begining of cohort
                I->status = COHORT_START;
                // Acquire at next level if not at the top level
                HMCSLock<level-1>::AcquireHelper(L->parent, &(L->node));
                return;
            } else {
                pred->next = I;
                for(;;){
                    uint64_t myStatus = I->status;
                    if(myStatus < ACQUIRE_PARENT) {
                        return;
                    }
                    if(myStatus == ACQUIRE_PARENT) {
                        // beginning of cohort
                        I->status = COHORT_START;
                        // This means this level is acquired and we can start the next level
                        HMCSLock<level-1>::AcquireHelper(L->parent, &(L->node));
                        return;
                    }
                    // spin back; (I->status == WAIT)
                }
            }
        }
        
        inline __attribute__((always_inline)) static void Acquire(HNode * L, QNode *I) {
            HMCSLock<level>::AcquireHelper(L, I);
            FORCE_INS_ORDERING();
        }
        
        inline __attribute__((always_inline)) static void ReleaseHelper(HNode * L, QNode *I) {
            
            uint64_t curCount = I->status;
            QNode * succ;
            
            // Lower level releases
            if(curCount == L->GetThreshold()) {
                // NO KNOWN SUCCESSORS / DESCENDENTS
                // reached threshold and have next level
                // release to next level
                HMCSLock<level - 1>::ReleaseHelper(L->parent, &(L->node));
                COMMIT_ALL_WRITES();
                // Tap successor at this level and ask to spin acquire next level lock
                NormalMCSReleaseWithValue(L, I, ACQUIRE_PARENT);
                return;
            }
            
            succ = I->next;
            // Not reached threshold
            if(succ) {
                succ->status = curCount + 1;
                return; // Released
            }
            // No known successor, so release
            HMCSLock<level - 1>::ReleaseHelper(L->parent, &(L->node));
            COMMIT_ALL_WRITES();
            // Tap successor at this level and ask to spin acquire next level lock
            NormalMCSReleaseWithValue(L, I, ACQUIRE_PARENT);
        }
        
        inline __attribute__((always_inline)) static void Release(HNode * L, QNode *I) {
            COMMIT_ALL_WRITES();
            HMCSLock<level>::ReleaseHelper(L, I);
        }
    };

    template <>
    struct HMCSLock<1> {
        inline __attribute__((always_inline)) static void AcquireHelper(HNode * L, QNode *I) {
            // Prepare the node for use.
            I->Reuse();
            QNode * pred = (QNode *) SWAP(&(L->lock), I);
            if(!pred) {
                // I am the first one at this level
                return;
            }
            pred->next = I;
            while(I->status==WAIT);
            return;
        }
        
        
        inline __attribute__((always_inline)) static void Acquire(HNode * L, QNode *I) {
            HMCSLock<1>::AcquireHelper(L, I);
            FORCE_INS_ORDERING();
        }
        
        inline __attribute__((always_inline)) static void ReleaseHelper(HNode * L, QNode *I) {
            // Top level release is usual MCS
            // At the top level MCS we always writr COHORT_START since
            // 1. It will release the lock
            // 2. Will never grow large
            // 3. Avoids a read from I->status
            NormalMCSReleaseWithValue(L, I, COHORT_START);
        }
        
        inline __attribute__((always_inline)) static void Release(HNode * L, QNode *I) {
            COMMIT_ALL_WRITES();
            HMCSLock<1>::ReleaseHelper(L, I);
        }
    };

    typedef void (*AcquireFP) (HNode *, QNode *);
    typedef void (*ReleaseFP) (HNode *, QNode *);
    struct HMCSLockWrapper{
        HNode * curNode;
        HNode * rootNode;
        QNode I;
        int curDepth;
        bool tookFP;
        
        inline __attribute__((always_inline)) void* operator new(size_t size) {
            void *storage = memalign(CACHE_LINE_SIZE, size);
            if(NULL == storage) {
                throw "allocation fail : no free memory";
            }
            return storage;
        }
        
        HMCSLockWrapper(HNode * h, int depth) : curNode(h), curDepth(depth), tookFP(false) {
            HNode * tmp;
            for(tmp = curNode; tmp->parent != NULL; tmp = tmp->parent);
            rootNode = tmp;
        }
        
        inline __attribute__((always_inline)) __attribute__((flatten)) void Acquire(){
            if(curNode->lock == NULL && rootNode->lock == NULL) {
                // go FP
                tookFP = true;
                HMCSLock<1>::Acquire(rootNode, &I);
                return;
            }
            switch(curDepth){
                case 1:  HMCSLock<1>::Acquire(curNode, &I); break;
                case 2:  HMCSLock<2>::Acquire(curNode, &I); break;
                case 3:  HMCSLock<3>::Acquire(curNode, &I); break;
                case 4:  HMCSLock<4>::Acquire(curNode, &I); break;
                case 5:  HMCSLock<5>::Acquire(curNode, &I); break;
                default: assert(0 && "NYI");
            }
            return;
        }
        
        inline __attribute__((always_inline)) __attribute__((flatten)) void Release(){
            //myRelease(curNode, I);
            if(tookFP) {
                HMCSLock<1>::Release(rootNode, &I);
                tookFP = false;
                return;
            }
            switch(curDepth){
                case 1:  HMCSLock<1>::Release(curNode, &I); break;
                case 2:  HMCSLock<2>::Release(curNode, &I); break;
                case 3:  HMCSLock<3>::Release(curNode, &I); break;
                case 4:  HMCSLock<4>::Release(curNode, &I); break;
                case 5:  HMCSLock<5>::Release(curNode, &I); break;
                default: assert(0 && "NYI");
            }
        }
    };


    static zm_thread_local int tid = -1;
    static int threadMappingMax;
    static int threadMappings[] = {0 , 36 , 1 , 37 , 2 , 38 , 3 , 39 , 4 , 40 , 5 , 41 , 6 , 42 , 7 , 43 , 8 , 44 , 9 , 45 , 10 , 46 , 11 , 47 , 12 , 48 , 13 , 49 , 14 , 50 , 15 , 51 , 16 , 52 , 17 , 53 , 18 , 54 , 19 , 55 , 20 , 56 , 21 , 57 , 22 , 58 , 23 , 59 , 24 , 60 , 25 , 61 , 26 , 62 , 27 , 63 , 28 , 64 , 29 , 65 , 30 , 66 , 31 , 67 , 32 , 68 , 33 , 69 , 34 , 70 , 35 , 71};




struct IzemHMCSLock{
    // Assumes tids range from [0.. maxThreads)
    // Assumes that tid 0 is close to tid and so on.
    HNode ** lockLocations __attribute__((aligned(CACHE_LINE_SIZE)));
    HMCSLockWrapper ** leafNodes __attribute__((aligned(CACHE_LINE_SIZE)));
    int GetRealTid(int id){ 
       for(int i = 0 ; i < threadMappingMax; i++)
           if(id == threadMappings[i])
                return i;

       assert(0 && "Should never reach here");
    }
    IzemHMCSLock(int maxThreads, int levels, int * participantsAtLevel){
         threadMappingMax = maxThreads;
        // Total locks needed = participantsPerLevel[1] + participantsPerLevel[2] + .. participantsPerLevel[levels-1] + 1
        // Save affinity
        pthread_t thread = pthread_self();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        
        int totalLocksNeeded = 0;
        
        for (int i=0; i < levels; i++) {
            totalLocksNeeded += maxThreads / participantsAtLevel[i] ;
        }
        lockLocations = (HNode**)memalign(CACHE_LINE_SIZE, sizeof(HNode*) * totalLocksNeeded);
        leafNodes = (HMCSLockWrapper**)memalign(CACHE_LINE_SIZE, sizeof(HMCSLockWrapper*) * maxThreads);
        
        
        for(int tid = 0 ; tid < maxThreads; tid ++){
            SetAffinity(threadMappings[tid]);
            // Pin me to hw-thread-id = tid
            int lastLockLocationEnd = 0;
            for(int curLevel = 0 ; curLevel < levels; curLevel++){
                if (tid%participantsAtLevel[curLevel] == 0) {
                    // master, initialize the lock
                    int lockLocation = lastLockLocationEnd + tid/participantsAtLevel[curLevel];
                    lastLockLocationEnd += maxThreads/participantsAtLevel[curLevel];
                    HNode * curLock = new HNode();
                    //curLock->threshold = GetThresholdAtLevel(curLevel);
                    curLock->threshold = DEFAULT_THRESHOLD;
                    curLock->parent = NULL;
                    curLock->lock = NULL;
                    lockLocations[lockLocation] = curLock;
                }
            }
        }
        
        // setup parents
        for(int tid = 0 ; tid < maxThreads; tid ++){
            SetAffinity(threadMappings[tid]);
            int lastLockLocationEnd = 0;
            for(int curLevel = 0 ; curLevel < levels - 1; curLevel++){
                if (tid%participantsAtLevel[curLevel] == 0) {
                    int lockLocation = lastLockLocationEnd + tid/participantsAtLevel[curLevel];
                    lastLockLocationEnd += maxThreads/participantsAtLevel[curLevel];
                    int parentLockLocation = lastLockLocationEnd + tid/participantsAtLevel[curLevel+1];
                    lockLocations[lockLocation]->parent = lockLocations[parentLockLocation];
                }
            }
            leafNodes[tid] = new HMCSLockWrapper(lockLocations[tid/participantsAtLevel[0]], levels);
        }
        free(lockLocations);
        // Restore affinity
        pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    }
    
    inline __attribute__((always_inline)) __attribute__((flatten)) void Acquire(){
        if (zm_unlikely(tid == -1)) {
            tid = sched_getcpu();
            tid = GetRealTid(tid);
        }
        leafNodes[tid]->Acquire();
    }
    
    inline __attribute__((always_inline)) __attribute__((flatten)) void Release(){
        leafNodes[tid]->Release();
    }
};


void IzemHMCSLockInit(zm_hmcs_t *handle){
    // Get config;
    int maxThreads = 72;
    int  levels = 3;
    int  participantsAtLevel[] = {2,18,72};
    *handle  = (zm_hmcs_t) new IzemHMCSLock(maxThreads, levels, participantsAtLevel);
}

extern "C" {

    int zm_hmcs_init(zm_hmcs_t * handle) {
        IzemHMCSLockInit(handle);
        return 0;
    }

    int zm_hmcs_acquire(zm_hmcs_t L){
        ((struct IzemHMCSLock*)L)->Acquire();
        return 0;
    }
    int zm_hmcs_release(zm_hmcs_t L){
        ((struct IzemHMCSLock*)L)->Release();
        return 0;
    }

}
