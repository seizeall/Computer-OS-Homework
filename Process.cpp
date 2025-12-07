#include "Process.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace std;

/**
 * 为本进程创建一个私有段:
 *  - 调用 MemoryManager::createSegment(shared=false)
 *  - 将全局段号添加到 segmentMap
 *  - 返回“本地段号”(即 vector 下标)
 */
size_t Process::createPrivateSegment(size_t segmentSizeBytes) {
    // 向全局内存管理器申请一个段
    size_t globalSegNo = mm->createSegment(segmentSizeBytes, false);
    if (globalSegNo == static_cast<size_t>(-1)) {
        cerr << "[Process " << pid << "] Failed to create private segment." << endl;
        return static_cast<size_t>(-1);
    }

    lock_guard<mutex> lock(procMtx);
    segmentMap.push_back(globalSegNo);
    size_t localSegNo = segmentMap.size() - 1;

    cout << "[Process " << pid << "] Created private segment (localSegNo="
        << localSegNo << ", globalSegNo=" << globalSegNo
        << ", size=" << segmentSizeBytes << " bytes)" << endl;

    return localSegNo;
}

/**
 * 将一个全局段映射到本进程(用于共享段 attach)
 */
size_t Process::attachSegment(size_t globalSegNo) {
    if (globalSegNo == static_cast<size_t>(-1)) {
        cerr << "[Process " << pid << "] attachSegment: invalid global segment." << endl;
        return static_cast<size_t>(-1);
    }

    lock_guard<mutex> lock(procMtx);
    segmentMap.push_back(globalSegNo);
    size_t localSegNo = segmentMap.size() - 1;

    cout << "[Process " << pid << "] Attached segment (localSegNo="
        << localSegNo << ", globalSegNo=" << globalSegNo << ")" << endl;
    return localSegNo;
}

/**
 * 本地段号 -> 全局段号
 */
size_t Process::getGlobalSegNo(size_t localSegNo) const {
    lock_guard<mutex> lock(procMtx);
    if (localSegNo >= segmentMap.size()) {
        return static_cast<size_t>(-1);
    }
    return segmentMap[localSegNo];
}

/**
 * 本地段号 + 偏移 -> 写字节
 */
bool Process::writeByte(size_t localSegNo, uint32_t offset, uint8_t value) {
    size_t globalSegNo = getGlobalSegNo(localSegNo);
    if (globalSegNo == static_cast<size_t>(-1)) {
        cerr << "[Process " << pid << "] writeByte: invalid localSegNo." << endl;
        return false;
    }
    bool ok = mm->writeByteGlobal(globalSegNo, offset, value);
    if (!ok) {
        cerr << "[Process " << pid << "] writeByte failed." << endl;
    }
    return ok;
}

/**
 * 本地段号 + 偏移 -> 读字节
 */
bool Process::readByte(size_t localSegNo, uint32_t offset, uint8_t& value) const {
    size_t globalSegNo = getGlobalSegNo(localSegNo);
    if (globalSegNo == static_cast<size_t>(-1)) {
        cerr << "[Process " << pid << "] readByte: invalid localSegNo." << endl;
        return false;
    }
    bool ok = mm->readByteGlobal(globalSegNo, offset, value);
    if (!ok) {
        cerr << "[Process " << pid << "] readByte failed." << endl;
    }
    return ok;
}

/**
 * 并发工作负载:
 *  - 用于在多线程中模拟进程的内存访问行为
 *  - 将 tag 用于打印区分不同线程/场景
 *  - 每次迭代:
 *      1. 写一个字节(偏移 baseOffset + i)
 *      2. 再读回并打印
 */
void Process::runWorkload(size_t localSegNo, const string& tag, int iterations, uint32_t baseOffset) {
    for (int i = 0; i < iterations; ++i) {
        uint32_t offset = baseOffset + static_cast<uint32_t>(i);
        uint8_t valueToWrite = static_cast<uint8_t>((pid * 10 + i) & 0xFF);

        // 写
        if (writeByte(localSegNo, offset, valueToWrite)) {
            uint8_t readValue = 0;
            if (readByte(localSegNo, offset, readValue)) {
                cout << "[Process " << pid << "][" << tag << "] Iter=" << i
                    << " offset=" << offset
                    << " write=0x" << hex << (int)valueToWrite
                    << " read=0x" << (int)readValue << dec << endl;
            }
        }

        // 模拟“时间片”切换: 每次休眠一点时间,让其他线程有机会运行
        this_thread::sleep_for(chrono::milliseconds(50));
    }
}
