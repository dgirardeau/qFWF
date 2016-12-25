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

//qCC_io
#include "BinFilter.h"

//Qt
#include <QSettings>

static const char COMMAND_EXTRACT_FWF_PEAKS[] = "FWF_PEAKS";

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
		
		int detectorType = 0;
		double minAmplitude = 0;
		int maxReturnCount = 1;
		QString exportFormat = "BIN";
		QSettings settings(filename, QSettings::IniFormat);
		{
			//mandatory parameters
			bool ok = false;
			detectorType = settings.value("Detector").toInt(&ok);
			if (!ok)
			{
				return cmd.error("Detector not specified");
			}

			//optional parameters
			minAmplitude = settings.value("MinAmplitude", minAmplitude).toDouble();
			maxReturnCount = settings.value("MaxReturnCount", maxReturnCount).toInt();
			exportFormat = settings.value("ExportFormat", exportFormat).toString();
		}

		if (	exportFormat != "BIN"
			&&	exportFormat != "LAS")
		{
			return cmd.error(QString("Unhandled export format: %1").arg(exportFormat));
		}

		FWFPeakDetectorInterface* detector = 0;
		switch (detectorType)
		{
		case 0:
			detector = new FWFPeakDetectorNaive;
			break;
		case 1:
			detector = new FWFPeakDetectorNLSGauss;
			break;
		default:
			assert(false);
			return cmd.error("Unknown peak detector type");
		}
		assert(detector);

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
				//replace current cloud by this one
				delete cloudDesc.pc;
				cloudDesc.pc = outputCloud;
				cloudDesc.basename += QString("_FWF_PEAKS");

				if (cmd.autoSaveMode())
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
			}

		}

		CC_FILE_ERROR result = CC_FERR_NO_ERROR;
		FileIOFilter::Shared filter = FileIOFilter::GetFilter(LASFWFFilter::GetFileFilter(), true);
		if (!filter)
		{
			return cmd.error("Internal error: failed to load the LAS FWF I/O filter");
		}

		return cmd.importFile(filename, filter);
	}
};

#endif //LAS_FWF_IO_PLUGIN_COMMANDS_HEADER
