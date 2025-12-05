#include <cstddef>
#include <cstdint>
#include <vector>
#include "Segment.h"
#include "Page.h"

using namespace std;

struct LogicalAddress
{
	uint16_t segment;
	uint32_t offset;
};

class MemoryManager {
public:
    MemoryManager(size_t pageSizeBytes, size_t numFrames);
    size_t createSegment(size_t segmentSizeBytes);
    bool translate(const LogicalAddress& la, size_t& physicalAddress) const;
    bool writeByte(const LogicalAddress& la, uint8_t value);
    bool readByte(const LogicalAddress& la, uint8_t& value) const;
    size_t getPageSize() const { return pageSize; }
    size_t getPhysicalMemorySize() const { return physicalMemory.size(); }
private:
    size_t pageSize;                 // 页大小(字节)
    size_t frameCount;               // 物理帧总数
    vector<uint8_t> physicalMemory;  // 模拟的物理内存
    vector<size_t> freeFrames;       // 空闲物理帧列表(存放帧号)
    SegmentTable segmentTable;       // 单进程的段表
    vector<PageTable> pageTables;    // 所有段对应的页表集合
    bool allocateFrame(size_t& frameNumber);
    size_t calcNumPages(size_t segmentSizeBytes) const;
};