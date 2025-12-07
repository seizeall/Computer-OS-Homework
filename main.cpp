#include <iostream>
#include <thread>
#include "MemoryManager.h"
#include "Process.h"
#include "SharedMemory.h"

using namespace std;


int main() {
    size_t pageSize = 1024;  
    size_t frameCount = 32;   

    MemoryManager mm(pageSize, frameCount);
    SharedMemoryManager shm(&mm);

    cout << "=== Phase 2: Multi-process + Shared Memory + Concurrent Access ===" << endl;
    cout << "Page size: " << pageSize << " bytes, frames: " << frameCount
        << ", physical memory: " << mm.getPhysicalMemorySize() << " bytes\n" << endl;

    Process p1(1, &mm);
    Process p2(2, &mm);

    size_t p1PrivateSeg = p1.createPrivateSegment(2000); 
    size_t p2PrivateSeg = p2.createPrivateSegment(1500); 

    int shmKey = 100;
    size_t sharedGlobalSeg = shm.createOrGet(shmKey, 4096);

    size_t p1SharedLocalSeg = p1.attachSegment(sharedGlobalSeg);
    size_t p2SharedLocalSeg = p2.attachSegment(sharedGlobalSeg);

    cout << "\n=== Start concurrent access on shared segment ===" << endl;

    thread t1([&]() {
        p1.runWorkload(p1SharedLocalSeg, "SHM-P1", 10, 0);
        });

    thread t2([&]() {
        p2.runWorkload(p2SharedLocalSeg, "SHM-P2", 10, 100);
        });

    t1.join();
    t2.join();

    cout << "\n=== After concurrent access, check visibility between processes ===" << endl;

    uint8_t v = 0;
    bool ok = p2.readByte(p2SharedLocalSeg, 0, v); 
    if (ok) {
        cout << "[Check] Process 2 read offset 0 in shared segment: 0x"
            << hex << (int)v << dec << endl;
    }
    else {
        cout << "[Check] Process 2 failed to read offset 0 in shared segment." << endl;
    }

    cout << "\n=== Detach shared memory and cleanup ===" << endl;
    shm.detach(shmKey); 
    shm.detach(shmKey);

    cout << "Program finished." << endl;
    return 0;
}
