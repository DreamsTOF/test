#include "PageCache.h"

PageCache PageCache::_instance;

SPan* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	if (k > NPAGES - 1) {
		//直接向堆申请
		void* ptr = SystemAlloc(k);
		//调用spanPool申请一个span，不使用malloc
		//SPan* span = new SPan;
		SPan* span = _spanPool.New();
		//初始化span
		span->_PageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_size = k;
		//_spanMap[span->_PageId] = span;
		_spanMap.set(span->_PageId, span);
		return span;
	}
	//先检查第k个桶里是否有空闲的span
	if (!_spanList[k].Empty()) {
		//return _spanList[k].PopFront();
			//将新span的页号添加到spanMap中，方便central cache回收内存时查找对应的span
		//从空闲链表中取出一个span，此时如果直接返回，span的页号没有被映射到spanMap中，需要再调用一次spanMap.Add
		SPan* Newspan = _spanList[k].PopFront();
		for (PAGE_ID i = 0; i < Newspan->_size; i++) {
			//_spanMap[Newspan->_PageId + i] = Newspan;
			_spanMap.set(Newspan->_PageId + i, Newspan);
		}
		return Newspan;
	}

	//检查一下后面的桶里有没有空闲的span，如果有则切分一块挂链一块再返回切分的那块
	for (size_t i = k + 1; i < NPAGES; i++) {
		if (!_spanList[i].Empty()) {
			SPan* kspan = _spanList[i].PopFront();
			//创建一个新span用于标识切分后的span
			//SPan* Newspan = new SPan;
			SPan* Newspan = _spanPool.New();
			//切分span
			Newspan->_PageId = kspan->_PageId;
			Newspan->_size = k;
			//页号移动k 页
			kspan->_PageId += k;
			//剩余页数-k
			kspan->_size -= k;
			//将剩下的span挂链
			_spanList[kspan->_size].PushFront(kspan);
			//存储切分后的span的首页号的映射，方便page cache回收内存时进行合并查找
			//_spanMap[kspan->_PageId] = kspan;
			_spanMap.set(kspan->_PageId, kspan);
			//_spanMap[kspan->_PageId + kspan->_size - 1] = kspan;
			_spanMap.set(kspan->_PageId + kspan->_size - 1, kspan);
			//将新span的页号添加到spanMap中，方便central cache回收内存时查找对应的span
			for (PAGE_ID i = 0; i < Newspan->_size; i++) {
				//_spanMap[Newspan->_PageId + i] = Newspan;
				_spanMap.set(Newspan->_PageId + i, Newspan);
			}
			return Newspan;
		}
	}
	//没有空闲的span，则申请一块内存，切分后挂链
	//SPan* bigspan = new SPan;
	SPan* bigspan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	bigspan->_PageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigspan->_size = NPAGES - 1;

	//将切分后的span挂链，然后递归调用NewSpan
	_spanList[bigspan->_size].PushFront(bigspan);
	return NewSpan(k);
}

SPan* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID pageId = (PAGE_ID)obj >> PAGE_SHIFT;
	//raii的锁，出了此作用域，自动释放
	/*std::unique_lock<std::mutex> lock(_pageMtx);
	auto it = _spanMap.find(pageId);
	if (it != _spanMap.end()) {
		return it->second;
	} else {
		assert(false);
		return nullptr;
	}*/
	auto ret = (SPan*)_spanMap.get(pageId);
	assert(ret);
	return ret;
}

// 释放空闲span回到Pagecache，并合并相邻的span
void PageCache::ReleaseSpanToPageCache(SPan* span)
{
	assert(span);
	//大于128页的直接还给堆
	if (span->_size > NPAGES - 1) {
		//通过页号来找到span
		void* ptr = (void*)(span->_PageId << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);
		return;
	}
	// 对span前后的页，尝试进行合并，缓解内存碎片问题
	while (1) {
		PAGE_ID prevId = span->_PageId - 1;
		//auto ret = _spanMap.find(prevId);
		auto ret = (SPan*)_spanMap.get(prevId);
		// 前面的页号没有，不合并了
		if (ret == nullptr) {
			break;
		}

		// 前面相邻页的span在使用，不合并了
		SPan* prevSpan = ret;
		if (prevSpan->_isUse == true) {
			break;
		}

		// 合并出超过128页的span没办法管理，不合并了
		if (prevSpan->_size + span->_size > NPAGES - 1) {
			break;
		}

		span->_PageId = prevSpan->_PageId;
		span->_size += prevSpan->_size;

		_spanList[prevSpan->_size].Erase(prevSpan);
		//delete prevSpan;
		_spanPool.Delete(prevSpan);
	}

	// 向后合并
	while (1) {
		PAGE_ID nextId = span->_PageId + span->_size;
		auto ret = (SPan*)_spanMap.get(nextId);
		// 后面的页号没有，不合并了
		if (ret == nullptr) {
			break;
		}

		SPan* nextSpan = ret;
		if (nextSpan->_isUse == true) {
			break;
		}

		if (nextSpan->_size + span->_size > NPAGES - 1) {
			break;
		}

		span->_size += nextSpan->_size;

		_spanList[nextSpan->_size].Erase(nextSpan);
		//delete nextSpan;
		_spanPool.Delete(nextSpan);
	}
	//将合并后的span加入到spanList
	_spanList[span->_size].PushFront(span);
	//将合并后的span置为未使用，并将其放入spanMap
	span->_isUse = false;
	//_spanMap[span->_PageId] = span;
	_spanMap.set(span->_PageId, span);
	//_spanMap[span->_PageId + span->_size - 1] = span;
	_spanMap.set(span->_PageId + span->_size - 1, span);
}