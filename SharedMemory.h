#pragma once
#pragma once
#include <cstddef>
#include <unordered_map>
#include <mutex>
#include "MemoryManager.h"

using namespace std;

/**
 * 共享内存管理器
 * 提供:
 *  - 通过 key 创建/获取共享段
 *  - 管理全局段表中的 refCount
 *
 * 注意:
 *  - refCount 存在 SegmentDescriptor 中
 *  - 多个进程 attach 同一 key 时,共享同一个全局段
 */
class SharedMemoryManager {
public:
    explicit SharedMemoryManager(MemoryManager* mm)
        : mm(mm) {
    }

    /**
     * 创建或获取一个共享段
     * @param key    : 共享内存标识(模拟 System V 里的 key)
     * @param size   : 段大小(字节)
     * @return 对应的全局段号,失败返回 (size_t)-1
     *
     * 行为:
     *  - 若 key 已存在: 返回已有的全局段号,并增加 refCount
     *  - 若 key 不存在: 通过 MemoryManager 创建 shared 段,记录 key->segment 的映射,初始refCount=1
     */
    size_t createOrGet(int key, size_t sizeBytes);

    /**
     * detach(分离)共享段:
     *  - 只是对 refCount 减1
     *  - 若减到0,则调用 MemoryManager::destroySegment 回收
     */
    void detach(int key);

    /**
     * 根据 key 查询当前全局段号,不存在返回 (size_t)-1
     */
    size_t getGlobalSegNo(int key) const;

private:
    MemoryManager* mm;

    // key -> 全局段号 的映射
    unordered_map<int, size_t> keyToSeg;
    mutable mutex mtx;
};
