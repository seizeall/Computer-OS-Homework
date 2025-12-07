#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <mutex>
#include <string>
#include "MemoryManager.h"

using namespace std;

/**
 * Process 类
 * 用于模拟操作系统中的进程:
 * - pid           : 进程ID
 * - mm            : 指向全局的 MemoryManager,用于实际的段/页操作
 * - segmentMap    : 本地段号 -> 全局段号 的映射
 *
 * 设计思路:
 *  - 真实OS中,每个进程有自己的段表/页表;
 *  - 此处简化为: MemoryManager维护“全局段表 + 页表”
 *  - Process仅记录本进程可见的段,并将本地段号映射到全局段号
 */
class Process {
public:
    Process(int pid, MemoryManager* mm)
        : pid(pid), mm(mm) {
    }

    int getPid() const { return pid; }

    /**
     * 为本进程创建一个私有段:
     *  - 内部调用 MemoryManager::createSegment(shared=false)
     *  - 将返回的全局段号放入 segmentMap
     *  - 返回本地段号(即 segmentMap 中的索引)
     */
    size_t createPrivateSegment(size_t segmentSizeBytes);

    /**
     * 将一个已存在的“全局段”映射到本进程地址空间
     *  - 通常用于共享段 attach
     *  - 返回本地段号
     */
    size_t attachSegment(size_t globalSegNo);

    /**
     * 根据本地段号获取对应的全局段号
     */
    size_t getGlobalSegNo(size_t localSegNo) const;

    /**
     * 利用本地段号 + 段内偏移 写一个字节
     * 内部:
     *  - 将本地段号映射为全局段号
     *  - 调用 MemoryManager::writeByteGlobal
     */
    bool writeByte(size_t localSegNo, uint32_t offset, uint8_t value);

    /**
     * 利用本地段号 + 段内偏移 读一个字节
     */
    bool readByte(size_t localSegNo, uint32_t offset, uint8_t& value) const;

    /**
     * 用于并发测试的工作负载函数:
     *  - 重复对某个段进行读写操作
     */
    void runWorkload(size_t localSegNo, const string& tag, int iterations, uint32_t baseOffset);

private:
    int pid;
    MemoryManager* mm;
    vector<size_t> segmentMap;    // 本地段号 -> 全局段号
    mutable mutex procMtx;        // 保护segmentMap的并发访问
};
