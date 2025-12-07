#include "MemoryManager.h"
#include <iostream>

using namespace std;

/**
 * 构造函数:
 *  - 初始化物理内存(全部清0)
 *  - 初始化空闲帧列表(0 ~ frameCount-1)
 */
MemoryManager::MemoryManager(size_t pageSizeBytes, size_t numFrames)
    : pageSize(pageSizeBytes),
    frameCount(numFrames),
    physicalMemory(pageSizeBytes* numFrames, 0) {
    freeFrames.reserve(numFrames);
    for (size_t i = 0; i < numFrames; ++i) {
        freeFrames.push_back(i);
    }
}

/**
 * 分配一个空闲物理帧(线程安全内部调用,需在外部加锁或只在类方法内调用)
 */
bool MemoryManager::allocateFrame(size_t& frameNumber) {
    if (freeFrames.empty()) {
        return false;
    }
    frameNumber = freeFrames.back();
    freeFrames.pop_back();
    return true;
}

/**
 * 计算需要的页数(向上取整)
 */
size_t MemoryManager::calcNumPages(size_t segmentSizeBytes) const {
    return (segmentSizeBytes + pageSize - 1) / pageSize;
}

/**
 * 创建一个段
 * shared=true 表示共享段,
 * shared=false 表示普通私有段。
 */
size_t MemoryManager::createSegment(size_t segmentSizeBytes, bool shared) {
    lock_guard<mutex> lock(mtx); // 整个创建过程加锁,防止并发干扰

    size_t numPages = calcNumPages(segmentSizeBytes);
    if (numPages > freeFrames.size()) {
        cerr << "[MemoryManager] Failed to create segment: not enough frames." << endl;
        return static_cast<size_t>(-1);
    }

    // 创建页表并为每页分配物理帧
    PageTable pt(numPages);
    for (size_t i = 0; i < numPages; ++i) {
        size_t frameNumber;
        if (!allocateFrame(frameNumber)) {
            // 按理说不会到这里,因为前面已经检查过
            cerr << "[MemoryManager] Unexpected: no free frame during createSegment." << endl;
            return static_cast<size_t>(-1);
        }
        PageTableEntry* entry = pt.getEntry(i);
        entry->present = true;
        entry->frameNumber = frameNumber;
    }

    // 将页表加入全局页表数组
    size_t pageTableIndex = pageTables.size();
    pageTables.push_back(pt);

    // 构造段表项
    SegmentDescriptor desc;
    desc.valid = true;
    desc.limit = segmentSizeBytes;
    desc.pageTableIndex = pageTableIndex;
    desc.shared = shared;
    desc.refCount = 1; // 初始持有者计数为1

    // 放入全局段表,返回全局段号
    size_t globalSegNo = segmentTable.addSegment(desc);
    return globalSegNo;
}

/**
 * 销毁一个全局段:
 *  - 仅当 refCount == 0 时才真正释放
 *  - 否则只是减引用,不做真实回收(此处由上层保证只在refCount为0时调用)
 */
bool MemoryManager::destroySegment(size_t globalSegNo) {
    lock_guard<mutex> lock(mtx);

    SegmentDescriptor* seg = segmentTable.getSegment(globalSegNo);
    if (!seg || !seg->valid) {
        cerr << "[MemoryManager] destroySegment: invalid segment " << globalSegNo << endl;
        return false;
    }

    if (seg->refCount != 0) {
        cerr << "[MemoryManager] destroySegment: refCount != 0, cannot destroy." << endl;
        return false;
    }

    // 找到对应页表,把所有物理帧回收到 freeFrames 中
    if (seg->pageTableIndex >= pageTables.size()) {
        cerr << "[MemoryManager] destroySegment: invalid pageTableIndex." << endl;
        return false;
    }

    PageTable& pt = pageTables[seg->pageTableIndex];
    for (size_t i = 0; i < pt.size(); ++i) {
        PageTableEntry* entry = pt.getEntry(i);
        if (entry->present) {
            freeFrames.push_back(entry->frameNumber);
            entry->present = false;
        }
    }

    // 将段标记为无效
    seg->valid = false;
    return true;
}

/**
 * 使用全局段号 + 段内偏移 做地址转换
 * (内部工具函数,对外 translateGlobal 提供封装)
 */
bool MemoryManager::translateGlobal(size_t globalSegNo, uint32_t offset, size_t& physicalAddress) const {
    lock_guard<mutex> lock(mtx);

    // 1. 查段表
    const SegmentDescriptor* segDesc = segmentTable.getSegment(globalSegNo);
    if (!segDesc || !segDesc->valid) {
        cerr << "[MemoryManager] translateGlobal: invalid segment " << globalSegNo << endl;
        return false;
    }

    // 2. 段界限检查
    if (offset >= segDesc->limit) {
        cerr << "[MemoryManager] translateGlobal: offset out of range." << endl;
        return false;
    }

    // 3. 拆分页号+页内偏移
    size_t pageNo = offset / pageSize;
    size_t pageOffset = offset % pageSize;

    if (segDesc->pageTableIndex >= pageTables.size()) {
        cerr << "[MemoryManager] translateGlobal: invalid pageTableIndex." << endl;
        return false;
    }
    const PageTable& pt = pageTables[segDesc->pageTableIndex];

    if (pageNo >= pt.size()) {
        cerr << "[MemoryManager] translateGlobal: page number out of range." << endl;
        return false;
    }

    const PageTableEntry* entry = pt.getEntry(pageNo);
    if (!entry || !entry->present) {
        cerr << "[MemoryManager] translateGlobal: page not present." << endl;
        return false;
    }

    size_t frameNumber = entry->frameNumber;
    physicalAddress = frameNumber * pageSize + pageOffset;

    if (physicalAddress >= physicalMemory.size()) {
        cerr << "[MemoryManager] translateGlobal: physical address out of range." << endl;
        return false;
    }

    return true;
}

/**
 * translate(const LogicalAddress&) 兼容阶段1接口:
 *  - 将 la.segment 视为“全局段号”
 */
bool MemoryManager::translate(const LogicalAddress& la, size_t& physicalAddress) const {
    return translateGlobal(la.segment, la.offset, physicalAddress);
}

/**
 * 全局写一个字节
 */
bool MemoryManager::writeByteGlobal(size_t globalSegNo, uint32_t offset, uint8_t value) {
    size_t pa;
    if (!translateGlobal(globalSegNo, offset, pa)) {
        return false;
    }
    lock_guard<mutex> lock(mtx); // 写物理内存时加锁
    physicalMemory[pa] = value;
    return true;
}

/**
 * 全局读一个字节
 */
bool MemoryManager::readByteGlobal(size_t globalSegNo, uint32_t offset, uint8_t& value) const {
    size_t pa;
    if (!translateGlobal(globalSegNo, offset, pa)) {
        return false;
    }
    lock_guard<mutex> lock(mtx); // 读物理内存时同样加锁,确保与写操作互斥
    value = physicalMemory[pa];
    return true;
}

/**
 * 段表访问接口
 */
SegmentDescriptor* MemoryManager::getSegmentDescriptor(size_t globalSegNo) {
    lock_guard<mutex> lock(mtx);
    return segmentTable.getSegment(globalSegNo);
}

const SegmentDescriptor* MemoryManager::getSegmentDescriptor(size_t globalSegNo) const {
    lock_guard<mutex> lock(mtx);
    return segmentTable.getSegment(globalSegNo);
}
