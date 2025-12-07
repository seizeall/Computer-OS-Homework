#include <cstddef>
#include <vector>

using namespace std;

struct SegmentDescriptor
{
	bool valid;
	size_t limit;
	size_t pageTableIndex;
	bool shared;
	size_t refCount;

	SegmentDescriptor(): valid(false),limit(0),pageTableIndex(0),shared(false),refCount(0){}
};

class SegmentTable {
public:
	const SegmentDescriptor* getSegment(size_t segNo) const {
		if (segNo >= segments.size()) {
			return nullptr;
		}
		return &segments[segNo];
	}
	
	SegmentDescriptor* getSegment(size_t segNo) {
		if (segNo >= segments.size()) {
			return nullptr;
		}
		return &segments[segNo];
	}

	size_t addSegment(const SegmentDescriptor& desc) {
		segments.push_back(desc);
		return segments.size() - 1;
	}

	size_t size() const {
		return segments.size();
	}

private:
	vector<SegmentDescriptor> segments;
};
