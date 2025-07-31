#include <stdlib.h>
#include <assert.h>
#include <intrin.h>
#include "DataStructures.h"

void MemoryArena::init(u32 capacity) {
	memory = malloc(capacity);
	this->capacity = capacity;
}

void* MemoryArena::alloc(u32 allocSize) {
	assert(this->size + allocSize <= capacity);
	size += allocSize;
	return (u8*)memory + size;
}

WorkQueue::WorkQueue(int maxSemaphore) {
	this->semaphore = CreateSemaphore(0, 0, maxSemaphore, 0);
	this->taskCompletionCount = 0;
	this->taskCount = 0;
	this->nextTask = 0;
}

void WorkQueue::addTask() {
	// TODO: барьеры?
	_WriteBarrier();
	_mm_sfence();
	taskCount++;
	ReleaseSemaphore(semaphore, 1, 0);
}

void WorkQueue::clearTasks() {
	taskCount = nextTask = taskCompletionCount = 0;
}

QueueTaskItem WorkQueue::getNextTask() {
	QueueTaskItem res;
	res.valid = false;

	if (nextTask < taskCount) {
		res.taskIndex = InterlockedIncrement((LONG volatile*)&nextTask) - 1;
		res.valid = true;
		// TODO: барьеры?
		_ReadBarrier();
	}

	return res;
}

void WorkQueue::setTaskCompleted() {
	InterlockedIncrement((LONG volatile*)&taskCompletionCount);
}

bool WorkQueue::workStillInProgress() {
	return taskCount != taskCompletionCount;
}

void WorkQueue::waitAndClear() {
	while (taskCount != taskCompletionCount) {}
	taskCount = nextTask = taskCompletionCount = 0;
}