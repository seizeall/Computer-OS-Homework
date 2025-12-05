#include "MemoryManager.h"
#include <iostream>
#include <cmath>    // std::ceil

using namespace std;

/**
 * 构造函数实现
 * 根据页大小和帧数初始化物理内存和空闲帧列表。
 */
MemoryManager::MemoryManager(size_t pageSizeBytes, size_t numFrames)
    : pageSize(pageSizeBytes),
    frameCount(numFrames),
    physicalMemory(pageSizeBytes* numFrames, 0) // 分配物理内存并初始化为0
{
    // 初始化空闲帧列表:一开始所有帧都是空闲的
    freeFrames.reserve(numFrames);
    for (size_t i = 0; i < numFrames; ++i) {
        freeFrames.push_back(i);
    }
}

/**
 * 从空闲帧列表中分配一个物理帧
 */
bool MemoryManager::allocateFrame(size_t& frameNumber) {
    if (freeFrames.empty()) {
        return false; // 没有空闲帧
    }
    // 简单策略:取向量末尾的帧号(栈式分配)
    frameNumber = freeFrames.back();
    freeFrames.pop_back();
    return true;
}

/**
 * 根据段大小计算需要的页数(向上取整)
 * 例如:
 * pageSize=1024, segmentSize=2049 -> 需要3页
 */
size_t MemoryManager::calcNumPages(size_t segmentSizeBytes) const {
    return (segmentSizeBytes + pageSize - 1) / pageSize; // 向上取整
}

/**
 * 创建一个新的段
 * 步骤:
 * 1. 计算该段需要的页数
 * 2. 检查空闲帧是否足够
 * 3. 分配一个新的页表并为每页分配物理帧
 * 4. 在段表中增加一项,记录段长度和页表索引
 */
size_t MemoryManager::createSegment(size_t segmentSizeBytes) {
    size_t numPages = calcNumPages(segmentSizeBytes);

    // 简单检查:如果物理帧数量不足以容纳该段的所有页,则创建失败
    if (numPages > freeFrames.size()) {
        cerr << "Failed to create segment: not enough physical frames." << endl;
        return static_cast<size_t>(-1); // 用-1表示失败
    }

    // 创建一个页表对象,内部包含numPages个PageTableEntry
    PageTable pt(numPages);

    // 为每个逻辑页分配一个物理帧
    for (size_t i = 0; i < numPages; ++i) {
        size_t frameNumber;
        bool ok = allocateFrame(frameNumber);
        if (!ok) {
            // 理论上不会进入此分支,因为前面已经检查numPages<=freeFrames.size()
            cerr << "Unexpected error: no free frame during segment creation." << endl;
            return static_cast<size_t>(-1);
        }
        PageTableEntry* entry = pt.getEntry(i);
        entry->present = true;
        entry->frameNumber = frameNumber;
    }

    // 将新页表加入全局页表数组中,记录其索引
    size_t pageTableIndex = pageTables.size();
    pageTables.push_back(pt);

    // 构造一个段表项
    SegmentDescriptor desc;
    desc.valid = true;
    desc.limit = segmentSizeBytes;     // 段长度(字节)
    desc.pageTableIndex = pageTableIndex; // 指向本段对应的页表

    // 将段表项加入段表,返回该段号
    size_t segNo = segmentTable.addSegment(desc);
    return segNo;
}

/**
 * 逻辑地址->物理地址转换
 * 输入:
 *  la.segment: 段号
 *  la.offset: 段内偏移
 *
 * 步骤:
 * 1. 根据段号从段表中取出段表项,检查是否有效
 * 2. 进行段界限检查:offset < limit
 * 3. 将offset拆分成页号pageNo和页内偏移pageOffset
 * 4. 根据段表项中的pageTableIndex找到对应页表
 * 5. 在页表中找到pageNo对应的页表项,检查present位
 * 6. 物理地址 = frameNumber * pageSize + pageOffset
 */
bool MemoryManager::translate(const LogicalAddress& la, size_t& physicalAddress) const {
    // 1. 查段表
    const SegmentDescriptor* segDesc = segmentTable.getSegment(la.segment);
    if (segDesc == nullptr || !segDesc->valid) {
        cerr << "Translation failed: invalid segment number " << la.segment << endl;
        return false;
    }

    // 2. 段界限检查(防止段内偏移越界)
    if (la.offset >= segDesc->limit) {
        cerr << "Translation failed: offset out of segment limit." << endl;
        return false;
    }

    // 3. 拆分offset为页号与页内偏移
    size_t pageNo = la.offset / pageSize;       // 逻辑页号
    size_t pageOffset = la.offset % pageSize;   // 页内偏移

    // 4. 获取对应的页表
    if (segDesc->pageTableIndex >= pageTables.size()) {
        cerr << "Translation failed: invalid page table index in segment descriptor." << endl;
        return false;
    }
    const PageTable& pt = pageTables[segDesc->pageTableIndex];

    // 检查页号是否在页表范围内
    if (pageNo >= pt.size()) {
        cerr << "Translation failed: page number out of range." << endl;
        return false;
    }

    // 5. 查页表项
    const PageTableEntry* entry = pt.getEntry(pageNo);
    if (entry == nullptr || !entry->present) {
        cerr << "Translation failed: page not present in memory." << endl;
        return false;
    }

    // 6. 计算物理地址
    size_t frameNumber = entry->frameNumber;
    physicalAddress = frameNumber * pageSize + pageOffset;

    // 再做一个简单的安全性检查:确保物理地址没有越过物理内存
    if (physicalAddress >= physicalMemory.size()) {
        cerr << "Translation failed: physical address out of range." << endl;
        return false;
    }

    return true;
}

/**
 * 通过逻辑地址写一个字节到物理内存
 */
bool MemoryManager::writeByte(const LogicalAddress& la, uint8_t value) {
    size_t pa;
    if (!translate(la, pa)) {
        return false; // 地址转换失败
    }
    physicalMemory[pa] = value;
    return true;
}

/**
 * 通过逻辑地址从物理内存读一个字节
 */
bool MemoryManager::readByte(const LogicalAddress& la, uint8_t& value) const {
    size_t pa;
    if (!translate(la, pa)) {
        return false; // 地址转换失败
    }
    value = physicalMemory[pa];
    return true;
}
