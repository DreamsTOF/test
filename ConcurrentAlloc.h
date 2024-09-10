#pragma once
#include"Common.h"
#include"ThreadCache.h"
#include"PageCache.h"
#include"ObjectPool.h"
static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES) {
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kpage = alignSize >> PAGE_SHIFT;

		PageCache::GetPageCache()->_pageMtx.lock();
		SPan* span = PageCache::GetPageCache()->NewSpan(kpage);
		span->_objectSize = size;
		PageCache::GetPageCache()->_pageMtx.unlock();

		void* ptr = (void*)(span->_PageId << PAGE_SHIFT);
		return ptr;
	} else {
		// 通过TLS 每个线程无锁的获取自己的专属的ThreadCache对象
		// 线程本地存储,通过 TLS 获取线程缓存，实现每个线程的独立缓存
		if (pTLSThreadCache == nullptr) {
			static ObjectPool<ThreadCache> tcPool;
			//pTLSThreadCache = new ThreadCache;
			pTLSThreadCache = tcPool.New();
		}

		return pTLSThreadCache->Allocate(size);
	}
}
static void ConcurrentFree(void* ptr)
{
	SPan* span = PageCache::GetPageCache()->MapObjectToSpan(ptr);
	size_t size = span->_objectSize;
	if (size > MAX_BYTES) {
		//通过哈希映射的键值，找到对应的页

		PageCache::GetPageCache()->_pageMtx.lock();
		PageCache::GetPageCache()->ReleaseSpanToPageCache(span);
		PageCache::GetPageCache()->_pageMtx.unlock();
	} else {
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}
