#pragma once
#include"Common.h"

//单例模式(饿汉)
class CentralCache
{
public:

private:
	CentralCache() {}
	CentralCache(const CentralCache&) = delete;
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}
	// 从中⼼缓存获取⼀定数量的对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);
	// 获取⼀个⾮空的span
	SPan* GetOneSpan(SpanList& list, size_t size);
	// 将⼀定数量的对象释放到中⼼缓存
	void ReleaseListToSpans(void* start, void* end, size_t size);
private:
	static CentralCache _sInst;
	SpanList _spanList[NFREELIST];
};
