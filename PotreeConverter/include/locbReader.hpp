#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <array>
#include <vector>
#include <algorithm>

template <typename T, typename std::enable_if<std::is_trivially_copyable<T>::value>::type * = nullptr>
void readBin(std::istream &in, T &v)
{
    in.read(reinterpret_cast<char *>(&v), sizeof(T));
}

namespace oni
{
    struct LocalizationResultsHeader
        {
            int numOfFrames;
            int numOfChannels;
            int numOfPoints;
            float x_min = 0, y_min = 0, z_min = 0;
            float x_max = 0, y_max = 0, z_max = 0;
            float intensity_min = 0, background_min = 0, x_sigma_min = 0, y_sigma_min = 0;
        };

    struct LocalizationResult
        {   float rawPosition_x;
            float rawPosition_y;
            float rawPosition_z;
            float sigma_x;
            float sigma_y;
            float intensity;
            float background;
            bool is_valid;
            float CRLB_xPosition;
            float CRLB_yPosition;
            float CRLB_intensity;
            float CRLB_background;
            float CRLB_sigma_x;
            float CRLB_sigma_y;
            float logLikelihood;
            float logLikelihoodRatio_PValue;
            uint32_t spotDetectionPixelPos_x;
            uint32_t spotDetectionPixelPos_y;
            int frameIndex;
            int channelIndex;
        };

    struct LocbAcquisitionFrameData
        {
            LocbAcquisitionFrameData() = default;
            uint32_t versionNumber;
            bool hasCameraFrameIndex;
            uint32_t frameIndex;

            // stage position measured relative to the stage's arbitrary starting position
            double stagePositionInMicrons_x;
            double stagePositionInMicrons_y;
            double stagePositionInMicrons_z;

            double illuminationAngleInDegrees;
            double temperatureInCelsius;
            bool outOfRangeAccelerationDetected = false;
            double zFocusOffset = 0.0;
        };

        using FileFrameIndicesToAcquisitionData = std::map<uint32_t, LocbAcquisitionFrameData>;
}  //oni

inline bool loadDataFromLocbFile(
    std::string                       const &fileName, 
    oni::LocalizationResultsHeader &localizationResultsHeader,
    std::vector<oni::LocalizationResult> &localizationResultMap,
    oni::FileFrameIndicesToAcquisitionData  &acquisitionFrameDataMap)
{
    std::array<char, 8> const binaryFileHeader =  {'n', 'i', 'm', '_', 'l', 'o', 'c', 'b'};

    // Version 1 - original
    // Version 2 - add spotDetectionPixelPos to LocalizationResult - using LocalizationResultBinaryVersion2
    // Version 3 - added acquisition per frame data
    // Version 4 - added offset per frame data (drift correction)
    // Version 5 - previous versions had p-value set to 1-p-value
    uint32_t const binaryFileVersionNumber = 5;

    std::cout << "Reading from file `" << fileName << "` ..." << std::endl;

    std::ifstream in(fileName, std::ios::binary);

    if (in.is_open())
    {
        std::cout << "Successfully opened binary file" << std::endl;
    }
    else
    {
        std::cerr << "Failed to open binary file" << std::endl;

        return false;
    }

    auto header = decltype(binaryFileHeader){};
    readBin(in, header);

    if (header == binaryFileHeader)
    {
        std::cout << "Header match" << std::endl;
    }
    else
    {
        std::cerr << "Invalid header" << std::endl;
        return false;
    }

    auto versionNumber = decltype(binaryFileVersionNumber){};
    readBin(in, versionNumber);

    if (versionNumber > binaryFileVersionNumber)
    {
        std::cerr << "Locb file is too new --- unable to read" << std::endl;
        return false;
    }
    else
    {
        std::cout << "Locb file version is " << versionNumber << std::endl;
    }

    bool driftApplied;
    readBin(in, driftApplied);

    uint64_t numberOfOffsets;

    readBin(in, numberOfOffsets);
    for (uint64_t i = 0; i < numberOfOffsets; i++)
    {
        uint32_t frame;
        float xOffset;
        float yOffset;
        std::map<uint32_t, std::vector<float>> offsets;
        readBin(in, frame);
        readBin(in, xOffset);
        readBin(in, yOffset);
        offsets[frame] = {xOffset, yOffset};
    }

    uint64_t numLocalizationResults;

    readBin(in, numLocalizationResults);

    {
        std::cout << "Total number of points: " << numLocalizationResults << std::endl;
    }
    std::vector<std::uint8_t> numChannels;
    for (uint64_t i = 0; i < numLocalizationResults; i++)
    {   uint32_t frameIndex;
        uint8_t channelIndex;
        oni::LocalizationResult currLocalizationResult;

        readBin(in, frameIndex);
        readBin(in, channelIndex);
        readBin(in, currLocalizationResult.rawPosition_x);
        readBin(in, currLocalizationResult.rawPosition_y);
        readBin(in, currLocalizationResult.rawPosition_z);
        readBin(in, currLocalizationResult.sigma_x);
        readBin(in, currLocalizationResult.sigma_y);
        readBin(in, currLocalizationResult.intensity);
        readBin(in, currLocalizationResult.background);
        readBin(in, currLocalizationResult.is_valid);
        readBin(in, currLocalizationResult.CRLB_xPosition);
        readBin(in, currLocalizationResult.CRLB_yPosition);
        readBin(in, currLocalizationResult.CRLB_intensity);
        readBin(in, currLocalizationResult.CRLB_background);
        readBin(in, currLocalizationResult.CRLB_sigma_x);
        readBin(in, currLocalizationResult.CRLB_sigma_y);
        readBin(in, currLocalizationResult.logLikelihood);
        readBin(in, currLocalizationResult.logLikelihoodRatio_PValue);
        readBin(in, currLocalizationResult.spotDetectionPixelPos_x);
        readBin(in, currLocalizationResult.spotDetectionPixelPos_y);

        currLocalizationResult.frameIndex = frameIndex;
        currLocalizationResult.channelIndex = channelIndex;

        localizationResultMap.push_back(currLocalizationResult);
        if (std::find(numChannels.begin(), numChannels.end(), channelIndex) == numChannels.end()) {
            numChannels.push_back(channelIndex);
        }

        // check for minimum values
        localizationResultsHeader.x_min = std::min(localizationResultsHeader.x_min, currLocalizationResult.rawPosition_x);
        localizationResultsHeader.x_max = std::max(localizationResultsHeader.x_max, currLocalizationResult.rawPosition_x);
        localizationResultsHeader.y_min = std::min(localizationResultsHeader.y_min, currLocalizationResult.rawPosition_y);
        localizationResultsHeader.y_max = std::min(localizationResultsHeader.y_max, currLocalizationResult.rawPosition_y);
        localizationResultsHeader.z_min = std::min(localizationResultsHeader.z_min, currLocalizationResult.rawPosition_z);
        localizationResultsHeader.z_max = std::min(localizationResultsHeader.z_max, currLocalizationResult.rawPosition_z);
        localizationResultsHeader.x_sigma_min = std::min(localizationResultsHeader.x_sigma_min, currLocalizationResult.sigma_x);
        localizationResultsHeader.y_sigma_min = std::min(localizationResultsHeader.y_sigma_min, currLocalizationResult.sigma_y);
        localizationResultsHeader.intensity_min = std::min(localizationResultsHeader.intensity_min, currLocalizationResult.intensity);
        localizationResultsHeader.background_min = std::min(localizationResultsHeader.background_min, currLocalizationResult.background);
    }

    uint64_t numAcquisitionData;
    readBin(in, numAcquisitionData);
    {
        std::cout << "Number of acquisition frames: " << numAcquisitionData << std::endl;
    }

    for (uint64_t i=0; i < numAcquisitionData; i++)
    {   uint32_t frameIndexFst;
        readBin(in, frameIndexFst);
        oni::LocbAcquisitionFrameData currAcquisitionFrameData;
        readBin(in, currAcquisitionFrameData.versionNumber);
        readBin(in, currAcquisitionFrameData.hasCameraFrameIndex);
        readBin(in, currAcquisitionFrameData.frameIndex);
        readBin(in, currAcquisitionFrameData.stagePositionInMicrons_x);
        readBin(in, currAcquisitionFrameData.stagePositionInMicrons_y);
        readBin(in, currAcquisitionFrameData.stagePositionInMicrons_z);
        readBin(in, currAcquisitionFrameData.illuminationAngleInDegrees);
        readBin(in, currAcquisitionFrameData.temperatureInCelsius);
        readBin(in, currAcquisitionFrameData.outOfRangeAccelerationDetected);
        if (currAcquisitionFrameData.versionNumber == 1)
        {
            readBin(in, currAcquisitionFrameData.zFocusOffset);
        }
        acquisitionFrameDataMap[frameIndexFst] = currAcquisitionFrameData;
    }

    // populate header with number of frames,channels and points
    localizationResultsHeader.numOfPoints = numLocalizationResults;
    localizationResultsHeader.numOfFrames = numAcquisitionData;
    localizationResultsHeader.numOfChannels = numChannels.size();
    {
    return true;
    }
}
