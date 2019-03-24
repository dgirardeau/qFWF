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

#include "PeakExtractionTools.h"

//Local
#include "FWFPeakDetectorInterface.h"

//plugins
#include <ccMainAppInterface.h>

//qCC_db
#include <ccPointCloud.h>
#include <ccScalarField.h>
#include <ccProgressDialog.h>

//qPDALIO
#include "../../core/IO/qPDALIO/src/LASFields.h"

//Qt
#include <QCoreApplication>
#include <QMainWindow>
#include <QAtomicInt>

struct PeakInfo
{
	PeakInfo()
		: pointIndex(0)
		, returnNumber(1)
	{}
	
	PeakInfo(const FWFPeakDetectorInterface::Peak& p, unsigned index, uint8_t rn)
		: peak(p)
		, pointIndex(index)
		, returnNumber(rn)
	{}

	FWFPeakDetectorInterface::Peak peak;
	unsigned pointIndex;
	uint8_t returnNumber;
};

ccPointCloud* PeakExtractionTools::ExtractNewPeaks(	const ccPointCloud* inputCloud,
													FWFPeakDetectorInterface* detector,
													int maxReturnCount,
													double minVal/*=0*/,
													ccMainAppInterface* app/*=0*/)
{
	if (!inputCloud || !detector)
	{
		if (app)
			app->dispToConsole(QString("Internal error: invalid input").arg(minVal), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		assert(false);
		return 0;
	}
	
	//number of points
	if (inputCloud->size() == 0 || !inputCloud->hasFWF())
	{
		if (app)
			app->dispToConsole(QString("Cloud has no associated FWF data!"), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return 0;
	}

	//output cloud
	QScopedPointer<ccPointCloud> outputCloud(0);

	//now for the conversion process
	try
	{
		//flag the points/waveforms to actually process (to avoid duplicates)
		std::vector<unsigned> toProcess;
		{
			std::set<uint64_t> processedWaveforms;
			toProcess.reserve(inputCloud->size());

			for (unsigned i = 0; i < inputCloud->size(); ++i)
			{
				ccWaveformProxy proxy = inputCloud->waveformProxy(i);
				if (!proxy.isValid())
				{
					continue;
				}

				uint64_t offset = proxy.waveform().dataOffset();
				if (processedWaveforms.find(offset) != processedWaveforms.end())
				{
					//waveform already flagged
					continue;
				}

				processedWaveforms.insert(offset);
				toProcess.push_back(i);
			}

			toProcess.shrink_to_fit();

			if (toProcess.empty())
			{
				if (app)
					app->dispToConsole(QString("Cloud has no valid FWF data?!"), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
				return 0;
			}
		}

		FWFPeakDetectorInterface::BaseParameters detectorParams;
		detectorParams.maxPeakCount = static_cast<unsigned>(std::max(1, maxReturnCount));
		detectorParams.minIntensity = minVal;

		//progress dialog
		ccProgressDialog pDlg(app ? app->getMainWindow() : 0);
		pDlg.setWindowTitle("Detecting new peaks");
		pDlg.setAutoClose(false);
		pDlg.setLabelText(QString("Waveforms: %1").arg(toProcess.size()));
		pDlg.setRange(0, 100);
		pDlg.show();
		QCoreApplication::processEvents();
		//CCLib::NormalizedProgress nProgress(&pDlg, toProcess.size()); //DGM: can't use it with OpenMP
		int progressCounter = 0;
		int progress = 0;

		std::vector<PeakInfo> detectedPeaks;
		QAtomicInt peakCount(0);

		bool error = false;
#if defined(_OPENMP)
		detectedPeaks.resize(static_cast<unsigned>(toProcess.size() * maxReturnCount));
#pragma omp parallel for
#endif
		for (int i = 0; i < static_cast<int>(toProcess.size()); ++i)
		{
			if (error)
			{
				continue;
			}

			int currentProgress = static_cast<int>((++progressCounter) * 100.0f / toProcess.size());
			if (currentProgress > progress)
			{
				if (pDlg.wasCanceled())
				{
					error = true;
					continue;
				}
				else
				{
					progress = currentProgress;
					QMetaObject::invokeMethod(&pDlg, "setValue", Qt::QueuedConnection, Q_ARG(int, currentProgress));
					QCoreApplication::processEvents();
				}
			}

			//grab the waveform 'values'
			ccWaveformProxy proxy = inputCloud->waveformProxy(i);
			assert(proxy.isValid());

			std::vector<double> waveform;
			if (!proxy.decodeSamples(waveform))
			{
				error = true;
				continue;
			}

			std::vector<FWFPeakDetectorInterface::Peak> peaks;
			if (detector->detectPeaks(peaks, waveform, detectorParams) && !peaks.empty())
			{
				//sort the peaks from the first to the last one
				std::sort(	peaks.begin(),
							peaks.end(),
							[](const FWFPeakDetectorInterface::Peak& a, const FWFPeakDetectorInterface::Peak& b) -> bool { return a.x < b.x; }
				);

				//now create the corresponding points
				uint8_t returnNumber = 1;
				for (FWFPeakDetectorInterface::Peak& peak : peaks)
				{
					//store the peak point
					detectedPeaks[peakCount++] = PeakInfo(peak, i, returnNumber++);
				}
			}
		}

		if (error)
		{
			if (app)
				app->dispToConsole("An error occurred", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return 0;
		}

		detectedPeaks.resize(peakCount);

		outputCloud.reset(new ccPointCloud(inputCloud->getName() + ".reprocessed"));
		ccScalarField* intensitySF = new ccScalarField(LAS_FIELD_NAMES[LAS_INTENSITY]);
		outputCloud->addScalarField(intensitySF);
		ccScalarField* returnNumberSF = 0;
		if (maxReturnCount > 1)
		{
			returnNumberSF = new ccScalarField(LAS_FIELD_NAMES[LAS_NUMBER_OF_RETURNS]);
			outputCloud->addScalarField(returnNumberSF);
		}
		if (	!outputCloud->reserve(static_cast<unsigned>(toProcess.size() * maxReturnCount))
			||	!outputCloud->reserveTheFWFTable()
			)
		{
			if (app)
				app->dispToConsole("Not enough memory", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return 0;
		}

		pDlg.setValue(0);
		CCLib::NormalizedProgress nProgress(&pDlg, static_cast<unsigned>(detectedPeaks.size()));

		//now create the corresponding points
		for (const PeakInfo& info : detectedPeaks)
		{
			if (!nProgress.oneStep())
			{
				//process cancelled by the user
				return 0;
			}
			const FWFPeakDetectorInterface::Peak& peak = info.peak;

			ccWaveformProxy proxy = inputCloud->waveformProxy(info.pointIndex);
			assert(proxy.isValid());

			const CCVector3* P0 = inputCloud->getPoint(info.pointIndex);

			CCVector3 P = proxy.getSamplePos(peak.x, *P0);
			outputCloud->addPoint(P);

			ccWaveform w = proxy.waveform();
			w.setEchoTime_ps(static_cast<uint32_t>(peak.x * proxy.descriptor().samplingRate_ps));
			w.setReturnIndex(info.returnNumber);
			outputCloud->waveforms().push_back(w);

			if (intensitySF)
			{
				ScalarType s = static_cast<ScalarType>(peak.y);
				intensitySF->addElement(s);
			}
			if (returnNumberSF)
			{
				ScalarType s = static_cast<ScalarType>(info.returnNumber);
				returnNumberSF->addElement(s);
			}
		}

		outputCloud->fwfDescriptors() = inputCloud->fwfDescriptors();
		outputCloud->fwfData() = inputCloud->fwfData();

		outputCloud->waveforms().shrink_to_fit();
		outputCloud->shrinkToFit();
		
		outputCloud->setGlobalShift(inputCloud->getGlobalShift());
		outputCloud->setGlobalScale(inputCloud->getGlobalScale());
		outputCloud->setMetaData(inputCloud->metaData(), true);

		if (outputCloud->hasScalarFields())
		{
			for (unsigned i = 0; i < outputCloud->getNumberOfScalarFields(); ++i)
			{
				outputCloud->getScalarField(static_cast<int>(i))->computeMinAndMax();
			}
			outputCloud->setCurrentDisplayedScalarField(0);
			outputCloud->showSF(true);
		}

	}
	catch (const std::bad_alloc&)
	{
		if (app)
			app->dispToConsole("Not enough memory", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return 0;
	}

	return outputCloud.take();
}
