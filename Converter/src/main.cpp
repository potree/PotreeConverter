

#include <iostream>
#include <execution>

#include "LasLoader/LasLoader.h"
#include "unsuck/unsuck.hpp"
#include "chunker_countsort_laszip.h"
#include "indexer.h"
#include "sampler_poisson.h"
#include "sampler_poisson_average.h"
#include "sampler_random.h"
#include "Attributes.h"

#include "arguments/Arguments.hpp"

using namespace std;

Options parseArguments(int argc, char** argv) {
	Arguments args(argc, argv);

	args.addArgument("source,i,", "input files");
	args.addArgument("outdir,o", "output directory");
	args.addArgument("method,m", "sampling method");
	args.addArgument("chunkMethod", "chunking method");
	args.addArgument("flags", "flags");
	args.addArgument("attributes", "attributes in output file");

	if(!args.has("source")) {
		cout << "/Converter <source> -o <outdir>" << endl;

		exit(1);
	}

	vector<string> source = args.get("source").as<vector<string>>();
	string outdir = args.get("outdir").as<string>();

	string method = args.get("method").as<string>("poisson");
	string chunkMethod = args.get("chunkMethod").as<string>("LASZIP");

	if (source.size() == 0) {
		cout << "/Converter <source> -o <outdir>" << endl;

		exit(1);
	}

	vector<string> flags = args.get("flags").as<vector<string>>();
	vector<string> attributes = args.get("attributes").as<vector<string>>();

	Options options;
	options.source = source;
	options.outdir = outdir;
	options.method = method;
	options.chunkMethod = chunkMethod;
	options.flags = flags;
	options.attributes = attributes;

	cout << "flags: ";
	for (string flag : options.flags) {
		cout << flag << ", ";
	}
	cout << endl;

	return options;
}

struct Curated{
	string name;
	vector<Source> files;
};
Curated curateSources(vector<string> paths) {

	string name = "";

	vector<string> expanded;
	for (auto path : paths) {
		if (fs::is_directory(path)) {
			for (auto& entry : fs::directory_iterator(path)) {
				string str = entry.path().string();

				if (iEndsWith(str, "las") || iEndsWith(str, "laz")) {
					expanded.push_back(str);
				}
			}
		} else if (fs::is_regular_file(path)) {
			if (iEndsWith(path, "las") || iEndsWith(path, "laz")) {
				expanded.push_back(path);
			}
		}

		if (name.size() == 0) {
			name = fs::path(path).stem().string();
		}
	}
	paths = expanded;

	cout << "#paths: " << paths.size() << endl;

	vector<Source> sources;
	sources.reserve(paths.size());

	mutex mtx;
	auto parallel = std::execution::par;
	for_each(parallel, paths.begin(), paths.end(), [&mtx, &sources](string path) {

		auto header = loadLasHeader(path);
		auto filesize = fs::file_size(path);

		Vector3 min = { header.min.x, header.min.y, header.min.z };
		Vector3 max = { header.max.x, header.max.y, header.max.z };

		Source source;
		source.path = path;
		source.min = min;
		source.max = max;
		source.numPoints = header.numPoints;
		source.filesize = filesize;

		lock_guard<mutex> lock(mtx);
		sources.push_back(source);
	});

	return {name, sources};
}

struct LasTypeInfo {
	AttributeType type = AttributeType::UNDEFINED;
	int numElements = 0;
};

LasTypeInfo lasTypeInfo(int typeID) {

	unordered_map<int, AttributeType> mapping = {
		{0, AttributeType::UNDEFINED},
		{1, AttributeType::UINT8},
		{2, AttributeType::INT8},
		{3, AttributeType::UINT16},
		{4, AttributeType::INT16},
		{5, AttributeType::UINT32},
		{6, AttributeType::INT32},
		{7, AttributeType::UINT64},
		{8, AttributeType::INT64},
		{9, AttributeType::FLOAT},
		{10, AttributeType::DOUBLE},

		{11, AttributeType::UINT8},
		{12, AttributeType::INT8},
		{13, AttributeType::UINT16},
		{14, AttributeType::INT16},
		{15, AttributeType::UINT32},
		{16, AttributeType::INT32},
		{17, AttributeType::UINT64},
		{18, AttributeType::INT64},
		{19, AttributeType::FLOAT},
		{20, AttributeType::DOUBLE},

		{21, AttributeType::UINT8},
		{22, AttributeType::INT8},
		{23, AttributeType::UINT16},
		{24, AttributeType::INT16},
		{25, AttributeType::UINT32},
		{26, AttributeType::INT32},
		{27, AttributeType::UINT64},
		{28, AttributeType::INT64},
		{29, AttributeType::FLOAT},
		{30, AttributeType::DOUBLE},
	};

	if (mapping.find(typeID) != mapping.end()) {

		AttributeType type = mapping[typeID];

		int numElements = 0;
		if (typeID <= 10) {
			numElements = 1;
		} else if (typeID <= 20) {
			numElements = 2;
		} else if (typeID <= 30) {
			numElements = 3;
		}

		LasTypeInfo info;
		info.type = type;
		info.numElements = numElements;

		return info;
	} else {
		cout << "ERROR: unkown extra attribute type: " << typeID << endl;
		exit(123);
	}


}

vector<Attribute> parseExtraAttributes(LasHeader& header) {

	vector<uint8_t> extraData;

	for (auto& vlr : header.vlrs) {
		if (vlr.recordID == 4) {
			extraData = vlr.data;
			break;
		}
	}

	//for (auto& vlr : header.extendedVLRs) {
	//	if (vlr.recordID == 4) {
	//		extraData = vlr.data;
	//		break;
	//	}
	//}

	constexpr int recordSize = 192;
	int numExtraAttributes = extraData.size() / recordSize;
	vector<Attribute> attributes;

	for (int i = 0; i < numExtraAttributes; i++) {

		int offset = i * recordSize;
		uint8_t type = read<uint8_t>(extraData, offset + 2);
		uint8_t options = read<uint8_t>(extraData, offset + 3);
		char chrName[32];
		memcpy(chrName, extraData.data() + offset + 4, 32);
		string name(chrName);

		auto info = lasTypeInfo(type);
		string typeName = getAttributeTypename(info.type);
		int elementSize = getAttributeTypeSize(info.type);

		int size = info.numElements * elementSize;
		Attribute xyz(name, size, info.numElements, elementSize, info.type);

		attributes.push_back(xyz);
	}

	return attributes;
}

Attributes computeOutputAttributes(vector<Source>& sources, Options& options) {
	// TODO: take all source files into account when computing the output attributes
	// TODO: a bit wasteful to iterate over source files and load headers twice

	vector<Attribute> list;

	{ // compute list of attributes from first source

		auto header = loadLasHeader(sources[0].path);
		auto format = header.pointDataFormat;

		Attribute xyz("position", 12, 3, 4, AttributeType::INT32);
		Attribute intensity("intensity", 2, 1, 2, AttributeType::UINT16);
		Attribute returns("returns", 1, 1, 1, AttributeType::UINT8);
		Attribute returnNumber("return number", 1, 1, 1, AttributeType::UINT8);
		Attribute numberOfReturns("number of returns", 1, 1, 1, AttributeType::UINT8);
		Attribute classification("classification", 1, 1, 1, AttributeType::UINT8);
		Attribute scanAngleRank("scan angle rank", 1, 1, 1, AttributeType::UINT8);
		Attribute userData("user data", 1, 1, 1, AttributeType::UINT8);
		Attribute pointSourceId("point source id", 2, 1, 2, AttributeType::UINT16);
		Attribute gpsTime("gps-time", 8, 1, 8, AttributeType::DOUBLE);
		Attribute rgb("rgb", 6, 3, 2, AttributeType::UINT16);
		Attribute wavePacketDescriptorIndex("wave packet descriptor index", 1, 1, 1, AttributeType::UINT8);
		Attribute byteOffsetToWaveformData("byte offset to waveform data", 8, 1, 8, AttributeType::UINT64);
		Attribute waveformPacketSize("waveform packet size", 4, 1, 4, AttributeType::UINT32);
		Attribute returnPointWaveformLocation("return point waveform location", 4, 1, 4, AttributeType::FLOAT);
		Attribute XYZt("XYZ(t)", 12, 3, 4, AttributeType::FLOAT);
		Attribute classificationFlags("classification flags", 1, 1, 1, AttributeType::UINT8);
		Attribute scanAngle("scan angle", 2, 1, 2, AttributeType::INT16);
	

		if (format == 0) {
			list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId };
		} else if (format == 1) {
			list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, gpsTime };
		} else if (format == 2) {
			list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, rgb };
		} else if (format == 3) {
			list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, gpsTime, rgb };
		} else if (format == 4) {
			list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, gpsTime,
				wavePacketDescriptorIndex, byteOffsetToWaveformData, waveformPacketSize, returnPointWaveformLocation,
				XYZt
			};
		} else if (format == 5) {
			list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, gpsTime, rgb,
				wavePacketDescriptorIndex, byteOffsetToWaveformData, waveformPacketSize, returnPointWaveformLocation,
				XYZt
			};
		} else if (format == 6) {
			list = { xyz, intensity, returnNumber, numberOfReturns, classificationFlags, classification, userData, scanAngle, pointSourceId, gpsTime };
		} else if (format == 7) {
			list = { xyz, intensity, returnNumber, numberOfReturns, classificationFlags, classification, userData, scanAngle, pointSourceId, gpsTime, rgb };
		} else {
			cout << "ERROR: currently unsupported LAS format: " << int(format) << endl;

			exit(123);
		}

		vector<Attribute> extraAttributes = parseExtraAttributes(header);

		list.insert(list.end(), extraAttributes.begin(), extraAttributes.end());
	}

	Vector3 scale = { Infinity, Infinity, Infinity};
	Vector3 offset = { Infinity, Infinity, Infinity};

	// compute scale and offset from all sources
	{
		mutex mtx;
		auto parallel = std::execution::par;
		for_each(parallel, sources.begin(), sources.end(), [&mtx, &sources, &scale, &offset](Source source) {

			auto header = loadLasHeader(source.path);

			mtx.lock();

			scale.x = std::min(scale.x, header.scale.x);
			scale.y = std::min(scale.y, header.scale.y);
			scale.z = std::min(scale.z, header.scale.z);

			offset.x = std::min(offset.x, source.min.x);
			offset.y = std::min(offset.y, source.min.y);
			offset.z = std::min(offset.z, source.min.z);

			mtx.unlock();
		});
	}

	// filter down to optionally specified attributes
	if(options.attributes.size() > 0){ 
		auto should = options.attributes;
		auto is = list;

		// always add position
		should.insert(should.begin(), { "position" });

		vector<Attribute> filtered;

		for (string name : should) {
			auto it = find_if(is.begin(), is.end(), [name](auto& value){
				return value.name == name;
			});

			if (it != is.end()) {
				filtered.push_back(*it);
			}
		}

		list = filtered;
	}

	auto attributes = Attributes(list);
	attributes.posScale = scale;
	attributes.posOffset = offset;

	{ // print infos


		cout << endl << "output attributes: " << endl;

		int c0 = 30;
		int c1 = 10;
		int c2 = 8;
		int ct = c0 + c1 + c2;

		cout << rightPad("name", c0) << leftPad("offset", c1) << leftPad("size", c2) << endl;
		cout << string(ct, '=') << endl;

		int offset = 0;
		for (auto attribute : attributes.list) {
			cout << rightPad(attribute.name, c0)
				<< leftPad(formatNumber(offset), c1)
				<< leftPad(formatNumber(attribute.size), c2)
				<< endl;

			offset += attribute.size;
		}
		cout << string(ct, '=') << endl;

		//cout << "bytes per point: " << attributes.bytes << endl;
		cout << leftPad(formatNumber(attributes.bytes), ct) << endl;
		cout << string(ct, '=') << endl;

		//exit(1234);
	}

	return attributes;
}

struct Stats {
	Vector3 min = { Infinity , Infinity , Infinity };
	Vector3 max = { -Infinity , -Infinity , -Infinity };
	int64_t totalBytes = 0;
	int64_t totalPoints = 0;
};

Stats computeStats(vector<Source> sources){

	Vector3 min = { Infinity , Infinity , Infinity };
	Vector3 max = { -Infinity , -Infinity , -Infinity };

	int64_t totalBytes = 0;
	int64_t totalPoints = 0;

	for(auto source : sources){
		min.x = std::min(min.x, source.min.x);
		min.y = std::min(min.y, source.min.y);
		min.z = std::min(min.z, source.min.z);
								
		max.x = std::max(max.x, source.max.x);
		max.y = std::max(max.y, source.max.y);
		max.z = std::max(max.z, source.max.z);

		totalPoints += source.numPoints;
		totalBytes += source.filesize;
	}


	double cubeSize = (max - min).max();
	Vector3 size = { cubeSize, cubeSize, cubeSize };
	max = min + cubeSize;

	string strMin = "[" + to_string(min.x) + ", " + to_string(min.y) + ", " + to_string(min.z) + "]";
	string strMax = "[" + to_string(max.x) + ", " + to_string(max.y) + ", " + to_string(max.z) + "]";
	string strSize = "[" + to_string(size.x) + ", " + to_string(size.y) + ", " + to_string(size.z) + "]";

	string strTotalFileSize;
	{
		int64_t KB = 1024;
		int64_t MB = 1024 * KB;
		int64_t GB = 1024 * MB;
		int64_t TB = 1024 * GB;

		if (totalBytes >= TB) {
			strTotalFileSize = formatNumber(double(totalBytes) / double(TB), 1) + " TB";
		} else if (totalBytes >= GB) {
			strTotalFileSize = formatNumber(double(totalBytes) / double(GB), 1) + " GB";
		} else if (totalBytes >= MB) {
			strTotalFileSize = formatNumber(double(totalBytes) / double(MB), 1) + " MB";
		} else {
			strTotalFileSize = formatNumber(double(totalBytes), 1) + " bytes";
		}
	}
	

	cout << "cubicAABB: {\n";
	cout << "	\"min\": " << strMin << ",\n";
	cout << "	\"max\": " << strMax << ",\n";
	cout << "	\"size\": " << strSize << "\n";
	cout << "}\n";

	cout << "#points: " << formatNumber(totalPoints) << endl;
	cout << "total file size: " << strTotalFileSize << endl;

	return { min, max, totalBytes, totalPoints };
}

struct Monitor {
	thread t;
	bool stopRequested = false;

	void stop() {

		stopRequested = true;

		t.join();
	}
};

shared_ptr<Monitor> startMonitoring(State& state) {

	shared_ptr<Monitor> monitor = make_shared<Monitor>();

	monitor->t = thread([monitor, &state]() {

		using namespace std::chrono_literals;

		std::this_thread::sleep_for(1'000ms);

		while (!monitor->stopRequested) {

			auto ram = getMemoryData();
			auto CPU = getCpuData();
			double GB = 1024.0 * 1024.0 * 1024.0;

			double throughput = (double(state.pointsProcessed) / state.duration) / 1'000'000.0;

			double progressPass = 100.0 * state.progress();
			double progressTotal = (100.0 * double(state.currentPass - 1) + progressPass) / double(state.numPasses);

			string strProgressPass = formatNumber(progressPass) + "%";
			string strProgressTotal = formatNumber(progressTotal) + "%";
			string strTime = formatNumber(now()) + "s";
			string strDuration = formatNumber(state.duration) + "s";
			string strThroughput = formatNumber(throughput) + "MPs";

			string strRAM = formatNumber(double(ram.virtual_usedByProcess) / GB, 1)
				+ "GB (highest " + formatNumber(double(ram.virtual_usedByProcess_max) / GB, 1) + "GB)";
			string strCPU = formatNumber(CPU.usage) + "%";

			stringstream ss;
			ss << "[" << strProgressTotal << ", " << strTime << "], "
				<< "[" << state.name << ": " << strProgressPass << ", duration: " << strDuration << ", throughput: " << strThroughput << "]"
				<< "[RAM: " << strRAM << ", CPU: " << strCPU << "]";

			cout << ss.str() << endl;

			std::this_thread::sleep_for(1'000ms);
		}

	});

	return monitor;
}


void chunking(Options& options, vector<Source>& sources, string targetDir, Stats& stats, State& state, Attributes outputAttributes) {

	if (options.hasFlag("no-chunking")) {
		return;
	}

	if (options.chunkMethod == "LASZIP") {

		chunker_countsort_laszip::doChunking(sources, targetDir, stats.min, stats.max, state, outputAttributes);

	} else if (options.chunkMethod == "LAS_CUSTOM") {

		//chunker_countsort::doChunking(sources[0].path, targetDir, state);

	} else if (options.chunkMethod == "SKIP") {

		// skip chunking

	} else {

		cout << "ERROR: unkown chunk method: " << options.chunkMethod << endl;
		exit(123);

	}
}

void indexing(Options& options, string targetDir, State& state) {

	if (options.hasFlag("no-indexing")) {
		return;
	}

	if (options.method == "random") {

		SamplerRandom sampler;
		indexer::doIndexing(targetDir, state, options, sampler);

	} else if (options.method == "poisson") {

		SamplerPoisson sampler;
		indexer::doIndexing(targetDir, state, options, sampler);

	} else if (options.method == "poisson_average") {

		SamplerPoissonAverage sampler;
		indexer::doIndexing(targetDir, state, options, sampler);

	}
}

void createReport(Options& options, vector<Source> sources, string targetDir, Stats& stats, State& state, double tStart) {
	double duration = now() - tStart;
	double throughputMB = (stats.totalBytes / duration) / (1024 * 1024);
	double throughputP = (double(stats.totalPoints) / double(duration)) / 1'000'000.0;

	double kb = 1024.0;
	double mb = 1024.0 * 1024.0;
	double gb = 1024.0 * 1024.0 * 1024.0;
	double inputSize = 0;
	string inputSizeUnit = "";
	if (stats.totalBytes <= 10.0 * kb) {
		inputSize = stats.totalBytes / kb;
		inputSizeUnit = "KB";
	} else if (stats.totalBytes <= 10.0 * mb) {
		inputSize = stats.totalBytes / mb;
		inputSizeUnit = "MB";
	} else if (stats.totalBytes <= 10.0 * gb) {
		inputSize = stats.totalBytes / gb;
		inputSizeUnit = "GB";
	} else {
		inputSize = stats.totalBytes / gb;
		inputSizeUnit = "GB";
	}

	cout << endl;
	cout << "=======================================" << endl;
	cout << "=== STATS                              " << endl;
	cout << "=======================================" << endl;

	cout << "#points:               " << formatNumber(stats.totalPoints) << endl;
	cout << "#input files:          " << formatNumber(sources.size()) << endl;
	cout << "sampling method:       " << options.method << endl;
	cout << "chunk method:          " << options.chunkMethod << endl;
	cout << "input file size:       " << formatNumber(inputSize, 1) << inputSizeUnit << endl;
	cout << "duration:              " << formatNumber(duration, 3) << "s" << endl;
	cout << "throughput (MB/s)      " << formatNumber(throughputMB) << "MB" << endl;
	cout << "throughput (points/s)  " << formatNumber(throughputP, 1) << "M" << endl;
	cout << "output location:       " << targetDir << endl;

	

	for (auto [key, value] : state.values) {
		cout << key << ": \t" << value << endl;
	}

	

}

int main(int argc, char** argv) {

	double tStart = now(); 

	launchMemoryChecker(2 * 1024, 0.1);

	auto options = parseArguments(argc, argv);

	auto [name, sources] = curateSources(options.source);
	if (options.name.size() == 0) {
		options.name = name;
	}

	auto outputAttributes = computeOutputAttributes(sources, options);

	auto stats = computeStats(sources);

	if (options.hasFlag("just-stats")) {
		exit(0);
	}

	string targetDir = options.outdir;

	State state;
	state.pointsTotal = stats.totalPoints;
	state.bytesProcessed = stats.totalBytes;

	auto monitor = startMonitoring(state);


	{ // this is the real important stuff

		chunking(options, sources, targetDir, stats, state, outputAttributes);

		indexing(options, targetDir, state);

	}

	monitor->stop();

	createReport(options, sources, targetDir, stats, state, tStart);


	return 0;
}