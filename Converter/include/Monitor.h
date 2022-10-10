#pragma once

#include <thread>
#include <map>

#include "converter_utils.h"

using std::thread;
using std::map;

struct Monitor {
	thread t;
	bool stopRequested = false;
	State* state = nullptr;
	map<string, string> messages;

	Monitor(State* state){
		this->state = state;
	}

	void _print(){
		auto ram = getMemoryData();
		auto CPU = getCpuData();
		double GB = 1024.0 * 1024.0 * 1024.0;

		double throughput = (double(this->state->pointsProcessed) / this->state->duration) / 1'000'000.0;

		double progressPass = 100.0 * this->state->progress();
		double progressTotal = (100.0 * double(this->state->currentPass - 1) + progressPass) / double(this->state->numPasses);

		string strProgressPass = formatNumber(progressPass) + "%";
		string strProgressTotal = formatNumber(progressTotal) + "%";
		string strTime = formatNumber(now()) + "s";
		string strDuration = formatNumber(this->state->duration) + "s";
		string strThroughput = formatNumber(throughput) + "MPs";

		string strRAM = formatNumber(double(ram.virtual_usedByProcess) / GB, 1)
			+ "GB (highest " + formatNumber(double(ram.virtual_usedByProcess_max) / GB, 1) + "GB)";
		string strCPU = formatNumber(CPU.usage) + "%";

		stringstream ss;
		ss << "[" << strProgressTotal << ", " << strTime << "], "
			<< "[" << this->state->name << ": " << strProgressPass 
			<< ", duration: " << strDuration 
			<< ", throughput: " << strThroughput << "]"
			<< "[RAM: " << strRAM << ", CPU: " << strCPU << "]" << endl;

		cout << ss.str() << std::flush;

	}

	void start(){

		Monitor* _this = this;
		this->t = thread([_this]() {

			using namespace std::chrono_literals;

			std::this_thread::sleep_for(1'000ms);
			
			cout << endl;

			while (!_this->stopRequested) {

				_this->_print();

				std::this_thread::sleep_for(1'000ms);
			}

		});

	}

	void print(string key, string message){

	}

	void stop() {

		stopRequested = true;

		t.join();
	}

};