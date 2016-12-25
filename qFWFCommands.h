//##########################################################################
//#                                                                        #
//#                       CLOUDCOMPARE PLUGIN: qFWF                        #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 or later of the License.      #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#                         COPYRIGHT: OSUR / CNRS                         #
//#                                                                        #
//##########################################################################

#ifndef FWF_PLUGIN_COMMANDS_HEADER
#define FWF_PLUGIN_COMMANDS_HEADER

#include "../ccCommandLineInterface.h"
#include "../qLAS_FWF/Filter/LASFWFFilter.h"

//Local
#include "PeakExtractionTools.h"
#include "FWFPeakDetectorNaive.h"
#include "FWFPeakDetectorNLSGauss.h"
#include "OrthoWFTools.h"

//qCC_io
#include "BinFilter.h"

//Qt
#include <QSettings>

//Commands
static const char COMMAND_EXTRACT_FWF_PEAKS[] = "FWF_PEAKS";
static const char COMMAND_ORTHO_FWF[]         = "FWF_ORTHO";

void ExportFWFCloud(CLCloudDesc& cloudDesc, const QString& exportFormat, ccCommandLineInterface& cmd)
{
	//backup the existing cloud export format first
	QString previousCloudExportFormat = cmd.cloudExportFormat();
	QString previousCloudExportExt = cmd.cloudExportExt();

	if (exportFormat == "BIN")
	{
		//force the export format to BIN
		cmd.setCloudExportFormat(BinFilter::GetFileFilter(), "bin");
	}
	else
	{
		//force the export format to LAS FWF
		cmd.setCloudExportFormat(LASFWFFilter::GetFileFilter(), "laz");
	}

	QString errorStr = cmd.exportEntity(cloudDesc);
	if (!errorStr.isEmpty())
	{
		cmd.warning(errorStr);
	}

	//restore the previous file output format
	cmd.setCloudExportFormat(previousCloudExportFormat, previousCloudExportExt);
}

FWFPeakDetectorInterface* GetDetector(QString detectorType)
{
	if (detectorType == "NAIVE")
	{
		return new FWFPeakDetectorNaive;
	}
	else if (detectorType == "NLS_GAUSS")
	{
		return new FWFPeakDetectorNLSGauss;
	}
	else
	{
		assert(false);
		return 0;
	}
}

struct CommandExtractFWFPeaks : public ccCommandLineInterface::Command
{
	CommandExtractFWFPeaks() : ccCommandLineInterface::Command("Extract FWF peaks", COMMAND_EXTRACT_FWF_PEAKS) {}

	virtual bool process(ccCommandLineInterface& cmd) override
	{
		cmd.print("[EXTRACT PEAKS]");
		if (cmd.arguments().empty())
		{
			return cmd.error(QString("Missing parameter: parameters filename after \"-%1\"").arg(COMMAND_EXTRACT_FWF_PEAKS));
		}

		//open specified file
		QString filename(cmd.arguments().takeFirst());
		cmd.print(QString("Settings file: '%1'").arg(filename));

		if (!QFile(filename).exists())
		{
			return cmd.error(QString("File '%1' does not exist").arg(filename));
		}

		QString detectorType = "NAIVE";
		double minAmplitude = 0;
		int maxReturnCount = 1;
		QString exportFormat = "BIN";
		QSettings settings(filename, QSettings::IniFormat);
		{
			settings.beginGroup("WaveformExtraction");
			{
				//mandatory parameters
				detectorType = settings.value("Detector").toString().toUpper();

				//optional parameters
				minAmplitude = settings.value("MinAmplitude", minAmplitude).toDouble();
				maxReturnCount = settings.value("MaxReturnCount", maxReturnCount).toInt();
			}
			settings.endGroup();

			settings.beginGroup("Export");
			{
				//mandatory parameters
				exportFormat = settings.value("Format", exportFormat).toString();
			}
			settings.endGroup();
		}

		if (	exportFormat != "BIN"
			&&	exportFormat != "LAS")
		{
			return cmd.error(QString("Unhandled export format: %1").arg(exportFormat));
		}

		FWFPeakDetectorInterface* detector = GetDetector(detectorType);
		if (!detector)
		{
			assert(false);
			return cmd.error("Unknown peak detector type");
		}

		for (CLCloudDesc& cloudDesc : cmd.clouds())
		{
			if (!cloudDesc.pc)
			{
				assert(false);
				continue;
			}
			if (!cloudDesc.pc->hasFWF())
			{
				cmd.warning(QString("Cloud %1 has no associated FWF data").arg(cloudDesc.pc->getName()));
				continue;
			}

			ccPointCloud* outputCloud = PeakExtractionTools::ExtractNewPeaks(cloudDesc.pc, detector, maxReturnCount, minAmplitude);
			if (outputCloud)
			{
				//replace the current cloud by this one
				delete cloudDesc.pc;
				cloudDesc.pc = outputCloud;
				cloudDesc.basename += QString("_FWF_PEAKS");

				if (cmd.autoSaveMode())
				{
					ExportFWFCloud(cloudDesc, exportFormat, cmd);
				}
			}

		}

		return true;
	}
};
struct CommandComputeOrthoFWF : public ccCommandLineInterface::Command
{
	CommandComputeOrthoFWF() : ccCommandLineInterface::Command("Extract FWF peaks", COMMAND_ORTHO_FWF) {}

	virtual bool process(ccCommandLineInterface& cmd) override
	{
		cmd.print("[COMPUTE ORTHO FWF]");
		if (cmd.arguments().empty())
		{
			return cmd.error(QString("Missing parameter: parameters filename after \"-%1\"").arg(COMMAND_ORTHO_FWF));
		}

		//open specified file
		QString filename(cmd.arguments().takeFirst());
		cmd.print(QString("Settings file: '%1'").arg(filename));

		if (!QFile(filename).exists())
		{
			return cmd.error(QString("File '%1' does not exist").arg(filename));
		}
		
		double horizStep = 0;
		double vertStep = 0;
		double interpRadius = 0;
		FWFGrid::InterpolationMethod interpMethod = FWFGrid::LINEAR;
		QString detectorType = "NAIVE";
		double minAmplitude = 0;
		int maxReturnCount = 1;
		int bps = 16;
		int numberOfSamples = 64;
		QString exportFormat = "BIN";
		QSettings settings(filename, QSettings::IniFormat);
		{
			settings.beginGroup("Projection");
			{
				//mandatory parameters
				bool ok = false;

				horizStep = settings.value("HorizStep").toDouble(&ok);
				if (!ok || horizStep <= 0)
				{
					return cmd.error("Missing or wrong parameter: horizontal step");
				}
				vertStep = settings.value("VertStep").toDouble(&ok);
				if (!ok || vertStep <= 0)
				{
					return cmd.error("Missing or wrong parameter: vertical step");
				}
				interpRadius = settings.value("InterpRadius").toDouble(&ok);
				if (!ok || interpRadius <= 0)
				{
					return cmd.error("Missing or wrong parameter: interpolation radius");
				}
				
				//optional parameters
				QString interpMethodName = settings.value("InterpMethod").toString().toUpper();
				if (!interpMethodName.isEmpty())
				{
					if (interpMethodName == "LINEAR")
					{
						interpMethod = FWFGrid::LINEAR;
					}
					else if (interpMethodName == "INV_SQUARE")
					{
						interpMethod = FWFGrid::INV_SQUARE;
					}
					else if (interpMethodName == "INV_EXP")
					{
						interpMethod = FWFGrid::INV_EXP;
					}
					else
					{
						return cmd.error("Wrong parameter: interpolation method (choices are 'LINEAR', 'INV_SQUARE' or 'INV_EXP')");
					}
				}
			}
			settings.endGroup();
			
			settings.beginGroup("WaveformExtraction");
			{
				//mandatory parameters
				detectorType = settings.value("Detector").toString().toUpper();

				//optional parameters
				minAmplitude = settings.value("MinAmplitude", minAmplitude).toDouble();
				maxReturnCount = settings.value("MaxReturnCount", maxReturnCount).toInt();
				bps = settings.value("BitPerSample", bps).toInt();
				numberOfSamples = settings.value("NumberOfSamples", numberOfSamples).toInt();
			}
			settings.endGroup();

			settings.beginGroup("Export");
			{
				//mandatory parameters
				exportFormat = settings.value("Format", exportFormat).toString();
			}
			settings.endGroup();
		}

		if (	exportFormat != "BIN"
			&&	exportFormat != "LAS")
		{
			return cmd.error(QString("Unhandled export format: %1").arg(exportFormat));
		}

		FWFPeakDetectorInterface* detector = GetDetector(detectorType);
		if (!detector)
		{
			assert(false);
			return cmd.error("Unknown peak detector type");
		}

		for (CLCloudDesc& cloudDesc : cmd.clouds())
		{
			if (!cloudDesc.pc)
			{
				assert(false);
				continue;
			}
			cmd.print("Cloud: " + cloudDesc.pc->getName());
			
			if (!cloudDesc.pc->hasFWF())
			{
				cmd.warning(QString("Cloud %1 has no associated FWF data").arg(cloudDesc.pc->getName()));
				continue;
			}

			CCVector3d steps(horizStep, horizStep, vertStep);
			ccBBox box = OrthoWFTools::ComputeFWFBox(cloudDesc.pc);
			CCVector3d minCorner = CCVector3d::fromArray(box.minCorner().u);
			Tuple3ui gridSize = OrthoWFTools::ComputeGridSize(box, horizStep, vertStep);

			//init the grid
			FWFGrid grid;
			if (!grid.init(gridSize, steps, minCorner))
			{
				cmd.warning("Failed to initialize the grid");
				continue;
			}

			//now fill it
			if (!grid.project(cloudDesc.pc, interpRadius, interpMethod, cmd.widgetParent()))
			{
				cmd.warning("Failed to fill the grid");
				continue;
			}

			//build the waveform descriptor
			WaveformDescriptor descriptor;
			{
				descriptor.bitsPerSample = bps;
				descriptor.numberOfSamples = numberOfSamples;

				//set the gain and offset automatically
				double rangeMin = 0, rangeMax = 0;
				cloudDesc.pc->computeFWFAmplitude(rangeMin, rangeMax);
				descriptor.digitizerOffset = rangeMin;
				if (descriptor.digitizerOffset > 0)
				{
					//auto-shift to 0
					descriptor.digitizerOffset = 0;
				}
				descriptor.digitizerGain = (rangeMax - descriptor.digitizerOffset) / (1 << descriptor.bitsPerSample);

				//compute the right sampling rate
				//(find the first valid waveform)
				for (const ccWaveform& w : cloudDesc.pc->waveforms())
				{
					if (w.descriptorID() != 0 && cloudDesc.pc->fwfDescriptors().contains(w.descriptorID()))
					{
						WaveformDescriptor& d = cloudDesc.pc->fwfDescriptors()[w.descriptorID()];

						double sampleStep = w.beamDir().normd() * d.samplingRate_ps;
						double gridStep = grid.steps().z;

						descriptor.samplingRate_ps = static_cast<uint32_t>(gridStep * d.samplingRate_ps / sampleStep);
						if (descriptor.samplingRate_ps > 100000)
						{
							cmd.warning("High sampling rate detected. Accuracy may be jeopardized (the vertical grid step may be too high).");
						}
						//descriptor.samplingRate_ps = 1000;
						break;
					}
				}
			}

			//eventually extract the FWF data
			ccPointCloud* outputCloud = OrthoWFTools::ConvertGridToFWFCloud(grid, detector, maxReturnCount, descriptor, minAmplitude, cmd.widgetParent());
			if (outputCloud)
			{
				//replace the current cloud by this one
				delete cloudDesc.pc;
				cloudDesc.pc = outputCloud;
				cloudDesc.basename += QString("_ORTHO_FWF");

				if (cmd.autoSaveMode())
				{
					ExportFWFCloud(cloudDesc, exportFormat, cmd);
				}
			}
		}

		return true;
	}
};

#endif //LAS_FWF_IO_PLUGIN_COMMANDS_HEADER
