#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <mutex>        // 线程安全
#include "Segment.h"
#include "Page.h"

using namespace std;

struct LogicalAddress {
    uint16_t segment;   // 全局段号
    uint32_t offset;    // 段内偏移
};

/**
 * MemoryManager
 * 负责:
 *  - 维护物理内存和空闲帧
 *  - 维护全局段表 + 全局页表数组
 *  - 提供创建段、销毁段的接口
 *  - 实现逻辑地址到物理地址的转换
 *  - 为多线程并发访问提供互斥保护
 */
class MemoryManager {
public:
    MemoryManager(size_t pageSizeBytes, size_t numFrames);

    /**
     * 创建一个段(可指定是否为共享段)
     * @param segmentSizeBytes 段大小(字节)
     * @param shared           是否作为共享段创建
     * @return 全局段号,失败返回 (size_t)-1
     */
    size_t createSegment(size_t segmentSizeBytes, bool shared = false);

    /**
     * 销毁一个全局段:
     *  - 仅当 refCount == 0 时才真正释放物理帧并标记无效
     *  - 一般由共享内存管理或进程结束时调用
     */
    bool destroySegment(size_t globalSegNo);

    /**
     * 逻辑地址 -> 物理地址
     * 这里的逻辑地址使用全局段号。
     */
    bool translate(const LogicalAddress& la, size_t& physicalAddress) const;

    /**
     * 使用指定的全局段号 + 段内偏移 转换为物理地址
     * 供多进程环境下,“本地段号->全局段号”转换后使用。
     */
    bool translateGlobal(size_t globalSegNo, uint32_t offset, size_t& physicalAddress) const;

    /**
     * 通过全局段号 + 段内偏移 写入一个字节
     */
    bool writeByteGlobal(size_t globalSegNo, uint32_t offset, uint8_t value);

    /**
     * 通过全局段号 + 段内偏移 读取一个字节
     */
    bool readByteGlobal(size_t globalSegNo, uint32_t offset, uint8_t& value) const;

    size_t getPageSize() const { return pageSize; }
    size_t getPhysicalMemorySize() const { return physicalMemory.size(); }

    /**
     * 访问全局段表接口(供共享内存管理使用)
     */
    SegmentDescriptor* getSegmentDescriptor(size_t globalSegNo);
    const SegmentDescriptor* getSegmentDescriptor(size_t globalSegNo) const;

private:
    size_t pageSize;
    size_t frameCount;
    vector<uint8_t> physicalMemory;
    vector<size_t> freeFrames;

    SegmentTable segmentTable;
    vector<PageTable> pageTables;

    // 互斥锁: 用于保护对物理内存、空闲帧、段表、页表的并发访问
    mutable mutex mtx;

    bool allocateFrame(size_t& frameNumber);
    size_t calcNumPages(size_t segmentSizeBytes) const;
};
