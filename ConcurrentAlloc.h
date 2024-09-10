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
		// ͨ��TLS ÿ���߳������Ļ�ȡ�Լ���ר����ThreadCache����
		// �̱߳��ش洢,ͨ�� TLS ��ȡ�̻߳��棬ʵ��ÿ���̵߳Ķ�������
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
		//ͨ����ϣӳ��ļ�ֵ���ҵ���Ӧ��ҳ

		PageCache::GetPageCache()->_pageMtx.lock();
		PageCache::GetPageCache()->ReleaseSpanToPageCache(span);
		PageCache::GetPageCache()->_pageMtx.unlock();
	} else {
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}
