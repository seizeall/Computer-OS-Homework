#include <iostream>
#include "MemoryManager.h"

using namespace std;

/**
 * 简单测试流程:
 * 1. 创建一个MemoryManager,假设:
 *    - 页大小: 1024字节
 *    - 物理帧数: 16(总内存=16KB)
 * 2. 创建一个大小为3000字节的段
 * 3. 在该段内写入一些字节
 * 4. 让用户输入逻辑地址(段号+偏移),输出对应物理地址和读取到的值
 */
int main() {
    // 1. 初始化内存管理器
    size_t pageSize = 1024;  // 每页1024字节
    size_t frameCount = 16;  // 16个物理帧,总物理内存=16*1024=16384字节

    MemoryManager mm(pageSize, frameCount);

    cout << "=== Phase 1: basic segmented-paged memory manager (single process) ===" << endl;
    cout << "Page size: " << pageSize << " bytes, frames: " << frameCount
        << ", physical memory: " << mm.getPhysicalMemorySize() << " bytes" << endl;

    // 2. 创建一个大小为3000字节的段
    size_t segmentSize = 3000; // 字节
    size_t segNo = mm.createSegment(segmentSize);
    if (segNo == static_cast<size_t>(-1)) {
        cerr << "Failed to create segment." << endl;
        return 1;
    }
    cout << "Created segment " << segNo << " with size " << segmentSize << " bytes." << endl;

    // 3. 在该段内写入一些测试数据,比如在偏移0,1000,2500处写入不同值
    LogicalAddress la0{ static_cast<uint16_t>(segNo), 0 };
    LogicalAddress la1{ static_cast<uint16_t>(segNo), 1000 };
    LogicalAddress la2{ static_cast<uint16_t>(segNo), 2500 };

    mm.writeByte(la0, 0x11);
    mm.writeByte(la1, 0x22);
    mm.writeByte(la2, 0x33);

    cout << "\nTest data written at offsets 0, 1000, 2500." << endl;

    // 4. 交互式测试:用户输入偏移,程序输出物理地址及其对应值
    while (true) {
        cout << "\nEnter logical offset within segment " << segNo
            << " (0~" << (segmentSize - 1)
            << ", negative to exit): ";
        long long offsetInput;
        cin >> offsetInput;
        if (!cin || offsetInput < 0) {
            cout << "Exit." << endl;
            break;
        }

        if (static_cast<size_t>(offsetInput) >= segmentSize) {
            cout << "Offset out of segment range, this should be rejected by translation." << endl;
        }

        LogicalAddress la{ static_cast<uint16_t>(segNo),
                           static_cast<uint32_t>(offsetInput) };

        size_t physicalAddress;
        if (mm.translate(la, physicalAddress)) {
            cout << "Logical address (segment=" << la.segment
                << ", offset=" << la.offset << ") -> physical address "
                << physicalAddress << endl;

            uint8_t value = 0;
            if (mm.readByte(la, value)) {
                cout << "Read value at physical address " << physicalAddress
                    << " = 0x" << hex << static_cast<int>(value) << dec << endl;
            }
            else {
                cout << "Read failed." << endl;
            }
        }
        else {
            cout << "Translation failed for this logical address." << endl;
        }
    }

    return 0;
}
