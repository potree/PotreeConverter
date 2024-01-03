

#include <iostream>
#include <execution>

#include "unsuck/unsuck.hpp"
#include "chunker_countsort_laszip.h"
#include "indexer.h"
#include "sampler_poisson.h"
#include "sampler_poisson_average.h"
#include "sampler_random.h"
#include "Attributes.h"
#include "PotreeConverter.h"
#include "logger.h"
#include "Monitor.h"

#include "arguments/Arguments.hpp"

using namespace std;

Options parseArguments(int argc, char** argv) {
	Arguments args(argc, argv);

	args.addArgument("source,i,", "Input file(s)");
	args.addArgument("help,h", "Display help information");
	args.addArgument("outdir,o", "Output directory");
	args.addArgument("encoding", "Encoding type \"BROTLI\", \"UNCOMPRESSED\" (default)");
	args.addArgument("method,m", "Point sampling method \"poisson\", \"poisson_average\", \"random\"");
	args.addArgument("chunkMethod", "Chunking method");
	args.addArgument("keep-chunks", "Skip deleting temporary chunks during conversion");
	args.addArgument("no-chunking", "Disable chunking phase");
	args.addArgument("no-indexing", "Disable indexing phase");
	args.addArgument("deterministic", "Same output files for same input");
	args.addArgument("attributes", "Attributes in output file");
	args.addArgument("projection", "Add the projection of the pointcloud to the metadata");
	args.addArgument("generate-page,p", "Generate a ready to use web page with the given name");
	args.addArgument("title", "Page title used when generating a web page");

	if (args.has("help")) {
		cout << "PotreeConverter <source> -o <outdir>" << endl;
		cout << endl << args.usage() << endl;
		exit(0);
	}

	if (!args.has("source")) {
		cout << "PotreeConverter <source> -o <outdir>" << endl;
		cout << endl << "For a list of options, use --help or -h" << endl;

		exit(1);
	}

	vector<string> source = args.get("source").as<vector<string>>();

	if (source.size() == 0) {
		cout << "PotreeConverter <source> -o <outdir>" << endl;
		cout << endl << "For a list of options, use --help or -h" << endl;

		exit(1);
	}

	string encoding = args.get("encoding").as<string>("DEFAULT");
	string method = args.get("method").as<string>("poisson");
	string chunkMethod = args.get("chunkMethod").as<string>("LASZIP");

	string outdir = "";
	if (args.has("outdir")) {
		outdir = args.get("outdir").as<string>();
	} else {

		string sourcepath = source[0];
		fs::path path(sourcepath);

		//cout << fs::canonical(source[0]) << endl;
		//exit(123);

		if (!fs::exists(path)) {

			logger::ERROR("file does not exist: " + source[0]);

			exit(123);
		} 

		path = fs::canonical(path);

		string suggestedBaseName = path.filename().string() + "_converted";
		outdir = sourcepath + "/../" + suggestedBaseName;

		int i = 1;
		while(fs::exists(outdir)) {
			outdir = sourcepath + "/../" + suggestedBaseName + "_" + std::to_string(i);

			if (i > 100) {

				logger::ERROR("unsuccessfully tried to find empty output directory. stopped at 100 iterations: " + outdir);

				exit(123);
			}

			i++;
		}

	}

	outdir = fs::weakly_canonical(fs::path(outdir)).string();

	//vector<string> flags = args.get("flags").as<vector<string>>();

	vector<string> attributes = args.get("attributes").as<vector<string>>();

	bool generatePage = args.has("generate-page");
	string pageName = "";
	if (generatePage) {
		pageName = args.get("generate-page").as<string>();
	}
	string pageTitle = args.get("title").as<string>();
	string projection = args.get("projection").as<string>();

	bool keepChunks = args.has("keep-chunks");
	bool noChunking = args.has("no-chunking");
	bool noIndexing = args.has("no-indexing");
	deterministicOperation = args.has("deterministic");

	Options options;
	options.source = source;
	options.outdir = outdir;
	options.method = method;
	options.encoding = encoding;
	options.chunkMethod = chunkMethod;
	//options.flags = flags;
	options.attributes = attributes;
	options.generatePage = generatePage;
	options.pageName = pageName;
	options.pageTitle = pageTitle;
	options.projection = projection;

	options.keepChunks = keepChunks;
	options.noChunking = noChunking;
	options.noIndexing = noIndexing;

	//cout << "flags: ";
	//for (string flag : options.flags) {
	//	cout << flag << ", ";
	//}
	//cout << endl;

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

	{ // sanity check
		bool sizeError = (size.x == 0.0) || (size.y == 0.0) || (size.z == 0);
		if (sizeError) {
			logger::ERROR("invalid bounding box. at least one axis has a size of zero.");

			exit(123);
		}
		
	}

	return { min, max, totalBytes, totalPoints };
}

// struct Monitor {
// 	thread t;
// 	bool stopRequested = false;

// 	void stop() {

// 		stopRequested = true;

// 		t.join();
// 	}
// };

// shared_ptr<Monitor> startMonitoring(State& state) {

// 	shared_ptr<Monitor> monitor = make_shared<Monitor>();

// 	monitor->t = thread([monitor, &state]() {

// 		using namespace std::chrono_literals;

// 		std::this_thread::sleep_for(1'000ms);

// 		while (!monitor->stopRequested) {

// 			auto ram = getMemoryData();
// 			auto CPU = getCpuData();
// 			double GB = 1024.0 * 1024.0 * 1024.0;

// 			double throughput = (double(state.pointsProcessed) / state.duration) / 1'000'000.0;

// 			double progressPass = 100.0 * state.progress();
// 			double progressTotal = (100.0 * double(state.currentPass - 1) + progressPass) / double(state.numPasses);

// 			string strProgressPass = formatNumber(progressPass) + "%";
// 			string strProgressTotal = formatNumber(progressTotal) + "%";
// 			string strTime = formatNumber(now()) + "s";
// 			string strDuration = formatNumber(state.duration) + "s";
// 			string strThroughput = formatNumber(throughput) + "MPs";

// 			string strRAM = formatNumber(double(ram.virtual_usedByProcess) / GB, 1)
// 				+ "GB (highest " + formatNumber(double(ram.virtual_usedByProcess_max) / GB, 1) + "GB)";
// 			string strCPU = formatNumber(CPU.usage) + "%";

// 			stringstream ss;
// 			ss << "[" << strProgressTotal << ", " << strTime << "], "
// 				<< "[" << state.name << ": " << strProgressPass << ", duration: " << strDuration << ", throughput: " << strThroughput << "]"
// 				<< "[RAM: " << strRAM << ", CPU: " << strCPU << "]";

// 			cout << ss.str() << endl;

// 			std::this_thread::sleep_for(1'000ms);
// 		}

// 	});

// 	return monitor;
// }


void chunking(Options& options, vector<Source>& sources, string targetDir, Stats& stats, State& state, Attributes outputAttributes, Monitor* monitor) {

	if (options.noChunking) {
		return;
	}

	if (options.chunkMethod == "LASZIP") {

		chunker_countsort_laszip::doChunking(sources, targetDir, stats.min, stats.max, state, outputAttributes, monitor);

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

	if (options.noIndexing) {
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

void generatePage(string exePath, string pagedir, string pagename) {
	string templateDir = exePath + "/resources/page_template";
	string templateSourcePath = templateDir + "/viewer_template.html";

	string pageTargetPath = pagedir + "/" + pagename + ".html";

	try{
		fs::copy(templateDir, pagedir, fs::copy_options::overwrite_existing | fs::copy_options::recursive);
	} catch (std::exception & e) {
		string msg = e.what();
		logger::ERROR(msg);
	}

	fs::remove(pagedir + "/viewer_template.html");

	{ // configure page template
		string strTemplate = readFile(templateSourcePath);

		string strPointcloudTemplate = 
		R"V0G0N(

		Potree.loadPointCloud("<!-- URL -->", "<!-- NAME -->", e => {
			let scene = viewer.scene;
			let pointcloud = e.pointcloud;
			
			let material = pointcloud.material;
			material.size = 1;
			material.pointSizeType = Potree.PointSizeType.ADAPTIVE;
			material.shape = Potree.PointShape.SQUARE;
			material.activeAttributeName = "rgba";
			
			scene.addPointCloud(pointcloud);
			
			viewer.fitToScreen();
		});

		)V0G0N";

		string url = "./pointclouds/" + pagename + "/metadata.json";

		string strPointcloud = stringReplace(strPointcloudTemplate, "<!-- URL -->", url);
		strPointcloud = stringReplace(strPointcloud, "<!-- NAME -->", pagename);

		string strPage = stringReplace(strTemplate, "<!-- INCLUDE POINTCLOUD -->", strPointcloud);


		writeFile(pageTargetPath, strPage);

	}

}

#include "HierarchyBuilder.h"

int main(int argc, char** argv) {

	
	// { // DEBUG STUFF

	// 	string hierarchyDir = "D:/dev/pointclouds/Riegl/retz_converted/.hierarchyChunks";
	// 	int hierarchyStepSize = 4;

	// 	HierarchyBuilder builder(hierarchyDir, hierarchyStepSize);
	// 	builder.build();

	// 	return 0;
	// }



	double tStart = now(); 

	auto exePath = fs::canonical(fs::absolute(argv[0])).parent_path().string();

	launchMemoryChecker(2 * 1024, 0.1);
	auto cpuData = getCpuData();

	cout << "#threads: " << cpuData.numProcessors << endl;

	auto options = parseArguments(argc, argv);
	if (deterministicOperation) {
		cout << "Deterministic Operation locks everything to single threading\n";
	}

	auto [name, sources] = curateSources(options.source);
	if (options.name.size() == 0) {
		options.name = name;
	}

	auto outputAttributes = computeOutputAttributes(sources, options.attributes);
	cout << toString(outputAttributes);

	auto stats = computeStats(sources);
	
	string targetDir = options.outdir;
	if (options.generatePage) {

		string pagedir = targetDir;
		generatePage(exePath, pagedir, options.pageName);

		targetDir = targetDir + "/pointclouds/" + options.pageName;
	}
	cout << "target directory: '" << targetDir << "'" << endl;
	fs::create_directories(targetDir);
	logger::addOutputFile(targetDir + "/log.txt");

	State state;
	state.pointsTotal = stats.totalPoints;
	state.bytesProcessed = stats.totalBytes;

	// auto monitor = startMonitoring(state);
	auto monitor = make_shared<Monitor>(&state);
	monitor->start();


	{ // this is the real important stuff

		chunking(options, sources, targetDir, stats, state, outputAttributes, monitor.get());

		indexing(options, targetDir, state);

	}

	monitor->stop();

	createReport(options, sources, targetDir, stats, state, tStart);


	return 0;
}
