﻿#pragma once
#include"Common.h"

class ThreadCache
{
public:
	// 申请和释放内存对象
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);
	// 从中⼼缓存获取对象
	void* FetchFromCentralCache(size_t index, size_t size);
	// 释放对象时，链表过⻓时，回收内存回到中⼼缓存
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeList[NFREELIST];
};
//TLS thread local storage
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;