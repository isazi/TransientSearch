// Copyright 2017 Netherlands Institute for Radio Astronomy (ASTRON)
// Copyright 2017 Netherlands eScience Center
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

#include <Kernels.hpp>

/**
 * @brief Generate the dedispersion OpenCL kernels.
 */
void generateDedispersionOpenCLKernels(const OpenCLRunTime &openclRunTime, const AstroData::Observation &observation,
                                       const Options &options, const DeviceOptions &deviceOptions,
                                       const KernelConfigurations &kernelConfigurations, const HostMemory &hostMemory,
                                       const DeviceMemory &deviceMemory, Kernels &kernels)
{
    std::string *code = nullptr;

    if (!options.subbandDedispersion)
    {
        code = Dedispersion::getDedispersionOpenCL<inputDataType, outputDataType>(*(kernelConfigurations.dedispersionSingleStepParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())), deviceOptions.padding.at(deviceOptions.deviceName), inputBits, inputDataName, intermediateDataName, outputDataName, observation, *hostMemory.shiftsSingleStep);
        kernels.dedispersionSingleStep = isa::OpenCL::compile("dedispersion", *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID));
        delete code;
        if (kernelConfigurations.dedispersionSingleStepParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->getSplitBatches())
        {
            // TODO: add support for splitBatches
        }
        else
        {
            kernels.dedispersionSingleStep->setArg(0, deviceMemory.dispersedData);
            kernels.dedispersionSingleStep->setArg(1, deviceMemory.dedispersedData);
            kernels.dedispersionSingleStep->setArg(2, deviceMemory.beamMapping);
            kernels.dedispersionSingleStep->setArg(3, deviceMemory.zappedChannels);
            kernels.dedispersionSingleStep->setArg(4, deviceMemory.shiftsSingleStep);
        }
    }
    else
    {
        code = Dedispersion::getSubbandDedispersionStepOneOpenCL<inputDataType, outputDataType>(*(kernelConfigurations.dedispersionStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true))), deviceOptions.padding.at(deviceOptions.deviceName), inputBits, inputDataName, intermediateDataName, outputDataName, observation, *hostMemory.shiftsStepOne);
        kernels.dedispersionStepOne = isa::OpenCL::compile("dedispersionStepOne", *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID));
        delete code;
        code = Dedispersion::getSubbandDedispersionStepTwoOpenCL<outputDataType>(*(kernelConfigurations.dedispersionStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())), deviceOptions.padding.at(deviceOptions.deviceName), outputDataName, observation, *hostMemory.shiftsStepTwo);
        kernels.dedispersionStepTwo = isa::OpenCL::compile("dedispersionStepTwo", *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID));
        delete code;
        if (kernelConfigurations.dedispersionStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true))->getSplitBatches())
        {
            // TODO: add support for splitBatches
        }
        else
        {
            kernels.dedispersionStepOne->setArg(0, deviceMemory.dispersedData);
            kernels.dedispersionStepOne->setArg(1, deviceMemory.subbandedData);
            kernels.dedispersionStepOne->setArg(2, deviceMemory.zappedChannels);
            kernels.dedispersionStepOne->setArg(3, deviceMemory.shiftsStepOne);
        }
        kernels.dedispersionStepTwo->setArg(0, deviceMemory.subbandedData);
        kernels.dedispersionStepTwo->setArg(1, deviceMemory.dedispersedData);
        kernels.dedispersionStepTwo->setArg(2, deviceMemory.beamMapping);
        kernels.dedispersionStepTwo->setArg(3, deviceMemory.shiftsStepTwo);
    }
}

/**
 * @brief Generate the integration OpenCL kernels.
 */
void generateIntegrationOpenCLKernels(const OpenCLRunTime &openclRunTime, const AstroData::Observation &observation,
                                      const Options &options, const DeviceOptions &deviceOptions,
                                      const KernelConfigurations &kernelConfigurations, const HostMemory &hostMemory,
                                      const DeviceMemory &deviceMemory, Kernels &kernels)
{
    std::string *code = nullptr;

    kernels.integration.reserve(hostMemory.integrationSteps.size());
    for (unsigned int stepNumber = 0; stepNumber < hostMemory.integrationSteps.size(); stepNumber++)
    {
        auto step = hostMemory.integrationSteps.begin();

        std::advance(step, stepNumber);
        if (*step > 1)
        {
            if (options.subbandDedispersion)
            {
                code = Integration::getIntegrationDMsSamplesOpenCL<outputDataType>(*(kernelConfigurations.integrationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(*step)), observation, outputDataName, *step, deviceOptions.padding.at(deviceOptions.deviceName));
            }
            else
            {
                code = Integration::getIntegrationDMsSamplesOpenCL<outputDataType>(*(kernelConfigurations.integrationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(*step)), observation, outputDataName, *step, deviceOptions.padding.at(deviceOptions.deviceName));
            }
            kernels.integration.push_back(isa::OpenCL::compile("integrationDMsSamples" + std::to_string(*step), *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID)));
            kernels.integration.at(stepNumber)->setArg(0, deviceMemory.dedispersedData);
            kernels.integration.at(stepNumber)->setArg(1, deviceMemory.integratedData);
        }
        delete code;
    }
}

/**
 * @brief Generate the SNR OpenCL kernels.
 */
void generateSNROpenCLKernels(const OpenCLRunTime &openclRunTime, const AstroData::Observation &observation,
                              const Options &options, const DeviceOptions &deviceOptions,
                              const KernelConfigurations &kernelConfigurations, const HostMemory &hostMemory,
                              const DeviceMemory &deviceMemory, Kernels &kernels)
{
    std::string *code = nullptr;

    if (options.snrMode == SNRMode::Standard)
    {
        kernels.snr.reserve(hostMemory.integrationSteps.size() + 1);
        for (unsigned int stepNumber = 0; stepNumber < hostMemory.integrationSteps.size(); stepNumber++)
        {
            auto step = hostMemory.integrationSteps.begin();

            std::advance(step, stepNumber);
            if (options.subbandDedispersion)
            {
                code = SNR::getSNRDMsSamplesOpenCL<outputDataType>(*(kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)), outputDataName, observation, observation.getNrSamplesPerBatch() / *step, deviceOptions.padding.at(deviceOptions.deviceName));
            }
            else
            {
                code = SNR::getSNRDMsSamplesOpenCL<outputDataType>(*(kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)), outputDataName, observation, observation.getNrSamplesPerBatch() / *step, deviceOptions.padding.at(deviceOptions.deviceName));
            }
            kernels.snr.push_back(isa::OpenCL::compile("snrDMsSamples" + std::to_string(observation.getNrSamplesPerBatch() / *step), *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID)));
            kernels.snr.at(stepNumber)->setArg(0, deviceMemory.integratedData);
            kernels.snr.at(stepNumber)->setArg(1, deviceMemory.snrData);
            kernels.snr.at(stepNumber)->setArg(2, deviceMemory.snrSamples);
            delete code;
        }
        if (!options.subbandDedispersion)
        {
            code = SNR::getSNRDMsSamplesOpenCL<outputDataType>(*(kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())), outputDataName, observation, observation.getNrSamplesPerBatch(), deviceOptions.padding.at(deviceOptions.deviceName));
        }
        else
        {
            code = SNR::getSNRDMsSamplesOpenCL<outputDataType>(*(kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())), outputDataName, observation, observation.getNrSamplesPerBatch(), deviceOptions.padding.at(deviceOptions.deviceName));
        }
        kernels.snr.push_back(isa::OpenCL::compile("snrDMsSamples" + std::to_string(observation.getNrSamplesPerBatch()), *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID)));
        kernels.snr.at(hostMemory.integrationSteps.size())->setArg(0, deviceMemory.dedispersedData);
        kernels.snr.at(hostMemory.integrationSteps.size())->setArg(1, deviceMemory.snrData);
        kernels.snr.at(hostMemory.integrationSteps.size())->setArg(2, deviceMemory.snrSamples);
        delete code;
    }
    else if (options.snrMode == SNRMode::Momad)
    {
        // Max kernel
        kernels.max.reserve(hostMemory.integrationSteps.size() + 1);
        for (unsigned int stepNumber = 0; stepNumber < hostMemory.integrationSteps.size(); stepNumber++)
        {
            auto step = hostMemory.integrationSteps.begin();

            std::advance(step, stepNumber);
            if (options.subbandDedispersion)
            {
                code = SNR::getMaxOpenCL<outputDataType>(*(kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)), SNR::DataOrdering::DMsSamples, outputDataName, observation, *step, deviceOptions.padding.at(deviceOptions.deviceName));
            }
            else
            {
                code = SNR::getMaxOpenCL<outputDataType>(*(kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)), SNR::DataOrdering::DMsSamples, outputDataName, observation, *step, deviceOptions.padding.at(deviceOptions.deviceName));
            }
            kernels.max.push_back(isa::OpenCL::compile("getmax_DMsSamples_" + std::to_string(observation.getNrSamplesPerBatch() / *step), *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID)));
            kernels.max.at(stepNumber)->setArg(0, deviceMemory.integratedData);
            kernels.max.at(stepNumber)->setArg(1, deviceMemory.maxValues);
            kernels.max.at(stepNumber)->setArg(2, deviceMemory.maxIndices);
            delete code;
        }
        if (!options.subbandDedispersion)
        {
            code = SNR::getMaxOpenCL<outputDataType>(*(kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())), SNR::DataOrdering::DMsSamples, outputDataName, observation, 1, deviceOptions.padding.at(deviceOptions.deviceName));
        }
        else
        {
            code = SNR::getMaxOpenCL<outputDataType>(*(kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())), SNR::DataOrdering::DMsSamples, outputDataName, observation, 1, deviceOptions.padding.at(deviceOptions.deviceName));
        }
        kernels.max.push_back(isa::OpenCL::compile("getMax_DMsSamples_" + std::to_string(observation.getNrSamplesPerBatch()), *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID)));
        kernels.max.at(hostMemory.integrationSteps.size())->setArg(0, deviceMemory.dedispersedData);
        kernels.max.at(hostMemory.integrationSteps.size())->setArg(1, deviceMemory.maxValues);
        kernels.max.at(hostMemory.integrationSteps.size())->setArg(2, deviceMemory.maxIndices);
        delete code;
        // Median of medians first step kernel
        kernels.medianOfMediansStepOne.reserve(hostMemory.integrationSteps.size() + 1);
        for (unsigned int stepNumber = 0; stepNumber < hostMemory.integrationSteps.size(); stepNumber++)
        {
            auto step = hostMemory.integrationSteps.begin();

            std::advance(step, stepNumber);
            if (options.subbandDedispersion)
            {
                code = SNR::getMedianOfMediansOpenCL<outputDataType>(*(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)), SNR::DataOrdering::DMsSamples, outputDataName, observation, *step, options.medianStepSize, deviceOptions.padding.at(deviceOptions.deviceName));
            }
            else
            {
                code = SNR::getMedianOfMediansOpenCL<outputDataType>(*(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)), SNR::DataOrdering::DMsSamples, outputDataName, observation, *step, options.medianStepSize, deviceOptions.padding.at(deviceOptions.deviceName));
            }
            kernels.medianOfMediansStepOne.push_back(isa::OpenCL::compile("medianOfMedians_DMsSamples_" + std::to_string(options.medianStepSize), *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID)));
            kernels.medianOfMediansStepOne.at(stepNumber)->setArg(0, deviceMemory.integratedData);
            kernels.medianOfMediansStepOne.at(stepNumber)->setArg(1, deviceMemory.medianOfMediansStepOne);
            delete code;
        }
        if (!options.subbandDedispersion)
        {
            code = SNR::getMedianOfMediansOpenCL<outputDataType>(*(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())), SNR::DataOrdering::DMsSamples, outputDataName, observation, 1, options.medianStepSize, deviceOptions.padding.at(deviceOptions.deviceName));
        }
        else
        {
            code = SNR::getMedianOfMediansOpenCL<outputDataType>(*(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())), SNR::DataOrdering::DMsSamples, outputDataName, observation, 1, deviceOptions.padding.at(deviceOptions.deviceName));
        }
        kernels.medianOfMediansStepOne.push_back(isa::OpenCL::compile("medianOfMedians_DMsSamples_" + std::to_string(options.medianStepSize), *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID)));
        kernels.medianOfMediansStepOne.at(hostMemory.integrationSteps.size())->setArg(0, deviceMemory.dedispersedData);
        kernels.medianOfMediansStepOne.at(hostMemory.integrationSteps.size())->setArg(1, deviceMemory.medianOfMediansStepOne);
        delete code;
        // Median of medians second step kernel
        kernels.medianOfMediansStepTwo.reserve(hostMemory.integrationSteps.size() + 1);
        for (unsigned int stepNumber = 0; stepNumber < hostMemory.integrationSteps.size(); stepNumber++)
        {
            auto step = hostMemory.integrationSteps.begin();

            std::advance(step, stepNumber);
            if (options.subbandDedispersion)
            {
                code = SNR::getMedianOfMediansOpenCL<outputDataType>(*(kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step / options.medianStepSize)), SNR::DataOrdering::DMsSamples, outputDataName, observation, *step, observation.getNrSamplesPerBatch() / *step / options.medianStepSize, deviceOptions.padding.at(deviceOptions.deviceName));
            }
            else
            {
                code = SNR::getMedianOfMediansOpenCL<outputDataType>(*(kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step / options.medianStepSize)), SNR::DataOrdering::DMsSamples, outputDataName, observation, *step, observation.getNrSamplesPerBatch() / *step / options.medianStepSize, deviceOptions.padding.at(deviceOptions.deviceName));
            }
            kernels.medianOfMediansStepTwo.push_back(isa::OpenCL::compile("medianOfMedians_DMsSamples_" + std::to_string(observation.getNrSamplesPerBatch() / *step / options.medianStepSize), *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID)));
            kernels.medianOfMediansStepTwo.at(stepNumber)->setArg(0, deviceMemory.medianOfMediansStepOne);
            kernels.medianOfMediansStepTwo.at(stepNumber)->setArg(1, deviceMemory.medianOfMediansStepTwo);
            delete code;
        }
        if (!options.subbandDedispersion)
        {
            code = SNR::getMedianOfMediansOpenCL<outputDataType>(*(kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / options.medianStepSize)), SNR::DataOrdering::DMsSamples, outputDataName, observation, 1, observation.getNrSamplesPerBatch() / options.medianStepSize, deviceOptions.padding.at(deviceOptions.deviceName));
        }
        else
        {
            code = SNR::getMedianOfMediansOpenCL<outputDataType>(*(kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / options.medianStepSize)), SNR::DataOrdering::DMsSamples, outputDataName, observation, 1, observation.getNrSamplesPerBatch() / options.medianStepSize, deviceOptions.padding.at(deviceOptions.deviceName));
        }
        kernels.medianOfMediansStepTwo.push_back(isa::OpenCL::compile("medianOfMedians_DMsSamples_" + std::to_string(observation.getNrSamplesPerBatch() / options.medianStepSize), *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID)));
        kernels.medianOfMediansStepTwo.at(hostMemory.integrationSteps.size())->setArg(0, deviceMemory.medianOfMediansStepOne);
        kernels.medianOfMediansStepTwo.at(hostMemory.integrationSteps.size())->setArg(1, deviceMemory.medianOfMediansStepTwo);
        delete code;
        // Median of medians absolute deviation kernel
        kernels.medianOfMediansAbsoluteDeviation.reserve(hostMemory.integrationSteps.size() + 1);
        for (unsigned int stepNumber = 0; stepNumber < hostMemory.integrationSteps.size(); stepNumber++)
        {
            auto step = hostMemory.integrationSteps.begin();

            std::advance(step, stepNumber);
            if (options.subbandDedispersion)
            {
                code = SNR::getMedianOfMediansAbsoluteDeviationOpenCL<outputDataType>(*(kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)), SNR::DataOrdering::DMsSamples, outputDataName, observation, *step, options.medianStepSize, deviceOptions.padding.at(deviceOptions.deviceName));
            }
            else
            {
                code = SNR::getMedianOfMediansAbsoluteDeviatioOpenCL<outputDataType>(*(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)), SNR::DataOrdering::DMsSamples, outputDataName, observation, *step, options.medianStepSize, deviceOptions.padding.at(deviceOptions.deviceName));
            }
            kernels.medianOfMediansAbsoluteDeviation.push_back(isa::OpenCL::compile("medianOfMediansAbsoluteDeviation_DMsSamples_" + std::to_string(options.medianStepSize), *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID)));
            kernels.medianOfMediansAbsoluteDeviation.at(stepNumber)->setArg(0, deviceMemory.medianOfMediansStepTwo);
            kernels.medianOfMediansAbsoluteDeviation.at(stepNumber)->setArg(1, deviceMemory.integratedData);
            kernels.medianOfMediansAbsoluteDeviation.at(stepNumber)->setArg(2, deviceMemory.medianOfMediansStepOne);
            delete code;
        }
        if (!options.subbandDedispersion)
        {
            code = SNR::getMedianOfMediansAbsoluteDeviationOpenCL<outputDataType>(*(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())), SNR::DataOrdering::DMsSamples, outputDataName, observation, 1, options.medianStepSize, deviceOptions.padding.at(deviceOptions.deviceName));
        }
        else
        {
            code = SNR::getMedianOfMediansAbsoluteDeviationOpenCL<outputDataType>(*(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())), SNR::DataOrdering::DMsSamples, outputDataName, observation, 1, deviceOptions.padding.at(deviceOptions.deviceName));
        }
        kernels.medianOfMediansAbsoluteDeviation.push_back(isa::OpenCL::compile("medianOfMediansAbsoluteDeviation_DMsSamples_" + std::to_string(options.medianStepSize), *code, "-cl-mad-enable -Werror", *openclRunTime.context, openclRunTime.devices->at(deviceOptions.deviceID)));
        kernels.medianOfMediansAbsoluteDeviation.at(hostMemory.integrationSteps.size())->setArg(0, deviceMemory.medianOfMediansStepTwo);
        kernels.medianOfMediansAbsoluteDeviation.at(hostMemory.integrationSteps.size())->setArg(1, deviceMemory.dedispersedData);
        kernels.medianOfMediansAbsoluteDeviation.at(hostMemory.integrationSteps.size())->setArg(2, deviceMemory.medianOfMediansStepOne);
        delete code;
    }
}

/**
 * @brief Generate all OpenCL kernels.
 */
void generateOpenCLKernels(const OpenCLRunTime &openclRunTime, const AstroData::Observation &observation,
                           const Options &options, const DeviceOptions &deviceOptions,
                           const KernelConfigurations &kernelConfigurations, const HostMemory &hostMemory,
                           const DeviceMemory &deviceMemory, Kernels &kernels)
{
    generateDedispersionOpenCLKernels(openclRunTime, observation, options, deviceOptions, kernelConfigurations,
                                      hostMemory, deviceMemory, kernels);
    generateIntegrationOpenCLKernels(openclRunTime, observation, options, deviceOptions, kernelConfigurations, hostMemory,
                                     deviceMemory, kernels);
    generateSNROpenCLKernels(openclRunTime, observation, options, deviceOptions, kernelConfigurations, hostMemory,
                             deviceMemory, kernels);
}

/**
 * @brief Generate the run-time configuration for the dedispersion OpenCL kernels.
 */
void generateDedispersionOpenCLRunTimeConfigurations(const AstroData::Observation &observation,
                                                     const Options &options, const DeviceOptions &deviceOptions,
                                                     const KernelConfigurations &kernelConfigurations,
                                                     const HostMemory &hostMemory,
                                                     KernelRunTimeConfigurations &kernelRunTimeConfigurations)
{
    if (!options.subbandDedispersion)
    {
        kernelRunTimeConfigurations.dedispersionSingleStepGlobal = cl::NDRange(isa::utils::pad(observation.getNrSamplesPerBatch() / kernelConfigurations.dedispersionSingleStepParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->getNrItemsD0(), kernelConfigurations.dedispersionSingleStepParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->getNrThreadsD0()), observation.getNrDMs() / kernelConfigurations.dedispersionSingleStepParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->getNrItemsD1(), observation.getNrSynthesizedBeams());
        kernelRunTimeConfigurations.dedispersionSingleStepLocal = cl::NDRange(kernelConfigurations.dedispersionSingleStepParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->getNrThreadsD0(), kernelConfigurations.dedispersionSingleStepParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->getNrThreadsD1(), 1);
        if (options.debug)
        {
            std::cout << "DedispersionSingleStep" << std::endl;
            std::cout << kernelConfigurations.dedispersionSingleStepParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->print() << std::endl;
            std::cout << std::endl;
        }
    }
    else
    {
        kernelRunTimeConfigurations.dedispersionStepOneGlobal = cl::NDRange(isa::utils::pad(observation.getNrSamplesPerBatch(true) / kernelConfigurations.dedispersionStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true))->getNrItemsD0(), kernelConfigurations.dedispersionStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true))->getNrThreadsD0()), observation.getNrDMs(true) / kernelConfigurations.dedispersionStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true))->getNrItemsD1(), observation.getNrBeams() * observation.getNrSubbands());
        kernelRunTimeConfigurations.dedispersionStepOneLocal = cl::NDRange(kernelConfigurations.dedispersionStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true))->getNrThreadsD0(), kernelConfigurations.dedispersionStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true))->getNrThreadsD1(), 1);
        if (options.debug)
        {
            std::cout << "DedispersionStepOne" << std::endl;
            std::cout << kernelConfigurations.dedispersionStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true))->print() << std::endl;
            std::cout << std::endl;
        }
        kernelRunTimeConfigurations.dedispersionStepTwoGlobal = cl::NDRange(isa::utils::pad(observation.getNrSamplesPerBatch(true) / kernelConfigurations.dedispersionStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->getNrItemsD0(), kernelConfigurations.dedispersionStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->getNrThreadsD0()), observation.getNrDMs() / kernelConfigurations.dedispersionStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->getNrItemsD1(), observation.getNrSynthesizedBeams() * observation.getNrDMs(true));
        kernelRunTimeConfigurations.dedispersionStepTwoLocal = cl::NDRange(kernelConfigurations.dedispersionStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->getNrThreadsD0(), kernelConfigurations.dedispersionStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->getNrThreadsD1(), 1);
        if (options.debug)
        {
            std::cout << "DedispersionStepTwo" << std::endl;
            std::cout << kernelConfigurations.dedispersionStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->print() << std::endl;
            std::cout << std::endl;
        }
    }
}

/**
 * @brief Generate the run-time configuration for the integration OpenCL kernels.
 */
void generateIntegrationOpenCLRunTimeConfigurations(const AstroData::Observation &observation, const Options &options,
                                                    const DeviceOptions &deviceOptions,
                                                    const KernelConfigurations &kernelConfigurations,
                                                    const HostMemory &hostMemory,
                                                    KernelRunTimeConfigurations &kernelRunTimeConfigurations)
{
    kernelRunTimeConfigurations.integrationGlobal.resize(hostMemory.integrationSteps.size());
    kernelRunTimeConfigurations.integrationLocal.resize(hostMemory.integrationSteps.size());
    for (unsigned int stepNumber = 0; stepNumber < hostMemory.integrationSteps.size(); stepNumber++)
    {
        auto step = hostMemory.integrationSteps.begin();

        std::advance(step, stepNumber);
        if (!options.subbandDedispersion)
        {
            kernelRunTimeConfigurations.integrationGlobal.at(stepNumber) = cl::NDRange(kernelConfigurations.integrationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(*step)->getNrThreadsD0() * ((observation.getNrSamplesPerBatch() / *step) / kernelConfigurations.integrationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(*step)->getNrItemsD0()), observation.getNrDMs(), observation.getNrSynthesizedBeams());
            kernelRunTimeConfigurations.integrationLocal.at(stepNumber) = cl::NDRange(kernelConfigurations.integrationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(*step)->getNrThreadsD0(), 1, 1);
            if (options.debug)
            {
                std::cout << "integration (" + std::to_string(*step) + ")" << std::endl;
                std::cout << kernelConfigurations.integrationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(*step)->print() << std::endl;
                std::cout << std::endl;
            }
        }
        else
        {
            kernelRunTimeConfigurations.integrationGlobal.at(stepNumber) = cl::NDRange(kernelConfigurations.integrationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(*step)->getNrThreadsD0() * ((observation.getNrSamplesPerBatch() / *step) / kernelConfigurations.integrationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(*step)->getNrItemsD0()), observation.getNrDMs(true) * observation.getNrDMs(), observation.getNrSynthesizedBeams());
            kernelRunTimeConfigurations.integrationLocal.at(stepNumber) = cl::NDRange(kernelConfigurations.integrationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(*step)->getNrThreadsD0(), 1, 1);
            if (options.debug)
            {
                std::cout << "integration (" + std::to_string(*step) + ")" << std::endl;
                std::cout << kernelConfigurations.integrationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(*step)->print() << std::endl;
                std::cout << std::endl;
            }
        }
    }
}

/**
 * @brief Generate the run-time configuration for the SNR OpenCL kernels.
 */
void generateSNROpenCLRunTimeConfigurations(const AstroData::Observation &observation, const Options &options,
                                            const DeviceOptions &deviceOptions,
                                            const KernelConfigurations &kernelConfigurations,
                                            const HostMemory &hostMemory,
                                            KernelRunTimeConfigurations &kernelRunTimeConfigurations)
{
    if (options.snrMode == SNRMode::Standard)
    {
        kernelRunTimeConfigurations.snrGlobal.resize(hostMemory.integrationSteps.size() + 1);
        kernelRunTimeConfigurations.snrLocal.resize(hostMemory.integrationSteps.size() + 1);
        if (!options.subbandDedispersion)
        {
            kernelRunTimeConfigurations.snrGlobal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), observation.getNrDMs(), observation.getNrSynthesizedBeams());
            kernelRunTimeConfigurations.snrLocal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), 1, 1);
            if (options.debug)
            {
                std::cout << "SNR (" + std::to_string(observation.getNrSamplesPerBatch()) + ")" << std::endl;
                std::cout << kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->print() << std::endl;
                std::cout << std::endl;
            }
        }
        else
        {
            kernelRunTimeConfigurations.snrGlobal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), observation.getNrDMs(true) * observation.getNrDMs(), observation.getNrSynthesizedBeams());
            kernelRunTimeConfigurations.snrLocal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), 1, 1);
            if (options.debug)
            {
                std::cout << "SNR (" + std::to_string(observation.getNrSamplesPerBatch()) + ")" << std::endl;
                std::cout << kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->print() << std::endl;
                std::cout << std::endl;
            }
        }
        for (unsigned int stepNumber = 0; stepNumber < hostMemory.integrationSteps.size(); stepNumber++)
        {
            auto step = hostMemory.integrationSteps.begin();

            std::advance(step, stepNumber);
            if (!options.subbandDedispersion)
            {
                kernelRunTimeConfigurations.snrGlobal.at(stepNumber) = cl::NDRange(kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), observation.getNrDMs(), observation.getNrSynthesizedBeams());
                kernelRunTimeConfigurations.snrLocal.at(stepNumber) = cl::NDRange(kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), 1, 1);
                if (options.debug)
                {
                    std::cout << "SNR (" + std::to_string(observation.getNrSamplesPerBatch() / *step) + ")" << std::endl;
                    std::cout << kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->print() << std::endl;
                    std::cout << std::endl;
                }
            }
            else
            {
                kernelRunTimeConfigurations.snrGlobal.at(stepNumber) = cl::NDRange(kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), observation.getNrDMs(true) * observation.getNrDMs(), observation.getNrSynthesizedBeams());
                kernelRunTimeConfigurations.snrLocal.at(stepNumber) = cl::NDRange(kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), 1, 1);
                if (options.debug)
                {
                    std::cout << "SNR (" + std::to_string(observation.getNrSamplesPerBatch() / *step) + ")" << std::endl;
                    std::cout << kernelConfigurations.snrParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->print() << std::endl;
                    std::cout << std::endl;
                }
            }
        }
    }
    else if (options.snrMode == SNRMode::Momad)
    {
        // Max kernel
        kernelRunTimeConfigurations.maxGlobal.resize(hostMemory.integrationSteps.size() + 1);
        kernelRunTimeConfigurations.maxLocal.resize(hostMemory.integrationSteps.size() + 1);
        if (!options.subbandDedispersion)
        {
            kernelRunTimeConfigurations.maxGlobal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), observation.getNrDMs(), observation.getNrSynthesizedBeams());
            kernelRunTimeConfigurations.maxLocal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), 1, 1);
            if (options.debug)
            {
                std::cout << "Max (" + std::to_string(observation.getNrSamplesPerBatch()) + ")" << std::endl;
                std::cout << kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->print() << std::endl;
                std::cout << std::endl;
            }
        }
        else
        {
            kernelRunTimeConfigurations.maxGlobal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), observation.getNrDMs(true) * observation.getNrDMs(), observation.getNrSynthesizedBeams());
            kernelRunTimeConfigurations.maxLocal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), 1, 1);
            if (options.debug)
            {
                std::cout << "Max (" + std::to_string(observation.getNrSamplesPerBatch()) + ")" << std::endl;
                std::cout << kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->print() << std::endl;
                std::cout << std::endl;
            }
        }
        for (unsigned int stepNumber = 0; stepNumber < hostMemory.integrationSteps.size(); stepNumber++)
        {
            auto step = hostMemory.integrationSteps.begin();

            std::advance(step, stepNumber);
            if (!options.subbandDedispersion)
            {
                kernelRunTimeConfigurations.maxGlobal.at(stepNumber) = cl::NDRange(kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), observation.getNrDMs(), observation.getNrSynthesizedBeams());
                kernelRunTimeConfigurations.maxLocal.at(stepNumber) = cl::NDRange(kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), 1, 1);
                if (options.debug)
                {
                    std::cout << "Max (" + std::to_string(observation.getNrSamplesPerBatch() / *step) + ")" << std::endl;
                    std::cout << kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->print() << std::endl;
                    std::cout << std::endl;
                }
            }
            else
            {
                kernelRunTimeConfigurations.maxGlobal.at(stepNumber) = cl::NDRange(kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), observation.getNrDMs(true) * observation.getNrDMs(), observation.getNrSynthesizedBeams());
                kernelRunTimeConfigurations.maxLocal.at(stepNumber) = cl::NDRange(kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), 1, 1);
                if (options.debug)
                {
                    std::cout << "Max (" + std::to_string(observation.getNrSamplesPerBatch() / *step) + ")" << std::endl;
                    std::cout << kernelConfigurations.maxParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->print() << std::endl;
                    std::cout << std::endl;
                }
            }
        }
        // Median of medians first step kernel
        kernelRunTimeConfigurations.medianOfMediansStepOneGlobal.resize(hostMemory.integrationSteps.size() + 1);
        kernelRunTimeConfigurations.medianOfMediansStepOneLocal.resize(hostMemory.integrationSteps.size() + 1);
        if (!options.subbandDedispersion)
        {
            kernelRunTimeConfigurations.medianOfMediansStepOneGlobal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0() * (observation.getNrSamplesPerBatch() / options.medianStepSize), observation.getNrDMs(), observation.getNrSynthesizedBeams());
            kernelRunTimeConfigurations.medianOfMediansStepOneLocal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), 1, 1);
            if (options.debug)
            {
                std::cout << "Median of medians first step (" + std::to_string(observation.getNrSamplesPerBatch()) + ")" << std::endl;
                std::cout << kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->print() << std::endl;
                std::cout << std::endl;
            }
        }
        else
        {
            kernelRunTimeConfigurations.medianOfMediansStepOneGlobal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0() * (observation.getNrSamplesPerBatch() / options.medianStepSize), observation.getNrDMs(true) * observation.getNrDMs(), observation.getNrSynthesizedBeams());
            kernelRunTimeConfigurations.medianOfMediansStepOneLocal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), 1, 1);
            if (options.debug)
            {
                std::cout << "Median of medians first step (" + std::to_string(observation.getNrSamplesPerBatch()) + ")" << std::endl;
                std::cout << kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->print() << std::endl;
                std::cout << std::endl;
            }
        }
        for (unsigned int stepNumber = 0; stepNumber < hostMemory.integrationSteps.size(); stepNumber++)
        {
            auto step = hostMemory.integrationSteps.begin();

            std::advance(step, stepNumber);
            if (!options.subbandDedispersion)
            {
                kernelRunTimeConfigurations.medianOfMediansStepOneGlobal.at(stepNumber) = cl::NDRange(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0() * (observation.getNrSamplesPerBatch() / *step / options.medianStepSize), observation.getNrDMs(), observation.getNrSynthesizedBeams());
                kernelRunTimeConfigurations.medianOfMediansStepOneLocal.at(stepNumber) = cl::NDRange(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), 1, 1);
                if (options.debug)
                {
                    std::cout << "Median of medians first step (" + std::to_string(observation.getNrSamplesPerBatch() / *step) + ")" << std::endl;
                    std::cout << kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->print() << std::endl;
                    std::cout << std::endl;
                }
            }
            else
            {
                kernelRunTimeConfigurations.medianOfMediansStepOneGlobal.at(stepNumber) = cl::NDRange(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0() * (observation.getNrSamplesPerBatch() / *step / options.medianStepSize), observation.getNrDMs(true) * observation.getNrDMs(), observation.getNrSynthesizedBeams());
                kernelRunTimeConfigurations.medianOfMediansStepOneLocal.at(stepNumber) = cl::NDRange(kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), 1, 1);
                if (options.debug)
                {
                    std::cout << "Median of medians first step (" + std::to_string(observation.getNrSamplesPerBatch() / *step) + ")" << std::endl;
                    std::cout << kernelConfigurations.medianOfMediansStepOneParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->print() << std::endl;
                    std::cout << std::endl;
                }
            }
        }
        // Median of medians second step kernel
        kernelRunTimeConfigurations.medianOfMediansStepTwoGlobal.resize(hostMemory.integrationSteps.size() + 1);
        kernelRunTimeConfigurations.medianOfMediansStepTwoLocal.resize(hostMemory.integrationSteps.size() + 1);
        if (!options.subbandDedispersion)
        {
            kernelRunTimeConfigurations.medianOfMediansStepTwoGlobal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0() * (observation.getNrSamplesPerBatch() / options.medianStepSize), observation.getNrDMs(), observation.getNrSynthesizedBeams());
            kernelRunTimeConfigurations.medianOfMediansStepTwoLocal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), 1, 1);
            if (options.debug)
            {
                std::cout << "Median of medians second step (" + std::to_string(observation.getNrSamplesPerBatch()) + ")" << std::endl;
                std::cout << kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->print() << std::endl;
                std::cout << std::endl;
            }
        }
        else
        {
            kernelRunTimeConfigurations.medianOfMediansStepTwoGlobal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0() * (observation.getNrSamplesPerBatch() / options.medianStepSize), observation.getNrDMs(true) * observation.getNrDMs(), observation.getNrSynthesizedBeams());
            kernelRunTimeConfigurations.medianOfMediansStepTwoLocal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), 1, 1);
            if (options.debug)
            {
                std::cout << "Median of medians second step (" + std::to_string(observation.getNrSamplesPerBatch()) + ")" << std::endl;
                std::cout << kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->print() << std::endl;
                std::cout << std::endl;
            }
        }
        for (unsigned int stepNumber = 0; stepNumber < hostMemory.integrationSteps.size(); stepNumber++)
        {
            auto step = hostMemory.integrationSteps.begin();

            std::advance(step, stepNumber);
            if (!options.subbandDedispersion)
            {
                kernelRunTimeConfigurations.medianOfMediansStepTwoGlobal.at(stepNumber) = cl::NDRange(kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0() * (observation.getNrSamplesPerBatch() / *step / options.medianStepSize), observation.getNrDMs(), observation.getNrSynthesizedBeams());
                kernelRunTimeConfigurations.medianOfMediansStepTwoLocal.at(stepNumber) = cl::NDRange(kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), 1, 1);
                if (options.debug)
                {
                    std::cout << "Median of medians second step (" + std::to_string(observation.getNrSamplesPerBatch() / *step) + ")" << std::endl;
                    std::cout << kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->print() << std::endl;
                    std::cout << std::endl;
                }
            }
            else
            {
                kernelRunTimeConfigurations.medianOfMediansStepTwoGlobal.at(stepNumber) = cl::NDRange(kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0() * (observation.getNrSamplesPerBatch() / *step / options.medianStepSize), observation.getNrDMs(true) * observation.getNrDMs(), observation.getNrSynthesizedBeams());
                kernelRunTimeConfigurations.medianOfMediansStepTwoLocal.at(stepNumber) = cl::NDRange(kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), 1, 1);
                if (options.debug)
                {
                    std::cout << "Median of medians second step (" + std::to_string(observation.getNrSamplesPerBatch() / *step) + ")" << std::endl;
                    std::cout << kernelConfigurations.medianOfMediansStepTwoParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->print() << std::endl;
                    std::cout << std::endl;
                }
            }
        }
        // Median of medians absolute deviation kernel
        kernelRunTimeConfigurations.medianOfMediansAbsoluteDeviationGlobal.resize(hostMemory.integrationSteps.size() + 1);
        kernelRunTimeConfigurations.medianOfMediansAbsoluteDeviationLocal.resize(hostMemory.integrationSteps.size() + 1);
        if (!options.subbandDedispersion)
        {
            kernelRunTimeConfigurations.medianOfMediansAbsoluteDeviationGlobal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0() * (observation.getNrSamplesPerBatch() / options.medianStepSize), observation.getNrDMs(), observation.getNrSynthesizedBeams());
            kernelRunTimeConfigurations.medianOfMediansAbsoluteDeviationLocal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), 1, 1);
            if (options.debug)
            {
                std::cout << "Median of medians absolute deviation (" + std::to_string(observation.getNrSamplesPerBatch()) + ")" << std::endl;
                std::cout << kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->print() << std::endl;
                std::cout << std::endl;
            }
        }
        else
        {
            kernelRunTimeConfigurations.medianOfMediansAbsoluteDeviationGlobal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0() * (observation.getNrSamplesPerBatch() / options.medianStepSize), observation.getNrDMs(true) * observation.getNrDMs(), observation.getNrSynthesizedBeams());
            kernelRunTimeConfigurations.medianOfMediansAbsoluteDeviationLocal.at(hostMemory.integrationSteps.size()) = cl::NDRange(kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->getNrThreadsD0(), 1, 1);
            if (options.debug)
            {
                std::cout << "Median of medians absolute deviation (" + std::to_string(observation.getNrSamplesPerBatch()) + ")" << std::endl;
                std::cout << kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch())->print() << std::endl;
                std::cout << std::endl;
            }
        }
        for (unsigned int stepNumber = 0; stepNumber < hostMemory.integrationSteps.size(); stepNumber++)
        {
            auto step = hostMemory.integrationSteps.begin();

            std::advance(step, stepNumber);
            if (!options.subbandDedispersion)
            {
                kernelRunTimeConfigurations.medianOfMediansAbsoluteDeviationGlobal.at(stepNumber) = cl::NDRange(kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0() * (observation.getNrSamplesPerBatch() / *step / options.medianStepSize), observation.getNrDMs(), observation.getNrSynthesizedBeams());
                kernelRunTimeConfigurations.medianOfMediansAbsoluteDeviationLocal.at(stepNumber) = cl::NDRange(kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), 1, 1);
                if (options.debug)
                {
                    std::cout << "Median of medians absolute deviation (" + std::to_string(observation.getNrSamplesPerBatch() / *step) + ")" << std::endl;
                    std::cout << kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->print() << std::endl;
                    std::cout << std::endl;
                }
            }
            else
            {
                kernelRunTimeConfigurations.medianOfMediansAbsoluteDeviationGlobal.at(stepNumber) = cl::NDRange(kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0() * (observation.getNrSamplesPerBatch() / *step / options.medianStepSize), observation.getNrDMs(true) * observation.getNrDMs(), observation.getNrSynthesizedBeams());
                kernelRunTimeConfigurations.medianOfMediansAbsoluteDeviationLocal.at(stepNumber) = cl::NDRange(kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->getNrThreadsD0(), 1, 1);
                if (options.debug)
                {
                    std::cout << "Median of medians absolute deviation (" + std::to_string(observation.getNrSamplesPerBatch() / *step) + ")" << std::endl;
                    std::cout << kernelConfigurations.medianOfMediansAbsoluteDeviationParameters.at(deviceOptions.deviceName)->at(observation.getNrDMs(true) * observation.getNrDMs())->at(observation.getNrSamplesPerBatch() / *step)->print() << std::endl;
                    std::cout << std::endl;
                }
            }
        }
    }
}

/**
 * @brief Generate OpenCL run-time configurations for all kernels.
 */
void generateOpenCLRunTimeConfigurations(const AstroData::Observation &observation, const Options &options,
                                         const DeviceOptions &deviceOptions,
                                         const KernelConfigurations &kernelConfigurations,
                                         const HostMemory &hostMemory,
                                         KernelRunTimeConfigurations &kernelRunTimeConfigurations)
{
    generateDedispersionOpenCLRunTimeConfigurations(observation, options, deviceOptions, kernelConfigurations, hostMemory,
                                                    kernelRunTimeConfigurations);
    generateIntegrationOpenCLRunTimeConfigurations(observation, options, deviceOptions, kernelConfigurations, hostMemory,
                                                   kernelRunTimeConfigurations);
    generateSNROpenCLRunTimeConfigurations(observation, options, deviceOptions, kernelConfigurations, hostMemory,
                                           kernelRunTimeConfigurations);
}
