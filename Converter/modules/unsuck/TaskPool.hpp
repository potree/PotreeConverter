
#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include <vector>
#include <functional>
#include <atomic>

//using namespace std;

using std::thread;
using std::atomic;
using std::mutex;
using std::vector;
using std::deque;
using std::function;
using std::lock_guard;

// might be better off using https://github.com/progschj/ThreadPool
template<class Task>
class TaskPool {
public:
	size_t numThreads = 0;
	deque<shared_ptr<Task>> tasks;
	using TaskProcessorType = function<void(shared_ptr<Task>)>;
	TaskProcessorType processor;

	vector<thread> threads;

	atomic<bool> isClosed = false;
	atomic<int> busyThreads = 0;

	mutex mtx_task;

	TaskPool(size_t numThreads, TaskProcessorType processor) {
		this->numThreads = numThreads;
		this->processor = processor;

		for (int i = 0; i < numThreads; i++) {

			threads.emplace_back([this]() {

				while (true) {

					shared_ptr<Task> task = nullptr;

					{ // retrieve task or leave thread if done
						lock_guard<mutex> lock(mtx_task);

						bool allDone = tasks.size() == 0 && isClosed;
						bool waitingForWork = tasks.size() == 0 && !allDone;
						bool workAvailable = tasks.size() > 0;

						if (allDone) {
							break;
						} else if (workAvailable) {
							task = tasks.front();
							tasks.pop_front();

							if (task != nullptr) {
								busyThreads++;
							}
						}


					}

					if (task != nullptr) {
						this->processor(task);
						busyThreads--;
					}

					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}

			});
		}

	}

	~TaskPool() {
		this->close();
	}

	void addTask(shared_ptr<Task> t) {
		lock_guard<mutex> lock(mtx_task);

		tasks.push_back(t);
	}

	void close() {
		if (isClosed) {
			return;
		}

		isClosed = true;

		for (thread& t : threads) {
			t.join();
		}
	}

	bool isWorkDone() {

		lock_guard<mutex> lock(mtx_task);

		bool noTasksLeft = tasks.size() == 0;
		bool noTasksInProcess = busyThreads == 0;

		return noTasksLeft && noTasksInProcess;
	}

	void waitTillEmpty() {

		while (true) {

			size_t size = 0;

			{
				lock_guard<mutex> lock(mtx_task);

				size = tasks.size();
			}

			if (size == 0) {
				return;
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

		}

	}

};

