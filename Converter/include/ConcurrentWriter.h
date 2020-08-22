
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <fstream>

#include "unsuck/unsuck.hpp"
#include "converter_utils.h"

using std::shared_ptr;
using std::string;
using std::unordered_map;
using std::vector;
using std::thread;
using std::mutex;
using std::lock_guard;
using std::fstream;
using std::ios;
using std::atomic_int64_t;

struct ConcurrentWriter {

	unordered_map<string, vector<shared_ptr<Buffer>>> todo;
	unordered_map<string, int> locks;
	atomic_int64_t todoBytes = 0;
	atomic_int64_t writtenBytes = 0;

	vector<thread> threads;

	mutex mtx_todo;
	size_t numThreads = 1;

	bool joinRequested = false;
	mutex mtx_join;

	double tStart = 0;

	ConcurrentWriter(size_t numThreads, State& state) {
		this->numThreads = numThreads;

		this->tStart = now();

		for (int i = 0; i < numThreads; i++) {
			threads.emplace_back([&]() {
				flushThread();
			});
		}

		using namespace std::chrono_literals;

		threads.emplace_back([&]() {
			while (true) {

				{
					lock_guard<mutex> lockT(mtx_todo);
					lock_guard<mutex> lockJ(mtx_join);

					bool nothingTodo = todo.size() == 0;

					if (nothingTodo && joinRequested) {
						return;
					}

					int64_t mbTodo = todoBytes / (1024 * 1024);
					int64_t mbDone = writtenBytes / (1024 * 1024);

					double duration = now() - tStart;
					double throughput = mbDone / duration;

					state.name = "DISTRIBUTING";
				}

				

				std::this_thread::sleep_for(100ms);
			}
		});

	}

	~ConcurrentWriter() {
		this->join();
	}

	void waitUntilMemoryBelow(int64_t maxMegabytesOutstanding) {

		using namespace std::chrono_literals;

		while (todoBytes / (1024 * 1024) > maxMegabytesOutstanding) {
			std::this_thread::sleep_for(10ms);
		}

	}

	void flushThread() {

		using namespace std::chrono_literals;

		while (true) {

			string path = "";
			vector<shared_ptr<Buffer>> work;

			{
				//auto tStart = now();
				lock_guard<mutex> lockT(mtx_todo);
				lock_guard<mutex> lockJ(mtx_join);
				//auto duration = now() - tStart;
				//if (duration > 0.01) {
				//	cout << "long lock duration: " + to_string(duration) << endl;
				//}

				bool nothingTodo = todo.size() == 0;

				if (nothingTodo && joinRequested) {
					return;
				} else {

					auto it = todo.begin();

					while (it != todo.end()) {

						string path = it->first;

						if (locks.find(path) == locks.end()) {
							break;
						}

						it++;
					}




					if (it != todo.end()) {
						path = it->first;
						work = it->second;

						todo.erase(it);
						locks[path] = 1;
					}

					
				}
			}

			// if no work available, sleep and try again later
			if (work.size() == 0) {
				std::this_thread::sleep_for(10ms);
				continue;
			} 

			fstream fout;
			fout.open(path, ios::out | ios::app | ios::binary);

			for (auto batch : work) {
				fout.write(batch->data_char, batch->size);

				todoBytes -= batch->size;
				writtenBytes += batch->size;
			}

			fout.close();

			{
				lock_guard<mutex> lockT(mtx_todo);
				lock_guard<mutex> lockJ(mtx_join);

				auto itLocks = locks.find(path);
				locks.erase(itLocks);
			}

		}

	}

	void write(string path, shared_ptr<Buffer> data) {
		lock_guard<mutex> lock(mtx_todo);

		todoBytes += data->size;

		todo[path].push_back(data);
	}

	void join() {
		{
			lock_guard<mutex> lock(mtx_join);

			//cout << "joinRequested" << endl;
			joinRequested = true;
		}
		

		for (auto& t : threads) {
			t.join();
		}

		threads.clear();

		//cout << "writer joined \n";

	}

};




