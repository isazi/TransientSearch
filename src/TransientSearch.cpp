// Copyright 2015 Alessio Sclocco <a.sclocco@vu.nl>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <string>
#include <exception>
#include <fstream>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <random>

#include <configuration.hpp>

#include <ArgumentList.hpp>
#include <utils.hpp>
#include <Timer.hpp>
#include <Observation.hpp>
#include <Platform.hpp>
#include <ReadData.hpp>
#include <Generator.hpp>
#include <InitializeOpenCL.hpp>
#include <Kernel.hpp>


int main(int argc, char * argv[]) {
  bool print = false;
	bool dataLOFAR = false;
	bool dataSIGPROC = false;
  bool limit = false;
	unsigned int clPlatformID = 0;
	unsigned int clDeviceID = 0;
	unsigned int bytesToSkip = 0;
  unsigned int secondsToBuffer = 0;
  unsigned int nrThreads = 0;
  unsigned int remainingSamples = 0;
  float threshold = 0.0f;
	std::string deviceName;
	std::string dataFile;
	std::string headerFile;
	std::string outputFile;
  std::vector< std::ofstream > output;
  isa::utils::ArgumentList args(argc, argv);
	// Observation object
  AstroData::Observation obs;
  // Configurations
  AstroData::paddingConf padding;
  AstroData::vectorWidthConf vectorWidth;
  PulsarSearch::tunedDedispersionConf dedispersionParameters;
  PulsarSearch::tunedSNRDedispersedConf snrDParameters;

	try {
		clPlatformID = args.getSwitchArgument< unsigned int >("-opencl_platform");
		clDeviceID = args.getSwitchArgument< unsigned int >("-opencl_device");
		deviceName = args.getSwitchArgument< std::string >("-device_name");

    AstroData::readPaddingConf(padding, args.getSwitchArgument< std::string >("-padding_file"));
    AstroData::readVectorWidthConf(vectorWidth, args.getSwitchArgument< std::string >("-vector_file"));
    PulsarSearch::readTunedDedispersionConf(dedispersionParameters, args.getSwitchArgument< std::string >("-dedispersion_file"));
    PulsarSearch::readTunedSNRDedispersedConf(snrDParameters, args.getSwitchArgument< std::string >("-snr_file"));

    print = args.getSwitch("-print");
		obs.setPadding(padding[deviceName]);

		dataLOFAR = args.getSwitch("-lofar");
		dataSIGPROC = args.getSwitch("-sigproc");
		if ( dataLOFAR && dataSIGPROC ) {
			std::cerr << "-lofar and -sigproc are mutually exclusive." << std::endl;
			throw std::exception();
		} else if ( dataLOFAR ) {
      obs.setNrBeams(1);
			headerFile = args.getSwitchArgument< std::string >("-header");
			dataFile = args.getSwitchArgument< std::string >("-data");
      limit = args.getSwitch("-limit");
      if ( limit ) {
        obs.setNrSeconds(args.getSwitchArgument< unsigned int >("-seconds"));
      }
		} else if ( dataSIGPROC ) {
      obs.setNrBeams(1);
			bytesToSkip = args.getSwitchArgument< unsigned int >("-header");
			dataFile = args.getSwitchArgument< std::string >("-data");
			obs.setNrSeconds(args.getSwitchArgument< unsigned int >("-seconds"));
      obs.setFrequencyRange(args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
			obs.setNrSamplesPerSecond(args.getSwitchArgument< unsigned int >("-samples"));
		} else {
      obs.setNrBeams(args.getSwitchArgument< unsigned int >("-beams"));
      obs.setNrSeconds(args.getSwitchArgument< unsigned int >("-seconds"));
      obs.setFrequencyRange(args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
      obs.setNrSamplesPerSecond(args.getSwitchArgument< unsigned int >("-samples"));
		}
		outputFile = args.getSwitchArgument< std::string >("-output");
    unsigned int tempUInts[3] = {args.getSwitchArgument< unsigned int >("-dm_node"), 0, 0};
    float tempFloats[2] = {args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step")};
    obs.setDMRange(tempUInts[0], tempFloats[0], tempFloats[1]);
    threshold = args.getSwitchArgument< float >("-threshold");
	} catch ( isa::utils::EmptyCommandLine & err ) {
    std::cerr <<  args.getName() << " -opencl_platform ... -opencl_device ... -device_name ... -padding_file ... -vector_file ... -dedispersion_file ... -snr_file ... [-print] [-lofar] [-sigproc] -output ... -dm_node ... -dm_first ... -dm_step ... -threshold ..."<< std::endl;
    std::cerr << "\t -lofar -header ... -data ... [-limit]" << std::endl;
    std::cerr << "\t\t -limit -seconds ..." << std::endl;
    std::cerr << "\t -sigproc -header ... -data ... -seconds ... -channels ... -min_freq ... -channel_bandwidth ... -samples ..." << std::endl;
    std::cerr << "\t -beams ... -seconds ... -channels ... -min_freq ... -channel_bandwidth ... -samples ..." << std::endl;
    return 1;
  } catch ( std::exception & err ) {
		std::cerr << err.what() << std::endl;
		return 1;
	}

	// Load observation data
  isa::utils::Timer loadTime;
  std::vector< std::vector< std::vector< dataType > * > * > input(obs.getNrBeams());
	if ( dataLOFAR ) {
    input[0] = new std::vector< std::vector< dataType > * >(obs.getNrSeconds());
    loadTime.start();
    if ( limit ) {
      AstroData::readLOFAR(headerFile, dataFile, obs, *(input[0]), obs.getNrSeconds());
    } else {
      AstroData::readLOFAR(headerFile, dataFile, obs, *(input[0]));
    }
    loadTime.stop();
	} else if ( dataSIGPROC ) {
    input[0] = new std::vector< std::vector< dataType > * >(obs.getNrSeconds());
    loadTime.start();
		input[0]->resize(obs.getNrSeconds());
    AstroData::readSIGPROC(obs, bytesToSkip, dataFile, *(input[0]));
    loadTime.stop();
	} else {
    std::default_random_engine generator(std::chrono::system_clock::now().time_since_epoch().count());
    auto pointer = new std::vector< dataType >(obs.getNrChannels() * obs.getNrSamplesPerPaddedSecond());

    std::fill(pointer->begin(), pointer->end(), 42);
    for ( auto item = pointer->begin(); item != pointer->end(); ++item ) {
      if ( generator() % 2 == 0 ) {
        *item = generator();
      }
    }
    for ( unsigned int beam = 0; beam < obs.getNrBeams(); beam++ ) {
      input[beam] = new std::vector< std::vector< dataType > * >(obs.getNrSeconds());
      for ( unsigned int second = 0; second < obs.getNrSeconds(); second++ ) {
        input[beam]->at(second) = pointer;
      }
    }
  }
	if ( DEBUG ) {
    std::cout << "Device: " << deviceName << std::endl;
    std::cout << "Padding: " << padding[deviceName] << std::endl;
    std::cout << "Vector: " << vectorWidth[deviceName] << std::endl;
    std::cout << std::endl;
    std::cout << "Beams: " << obs.getNrBeams() << std::endl;
    std::cout << "Seconds: " << obs.getNrSeconds() << std::endl;
    std::cout << "Samples: " << obs.getNrSamplesPerSecond() << std::endl;
    std::cout << "Frequency range: " << obs.getMinFreq() << " MHz, " << obs.getMaxFreq() << " MHz" << std::endl;
    std::cout << "Channels: " << obs.getNrChannels() << " (" << obs.getChannelBandwidth() << " MHz)" << std::endl;
    std::cout << std::endl;
		std::cout << "Time to load the input: " << std::fixed << std::setprecision(6) << loadTime.getTotalTime() << " seconds." << std::endl;
    std::cout << std::endl;
	}

	// Initialize OpenCL
	cl::Context * clContext = new cl::Context();
	std::vector< cl::Platform > * clPlatforms = new std::vector< cl::Platform >();
	std::vector< cl::Device > * clDevices = new std::vector< cl::Device >();
	std::vector< std::vector< cl::CommandQueue > > * clQueues = new std::vector< std::vector < cl::CommandQueue > >();

	try {
    isa::OpenCL::initializeOpenCL(clPlatformID, obs.getNrBeams(), clPlatforms, clContext, clDevices, clQueues);
	} catch ( isa::OpenCL::OpenCLError & err ) {
		std::cerr << err.what() << std::endl;
		return 1;
	}

	// Host memory allocation
  std::vector< float > * shifts = PulsarSearch::getShifts(obs);
  obs.setNrSamplesPerDispersedChannel(obs.getNrSamplesPerSecond() + static_cast< unsigned int >(shifts->at(0) * (obs.getFirstDM() + ((obs.getNrDMs() - 1) * obs.getDMStep()))));
  secondsToBuffer = obs.getNrSamplesPerDispersedChannel() / obs.getNrSamplesPerSecond();
  remainingSamples = obs.getNrSamplesPerDispersedChannel() % obs.getNrSamplesPerSecond();
  std::vector< std::vector< dataType > > dispersedData(obs.getNrBeams());
  std::vector< std::vector< dataType > > dedispersedData(obs.getNrBeams());
  std::vector< std::vector< float > > snrData(obs.getNrBeams());

  for ( unsigned int beam = 0; beam < obs.getNrBeams(); beam++ ) {
    dispersedData[beam] = std::vector< dataType >(obs.getNrChannels() * obs.getNrSamplesPerDispersedChannel());
    dedispersedData[beam] = std::vector< dataType >(obs.getNrDMs() * obs.getNrSamplesPerPaddedSecond());
    snrData[beam] = std::vector< float >(obs.getNrPaddedDMs());
  }

  // Device memory allocation and data transfers
  cl::Buffer shifts_d;
  std::vector< cl::Buffer > dispersedData_d(obs.getNrBeams()), dedispersedData_d(obs.getNrBeams()), snrData_d(obs.getNrBeams());

  try {
    shifts_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, shifts->size() * sizeof(float), 0, 0);
    for ( unsigned int beam = 0; beam < obs.getNrBeams(); beam++ ) {
      dispersedData_d[beam] = cl::Buffer(*clContext, CL_MEM_READ_ONLY, dispersedData[beam].size() * sizeof(dataType), 0, 0);
      dedispersedData_d[beam] = cl::Buffer(*clContext, CL_MEM_READ_WRITE, dedispersedData[beam].size() * sizeof(dataType), 0, 0);
      snrData_d[beam] = cl::Buffer(*clContext, CL_MEM_WRITE_ONLY, snrData[beam].size() * sizeof(float), 0, 0);
    }
    clQueues->at(clDeviceID)[0].enqueueWriteBuffer(shifts_d, CL_TRUE, 0, shifts->size() * sizeof(float), reinterpret_cast< void * >(shifts->data()));
  } catch ( cl::Error & err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }

	if ( DEBUG ) {
		double hostMemory = 0.0;
		double deviceMemory = 0.0;

    hostMemory += obs.getNrBeams() * dispersedData[0].size() * sizeof(dataType);
    hostMemory += obs.getNrBeams() * snrData[0].size() * sizeof(float);
    deviceMemory += hostMemory;
    deviceMemory += shifts->size() * sizeof(float);
    deviceMemory += obs.getNrBeams() * obs.getNrDMs() * obs.getNrSamplesPerPaddedSecond() * sizeof(dataType);

		std::cout << "Allocated host memory: " << std::fixed << std::setprecision(3) << isa::utils::giga(hostMemory) << " GB." << std::endl;
		std::cout << "Allocated device memory: " << std::fixed << std::setprecision(3) << isa::utils::giga(deviceMemory) << "GB." << std::endl;
    std::cout << std::endl;
	}

	// Generate OpenCL kernels
  std::string * code;
  std::vector< cl::Kernel * > dedispersionK(obs.getNrBeams()), snrDedispersedK(obs.getNrBeams());

  code = PulsarSearch::getDedispersionOpenCL(dedispersionParameters[deviceName][obs.getNrDMs()], dataName, obs, *shifts);
	try {
    for ( unsigned int beam = 0; beam < obs.getNrBeams(); beam++ ) {
      dedispersionK[beam] = isa::OpenCL::compile("dedispersion", *code, "-cl-mad-enable -Werror", *clContext, clDevices->at(clDeviceID));
      dedispersionK[beam]->setArg(0, dispersedData_d[beam]);
      dedispersionK[beam]->setArg(1, dedispersedData_d[beam]);
      dedispersionK[beam]->setArg(2, shifts_d);
    }
	} catch ( isa::OpenCL::OpenCLError & err ) {
    std::cerr << err.what() << std::endl;
		return 1;
	}
  delete shifts;
  delete code;
  code = PulsarSearch::getSNRDedispersedOpenCL(snrDParameters[deviceName][obs.getNrDMs()], dataName, obs);
  try {
    for ( unsigned int beam = 0; beam < obs.getNrBeams(); beam++ ) {
      snrDedispersedK[beam] = isa::OpenCL::compile("snrDedispersed", *code, "-cl-mad-enable -Werror", *clContext, clDevices->at(clDeviceID));
      snrDedispersedK[beam]->setArg(0, dedispersedData_d[beam]);
      snrDedispersedK[beam]->setArg(1, snrData_d[beam]);
    }
  } catch ( isa::OpenCL::OpenCLError & err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }
  delete code;

  // Set execution parameters
  if ( obs.getNrSamplesPerSecond() % (dedispersionParameters[deviceName][obs.getNrDMs()].getNrSamplesPerBlock() * dedispersionParameters[deviceName][obs.getNrDMs()].getNrSamplesPerThread()) == 0 ) {
    nrThreads = obs.getNrSamplesPerSecond() / dedispersionParameters[deviceName][obs.getNrDMs()].getNrSamplesPerThread();
  } else if ( obs.getNrSamplesPerPaddedSecond() % (dedispersionParameters[deviceName][obs.getNrDMs()].getNrSamplesPerBlock() * dedispersionParameters[deviceName][obs.getNrDMs()].getNrSamplesPerThread()) == 0 ) {
    nrThreads = obs.getNrSamplesPerPaddedSecond() / dedispersionParameters[deviceName][obs.getNrDMs()].getNrSamplesPerThread();
  } else {
    nrThreads = isa::utils::pad(obs.getNrSamplesPerSecond() / dedispersionParameters[deviceName][obs.getNrDMs()].getNrSamplesPerThread(), dedispersionParameters[deviceName][obs.getNrDMs()].getNrSamplesPerBlock());
  }
  cl::NDRange dedispersionGlobal(nrThreads, obs.getNrDMs() / dedispersionParameters[deviceName][obs.getNrDMs()].getNrDMsPerThread());
  cl::NDRange dedispersionLocal(dedispersionParameters[deviceName][obs.getNrDMs()].getNrSamplesPerBlock(), dedispersionParameters[deviceName][obs.getNrDMs()].getNrDMsPerBlock());
  if ( DEBUG ) {
    std::cout << "Dedispersion" << std::endl;
    std::cout << "Global: " << nrThreads << ", " << obs.getNrDMs() / dedispersionParameters[deviceName][obs.getNrDMs()].getNrDMsPerThread() << std::endl;
    std::cout << "Local: " << dedispersionParameters[deviceName][obs.getNrDMs()].getNrSamplesPerBlock() << ", " << dedispersionParameters[deviceName][obs.getNrDMs()].getNrDMsPerBlock() << std::endl;
    std::cout << "Parameters: ";
    std::cout << dedispersionParameters[deviceName][obs.getNrDMs()].print() << std::endl;
    std::cout << std::endl;
  }
  nrThreads = snrDParameters[deviceName][obs.getNrDMs()].getNrSamplesPerBlock();
  cl::NDRange snrDedispersedGlobal(nrThreads, obs.getNrDMs());
  cl::NDRange snrDedispersedLocal(snrDParameters[deviceName][obs.getNrDMs()].getNrSamplesPerBlock(), 1);
  if ( DEBUG ) {
    std::cout << "SNRDedispersed" << std::endl;
    std::cout << "Global: " << nrThreads << ", " << obs.getNrDMs() << std::endl;
    std::cout << "Local: " << snrDParameters[deviceName][obs.getNrDMs()].getNrSamplesPerBlock() << ", 1" << std::endl;
    std::cout << "Parameters: ";
    std::cout << snrDParameters[deviceName][obs.getNrDMs()].print() << std::endl;
    std::cout << std::endl;
  }

	// Search loop
  isa::utils::Timer nodeTime;
  std::vector< cl::Event > syncPoint(obs.getNrBeams());
  std::vector< isa::utils::Timer > searchTime(obs.getNrBeams()), inputHandlingTime(obs.getNrBeams()), inputCopyTime(obs.getNrBeams()), dedispTime(obs.getNrBeams()), snrDedispersedTime(obs.getNrBeams()), outputCopyTime(obs.getNrBeams()), triggerTime(obs.getNrBeams());

  output = std::vector< std::ofstream >(obs.getNrBeams());
  for ( unsigned int beam = 0; beam < obs.getNrBeams(); beam++ ) {
    output[beam].open(outputFile + "_B" + isa::utils::toString(beam) + ".trigger");
    output[beam] << "# second DM SNR" << std::endl;
  }
  nodeTime.start();
  for ( unsigned int second = 0; second < obs.getNrSeconds() - secondsToBuffer; second++ ) {
    #pragma omp parallel for schedule(static, 1)
    for ( unsigned int beam = 0; beam < obs.getNrBeams(); beam++ ) {
      searchTime[beam].start();
      // Load the input
      inputHandlingTime[beam].start();
      for ( unsigned int channel = 0; channel < obs.getNrChannels(); channel++ ) {
        for ( unsigned int chunk = 0; chunk < secondsToBuffer; chunk++ ) {
          memcpy(reinterpret_cast< void * >(&(dispersedData[beam].data()[(channel * obs.getNrSamplesPerDispersedChannel()) + (chunk * obs.getNrSamplesPerSecond())])), reinterpret_cast< void * >(&((input[beam]->at(second + chunk))->at(channel * obs.getNrSamplesPerPaddedSecond()))), obs.getNrSamplesPerSecond() * sizeof(dataType));
        }
        memcpy(reinterpret_cast< void * >(&(dispersedData[beam].data()[(channel * obs.getNrSamplesPerDispersedChannel()) + (secondsToBuffer * obs.getNrSamplesPerSecond())])), reinterpret_cast< void * >(&((input[beam]->at(second + secondsToBuffer))->at(channel * obs.getNrSamplesPerPaddedSecond()))), remainingSamples * sizeof(dataType));
      }
      try {
        if ( SYNC ) {
          inputCopyTime[beam].start();
          clQueues->at(clDeviceID)[beam].enqueueWriteBuffer(dispersedData_d[beam], CL_TRUE, 0, dispersedData[beam].size() * sizeof(dataType), reinterpret_cast< void * >(dispersedData[beam].data()), 0, &syncPoint[beam]);
          syncPoint[beam].wait();
          inputCopyTime[beam].stop();
        } else {
          clQueues->at(clDeviceID)[beam].enqueueWriteBuffer(dispersedData_d[beam], CL_FALSE, 0, dispersedData[beam].size() * sizeof(dataType), reinterpret_cast< void * >(dispersedData[beam].data()));
        }
        if ( DEBUG ) {
          if ( print ) {
            std::cout << std::fixed << std::setprecision(3);
            for ( unsigned int channel = 0; channel < obs.getNrChannels(); channel++ ) {
              std::cout << channel << " : ";
              for ( unsigned int sample = 0; sample < obs.getNrSamplesPerDispersedChannel(); sample++ ) {
                std::cout << dispersedData[beam][(channel * obs.getNrSamplesPerDispersedChannel()) + sample] << " ";
              }
              std::cout << std::endl;
            }
            std::cout << std::endl;
          }
        }
      } catch ( cl::Error & err ) {
        std::cerr << "Beam: " << isa::utils::toString(beam) << ", Second: " << isa::utils::toString(second) << ", " << err.what() << " " << err.err() << std::endl;
      }
      inputHandlingTime[beam].stop();

      // Run the kernels
      try {
        if ( SYNC ) {
          dedispTime[beam].start();
          clQueues->at(clDeviceID)[beam].enqueueNDRangeKernel(*dedispersionK[beam], cl::NullRange, dedispersionGlobal, dedispersionLocal, 0, &syncPoint[beam]);
          syncPoint[beam].wait();
          dedispTime[beam].stop();
        } else {
          clQueues->at(clDeviceID)[beam].enqueueNDRangeKernel(*dedispersionK[beam], cl::NullRange, dedispersionGlobal, dedispersionLocal);
        }
        if ( DEBUG ) {
          if ( print ) {
            clQueues->at(clDeviceID)[beam].enqueueReadBuffer(dedispersedData_d[beam], CL_TRUE, 0, dedispersedData[beam].size() * sizeof(dataType), reinterpret_cast< void * >(dedispersedData[beam].data()));
            std::cout << std::fixed << std::setprecision(3);
            for ( unsigned int dm = 0; dm < obs.getNrDMs(); dm++ ) {
              std::cout << dm << " : ";
              for ( unsigned int sample = 0; sample < obs.getNrSamplesPerSecond(); sample++ ) {
                std::cout << dedispersedData[beam][(dm * obs.getNrSamplesPerPaddedSecond()) + sample] << " ";
              }
              std::cout << std::endl;
            }
            std::cout << std::endl;
          }
        }
        if ( SYNC ) {
          snrDedispersedTime[beam].start();
          clQueues->at(clDeviceID)[beam].enqueueNDRangeKernel(*snrDedispersedK[beam], cl::NullRange, snrDedispersedGlobal, snrDedispersedLocal, 0, &syncPoint[beam]);
          syncPoint[beam].wait();
          snrDedispersedTime[beam].stop();
          outputCopyTime[beam].start();
          clQueues->at(clDeviceID)[beam].enqueueReadBuffer(snrData_d[beam], CL_TRUE, 0, snrData[beam].size() * sizeof(float), reinterpret_cast< void * >(snrData[beam].data()), 0, &syncPoint[beam]);
          syncPoint[beam].wait();
          outputCopyTime[beam].stop();
        } else {
          clQueues->at(clDeviceID)[beam].enqueueNDRangeKernel(*snrDedispersedK[beam], cl::NullRange, snrDedispersedGlobal, snrDedispersedLocal);
          clQueues->at(clDeviceID)[beam].enqueueReadBuffer(snrData_d[beam], CL_FALSE, 0, snrData[beam].size() * sizeof(float), reinterpret_cast< void * >(snrData[beam].data()));
          clQueues->at(clDeviceID)[beam].finish();
        }
        triggerTime[beam].start();
        for ( unsigned int dm = 0; dm < obs.getNrDMs(); dm++ ) {
          if ( snrData[beam][dm] >= threshold ) {
            output[beam] << second << " " << obs.getFirstDM() + (dm * obs.getDMStep())  << " " << snrData[beam][dm] << std::endl;
          }
        }
        triggerTime[beam].stop();
        if ( DEBUG ) {
          if ( print ) {
            std::cout << std::fixed << std::setprecision(6);
            for ( unsigned int dm = 0; dm < obs.getNrDMs(); dm++ ) {
              std::cout << dm << ": " << snrData[beam][dm] << std::endl;
            }
            std::cout << std::endl;
          }
        }
      } catch ( cl::Error & err ) {
        std::cerr << "Beam: " << isa::utils::toString(beam) << ", Second: " << isa::utils::toString(second) << ", " << err.what() << " " << err.err() << std::endl;
      }
      searchTime[beam].stop();
    }
  }
  nodeTime.stop();

  for ( unsigned int beam = 0; beam < obs.getNrBeams(); beam++ ) {
    output[beam].close();
  }

  // Store statistics
  for ( unsigned int beam = 0; beam < obs.getNrBeams(); beam++ ) {
    output[beam].open(outputFile + "_B" + isa::utils::toString(beam) + ".stats");
    output[beam] << "# nrDMs nodeTime searchTime inputHandlingTotal inputHandlingAvg err inputCopyTotal inputCopyAvg err dedispersionTotal dedispersionAvg err snrDedispersedTotal snrDedispersedAvg err outputCopyTotal outputCopyAvg err triggerTimeTotal triggerTimeAvg err" << std::endl;
    output[beam] << std::fixed << std::setprecision(6);
    output[beam] << obs.getNrDMs() << " ";
    output[beam] << nodeTime.getTotalTime() << " ";
    output[beam] << searchTime[beam].getTotalTime() << " ";
    output[beam] << inputHandlingTime[beam].getTotalTime() << " " << inputHandlingTime[beam].getAverageTime() << " " << inputHandlingTime[beam].getStandardDeviation() << " ";
    output[beam] << inputCopyTime[beam].getTotalTime() << " " << inputCopyTime[beam].getAverageTime() << " " << inputCopyTime[beam].getStandardDeviation() << " ";
    output[beam] << dedispTime[beam].getTotalTime() << " " << dedispTime[beam].getAverageTime() << " " << dedispTime[beam].getStandardDeviation() << " ";
    output[beam] << snrDedispersedTime[beam].getTotalTime() << " " << snrDedispersedTime[beam].getAverageTime() << " " << snrDedispersedTime[beam].getStandardDeviation() << " ";
    output[beam] << outputCopyTime[beam].getTotalTime() << " " << outputCopyTime[beam].getAverageTime() << " " << outputCopyTime[beam].getStandardDeviation() << " ";
    output[beam] << triggerTime[beam].getTotalTime() << " " << triggerTime[beam].getAverageTime() << " " << triggerTime[beam].getStandardDeviation() << " ";
    output[beam] << std::endl;
    output[beam].close();
  }

	return 0;
}

