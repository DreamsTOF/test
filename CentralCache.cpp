#include "CentralCache.h"
#include "PageCache.h"
CentralCache CentralCache::_sInst;

// 获取⼀个⾮空的span,size为单个对象的大小
SPan* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//查看当前的span链表中是否还有未分配对象的span
	SPan* OneSpan = list.Begin();
	while (OneSpan != list.End()) {
		if (OneSpan->_freeList != nullptr) {
			return OneSpan;
		}
		OneSpan = OneSpan->_Next;
	}

	//解开锁，这样其他线程释放内存对象回来就不会堵塞
	list._spanMtx.unlock();
	// 没有空闲span，找page cache要
	PageCache::GetPageCache()->_pageMtx.lock();
	OneSpan = PageCache::GetPageCache()->NewSpan(SizeClass::NumMovePage(size));
	OneSpan->_isUse = true;
	OneSpan->_objectSize = size;
	PageCache::GetPageCache()->_pageMtx.unlock();
	//对获取span进行切分时不需要加锁，因为其他线程在切分完成前不会访问到该span
	//计算span的大块内存的起始地址和大块内存的（大小）字节数
	char* start = (char*)(OneSpan->_PageId << PAGE_SHIFT);
	size_t bytes = OneSpan->_size << PAGE_SHIFT;
	char* end = start + bytes;

	//把大块内存切成链表链接
	OneSpan->_freeList = start;
	start += size;
	void* tail = OneSpan->_freeList;
	while (start < end) {
		NextObj(tail) = start;
		tail = NextObj(tail);
		start += size;
	}
	//尾插最后一个节点的next置空
	NextObj(tail) = nullptr;
	// 切好span以后，需要把span挂到桶里面去的时候，再加锁
	list._spanMtx.lock();
	list.PushFront(OneSpan);
	return OneSpan;
}

// 从中⼼缓存获取⼀定数量的对象给thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	//计算在哪个桶⽚
	size_t index = SizeClass::Index(size);

	// 从span链表获取⼀个span
	_spanList[index]._spanMtx.lock();
	SPan* span = GetOneSpan(_spanList[index], size);
	assert(span);
	assert(span->_freeList);
	start = span->_freeList;
	end = start;
	//size_t i = 0;
	//实际获取到的对象数量
	size_t actualNum = 1;
	//10:50分
	// 从span中获取⾮空对象
	while (actualNum < batchNum - 1 && NextObj(end) != nullptr) {
		end = NextObj(end);
		++actualNum;
	}
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	// 更新span的已分配对象数量
	span->_useCount += actualNum;
	_spanList[index]._spanMtx.unlock();

	return actualNum;
}

void CentralCache::ReleaseListToSpans(void* start, void* end, size_t size)
{
	//计算在哪个桶⽚
	size_t index = SizeClass::Index(size);
	_spanList[index]._spanMtx.lock();
	//归还的内存属于哪个span,用map映射找到
	while (start) {
		void* next = NextObj(start);
		SPan* span = PageCache::GetPageCache()->MapObjectToSpan(start);
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;
		//这个span切出去的所有内存都回来了，此时就可以回收该span了
		if (span->_useCount == 0) {
			_spanList[index].Erase(span);
			span->_freeList = nullptr;
			span->_Next = nullptr;
			span->_Prev = nullptr;
			//解开桶锁
			_spanList[index]._spanMtx.unlock();
			//归还给page cache
			PageCache::GetPageCache()->_pageMtx.lock();
			PageCache::GetPageCache()->ReleaseSpanToPageCache(span);
			PageCache::GetPageCache()->_pageMtx.unlock();
			_spanList[index]._spanMtx.lock();
		}
		start = next;
	}
	_spanList[index]._spanMtx.unlock();
}