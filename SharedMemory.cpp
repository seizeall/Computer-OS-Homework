#include "SharedMemory.h"
#include <iostream>

using namespace std;

/**
 * 创建或获取共享段
 */
size_t SharedMemoryManager::createOrGet(int key, size_t sizeBytes) {
    lock_guard<mutex> lock(mtx);

    auto it = keyToSeg.find(key);
    if (it != keyToSeg.end()) {
        // 已有共享段,增加引用计数
        size_t globalSegNo = it->second;
        SegmentDescriptor* seg = mm->getSegmentDescriptor(globalSegNo);
        if (!seg || !seg->valid) {
            cerr << "[SharedMemoryManager] Existing segment invalid for key=" << key << endl;
            return static_cast<size_t>(-1);
        }
        seg->refCount++;
        cout << "[SharedMemoryManager] Reuse shared segment: key=" << key
            << ", globalSegNo=" << globalSegNo
            << ", refCount=" << seg->refCount << endl;
        return globalSegNo;
    }

    // 不存在则新建共享段
    size_t globalSegNo = mm->createSegment(sizeBytes, true);
    if (globalSegNo == static_cast<size_t>(-1)) {
        cerr << "[SharedMemoryManager] Failed to create new shared segment for key=" << key << endl;
        return static_cast<size_t>(-1);
    }

    SegmentDescriptor* seg = mm->getSegmentDescriptor(globalSegNo);
    if (!seg) {
        cerr << "[SharedMemoryManager] Created segment descriptor not found." << endl;
        return static_cast<size_t>(-1);
    }
    seg->shared = true;
    seg->refCount = 1;

    keyToSeg[key] = globalSegNo;

    cout << "[SharedMemoryManager] Created new shared segment: key=" << key
        << ", globalSegNo=" << globalSegNo
        << ", size=" << sizeBytes << " bytes" << endl;

    return globalSegNo;
}

/**
 * detach 共享段:
 *  - 引用计数减1
 *  - 当refCount降为0时,调用 MemoryManager::destroySegment
 */
void SharedMemoryManager::detach(int key) {
    lock_guard<mutex> lock(mtx);

    auto it = keyToSeg.find(key);
    if (it == keyToSeg.end()) {
        cerr << "[SharedMemoryManager] detach: key not found: " << key << endl;
        return;
    }

    size_t globalSegNo = it->second;
    SegmentDescriptor* seg = mm->getSegmentDescriptor(globalSegNo);
    if (!seg || !seg->valid) {
        cerr << "[SharedMemoryManager] detach: invalid segment for key=" << key << endl;
        return;
    }

    if (seg->refCount == 0) {
        cerr << "[SharedMemoryManager] detach: refCount already 0 for key=" << key << endl;
        return;
    }

    seg->refCount--;
    cout << "[SharedMemoryManager] detach key=" << key
        << ", globalSegNo=" << globalSegNo
        << ", new refCount=" << seg->refCount << endl;

    if (seg->refCount == 0) {
        // 真正销毁段
        if (mm->destroySegment(globalSegNo)) {
            cout << "[SharedMemoryManager] Segment destroyed for key=" << key << endl;
        }
        // 从映射表中移除
        keyToSeg.erase(it);
    }
}

/**
 * 根据 key 查询全局段号
 */
size_t SharedMemoryManager::getGlobalSegNo(int key) const {
    lock_guard<mutex> lock(mtx);
    auto it = keyToSeg.find(key);
    if (it == keyToSeg.end()) return static_cast<size_t>(-1);
    return it->second;
}
