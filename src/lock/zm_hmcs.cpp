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

#include <stdlib.h>
#include <assert.h>
#include <malloc.h>
#include <errno.h>
#include <pthread.h>
#include "lock/zm_lock_types.h"

#define DEFAULT_THRESHOLD 256

#define WAIT (0xffffffff)
#define COHORT_START (0x1)
#define ACQUIRE_PARENT (0xcffffffc)

#define handle_error_en(en, msg) \
do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

void SetAffinity(int tid){
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
}

/* Check the actual affinity mask assigned to the thread */
static inline void checkAffinity(int tid) {
    int s, j;
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();
    s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (s != 0)
        handle_error_en(s, "pthread_getaffinity_np");

    int num_hw_threads = 0;
    for (j = 0; j < CPU_SETSIZE; j++)
        if (CPU_ISSET(j, &cpuset))
            num_hw_threads++;
    assert(num_hw_threads==1);
}

static inline void reuse_qnode(zm_mcs_qnode_t *I){
    zm_atomic_store(&I->status, WAIT, zm_memord_release);
    zm_atomic_store(&I->next, ZM_NULL, zm_memord_release);
}

    struct HNode{
        unsigned threshold __attribute__((aligned(ZM_CACHELINE_SIZE)));
        struct HNode * parent __attribute__((aligned(ZM_CACHELINE_SIZE)));
        zm_mcs_t lock __attribute__((aligned(ZM_CACHELINE_SIZE)));
        zm_mcs_qnode_t node __attribute__((aligned(ZM_CACHELINE_SIZE)));

        inline void* operator new(size_t size) {
            void *storage = memalign(ZM_CACHELINE_SIZE, size);
            if(NULL == storage) {
                throw "allocation fail : no free memory";
            }
            return storage;
        }

        inline bool IsTopLevel() {
            return parent == NULL ? true : false;
        }
        /* TODO: Macro or Template this for fast comprison */
        inline unsigned GetThreshold()const {
            return threshold;
        }

        inline void SetThreshold(unsigned t) {
            threshold = t;
        }

    }__attribute__((aligned(ZM_CACHELINE_SIZE)));

    inline static void NormalMCSReleaseWithValue(HNode * L, zm_mcs_qnode_t *I, unsigned val){

        zm_mcs_qnode_t *succ = (zm_mcs_qnode_t *)zm_atomic_load(&I->next, zm_memord_acquire);
        if(succ) {
            zm_atomic_store(&succ->status, val, zm_memord_release);
            return;
        }
        zm_mcs_qnode_t *tmp = I;
        if (zm_atomic_compare_exchange_strong(&(L->lock),
                                              (zm_ptr_t*)&tmp,
                                               ZM_NULL,
                                               zm_memord_acq_rel,
                                               zm_memord_acquire))
            return;
        while(succ == NULL)
            succ = (zm_mcs_qnode_t *)zm_atomic_load(&I->next, zm_memord_acquire); /* SPIN */
        zm_atomic_store(&succ->status, val, zm_memord_release);
        return;
    }

    template<int level>
    struct HMCSLock{
        inline static void AcquireHelper(HNode * L, zm_mcs_qnode_t *I) {
            // Prepare the node for use.
            reuse_qnode(I);
            zm_mcs_qnode_t* pred = (zm_mcs_qnode_t*)zm_atomic_exchange(&(L->lock), (zm_ptr_t)I, zm_memord_acq_rel);
            if(!pred) {
                // I am the first one at this level
                // begining of cohort
                zm_atomic_store(&I->status, COHORT_START, zm_memord_release);
                // Acquire at next level if not at the top level
                HMCSLock<level-1>::AcquireHelper(L->parent, &(L->node));
                return;
            } else {
                zm_atomic_store(&pred->next, I, zm_memord_release);
                for(;;){
                    unsigned myStatus = zm_atomic_load(&I->status, zm_memord_acquire);
                    if(myStatus < ACQUIRE_PARENT) {
                        return;
                    }
                    if(myStatus == ACQUIRE_PARENT) {
                        // beginning of cohort
                        zm_atomic_store(&I->status, COHORT_START, zm_memord_release);
                        // This means this level is acquired and we can start the next level
                        HMCSLock<level-1>::AcquireHelper(L->parent, &(L->node));
                        return;
                    }
                    // spin back; (I->status == WAIT)
                }
            }
        }

        inline static void Acquire(HNode * L, zm_mcs_qnode_t *I) {
            HMCSLock<level>::AcquireHelper(L, I);
            //FORCE_INS_ORDERING();
        }

        inline static void ReleaseHelper(HNode * L, zm_mcs_qnode_t *I) {

            unsigned curCount = zm_atomic_load(&(I->status), zm_memord_acquire) ;
            zm_mcs_qnode_t * succ;

            // Lower level releases
            if(curCount == L->GetThreshold()) {
                // NO KNOWN SUCCESSORS / DESCENDENTS
                // reached threshold and have next level
                // release to next level
                HMCSLock<level - 1>::ReleaseHelper(L->parent, &(L->node));
                //COMMIT_ALL_WRITES();
                // Tap successor at this level and ask to spin acquire next level lock
                NormalMCSReleaseWithValue(L, I, ACQUIRE_PARENT);
                return;
            }

            succ = (zm_mcs_qnode_t*)zm_atomic_load(&(I->next), zm_memord_acquire);
            // Not reached threshold
            if(succ) {
                zm_atomic_store(&succ->status, curCount + 1, zm_memord_release);
                return; // Released
            }
            // No known successor, so release
            HMCSLock<level - 1>::ReleaseHelper(L->parent, &(L->node));
            //COMMIT_ALL_WRITES();
            // Tap successor at this level and ask to spin acquire next level lock
            NormalMCSReleaseWithValue(L, I, ACQUIRE_PARENT);
        }

        inline static void Release(HNode * L, zm_mcs_qnode_t *I) {
            //COMMIT_ALL_WRITES();
            HMCSLock<level>::ReleaseHelper(L, I);
        }

        inline static bool NoWaiters(HNode * L, zm_mcs_qnode_t *I) {
            if(zm_atomic_load(&I->next, zm_memord_acquire) != ZM_NULL)
                return false;
            else
                return HMCSLock<level - 1>::NoWaiters(L->parent, &(L->node));
        }
    };

    template <>
    struct HMCSLock<1> {
        inline static void AcquireHelper(HNode * L, zm_mcs_qnode_t *I) {
            // Prepare the node for use.
            reuse_qnode(I);
            zm_mcs_qnode_t *pred = (zm_mcs_qnode_t*) zm_atomic_exchange(&(L->lock), (zm_ptr_t)I, zm_memord_acq_rel);

            if(!pred) {
                // I am the first one at this level
                return;
            }

            zm_atomic_store(&pred->next, I, zm_memord_release);
            while(zm_atomic_load(&I->status, zm_memord_acquire) == WAIT)
                ; /* SPIN */
            return;
        }


        inline static void Acquire(HNode * L, zm_mcs_qnode_t *I) {
            HMCSLock<1>::AcquireHelper(L, I);
            //FORCE_INS_ORDERING();
        }

        inline static void ReleaseHelper(HNode * L, zm_mcs_qnode_t *I) {
            // Top level release is usual MCS
            // At the top level MCS we always writr COHORT_START since
            // 1. It will release the lock
            // 2. Will never grow large
            // 3. Avoids a read from I->status
            NormalMCSReleaseWithValue(L, I, COHORT_START);
        }

        inline static void Release(HNode * L, zm_mcs_qnode_t *I) {
            //COMMIT_ALL_WRITES();
            HMCSLock<1>::ReleaseHelper(L, I);
        }

        inline static bool NoWaiters(HNode * L, zm_mcs_qnode_t *I) {
            return (zm_atomic_load(&I->next, zm_memord_acquire) == ZM_NULL);
        }
    };

    typedef void (*AcquireFP) (HNode *, zm_mcs_qnode_t *);
    typedef void (*ReleaseFP) (HNode *, zm_mcs_qnode_t *);
    struct HMCSLockWrapper{
        HNode * curNode;
        HNode * rootNode;
        zm_mcs_qnode_t I;
        int curDepth;
        bool tookFP;

        inline void* operator new(size_t size) {
            void *storage = memalign(ZM_CACHELINE_SIZE, size);
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

        inline __attribute__((flatten)) void Acquire(){
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

        inline __attribute__((flatten)) void Release(){
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

        inline __attribute__((flatten)) bool NoWaiters(){
            if(tookFP) {
                return HMCSLock<1>::NoWaiters(curNode, &I);
            }

            switch(curDepth){
                case 1:  return HMCSLock<1>::NoWaiters(curNode, &I);
                case 2:  return HMCSLock<2>::NoWaiters(curNode, &I);
                case 3:  return HMCSLock<3>::NoWaiters(curNode, &I);
                case 4:  return HMCSLock<4>::NoWaiters(curNode, &I);
                case 5:  return HMCSLock<5>::NoWaiters(curNode, &I);
                default: assert(0 && "NYI");
            }
        }
    };


    static zm_thread_local int tid = -1;
    static int threadMappingMax;

//#define IT_MACHINE
//#define THING_MACHINE
#define FIRESTONE_MACHINE
//#define LAPTOP_MACHINE

#if defined(IT_MACHINE)
    static int threadMappings[] = {0, 44, 1, 45, 2, 46, 3, 47, 4, 48, 5, 49, 6, 50, 7, 51, 8, 52, 9, 53, 10, 54, 11, 55, 12, 56, 13, 57, 14, 58, 15, 59, 16, 60, 17, 61, 18, 62, 19, 63, 20, 64, 21, 65, 22, 66, 23, 67, 24, 68, 25, 69, 26, 70, 27, 71, 28, 72, 29, 73, 30, 74, 31, 75, 32, 76, 33, 77, 34, 78, 35, 79, 36, 80, 37, 81, 38, 82, 39, 83, 40, 84, 41, 85, 42, 86, 43, 87};
#elif defined(THING_MACHINE)
    static int threadMappings[] = {0 , 36 , 1 , 37 , 2 , 38 , 3 , 39 , 4 , 40 , 5 , 41 , 6 , 42 , 7 , 43 , 8 , 44 , 9 , 45 , 10 , 46 , 11 , 47 , 12 , 48 , 13 , 49 , 14 , 50 , 15 , 51 , 16 , 52 , 17 , 53 , 18 , 54 , 19 , 55 , 20 , 56 , 21 , 57 , 22 , 58 , 23 , 59 , 24 , 60 , 25 , 61 , 26 , 62 , 27 , 63 , 28 , 64 , 29 , 65 , 30 , 66 , 31 , 67 , 32 , 68 , 33 , 69 , 34 , 70 , 35 , 71};
#elif defined(FIRESTONE_MACHINE)
static int threadMappings[] = {0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10 , 11 , 12 , 13 , 14 , 15 , 16 , 17 , 18 , 19 , 20 , 21 , 22 , 23 , 24 , 25 , 26 , 27 , 28 , 29 , 30 , 31 , 32 , 33 , 34 , 35 , 36 , 37 , 38 , 39 , 40 , 41 , 42 , 43 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 53 , 54 , 55 , 56 , 57 , 58 , 59 , 60 , 61 , 62 , 63 , 64 , 65 , 66 , 67 , 68 , 69 , 70 , 71 , 72 , 73 , 74 , 75 , 76 , 77 , 78 , 79 , 80 , 81 , 82 , 83 , 84 , 85 , 86 , 87 , 88 , 89 , 90 , 91 , 92 , 93 , 94 , 95 , 96 , 97 , 98 , 99 , 100 , 101 , 102 , 103 , 104 , 105 , 106 , 107 , 108 , 109 , 110 , 111 , 112 , 113 , 114 , 115 , 116 , 117 , 118 , 119 , 120 , 121 , 122 , 123 , 124 , 125 , 126 , 127 , 128 , 129 , 130 , 131 , 132 , 133 , 134 , 135 , 136 , 137 , 138 , 139 , 140 , 141 , 142 , 143 , 144 , 145 , 146 , 147 , 148 , 149 , 150 , 151 , 152 , 153 , 154 , 155 , 156 , 157 , 158 , 159};
#elif defined(LAPTOP_MACHINE)
    static int threadMappings[] = {0 , 1 , 2 , 3};
#else
#error "Machine topology not recognized"
#endif


struct IzemHMCSLock{
    // Assumes tids range from [0.. maxThreads)
    // Assumes that tid 0 is close to tid and so on.
    HNode ** lockLocations __attribute__((aligned(ZM_CACHELINE_SIZE)));
    HMCSLockWrapper ** leafNodes __attribute__((aligned(ZM_CACHELINE_SIZE)));
    int GetHWThreadId(int id){
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
        lockLocations = (HNode**)memalign(ZM_CACHELINE_SIZE, sizeof(HNode*) * totalLocksNeeded);
        leafNodes = (HMCSLockWrapper**)memalign(ZM_CACHELINE_SIZE, sizeof(HMCSLockWrapper*) * maxThreads);


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

    inline __attribute__((flatten)) void Acquire(){
        if (zm_unlikely(tid == -1)) {
            tid = sched_getcpu();
            checkAffinity(tid);
            tid = GetHWThreadId(tid);
        }
        leafNodes[tid]->Acquire();
    }

    inline __attribute__((flatten)) void Release(){
        leafNodes[tid]->Release();
    }

    inline __attribute__((flatten)) bool NoWaiters(){
        return leafNodes[tid]->NoWaiters();
    }
};


void IzemHMCSLockInit(zm_hmcs_t *handle){
    // Get config;
#if defined(IT_MACHINE)
    int maxThreads = 88;
    int  levels = 3;
    int  participantsAtLevel[] = {2,22,88};
#elif defined(THING_MACHINE)
    int maxThreads = 72;
    int  levels = 3;
    int  participantsAtLevel[] = {2,18,72};
#elif defined(FIRESTONE_MACHINE)
    int maxThreads = 160;
    int  levels = 3;
    int  participantsAtLevel[] = {8,80,160};
#elif defined(LAPTOP_MACHINE)
    int maxThreads = 4;
    int  levels = 2;
    int  participantsAtLevel[] = {2,4};
#endif
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
    int zm_hmcs_nowaiters(zm_hmcs_t L){
        return (int) ((struct IzemHMCSLock*)L)->NoWaiters();
    }
}
