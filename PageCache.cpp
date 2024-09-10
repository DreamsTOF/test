#include "PageCache.h"

PageCache PageCache::_instance;

SPan* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	if (k > NPAGES - 1) {
		//ֱ���������
		void* ptr = SystemAlloc(k);
		//����spanPool����һ��span����ʹ��malloc
		//SPan* span = new SPan;
		SPan* span = _spanPool.New();
		//��ʼ��span
		span->_PageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_size = k;
		//_spanMap[span->_PageId] = span;
		_spanMap.set(span->_PageId, span);
		return span;
	}
	//�ȼ���k��Ͱ���Ƿ��п��е�span
	if (!_spanList[k].Empty()) {
		//return _spanList[k].PopFront();
			//����span��ҳ����ӵ�spanMap�У�����central cache�����ڴ�ʱ���Ҷ�Ӧ��span
		//�ӿ���������ȡ��һ��span����ʱ���ֱ�ӷ��أ�span��ҳ��û�б�ӳ�䵽spanMap�У���Ҫ�ٵ���һ��spanMap.Add
		SPan* Newspan = _spanList[k].PopFront();
		for (PAGE_ID i = 0; i < Newspan->_size; i++) {
			//_spanMap[Newspan->_PageId + i] = Newspan;
			_spanMap.set(Newspan->_PageId + i, Newspan);
		}
		return Newspan;
	}

	//���һ�º����Ͱ����û�п��е�span����������з�һ�����һ���ٷ����зֵ��ǿ�
	for (size_t i = k + 1; i < NPAGES; i++) {
		if (!_spanList[i].Empty()) {
			SPan* kspan = _spanList[i].PopFront();
			//����һ����span���ڱ�ʶ�зֺ��span
			//SPan* Newspan = new SPan;
			SPan* Newspan = _spanPool.New();
			//�з�span
			Newspan->_PageId = kspan->_PageId;
			Newspan->_size = k;
			//ҳ���ƶ�k ҳ
			kspan->_PageId += k;
			//ʣ��ҳ��-k
			kspan->_size -= k;
			//��ʣ�µ�span����
			_spanList[kspan->_size].PushFront(kspan);
			//�洢�зֺ��span����ҳ�ŵ�ӳ�䣬����page cache�����ڴ�ʱ���кϲ�����
			//_spanMap[kspan->_PageId] = kspan;
			_spanMap.set(kspan->_PageId, kspan);
			//_spanMap[kspan->_PageId + kspan->_size - 1] = kspan;
			_spanMap.set(kspan->_PageId + kspan->_size - 1, kspan);
			//����span��ҳ����ӵ�spanMap�У�����central cache�����ڴ�ʱ���Ҷ�Ӧ��span
			for (PAGE_ID i = 0; i < Newspan->_size; i++) {
				//_spanMap[Newspan->_PageId + i] = Newspan;
				_spanMap.set(Newspan->_PageId + i, Newspan);
			}
			return Newspan;
		}
	}
	//û�п��е�span��������һ���ڴ棬�зֺ����
	//SPan* bigspan = new SPan;
	SPan* bigspan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	bigspan->_PageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigspan->_size = NPAGES - 1;

	//���зֺ��span������Ȼ��ݹ����NewSpan
	_spanList[bigspan->_size].PushFront(bigspan);
	return NewSpan(k);
}

SPan* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID pageId = (PAGE_ID)obj >> PAGE_SHIFT;
	//raii���������˴��������Զ��ͷ�
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

// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
void PageCache::ReleaseSpanToPageCache(SPan* span)
{
	assert(span);
	//����128ҳ��ֱ�ӻ�����
	if (span->_size > NPAGES - 1) {
		//ͨ��ҳ�����ҵ�span
		void* ptr = (void*)(span->_PageId << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);
		return;
	}
	// ��spanǰ���ҳ�����Խ��кϲ��������ڴ���Ƭ����
	while (1) {
		PAGE_ID prevId = span->_PageId - 1;
		//auto ret = _spanMap.find(prevId);
		auto ret = (SPan*)_spanMap.get(prevId);
		// ǰ���ҳ��û�У����ϲ���
		if (ret == nullptr) {
			break;
		}

		// ǰ������ҳ��span��ʹ�ã����ϲ���
		SPan* prevSpan = ret;
		if (prevSpan->_isUse == true) {
			break;
		}

		// �ϲ�������128ҳ��spanû�취�������ϲ���
		if (prevSpan->_size + span->_size > NPAGES - 1) {
			break;
		}

		span->_PageId = prevSpan->_PageId;
		span->_size += prevSpan->_size;

		_spanList[prevSpan->_size].Erase(prevSpan);
		//delete prevSpan;
		_spanPool.Delete(prevSpan);
	}

	// ���ϲ�
	while (1) {
		PAGE_ID nextId = span->_PageId + span->_size;
		auto ret = (SPan*)_spanMap.get(nextId);
		// �����ҳ��û�У����ϲ���
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
	//���ϲ����span���뵽spanList
	_spanList[span->_size].PushFront(span);
	//���ϲ����span��Ϊδʹ�ã����������spanMap
	span->_isUse = false;
	//_spanMap[span->_PageId] = span;
	_spanMap.set(span->_PageId, span);
	//_spanMap[span->_PageId + span->_size - 1] = span;
	_spanMap.set(span->_PageId + span->_size - 1, span);
}