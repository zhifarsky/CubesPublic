#pragma once
#include <windows.h>
#include "Typedefs.h"

template<typename Type>
struct DynamicArray {
	Type* items;
	int count;
	int capacity;

	void append(Type& item) {
		if (count >= capacity) {
			if (capacity == 0) capacity = 256;
			else capacity *= 2;
			items = (Type*)realloc(items, capacity * sizeof(Type));
		}
		items[count++] = item;
	}

	Type operator[](int index) {
		return items[index];
	}
};

struct MemoryArena {
	void* memory;
	u32 size;
	u32 capacity;

	void init(u32 capacity);
	void* alloc(u32 allocSize);
};

struct QueueTaskItem {
	int taskIndex;
	bool valid;
};

struct WorkQueue {
	int volatile taskCount;
	int volatile nextTask;
	int volatile taskCompletionCount;
	HANDLE semaphore;

	WorkQueue(int maxSemaphore);

	void addTask(); // добавить задачу
	void clearTasks();

	QueueTaskItem getNextTask(); 	// получить индекс задачи, которую нужно выполнить
	void setTaskCompleted(); // выполнение задачи закончено


	void waitAndClear(); // ожидание выполнения всех задач
	bool workStillInProgress();

};