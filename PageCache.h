#pragma once
#include"Common.h"
#include "PageMap.h"
class PageCache
{
public:
	//��ȡ����
	static PageCache* GetPageCache()
	{
		return &_instance;
	}
	//��ȡһ��kҳ��span
	SPan* NewSpan(size_t k);
	// ��ȡ�Ӷ���span��ӳ��
	SPan* MapObjectToSpan(void* obj);

	// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
	void ReleaseSpanToPageCache(SPan* span);

	std::mutex _pageMtx;
private:
	PageCache() {}
	PageCache(const PageCache&) = delete;
private:
	ObjectPool<SPan> _spanPool;
	//ҳ�ź�span��ӳ��
	//std::unordered_map<PAGE_ID, SPan*> _spanMap;
#ifdef _WIN64
	TCMalloc_PageMap3<64 - PAGE_SHIFT> _spanMap;
#else
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _spanMap;
#endif
	SpanList _spanList[NPAGES];

	static PageCache _instance;
};