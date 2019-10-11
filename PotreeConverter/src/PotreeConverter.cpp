

#include <experimental/filesystem>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include "PotreeConverter.h"
#include "stuff.h"
#include "LASPointReader.h"
#include "PTXPointReader.h"
#include "PotreeException.h"
#include "PotreeWriter.h"
#include "LASPointWriter.hpp"
#include "BINPointWriter.hpp"
#include "BINPointReader.hpp"
#include "PlyPointReader.h"
#include "XYZPointReader.hpp"
#include "ExtraBytes.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <math.h>
#include <fstream>




using rapidjson::Document;
using rapidjson::StringBuffer;
using rapidjson::Writer;
using rapidjson::PrettyWriter;
using rapidjson::Value;

using std::stringstream;
using std::map;
using std::string;
using std::vector;
using std::find;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::duration_cast;
using std::fstream;

namespace fs = std::experimental::filesystem;

namespace Potree{

PointReader *PotreeConverter::createPointReader(string path, PointAttributes pointAttributes){
	PointReader *reader = NULL;
	if(iEndsWith(path, ".las") || iEndsWith(path, ".laz")){
		reader = new LASPointReader(path);
	}else if(iEndsWith(path, ".ptx")){
		reader = new PTXPointReader(path);
	}else if(iEndsWith(path, ".ply")){
		reader = new PlyPointReader(path);
	}else if(iEndsWith(path, ".xyz") || iEndsWith(path, ".txt")){
		reader = new XYZPointReader(path, format, colorRange, intensityRange);
	}else if(iEndsWith(path, ".pts")){
		vector<double> intensityRange;

		if(this->intensityRange.size() == 0){
				intensityRange.push_back(-2048);
				intensityRange.push_back(+2047);
		}

		reader = new XYZPointReader(path, format, colorRange, intensityRange);
 	}else if(iEndsWith(path, ".bin")){
		reader = new BINPointReader(path, aabb, scale, pointAttributes);
	}

	return reader;
}

PotreeConverter::PotreeConverter(string executablePath, string workDir, vector<string> sources){
    this->executablePath = executablePath;
	this->workDir = workDir;
	this->sources = sources;
}

vector<PointAttribute> checkAvailableStandardAttributes(string file) {

	vector<PointAttribute> attributes;

	bool isLas = iEndsWith(file, ".las") || iEndsWith(file, ".laz");
	if (!isLas) {
		return attributes;
	}

	laszip_POINTER laszip_reader;
	laszip_header* header;

	laszip_create(&laszip_reader);

	laszip_BOOL request_reader = 1;
	laszip_request_compatibility_mode(laszip_reader, request_reader);

	bool hasClassification = false;
	bool hasGpsTime = false;
	bool hasIntensity = false;
	bool hasNumberOfReturns = false;
	bool hasReturnNumber = false;
	bool hasPointSourceId = false;

	{
		laszip_BOOL is_compressed = iEndsWith(file, ".laz") ? 1 : 0;
		laszip_open_reader(laszip_reader, file.c_str(), &is_compressed);

		laszip_get_header_pointer(laszip_reader, &header);

		long long npoints = (header->number_of_point_records ? header->number_of_point_records : header->extended_number_of_point_records);

		laszip_point* point;
		laszip_get_point_pointer(laszip_reader, &point);

		for (int i = 0; i < 1'000'000 && i < npoints; i++) {
			laszip_read_point(laszip_reader);

			hasClassification |= point->classification != 0;
			hasGpsTime |= point->gps_time != 0;
			hasIntensity |= point->intensity != 0;
			hasNumberOfReturns |= point->number_of_returns != 0;
			hasReturnNumber |= point->return_number != 0;
			hasPointSourceId |= point->point_source_ID != 0;
		}
	}

	laszip_close_reader(laszip_reader);
	laszip_destroy(laszip_reader);

	if (hasClassification) {
		attributes.push_back(PointAttribute::CLASSIFICATION);
	}

	if (hasGpsTime) {
		attributes.push_back(PointAttribute::GPS_TIME);
	}

	if (hasIntensity) {
		attributes.push_back(PointAttribute::INTENSITY);
	}

	if (hasNumberOfReturns) {
		attributes.push_back(PointAttribute::NUMBER_OF_RETURNS);
	}

	if (hasReturnNumber) {
		attributes.push_back(PointAttribute::RETURN_NUMBER);
	}

	if (hasPointSourceId) {
		attributes.push_back(PointAttribute::SOURCE_ID);
	}

	return attributes;
}

vector<PointAttribute> parseExtraAttributes(string file) {

	vector<PointAttribute> attributes;

	bool isLas = iEndsWith(file, ".las") || iEndsWith(file, ".laz");
	if(!isLas) {
		return attributes;
	}

	laszip_POINTER laszip_reader;
	laszip_header* header;

	laszip_create(&laszip_reader);

	laszip_BOOL request_reader = 1;
	laszip_request_compatibility_mode(laszip_reader, request_reader);

	laszip_BOOL is_compressed = iEndsWith(file, ".laz") ? 1 : 0;
	laszip_open_reader(laszip_reader, file.c_str(), &is_compressed);

	laszip_get_header_pointer(laszip_reader, &header);

	{ // read extra bytes

		for (int i = 0; i < header->number_of_variable_length_records; i++) {
			laszip_vlr_struct vlr = header->vlrs[i];

			cout << "record id: " << vlr.record_id << endl;
			cout << "record_length_after_header: " << vlr.record_length_after_header << endl;

			int numExtraBytes = vlr.record_length_after_header / sizeof(ExtraBytesRecord);

			ExtraBytesRecord* extraBytes = reinterpret_cast<ExtraBytesRecord*>(vlr.data);

			for (int j = 0; j < numExtraBytes; j++) {
				ExtraBytesRecord extraAttribute = extraBytes[j];

				string name = string(extraAttribute.name);

				cout << "name: " << name << endl;

				//ExtraType type = extraTypeFromID(extraAttribute.data_type);
				ExtraType type = typeToExtraType.at(extraAttribute.data_type);
				int byteSize = type.size;
				PointAttribute attribute(123, name, type.type, type.numElements, byteSize);

				attributes.push_back(attribute);
			}
		}
	}

	laszip_close_reader(laszip_reader);
	laszip_destroy(laszip_reader);

	return attributes;
}

void PotreeConverter::prepare(){

	// if sources contains directories, use files inside the directory instead
	vector<string> sourceFiles;
	for (const auto &source : sources) {
		fs::path pSource(source);
		if(fs::is_directory(pSource)){
			fs::directory_iterator it(pSource);
			for(;it != fs::directory_iterator(); it++){
				fs::path pDirectoryEntry = it->path();
				if(fs::is_regular_file(pDirectoryEntry)){
					string filepath = pDirectoryEntry.string();
					if(iEndsWith(filepath, ".las") 
						|| iEndsWith(filepath, ".laz") 
						|| iEndsWith(filepath, ".xyz")
						|| iEndsWith(filepath, ".pts")
						|| iEndsWith(filepath, ".ptx")
						|| iEndsWith(filepath, ".ply")){
						sourceFiles.push_back(filepath);
					}
				}
			}
		}else if(fs::is_regular_file(pSource)){
			sourceFiles.push_back(source);
		}
	}
	this->sources = sourceFiles;

	pointAttributes = PointAttributes();
	pointAttributes.add(PointAttribute::POSITION_CARTESIAN);

	bool addExtraAttributes = false;

	if(outputAttributes.size() > 0){
		for(const auto &attribute : outputAttributes){
			if(attribute == "RGB"){
				pointAttributes.add(PointAttribute::COLOR_PACKED);
			}else if(attribute == "INTENSITY"){
				pointAttributes.add(PointAttribute::INTENSITY);
			} else if (attribute == "CLASSIFICATION") {
				pointAttributes.add(PointAttribute::CLASSIFICATION);
			} else if (attribute == "RETURN_NUMBER") {
				pointAttributes.add(PointAttribute::RETURN_NUMBER);
			} else if (attribute == "NUMBER_OF_RETURNS") {
				pointAttributes.add(PointAttribute::NUMBER_OF_RETURNS);
			} else if (attribute == "SOURCE_ID") {
				pointAttributes.add(PointAttribute::SOURCE_ID);
			} else if (attribute == "GPS_TIME") {
				pointAttributes.add(PointAttribute::GPS_TIME);
			} else if(attribute == "NORMAL"){
				pointAttributes.add(PointAttribute::NORMAL_OCT16);
			} else if (attribute == "EXTRA") {
				addExtraAttributes = true;
			}
		} 
	} else {
		string file = sourceFiles[0];

		// always add colors?
		pointAttributes.add(PointAttribute::COLOR_PACKED);

		vector<PointAttribute> attributes = checkAvailableStandardAttributes(file);

		for (PointAttribute attribute : attributes) {
			pointAttributes.add(attribute);

			//cout << attribute.name << ", " << attribute.byteSize << endl;
		}

		addExtraAttributes = true;
	}

	if(addExtraAttributes){
		string file = sourceFiles[0];

		vector<PointAttribute> extraAttributes = parseExtraAttributes(file);
		for (PointAttribute attribute : extraAttributes) {
			pointAttributes.add(attribute);
		
			//cout << attribute.name << ", " << attribute.byteSize << endl;
		}
	}

	cout << "processing following attributes: " << endl;
	for (PointAttribute& attribute : pointAttributes.attributes) {
		cout << attribute.name << endl;
	}
	cout << endl;
}

AABB PotreeConverter::calculateAABB(){
	AABB aabb;
	if(aabbValues.size() == 6){
		Vector3<double> userMin(aabbValues[0],aabbValues[1],aabbValues[2]);
		Vector3<double> userMax(aabbValues[3],aabbValues[4],aabbValues[5]);
		aabb = AABB(userMin, userMax);
	}else{
		for(string source : sources){

			PointReader *reader = createPointReader(source, pointAttributes);
			
			AABB lAABB = reader->getAABB();
			aabb.update(lAABB.min);
			aabb.update(lAABB.max);

			reader->close();
			delete reader;
		}
	}

	return aabb;
}

void PotreeConverter::generatePage(string name){

	string pagedir = this->workDir;
    string templateSourcePath = this->executablePath + "/resources/page_template/viewer_template.html";
    string mapTemplateSourcePath = this->executablePath + "/resources/page_template/lasmap_template.html";
    string templateDir = this->executablePath + "/resources/page_template";

        if(!this->pageTemplatePath.empty()) {
		templateSourcePath = this->pageTemplatePath + "/viewer_template.html";
		mapTemplateSourcePath = this->pageTemplatePath + "/lasmap_template.html";
		templateDir = this->pageTemplatePath;
	}

	string templateTargetPath = pagedir + "/" + name + ".html";
	string mapTemplateTargetPath = pagedir + "/lasmap_" + name + ".html";

    Potree::copyDir(fs::path(templateDir), fs::path(pagedir));
	fs::remove(pagedir + "/viewer_template.html");
	fs::remove(pagedir + "/lasmap_template.html");

	if(!this->sourceListingOnly){ // change viewer template
		ifstream in( templateSourcePath );
		ofstream out( templateTargetPath );

		string line;
		while(getline(in, line)){
			if(line.find("<!-- INCLUDE POINTCLOUD -->") != string::npos){
				out << "\t\tPotree.loadPointCloud(\"pointclouds/" << name << "/cloud.js\", \"" << name << "\", e => {" << endl;
				out << "\t\t\tlet pointcloud = e.pointcloud;\n";
				out << "\t\t\tlet material = pointcloud.material;\n";

				out << "\t\t\tviewer.scene.addPointCloud(pointcloud);" << endl;

				out << "\t\t\t" << "material.pointColorType = Potree.PointColorType." << material << "; // any Potree.PointColorType.XXXX \n";
				out << "\t\t\tmaterial.size = 1;\n";
				out << "\t\t\tmaterial.pointSizeType = Potree.PointSizeType.ADAPTIVE;\n";
				out << "\t\t\tmaterial.shape = Potree.PointShape.SQUARE;\n";

				out << "\t\t\tviewer.fitToScreen();" << endl;
				out << "\t\t});" << endl;
			}else if(line.find("<!-- INCLUDE SETTINGS HERE -->") != string::npos){
				out << std::boolalpha;
				out << "\t\t" << "document.title = \"" << title << "\";\n";
				out << "\t\t" << "viewer.setEDLEnabled(" << edlEnabled << ");\n";
				if(showSkybox){
					out << "\t\t" << "viewer.setBackground(\"skybox\"); // [\"skybox\", \"gradient\", \"black\", \"white\"];\n";
				}else{
					out << "\t\t" << "viewer.setBackground(\"gradient\"); // [\"skybox\", \"gradient\", \"black\", \"white\"];\n";
				}
				
				string descriptionEscaped = string(description);
				std::replace(descriptionEscaped.begin(), descriptionEscaped.end(), '`', '\'');

				out << "\t\t" << "viewer.setDescription(`" << descriptionEscaped << "`);\n";
			}else{
				out << line << endl;
			}
			
		}

		in.close();
		out.close();
	}

	// change lasmap template
	if(!this->projection.empty()){ 
		ifstream in( mapTemplateSourcePath );
		ofstream out( mapTemplateTargetPath );

		string line;
		while(getline(in, line)){
			if(line.find("<!-- INCLUDE SOURCE -->") != string::npos){
				out << "\tvar source = \"" << "pointclouds/" << name << "/sources.json" << "\";";
			}else{
				out << line << endl;
			}
			
		}

		in.close();
		out.close();
	}

	//{ // write settings
	//	stringstream ssSettings;
	//
	//	ssSettings << "var sceneProperties = {" << endl;
	//	ssSettings << "\tpath: \"" << "../resources/pointclouds/" << name << "/cloud.js\"," << endl;
	//	ssSettings << "\tcameraPosition: null, 		// other options: cameraPosition: [10,10,10]," << endl;
	//	ssSettings << "\tcameraTarget: null, 		// other options: cameraTarget: [0,0,0]," << endl;
	//	ssSettings << "\tfov: 60, 					// field of view in degrees," << endl;
	//	ssSettings << "\tsizeType: \"Adaptive\",	// other options: \"Fixed\", \"Attenuated\"" << endl;
	//	ssSettings << "\tquality: null, 			// other options: \"Circles\", \"Interpolation\", \"Splats\"" << endl;
	//	ssSettings << "\tmaterial: \"RGB\", 		// other options: \"Height\", \"Intensity\", \"Classification\"" << endl;
	//	ssSettings << "\tpointLimit: 1,				// max number of points in millions" << endl;
	//	ssSettings << "\tpointSize: 1,				// " << endl;
	//	ssSettings << "\tnavigation: \"Orbit\",		// other options: \"Orbit\", \"Flight\"" << endl;
	//	ssSettings << "\tuseEDL: false,				" << endl;
	//	ssSettings << "};" << endl;
	//
	//
	//	ofstream fSettings;
	//	fSettings.open(pagedir + "/examples/" + name + ".js", ios::out);
	//	fSettings << ssSettings.str();
	//	fSettings.close();
	//}
}

void writeSources(string path, vector<string> sourceFilenames, vector<int> numPoints, vector<AABB> boundingBoxes, string projection){
	Document d(rapidjson::kObjectType);

	AABB bb;


	Value jProjection(projection.c_str(), (rapidjson::SizeType)projection.size());

	Value jSources(rapidjson::kObjectType);
	jSources.SetArray();
	for(int i = 0; i < sourceFilenames.size(); i++){
		string &source = sourceFilenames[i];
		int points = numPoints[i];
		AABB boundingBox = boundingBoxes[i];

		bb.update(boundingBox);

		Value jSource(rapidjson::kObjectType);

		Value jName(source.c_str(), (rapidjson::SizeType)source.size());
		Value jPoints(points);
		Value jBounds(rapidjson::kObjectType);

		{
			Value bbMin(rapidjson::kObjectType);	
			Value bbMax(rapidjson::kObjectType);	

			bbMin.SetArray();
			bbMin.PushBack(boundingBox.min.x, d.GetAllocator());
			bbMin.PushBack(boundingBox.min.y, d.GetAllocator());
			bbMin.PushBack(boundingBox.min.z, d.GetAllocator());

			bbMax.SetArray();
			bbMax.PushBack(boundingBox.max.x, d.GetAllocator());
			bbMax.PushBack(boundingBox.max.y, d.GetAllocator());
			bbMax.PushBack(boundingBox.max.z, d.GetAllocator());

			jBounds.AddMember("min", bbMin, d.GetAllocator());
			jBounds.AddMember("max", bbMax, d.GetAllocator());
		}

		jSource.AddMember("name", jName, d.GetAllocator());
		jSource.AddMember("points", jPoints, d.GetAllocator());
		jSource.AddMember("bounds", jBounds, d.GetAllocator());

		jSources.PushBack(jSource, d.GetAllocator());
	}

	Value jBoundingBox(rapidjson::kObjectType);
	{
		Value bbMin(rapidjson::kObjectType);	
		Value bbMax(rapidjson::kObjectType);	

		bbMin.SetArray();
		bbMin.PushBack(bb.min.x, d.GetAllocator());
		bbMin.PushBack(bb.min.y, d.GetAllocator());
		bbMin.PushBack(bb.min.z, d.GetAllocator());

		bbMax.SetArray();
		bbMax.PushBack(bb.max.x, d.GetAllocator());
		bbMax.PushBack(bb.max.y, d.GetAllocator());
		bbMax.PushBack(bb.max.z, d.GetAllocator());

		jBoundingBox.AddMember("min", bbMin, d.GetAllocator());
		jBoundingBox.AddMember("max", bbMax, d.GetAllocator());
	}

	d.AddMember("bounds", jBoundingBox, d.GetAllocator());
	d.AddMember("projection", jProjection, d.GetAllocator());
	d.AddMember("sources", jSources, d.GetAllocator());

	StringBuffer buffer;
	//PrettyWriter<StringBuffer> writer(buffer);
	Writer<StringBuffer> writer(buffer);
	d.Accept(writer);
	
	if(!fs::exists(fs::path(path))){
		fs::path pcdir(path);
		fs::create_directories(pcdir);
	}

	ofstream sourcesOut(path + "/sources.json", ios::out);
	sourcesOut << buffer.GetString();
	sourcesOut.close();
}

void PotreeConverter::convert(){
	auto start = high_resolution_clock::now();

	prepare();

	long long pointsProcessed = 0;

	AABB aabb = calculateAABB();
	cout << "AABB: " << endl << aabb << endl;
	aabb.makeCubic();
	cout << "cubic AABB: " << endl << aabb << endl;

	if (diagonalFraction != 0) {
		spacing = (float)(aabb.size.length() / diagonalFraction);
		cout << "spacing calculated from diagonal: " << spacing << endl;
	}

	if(pageName.size() > 0){
		generatePage(pageName);
		workDir = workDir + "/pointclouds/" + pageName;
	}

	PotreeWriter *writer = NULL;
	if(fs::exists(fs::path(this->workDir + "/cloud.js"))){

		if(storeOption == StoreOption::ABORT_IF_EXISTS){
			cout << "ABORTING CONVERSION: target already exists: " << this->workDir << "/cloud.js" << endl;
			cout << "If you want to overwrite the existing conversion, specify --overwrite" << endl;
			cout << "If you want add new points to the existing conversion, make sure the new points ";
			cout << "are contained within the bounding box of the existing conversion and then specify --incremental" << endl;

			return;
		}else if(storeOption == StoreOption::OVERWRITE){
			fs::remove_all(workDir + "/data");
			fs::remove_all(workDir + "/temp");
			fs::remove(workDir + "/cloud.js");
			writer = new PotreeWriter(this->workDir, aabb, spacing, maxDepth, scale, outputFormat, pointAttributes, quality);
			writer->setProjection(this->projection);
		}else if(storeOption == StoreOption::INCREMENTAL){
			writer = new PotreeWriter(this->workDir, quality);
			writer->loadStateFromDisk();
		}
	}else{
		writer = new PotreeWriter(this->workDir, aabb, spacing, maxDepth, scale, outputFormat, pointAttributes, quality);
		writer->setProjection(this->projection);
	}

	if(writer == NULL){
		return;
	}

	writer->storeSize = storeSize;

	vector<AABB> boundingBoxes;
	vector<int> numPoints;
	vector<string> sourceFilenames;

	for (const auto &source : sources) {
		cout << "READING:  " << source << endl;

		PointReader *reader = createPointReader(source, pointAttributes);

		boundingBoxes.push_back(reader->getAABB());
		numPoints.push_back(reader->numPoints());
		sourceFilenames.push_back(fs::path(source).filename().string());

		writeSources(this->workDir, sourceFilenames, numPoints, boundingBoxes, this->projection);
		if(this->sourceListingOnly){
			reader->close();
			delete reader;

			continue;
		}

		while(reader->readNextPoint()){
			pointsProcessed++;

			Point p = reader->getPoint();
			writer->add(p);

			if((pointsProcessed % (1'000'000)) == 0){
				writer->processStore();
				writer->waitUntilProcessed();

				auto end = high_resolution_clock::now();
				long long duration = duration_cast<milliseconds>(end-start).count();
				float seconds = duration / 1'000.0f;

				stringstream ssMessage;

				ssMessage.imbue(std::locale(""));
				ssMessage << "INDEXING: ";
				ssMessage << pointsProcessed << " points processed; ";
				ssMessage << writer->numAccepted << " points written; ";
				ssMessage << seconds << " seconds passed";

				cout << ssMessage.str() << endl;
			}
			if((pointsProcessed % (flushLimit)) == 0){
				cout << "FLUSHING: ";
			
				auto start = high_resolution_clock::now();
			
				writer->flush();
			
				auto end = high_resolution_clock::now();
				long long duration = duration_cast<milliseconds>(end-start).count();
				float seconds = duration / 1'000.0f;
			
				cout << seconds << "s" << endl;
			}

			//if(pointsProcessed >= 10'000'000){
			//	break;
			//}
		}
		reader->close();
		delete reader;

		
	}
	
	cout << "closing writer" << endl;
	writer->flush();
	writer->close();

	writeSources(this->workDir + "/sources.json", sourceFilenames, numPoints, boundingBoxes, this->projection);

	float percent = (float)writer->numAccepted / (float)pointsProcessed;
	percent = percent * 100;

	auto end = high_resolution_clock::now();
	long long duration = duration_cast<milliseconds>(end-start).count();

	
	cout << endl;
	cout << "conversion finished" << endl;
	cout << pointsProcessed << " points were processed and " << writer->numAccepted << " points ( " << percent << "% ) were written to the output. " << endl;

	cout << "duration: " << (duration / 1000.0f) << "s" << endl;
}

}
