
#include "unsuck.hpp"

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


void printMemoryReport() {

	auto memoryData = getMemoryData();
	double vm = double(memoryData.virtual_usedByProcess) / (1024.0 * 1024.0 * 1024.0);
	double pm = double(memoryData.physical_usedByProcess) / (1024.0 * 1024.0 * 1024.0);

	stringstream ss;
	ss << "memory usage: "
		<< "virtual: " << formatNumber(vm, 1) << " GB, "
		<< "physical: " << formatNumber(pm, 1) << " GB"
		<< endl;

	cout << ss.str();

}

void launchMemoryChecker(int64_t maxMB, double checkInterval) {

	auto interval = std::chrono::milliseconds(int64_t(checkInterval * 1000));

	thread t([maxMB, interval]() {

		static double lastReport = 0.0;
		static double reportInterval = 1.0;
		static double lastUsage = 0.0;
		static double largestUsage = 0.0;

		while (true) {
			auto memdata = getMemoryData();

			using namespace std::chrono_literals;
			std::this_thread::sleep_for(interval);
		}

	});
	t.detach();

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


void printMemoryReport() {

	auto memoryData = getMemoryData();
	double vm = double(memoryData.virtual_usedByProcess) / (1024.0 * 1024.0 * 1024.0);
	double pm = double(memoryData.physical_usedByProcess) / (1024.0 * 1024.0 * 1024.0);

	stringstream ss;
	ss << "memory usage: "
		<< "virtual: " << formatNumber(vm, 1) << " GB, "
		<< "physical: " << formatNumber(pm, 1) << " GB"
		<< endl;

	cout << ss.str();

}

void launchMemoryChecker(int64_t maxMB, double checkInterval) {

	auto interval = std::chrono::milliseconds(int64_t(checkInterval * 1000));

	thread t([maxMB, interval]() {

		static double lastReport = 0.0;
		static double reportInterval = 1.0;
		static double lastUsage = 0.0;
		static double largestUsage = 0.0;

		while (true) {
			auto memdata = getMemoryData();

			using namespace std::chrono_literals;
			std::this_thread::sleep_for(interval);
		}

	});
	t.detach();

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



#elif defined(__APPLE__)

// see https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process

#include <string>
#include <sstream>
#include <mach/mach.h>
#include <sys/sysctl.h> // for sysctl

#include "memory.h"

#include <unistd.h> // usleep()

// #include "cpu.h"

int64_t mem_status()
{
  // These values are in bytes
  float used_mem;
  float percentage_mem;
  float free_mem;
  float free_mem_in_gigabytes; // used to check if free mem < 1 GB
  //u_int64_t unused_mem;

  vm_size_t page_size;
  vm_statistics_data_t vm_stats;

  mach_port_t mach_port = mach_host_self();
  mach_msg_type_number_t count = sizeof( vm_stats ) / sizeof( natural_t );
  if( KERN_SUCCESS == host_page_size( mach_port, &page_size ) &&
      KERN_SUCCESS == host_statistics( mach_port, HOST_VM_INFO,
        ( host_info_t )&vm_stats, &count )
    )
  {
    //unused_mem = static_cast<u_int64_t>( vm_stats.free_count * page_size );

    used_mem = static_cast<float>(
        ( vm_stats.active_count + vm_stats.wire_count ) * page_size);
  }

  return used_mem;
}

int64_t getPhysicalMemoryUsedByProcess(){ //Note: this value is in KB!
    int64_t result = mem_status();
	
	result = result * 1024;
	
    return result;
}


MemoryData getMemoryData() {
  	u_int64_t total_mem;

	int mib[] = { CTL_HW, HW_MEMSIZE };
  	size_t length = sizeof( total_mem );
  	sysctl( mib, 2, &total_mem, &length, NULL, 0 );

	int64_t totalVirtualMem = 0;
	totalVirtualMem += 0;
	totalVirtualMem *= 0;

	int64_t virtualMemUsed = 0;
	virtualMemUsed += 0;
	virtualMemUsed *= 0;
	
	int64_t totalPhysMem = total_mem;
	totalPhysMem *= 1;
	

	int64_t virtualMemUsedByMe = 0;
	int64_t physMemUsedByMe = getPhysicalMemoryUsedByProcess();

	
	long long physMemUsed = total_mem - physMemUsedByMe;
	physMemUsed *= 1;


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


void printMemoryReport() {

	auto memoryData = getMemoryData();
	double vm = double(memoryData.virtual_usedByProcess) / (1024.0 * 1024.0 * 1024.0);
	double pm = double(memoryData.physical_usedByProcess) / (1024.0 * 1024.0 * 1024.0);

	stringstream ss;
	ss << "memory usage: "
		<< "virtual: " << formatNumber(vm, 1) << " GB, "
		<< "physical: " << formatNumber(pm, 1) << " GB"
		<< endl;

	cout << ss.str();

}

void launchMemoryChecker(int64_t maxMB, double checkInterval) {

	auto interval = std::chrono::milliseconds(int64_t(checkInterval * 1000));

	thread t([maxMB, interval]() {

		static double lastReport = 0.0;
		static double reportInterval = 1.0;
		static double lastUsage = 0.0;
		static double largestUsage = 0.0;

		while (true) {
			auto memdata = getMemoryData();

			using namespace std::chrono_literals;
			std::this_thread::sleep_for(interval);
		}

	});
	t.detach();

}

static int numProcessors;
static bool initialized = false;

void init() {
	numProcessors = std::thread::hardware_concurrency();

	initialized = true;
}

uint32_t get_cpu_count()
{
  return sysconf( _SC_NPROCESSORS_ONLN );
}

double getCpuUsage(){
  
    return cpu_percentage(0);
}

host_cpu_load_info_data_t _get_cpu_percentage()
{
  kern_return_t              error;
  mach_msg_type_number_t     count;
  host_cpu_load_info_data_t  r_load;
  mach_port_t                mach_port;

  count = HOST_CPU_LOAD_INFO_COUNT;
  mach_port = mach_host_self();
  error = host_statistics(mach_port, HOST_CPU_LOAD_INFO, 
      ( host_info_t )&r_load, &count );

  if ( error != KERN_SUCCESS )
  {
    return host_cpu_load_info_data_t();
  }

  return r_load;
}

double cpu_percentage( unsigned int cpu_usage_delay )
{
  // Get the load times from the XNU kernel
  host_cpu_load_info_data_t load1 = _get_cpu_percentage();
  usleep( cpu_usage_delay );
  host_cpu_load_info_data_t load2 = _get_cpu_percentage();

  // Current load times
  unsigned long long current_user = load1.cpu_ticks[CP_USER];
  unsigned long long current_system = load1.cpu_ticks[CP_SYS];
  unsigned long long current_nice = load1.cpu_ticks[CP_NICE];
  unsigned long long current_idle = load1.cpu_ticks[CP_IDLE];
  // Next load times
  unsigned long long next_user = load2.cpu_ticks[CP_USER];
  unsigned long long next_system = load2.cpu_ticks[CP_SYS];
  unsigned long long next_nice = load2.cpu_ticks[CP_NICE];
  unsigned long long next_idle = load2.cpu_ticks[CP_IDLE];
  // Difference between the two
  unsigned long long diff_user = next_user - current_user;
  unsigned long long diff_system = next_system - current_system;
  unsigned long long diff_nice = next_nice - current_nice;
  unsigned long long diff_idle = next_idle - current_idle;

  return static_cast<double>( diff_user + diff_system + diff_nice ) / 
    static_cast<double>( diff_user + diff_system + diff_nice + diff_idle ) * 
    100.0;
}

CpuData getCpuData() {
	
	if (!initialized) {
		init();
	}

	CpuData data;
	data.numProcessors = get_cpu_count();
	data.usage = getCpuUsage();

	return data;
}

#endif