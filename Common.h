#pragma once
#define _CRT_SECURE_NO_WARNINGS 1
#include <iostream>
#include <vector>
#include <algorithm>
#include<unordered_map>

#include <time.h>
#include <assert.h>

#include <thread>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#endif
using std::cout;
using std::endl;

// ⼩于等于MAX_BYTES，就找thread cache申请
// ⼤于MAX_BYTES，就直接找page cache或者系统堆申请
static const size_t MAX_BYTES = 256 * 1024;
// thread cache 和 central cache⾃由链表哈希桶的表⼤⼩
static const size_t NFREELIST = 208;
// page cache 管理span list哈希表⼤⼩,数组下标从0开始，这里多给一个位置，占位
static const size_t NPAGES = 129;
// ⻚⼤⼩转换偏移, 即⼀⻚定义为2^13,也就是8KB
static const size_t PAGE_SHIFT = 13;

//条件编译，32位系统下为4，64位系统下为8
#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#else _WIN32
typedef size_t PAGE_ID;
#endif //_WIN32

//内存申请函数，脱离malloc函数，直接调用操作系统分配内存
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}
//内存释放函数，脱离free函数，直接调用操作系统释放内存
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}

//获取对象的头4/8个字节用作指针
static void*& NextObj(void* obj)
{
	return *(void**)obj;
}
//管理切分好的小对象的自由链表
class FreeList
{
public:
	//头插
	void Push(void* obj)
	{
		assert(obj);
		NextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}
	//批量插入
	void PushRange(void* begin, void* end, size_t num)
	{
		NextObj(end) = _freeList;
		_freeList = begin;
		_size += num;
	}
	//头删
	void* Pop()
	{
		assert(_freeList);
		void* obj = _freeList;
		_freeList = NextObj(obj);
		--_size;
		return obj;
	}

	void PopRange(void*& begin, void*& end, size_t num)
	{
		assert(num >= 0 && num <= _size);
		begin = _freeList;
		end = begin;
		for (size_t i = 0; i < num - 1; ++i) {
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= num;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}

	size_t& MaxSize()
	{
		return _maxsize;
	}
	size_t Size()
	{
		return _size;
	}
private:
	void* _freeList = nullptr;
	size_t _maxsize = 1;
	size_t _size = 0;
};

//计算对象大小的对齐映射规则
class SizeClass
{
public:
	//size_t _RoundUp(size_t bytes,size_t Align)
	//{
	//	size_t alignSize=0;
	//	if (bytes % 8 != 0) {
	//		alignSize= bytes % 8;
	//	} else {
	//		alignSize=(bytes/Align+1)* Align;
	//	}
	//	return alignSize;
	//}
	//整体控制在最多10%的内存碎片浪费
	// [1,128] 8byte对⻬ freelist[0,16)
	// [128+1,1024] 16byte对⻬ freelist[16,72)
	// [1024+1,81024] 128byte对⻬ freelist[72,128)
	// [8*1024+1,641024] 1024byte对⻬ freelist[128,184)
	// [64*1024+1,256*1024] 8*1024byte对⻬ freelist[184,208)
	static inline size_t _RoundUp(size_t bytes, size_t Align)
	{
		return ((bytes + Align - 1) & ~(Align - 1));
	}
	static inline size_t RoundUp(size_t bytes)
	{
		//8Byte对齐
		if (bytes <= 128) {
			return _RoundUp(bytes, 8);
			//16Byte对齐
		} else if (bytes <= 1024) {
			return _RoundUp(bytes, 16);
			//128Byte对齐
		} else if (bytes <= 8 * 1024) {
			return _RoundUp(bytes, 128);
			//1KByte对齐
		} else if (bytes <= 64 * 1024) {
			return _RoundUp(bytes, 1024);
			//8KByte对齐
		} else if (bytes <= 256 * 1024) {
			return _RoundUp(bytes, 8 * 1024);
		} else {
			return _RoundUp(bytes, 1 << PAGE_SHIFT);
		}
		return -1;
	}
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		// 计算⼤⼩在链表上的索引（位运算）
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	// 计算映射的哪⼀个⾃由链表桶
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 56 };
		if (bytes <= 128) {
			return _Index(bytes, 3);
		} else if (bytes <= 1024) {
			return _Index(bytes - 128, 4) + group_array[0];
		} else if (bytes <= 8 * 1024) {
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		} else if (bytes <= 64 * 1024) {
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
		} else if (bytes <= 256 * 1024) {
			return _Index(bytes - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
		} else {
			assert(false);
		}

		return -1;
	}

	// ⼀次从中⼼缓存获取多少个
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		// [2, 512]，⼀次批量移动多少个对象的(慢启动)上限值
		// ⼩对象⼀次批量上限⾼
		// ⼩对象⼀次批量上限低
		int num = MAX_BYTES / size;
		//下限为2
		if (num < 2)
			num = 2;
		// 上限为512
		if (num > 512)
			num = 512;
		return num;
	}
	// 计算⼀次向系统获取⼏个⻚
	// 单个对象 8byte
	// ...
	// 单个对象 256KB
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;
		npage >>= PAGE_SHIFT;
		if (npage == 0)
			npage = 1;
		return npage;
	}
};

// span管理类，管理多个连续页大块内存跨度结构
class SPan
{
public:
	// 大块内存起始页的页ID
	PAGE_ID _PageId = 0;
	// ⻚数
	size_t _size = 0;
	//切好的对象的大小
	size_t _objectSize = 0;
	//切好的小块内存，被分配给thread cache的计数
	size_t _useCount = 0;
	//双向链表
	SPan* _Next = nullptr;
	SPan* _Prev = nullptr;
	//切好的小块内存的自由链表
	void* _freeList = nullptr;
	//判断是否被使用
	bool _isUse = false;
};

//带头双向循环链表
class SpanList
{
public:
	SpanList()
	{
		_head = new SPan;
		_head->_Next = _head;
		_head->_Prev = _head;
	}
	//不单独写一个迭代器，直接用函数迭代
	SPan* Begin()
	{
		return _head->_Next;
	}

	SPan* End()
	{
		return _head;
	}
	bool Empty()
	{
		return Begin() == End();
	}

	void PushFront(SPan* span)
	{
		assert(span);
		Insert(span, Begin());
	}

	SPan* PopFront()
	{
		assert(!Empty());
		SPan* span = Begin();
		Erase(span);
		return span;
	}

	void Insert(SPan* newSpan, SPan* pos)
	{
		assert(newSpan);
		assert(pos);
		SPan* prev = pos->_Prev;
		prev->_Next = newSpan;
		newSpan->_Prev = prev;
		newSpan->_Next = pos;
		pos->_Prev = newSpan;
	}

	void Erase(SPan* pos)
	{
		assert(pos);
		assert(pos != _head);
		SPan* prev = pos->_Prev;
		SPan* next = pos->_Next;
		prev->_Next = next;
		next->_Prev = prev;
	}
	//桶锁
	std::mutex _spanMtx;
private:
	SPan* _head;
};
