#include "unsuck.hpp"

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <thread>

void printMemoryReport() {
	static constexpr auto gb = 1024.0 * 1024.0 * 1024.0;
	const auto memoryData = getMemoryData();
	const auto vm = double(memoryData.virtual_usedByProcess) / gb;
	const auto pm = double(memoryData.physical_usedByProcess) / gb;

	std::cout
		<< "memory usage: "
		<< "virtual: " << formatNumber(vm, 1) << " GB, "
		<< "physical: " << formatNumber(pm, 1) << " GB"
		<< std::endl;
}

void launchMemoryChecker(int64_t maxMB, double checkInterval) {
	const auto interval = std::chrono::milliseconds(int64_t(checkInterval * 1000));

	std::thread([maxMB, interval]() {
		static constexpr double lastReport = 0.0;
		static constexpr double reportInterval = 1.0;
		static constexpr double lastUsage = 0.0;
		static constexpr double largestUsage = 0.0;

		while (true) {
			auto memdata = getMemoryData();
			std::this_thread::sleep_for(interval);
		}
	}).detach();
}

#ifdef _WIN32
	#include "TCHAR.h"
	#include "pdh.h"
	#include "windows.h"
	#include "psapi.h"

// see https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
MemoryData getMemoryData() {

	MemoryData data;

	{
		MEMORYSTATUSEX memInfo;
		memInfo.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&memInfo);
		DWORDLONG totalVirtualMem = memInfo.ullTotalPageFile;
		DWORDLONG virtualMemUsed = memInfo.ullTotalPageFile - memInfo.ullAvailPageFile;;
		DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
		DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;

		data.virtual_total = totalVirtualMem;
		data.virtual_used = virtualMemUsed;

		data.physical_total = totalPhysMem;
		data.physical_used = physMemUsed;

	}

	{
		PROCESS_MEMORY_COUNTERS_EX pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
		SIZE_T virtualMemUsedByMe = pmc.PrivateUsage;
		SIZE_T physMemUsedByMe = pmc.WorkingSetSize;

		static size_t virtualUsedMax = 0;
		static size_t physicalUsedMax = 0;

		virtualUsedMax = max(virtualMemUsedByMe, virtualUsedMax);
		physicalUsedMax = max(physMemUsedByMe, physicalUsedMax);

		data.virtual_usedByProcess = virtualMemUsedByMe;
		data.virtual_usedByProcess_max = virtualUsedMax;
		data.physical_usedByProcess = physMemUsedByMe;
		data.physical_usedByProcess_max = physicalUsedMax;
	}


	return data;
}


static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
static int numProcessors;
static HANDLE self;
static bool initialized = false;

void init() {
	SYSTEM_INFO sysInfo;
	FILETIME ftime, fsys, fuser;

	GetSystemInfo(&sysInfo);
	// numProcessors = sysInfo.dwNumberOfProcessors;
	numProcessors = std::thread::hardware_concurrency();

	GetSystemTimeAsFileTime(&ftime);
	memcpy(&lastCPU, &ftime, sizeof(FILETIME));

	self = GetCurrentProcess();
	GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
	memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
	memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));

	initialized = true;
}

CpuData getCpuData() {
	FILETIME ftime, fsys, fuser;
	ULARGE_INTEGER now, sys, user;
	double percent;

	if (!initialized) {
		init();
	}

	GetSystemTimeAsFileTime(&ftime);
	memcpy(&now, &ftime, sizeof(FILETIME));

	GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
	memcpy(&sys, &fsys, sizeof(FILETIME));
	memcpy(&user, &fuser, sizeof(FILETIME));
	percent = (sys.QuadPart - lastSysCPU.QuadPart) +
		(user.QuadPart - lastUserCPU.QuadPart);
	percent /= (now.QuadPart - lastCPU.QuadPart);
	percent /= numProcessors;
	lastCPU = now;
	lastUserCPU = user;
	lastSysCPU = sys;

	CpuData data;
	data.numProcessors = numProcessors;
	data.usage = percent * 100.0;

	return data;
}

#elif defined(__linux__)

// see https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process

#include "sys/types.h"
#include "sys/sysinfo.h"

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

int parseLine(char* line){
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char* p = line;
    
	while (*p < '0' || *p > '9'){ 
		p++;
	}
	
    line[i - 3] = '\0';
    i = atoi(p);
	
    return i;
}

int64_t getVirtualMemoryUsedByProcess(){ //Note: this value is in KB!
    FILE* file = fopen("/proc/self/status", "r");
    int64_t result = -1;
    char line[128];

    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmSize:", 7) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(file);
	
	result = result * 1024;
	
    return result;
}

int64_t getPhysicalMemoryUsedByProcess(){ //Note: this value is in KB!
    FILE* file = fopen("/proc/self/status", "r");
    int64_t result = -1;
    char line[128];

    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(file);
	
	result = result * 1024;
	
    return result;
}


MemoryData getMemoryData() {
	
	struct sysinfo memInfo;

	sysinfo (&memInfo);
	int64_t totalVirtualMem = memInfo.totalram;
	totalVirtualMem += memInfo.totalswap;
	totalVirtualMem *= memInfo.mem_unit;

	int64_t virtualMemUsed = memInfo.totalram - memInfo.freeram;
	virtualMemUsed += memInfo.totalswap - memInfo.freeswap;
	virtualMemUsed *= memInfo.mem_unit;
	
	int64_t totalPhysMem = memInfo.totalram;
	totalPhysMem *= memInfo.mem_unit;
	
	long long physMemUsed = memInfo.totalram - memInfo.freeram;
	physMemUsed *= memInfo.mem_unit;

	int64_t virtualMemUsedByMe = getVirtualMemoryUsedByProcess();
	int64_t physMemUsedByMe = getPhysicalMemoryUsedByProcess();


	MemoryData data;
	
	static int64_t virtualUsedMax = 0;
	static int64_t physicalUsedMax = 0;

	virtualUsedMax = std::max(virtualMemUsedByMe, virtualUsedMax);
	physicalUsedMax = std::max(physMemUsedByMe, physicalUsedMax);

	{
		data.virtual_total = totalVirtualMem;
		data.virtual_used = virtualMemUsed;
		data.physical_total = totalPhysMem;
		data.physical_used = physMemUsed;

	}

	{
		data.virtual_usedByProcess = virtualMemUsedByMe;
		data.virtual_usedByProcess_max = virtualUsedMax;
		data.physical_usedByProcess = physMemUsedByMe;
		data.physical_usedByProcess_max = physicalUsedMax;
	}


	return data;
}


static int numProcessors;
static bool initialized = false;
static unsigned long long lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle;

void init() {
	numProcessors = std::thread::hardware_concurrency();
	
	FILE* file = fopen("/proc/stat", "r");
    fscanf(file, "cpu %llu %llu %llu %llu", &lastTotalUser, &lastTotalUserLow, &lastTotalSys, &lastTotalIdle);
    fclose(file);

	initialized = true;
}

double getCpuUsage(){
    double percent;
    FILE* file;
    unsigned long long totalUser, totalUserLow, totalSys, totalIdle, total;

    file = fopen("/proc/stat", "r");
    fscanf(file, "cpu %llu %llu %llu %llu", &totalUser, &totalUserLow, &totalSys, &totalIdle);
    fclose(file);

    if (totalUser < lastTotalUser || totalUserLow < lastTotalUserLow ||
        totalSys < lastTotalSys || totalIdle < lastTotalIdle){
        //Overflow detection. Just skip this value.
        percent = -1.0;
    }else{
        total = (totalUser - lastTotalUser) 
			+ (totalUserLow - lastTotalUserLow) 
			+ (totalSys - lastTotalSys);
        percent = total;
        total += (totalIdle - lastTotalIdle);
        percent /= total;
        percent *= 100;
    }

    lastTotalUser = totalUser;
    lastTotalUserLow = totalUserLow;
    lastTotalSys = totalSys;
    lastTotalIdle = totalIdle;

    return percent;
}

CpuData getCpuData() {
	
	if (!initialized) {
		init();
	}

	CpuData data;
	data.numProcessors = numProcessors;
	data.usage = getCpuUsage();

	return data;
}

#elif defined(__APPLE__) && defined(__MACH__)

#include <mach/host_info.h>
#include <mach/kern_return.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_vm.h>
#include <mach/machine.h>
#include <mach/message.h>
#include <mach/port.h>
#include <mach/shared_region.h>
#include <mach/task.h>
#include <mach/task_info.h>
#include <mach/vm_prot.h>
#include <mach/vm_region.h>
#include <mach/vm_statistics.h>
#include <sys/sysctl.h>
#include <unistd.h>

namespace {

struct MachPort {
	mach_port_t port;

	~MachPort() {
		if (port != MACH_PORT_NULL) {
			mach_port_deallocate(mach_task_self(), port);
		}
	}
};

auto getMachHostPort() {
	static auto machHost = MachPort{ mach_host_self() };
	return machHost.port;
}

} // namespace

MemoryData getMemoryData() {
	const auto pageSize = sysconf(_SC_PAGESIZE);
	if (pageSize < 0) {
		return {};
	}

	MemoryData data;

	{
		size_t physMem = 0;
		auto length = sizeof(physMem);
		int mib[2] = { CTL_HW, HW_MEMSIZE };
		if (sysctl(mib, 2, &physMem, &length, nullptr, 0) == 0) {
			data.physical_total = physMem;
		}
	}
	{
		xsw_usage vmusage;
		size_t size = sizeof(vmusage);
		int mib[2] = { CTL_VM, VM_SWAPUSAGE };
		if (sysctl(mib, 2, &vmusage, &size, nullptr, 0) == 0) {
			data.virtual_used = vmusage.xsu_used;
			
			// Note: Unlike most UNIX-based operating systems, OS X does not use a preallocated disk partition for the backing store. Instead, it uses all of the available space on the machine’s boot partition.
			// https://developer.apple.com/library/archive/documentation/Performance/Conceptual/ManagingMemory/Articles/AboutMemory.html
			// https://superuser.com/questions/1396826/macos-virtual-memory-limit-architectural-or-practical
			data.virtual_total = vmusage.xsu_total;
		}
	}
	auto count = HOST_VM_INFO_COUNT;
	vm_statistics_data_t vm_stat;
	if (host_statistics(getMachHostPort(), HOST_VM_INFO, (host_info_t)&vm_stat, &count)
		== KERN_SUCCESS)
	{
		const auto usedMem =
			((size_t)vm_stat.active_count + vm_stat.inactive_count + vm_stat.wire_count) * pageSize;
		// const auto freeMem = vm_stat.free_count * (size_t)pageSize;

		data.physical_used = usedMem;
	}
	{
		mach_task_basic_info info;
		mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
		if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount)
			== KERN_SUCCESS) {
			data.physical_usedByProcess = info.resident_size;
		}
	}
	{ // https://stackoverflow.com/questions/78815207/why-is-virtual-size-on-macos-so-different-from-vmsize-on-linux
		const auto isSharedRegion = [](mach_vm_address_t address) {
			return address >= SHARED_REGION_BASE && address < (SHARED_REGION_BASE + SHARED_REGION_SIZE);
		};

		mach_vm_address_t address = 0;
		size_t sum = 0;
		size_t sharedSum = 0;
		mach_vm_size_t vmsize = 0;
		vm_region_basic_info_data_64_t info;
		while (true) {
			auto infoCount = VM_REGION_BASIC_INFO_COUNT_64;
			MachPort objName;
			if (mach_vm_region(mach_task_self(), &address, &vmsize, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &infoCount, &objName.port)
				!= KERN_SUCCESS) {
				break;
			}

			if (info.protection == VM_PROT_NONE) {
				address += vmsize;
				continue;
			}

			if (isSharedRegion(address)) {
				sharedSum += vmsize;
			}
			address += vmsize;
			sum += vmsize;
		}
		data.virtual_usedByProcess = sum - sharedSum;
	}

	static size_t physicalUsedMax = 0;
	static size_t virtualUsedMax = 0;
	physicalUsedMax = std::max(data.physical_usedByProcess, physicalUsedMax);
	virtualUsedMax = std::max(data.virtual_usedByProcess, virtualUsedMax);
	data.physical_usedByProcess_max = physicalUsedMax;
	data.virtual_usedByProcess_max = virtualUsedMax;

	return data;
}

CpuData getCpuData() {
	static auto numProcessors = std::thread::hardware_concurrency();

	CpuData data;
	data.numProcessors = numProcessors;

	struct CPULoadState {
		size_t prevTotalTicks = 0;
		size_t prevIdleTicks = 0;
	};
	static CPULoadState state;

	host_cpu_load_info_data_t cpuInfo;
	auto count = HOST_CPU_LOAD_INFO_COUNT;
	if (host_statistics(getMachHostPort(), HOST_CPU_LOAD_INFO, (host_info_t)&cpuInfo, &count)
		!= KERN_SUCCESS)
	{
		data.usage = NAN;
		return data;
	}

	size_t totalTicks = 0;
	for (uint32_t i = 0; i < CPU_STATE_MAX; ++i)
	{
		totalTicks += cpuInfo.cpu_ticks[i];
	}
	const auto idleTicks = cpuInfo.cpu_ticks[CPU_STATE_IDLE];

	const auto totalTicksSinceLast = totalTicks - state.prevTotalTicks;
	const auto idleTicksSinceLast = idleTicks - state.prevIdleTicks;

	data.usage = (1.0 - ((double)idleTicksSinceLast / (double)totalTicksSinceLast)) * 100.0;

	state.prevTotalTicks = totalTicks;
	state.prevIdleTicks = idleTicks;

	return data;
}

#endif
