#pragma once
#include"Common.h"
#include "PageMap.h"
class PageCache
{
public:
	//获取单例
	static PageCache* GetPageCache()
	{
		return &_instance;
	}
	//获取一个k页的span
	SPan* NewSpan(size_t k);
	// 获取从对象到span的映射
	SPan* MapObjectToSpan(void* obj);

	// 释放空闲span回到Pagecache，并合并相邻的span
	void ReleaseSpanToPageCache(SPan* span);

	std::mutex _pageMtx;
private:
	PageCache() {}
	PageCache(const PageCache&) = delete;
private:
	ObjectPool<SPan> _spanPool;
	//页号和span的映射
	//std::unordered_map<PAGE_ID, SPan*> _spanMap;
#ifdef _WIN64
	TCMalloc_PageMap3<64 - PAGE_SHIFT> _spanMap;
#else
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _spanMap;
#endif
	SpanList _spanList[NPAGES];

	static PageCache _instance;
};