#pragma once
#include <cstddef>
#include <vector>

using namespace std;

/**
 * 页表项结构
 * 当前阶段只需要:
 * - present: 该页是否在内存中
 * - frameNumber: 该页对应的物理帧号
 * 后续阶段可以在此扩展访问权限、用户/内核位等。
 */
struct PageTableEntry {
    bool present;       // 是否在内存中
    size_t frameNumber; // 对应的物理帧号

    PageTableEntry()
        : present(false), frameNumber(0) {
    }
};

/**
 * 页表类
 * 一个段对应一个页表,页表中每项对应该段中的一个虚拟页。
 */
class PageTable {
public:
    // 初始化页表,创建n个页表项
    explicit PageTable(size_t numPages = 0) {
        entries.resize(numPages);
    }

    // 获取页表项(常量版本)
    const PageTableEntry* getEntry(size_t pageNo) const {
        if (pageNo >= entries.size()) {
            return nullptr; // 页号越界
        }
        return &entries[pageNo];
    }

    // 获取页表项(可修改版本)
    PageTableEntry* getEntry(size_t pageNo) {
        if (pageNo >= entries.size()) {
            return nullptr;
        }
        return &entries[pageNo];
    }

    // 返回页表项数量
    size_t size() const {
        return entries.size();
    }

private:
    vector<PageTableEntry> entries; // 存放所有页表项
};
