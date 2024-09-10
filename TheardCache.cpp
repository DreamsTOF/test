#include "ThreadCache.h"
#include "CentralCache.h"
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// 从CentralCache获取内存，采用慢开始反馈调节算法
	//如果你要的不止这个size大小的内存需求，那么batchNum会不断增长，直到达到MAX_SIZE
	//size越大，一次分配的个数batchNum会越小
	size_t batchNum = min(_freeList[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (batchNum == _freeList[index].MaxSize()) {
		_freeList[index].MaxSize() += 1;
	}
	void* start = nullptr, * end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum >= 1);
	if (actualNum == 1) {
		assert(start == end);
		return start;
	} else {
		_freeList[index].PushRange(NextObj(start), end, actualNum - 1);
		return start;
	}
}

void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	size_t align = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);
	//如果链表不为空，则直接从链表取
	if (!_freeList[index].Empty()) {
		return _freeList[index].Pop();
	} else {
		return FetchFromCentralCache(index, align);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	// 找对映射的自由链表桶，对象插入进入
	size_t index = SizeClass::Index(size);
	_freeList[index].Push(ptr);

	// 当链表长度大于一次批量申请的内存时就开始还一段list给central cache
	if (_freeList[index].Size() >= _freeList[index].MaxSize()) {
		ListTooLong(_freeList[index], size);
	}
}

void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	//如果链表长度大于kMaxFreeListSize，则将链表长度的1/2数据返回CentralCache
	//注意，这里只是将链表长度的1/2数据返回，但是链表内存并没有释放
	//链表内存是放在ThreadCache中，所以不会被释放
	//链表内存是放在CentralCache中，所以会被释放
	//size_t moveNum = list.Size() / 2;
	void* start = nullptr, * end = nullptr;
	list.PopRange(start, end, list.Size());
	CentralCache::GetInstance()->ReleaseListToSpans(start, end, size);
}