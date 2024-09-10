#include "ThreadCache.h"
#include "CentralCache.h"
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// ��CentralCache��ȡ�ڴ棬��������ʼ���������㷨
	//�����Ҫ�Ĳ�ֹ���size��С���ڴ�������ôbatchNum�᲻��������ֱ���ﵽMAX_SIZE
	//sizeԽ��һ�η���ĸ���batchNum��ԽС
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
	//�������Ϊ�գ���ֱ�Ӵ�����ȡ
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

	// �Ҷ�ӳ�����������Ͱ������������
	size_t index = SizeClass::Index(size);
	_freeList[index].Push(ptr);

	// �������ȴ���һ������������ڴ�ʱ�Ϳ�ʼ��һ��list��central cache
	if (_freeList[index].Size() >= _freeList[index].MaxSize()) {
		ListTooLong(_freeList[index], size);
	}
}

void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	//��������ȴ���kMaxFreeListSize���������ȵ�1/2���ݷ���CentralCache
	//ע�⣬����ֻ�ǽ������ȵ�1/2���ݷ��أ����������ڴ沢û���ͷ�
	//�����ڴ��Ƿ���ThreadCache�У����Բ��ᱻ�ͷ�
	//�����ڴ��Ƿ���CentralCache�У����Իᱻ�ͷ�
	//size_t moveNum = list.Size() / 2;
	void* start = nullptr, * end = nullptr;
	list.PopRange(start, end, list.Size());
	CentralCache::GetInstance()->ReleaseListToSpans(start, end, size);
}