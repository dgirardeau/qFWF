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

#include "OrthoWFTools.h"

//Local
#include "FWFPeakDetectorInterface.h"

//plugins
#include <ccMainAppInterface.h>

//qLASIO
#include "LasDetails.h"

//qCC_db
#include <ccPointCloud.h>
#include <ccScalarField.h>
#include <ccProgressDialog.h>

//Qt
#include <QCoreApplication>
#include <QMainWindow>

#if defined(_OPENMP)
#include <omp.h>
#endif

Tuple3ui OrthoWFTools::ComputeGridSize(const ccBBox& box, double hGridStep, double vGridStep)
{
	if (!box.isValid() || hGridStep == 0 || vGridStep == 0)
	{
		assert(false);
		//invalid box
		return Tuple3ui(0, 0, 0);
	}

	CCVector3d boxDiag = CCVector3d::fromArray(box.maxCorner().u) - CCVector3d::fromArray(box.minCorner().u);
	if (boxDiag.x <= 0 || boxDiag.y <= 0 || boxDiag.z <= 0)
	{
		//invalid box
		return Tuple3ui(0, 0, 0);
	}

	//DGM: we now use the 'PixelIsArea' convention (the height value is computed at the grid cell center)
	Tuple3ui size;
	size.x = 1 + static_cast<unsigned>(boxDiag.x / hGridStep + 0.5);
	size.y = 1 + static_cast<unsigned>(boxDiag.y / hGridStep + 0.5);
	size.z = 1 + static_cast<unsigned>(boxDiag.z / vGridStep + 0.5);

	return size;
}

ccPointCloud* OrthoWFTools::ConvertGridToGridCloud(	const FWFGrid& grid,
													double minVal/*=0*/,
													ccMainAppInterface* app/*=0*/)
{
	//count the number of valid cells
	unsigned validCellCount = grid.countValidCells(minVal);
	if (validCellCount == 0)
	{
		if (app)
			app->dispToConsole(QString("Grid has no cell with a value above %1?!").arg(minVal), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return 0;
	}

	//create the cloud
	ccPointCloud* cloud = 0;
	{
		cloud = new ccPointCloud("fwf_grid");
		ccScalarField::Shared amplitudes = std::make_shared<ccScalarField>("Mean amplitude");
		ccScalarField::Shared stdDev     = std::make_shared<ccScalarField>("Amplitude std. dev.");
		//ccScalarField* weights = new ccScalarField("Weights");

		cloud->addScalarField(amplitudes);
		cloud->addScalarField(stdDev);
		//cloud->addScalarField(weights);

		if (!cloud->reserve(validCellCount))
		{
			//not enough memory
			if (app)
				app->dispToConsole("Not enough memory", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			delete cloud;
			return 0;
		}

		for (unsigned z = 0; z < grid.size().z; ++z)
		{
			const FWFGrid::Slice& slice = grid.getSlice(z);
			const FWFGrid::CellType* _cell = &(slice[0]);

			Tuple3i currentVoxel(0, 0, static_cast<int>(z));
			for (unsigned y = 0; y < grid.size().y; ++y)
			{
				currentVoxel.y = static_cast<int>(y);
				for (unsigned x = 0; x < grid.size().x; ++x, ++_cell)
				{
					if (_cell->weightSum == 0)
					{
						continue;
					}

					double meanVal = _cell->mean();
					if (meanVal > minVal)
					{
						currentVoxel.x = static_cast<int>(x);
						CCVector3d C = grid.computeCellCenter(currentVoxel);

						cloud->addPoint(CCVector3::fromArray(C.u));
						amplitudes->addElement(meanVal);
						ScalarType s = static_cast<ScalarType>(_cell->stdDev());
						stdDev->addElement(s);
						//s = static_cast<ScalarType>(_cell->weightSum);
						//weights->addElement(s);
					}
				}
			}
		}

		amplitudes->computeMinAndMax();
		stdDev->computeMinAndMax();
		//weights->computeMinAndMax();

		cloud->setCurrentDisplayedScalarField(0);
		cloud->showSF(true);
	}

	return cloud;
}

struct PeakInfo
{
	PeakInfo()
		: xy(-1)
		, returnNumber(1)
	{}
	
	PeakInfo(const FWFPeakDetectorInterface::Peak& p, int pos, uint8_t rn)
		: peak(p)
		, xy(pos)
		, returnNumber(rn)
	{}

	FWFPeakDetectorInterface::Peak peak;
	int xy;
	uint8_t returnNumber;
};

ccPointCloud* OrthoWFTools::ConvertGridToFWFCloud(	const FWFGrid& grid,
													FWFPeakDetectorInterface* detector,
													int maxReturnCount,
													const WaveformDescriptor& descriptor,
													double minVal/*=0*/,
													QWidget* parentWidget/*=0*/,
													ccMainAppInterface* app/*=0*/)
{
	if (!detector)
	{
		if (app)
			app->dispToConsole(QString("Internal error: invalid input detector").arg(minVal), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		assert(false);
		return nullptr;
	}
	
	//count the number of valid cells
	unsigned validCellCount = grid.countValidCells(minVal);
	if (validCellCount == 0)
	{
		if (app)
			app->dispToConsole(QString("Grid has no cell with a value above %1?!").arg(minVal), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return nullptr;
	}

	std::unique_ptr<ccPointCloud> cloud;

	//now for the conversion process
	try
	{
		//we'll generate synthetic waveforms from the grid 'columns'
		FWFPeakDetectorInterface::BaseParameters detectorParams;
		detectorParams.maxPeakCount = static_cast<unsigned>(std::max(1, maxReturnCount));
		detectorParams.minIntensity = minVal;

		unsigned sliceSize = grid.size().x * grid.size().y;

		//progress dialog
		QScopedPointer<ccProgressDialog> pDlg(0);
		if (parentWidget)
		{
			pDlg.reset(new ccProgressDialog(false, parentWidget));
			pDlg->setWindowTitle("FWF");
			pDlg->setAutoClose(false);
			pDlg->setLabelText("Detecting the peaks...");
			pDlg->setRange(0, 100);
			pDlg->show();
			QCoreApplication::processEvents();
		}
		//CCCoreLib::NormalizedProgress nProgress2(pDlg, static_cast<unsigned>(sliceSize)); //DGM: can't use it with OpenMP
		int progressCounter = 0;
		int progress = 0;

		std::vector<PeakInfo> detectedPeaks;
		QAtomicInt peakCount(0);

		bool error = false;
		int numProcess = 1;

#if defined(_OPENMP)
		detectedPeaks.resize(sliceSize * maxReturnCount); //we must prevent any reallocation during the parallel process!!!
		numProcess = std::max(1, omp_get_max_threads());
#endif

		//pre-allocate memory to speed-up the process
		std::vector< std::vector<double> > waveforms;
		try
		{
			waveforms.resize(numProcess);
			for (int i = 0; i < numProcess; ++i)
			{
				waveforms[i].resize(grid.size().z);
			}
		}
		catch (const std::bad_alloc&)
		{
			if (app)
				app->dispToConsole(QObject::tr("Not enough memory"), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return nullptr;
		}

#if defined(_OPENMP)
#pragma omp parallel for
#endif
		for (int xy = 0; xy < static_cast<int>(sliceSize); ++xy)
		{
			if (error)
			{
				continue;
			}

			if (pDlg)
			{
				int currentProgress = static_cast<int>((++progressCounter) * 100.0f / sliceSize);
				if (currentProgress > progress)
				{
					if (pDlg->wasCanceled())
					{
						//process cancelled by the user
						error = true;
						continue;
					}
					else
					{
						progress = currentProgress;
						QMetaObject::invokeMethod(pDlg.data(), "setValue", Qt::QueuedConnection, Q_ARG(int, currentProgress));
						QCoreApplication::processEvents();
					}
				}
			}

			unsigned x = (static_cast<unsigned>(xy) % grid.size().x);
			unsigned y = (static_cast<unsigned>(xy) - x) / grid.size().x;

			//grab the signal 'values'
#if defined(_OPENMP)
			std::vector<double>& waveform = waveforms[omp_get_thread_num()];
#else
			std::vector<double>& waveform = waveforms[0];
#endif
			if (!grid.getColumnValues(x, y, waveform, true))
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

				//we reserve the right 'range' of peaks so as to store them next to each other
				//(even in parallel mode)
				int candidatePeakCount = 0;
				for (const FWFPeakDetectorInterface::Peak& peak : peaks)
				{
					if (peak.y >= minVal)
						++candidatePeakCount;
				}
				int targetIndex = peakCount.fetchAndAddRelaxed(candidatePeakCount);

				uint8_t returnNumber = 1;
				for (const FWFPeakDetectorInterface::Peak& peak : peaks)
				{
					if (peak.y >= minVal)
					{
						detectedPeaks[targetIndex++] = PeakInfo(peak, xy, returnNumber++);
					}
				}
			}
		}

		if (error)
		{
			if (app)
				app->dispToConsole("An error occurred", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return nullptr;
		}

		if (peakCount == 0)
		{
			if (app)
				app->dispToConsole("No peak detected", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return nullptr;
		}
		detectedPeaks.resize(peakCount);
		
		//reserve memory for the points
		cloud.reset(new ccPointCloud("Grid2FWF"));
		ccScalarField::Shared intensitySF = std::make_shared<ccScalarField>(LasNames::Intensity);
		cloud->addScalarField(intensitySF);
		ccScalarField::Shared returnNumberSF;
		if (maxReturnCount > 1)
		{
			returnNumberSF = std::make_shared<ccScalarField>(LasNames::ReturnNumber);
			cloud->addScalarField(returnNumberSF);
		}
		if (	!cloud->reserve(static_cast<unsigned>(detectedPeaks.size()))
			||	!cloud->reserveTheFWFTable())
		{
			if (app)
				app->dispToConsole("Not enough memory", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return nullptr;
		}

		//the descriptor has already been specified by the caller
		cloud->fwfDescriptors().insert(1, descriptor);

		//number of samples to save before the echo (if possible)
		static const float s_relativeMargin = 0.25f;
		int waveMargin = static_cast<int>(descriptor.numberOfSamples * s_relativeMargin);

		//max raw signal value
		qint64 maxRawValue = 1;
		maxRawValue <<= descriptor.bitsPerSample;
		maxRawValue -= 1;

		//number of bytes per sample
		uint32_t bytesPerSample = descriptor.bitsPerSample / 8;
		if (bytesPerSample * 8 < descriptor.bitsPerSample)
		{
			assert(false); //not supported)
			++bytesPerSample;
		}

		//FWF data block
		std::unique_ptr<ccPointCloud::FWFDataContainer> fwfData = std::make_unique<ccPointCloud::FWFDataContainer>();

		if (pDlg)
		{
			pDlg->setLabelText("Creating the waveforms...");
			pDlg->show();
			QCoreApplication::processEvents();
		}
		CCCoreLib::NormalizedProgress nProgress(pDlg.data(), static_cast<unsigned>(detectedPeaks.size()));
		CCVector3d columnOrigin;
		int previousWaveStart = -1;
		int previousWaveStop = -1;
		size_t waveOffset = 0;
		std::vector<double> waveform;
		//waveform.resize(grid.size().z); //automatically done on the first call to grid.getColumnValues

		for (size_t i = 0; i < detectedPeaks.size(); ++i)
		{
			if (pDlg && !nProgress.oneStep())
			{
				//process cancelled by the user
				return nullptr;
			}

			const PeakInfo& peakInfo = detectedPeaks[i];

			unsigned x = (static_cast<unsigned>(peakInfo.xy) % grid.size().x);
			unsigned y = (static_cast<unsigned>(peakInfo.xy) - x) / grid.size().x;

			if (i == 0 || peakInfo.xy != detectedPeaks[i - 1].xy)
			{
				//new colum: compute the origin
				columnOrigin = grid.computeCellCenter(	Tuple3i(static_cast<int>(x),
																static_cast<int>(y),
																static_cast<int>(grid.size().z - 1)
																)
													);

				//and grab the signal 'values'
				grid.getColumnValues(x, y, waveform, true);

				//reset state
				previousWaveStart = -1;
				previousWaveStop = -1;
				waveOffset = fwfData->size();
			}

			const FWFPeakDetectorInterface::Peak& peak = peakInfo.peak;
			
			//add the peak point
			CCVector3d P = columnOrigin;
			P.z -= peak.x * grid.steps().z;
			CCVector3 PP = CCVector3::fromArray(P.u);
			cloud->addPoint(PP);

			if (intensitySF)
			{
				ScalarType s = static_cast<ScalarType>(peak.y);
				intensitySF->addElement(s);
			}
			if (returnNumberSF)
			{
				ScalarType s = static_cast<ScalarType>(peakInfo.returnNumber);
				returnNumberSF->addElement(s);
			}

			int peakIndex = static_cast<int>(floor(peak.x));
			//just in case
			peakIndex = std::max(peakIndex, 0);
			peakIndex = std::min(peakIndex, static_cast<int>(waveform.size() - 1));

			//data to actually copy and add to the main data block
			int copyStartIndex = -1;
			int copyStopIndex = -1;

			int leftMargin = std::min(peakIndex, waveMargin);
			if (previousWaveStart < 0 || previousWaveStop < peakIndex - leftMargin)
			{
				//start a new 'wave'
				copyStartIndex = peakIndex - leftMargin;
				assert(copyStartIndex >= 0);
				copyStopIndex = copyStartIndex + static_cast<int>(descriptor.numberOfSamples);
				if (copyStopIndex > static_cast<int>(waveform.size()) && copyStartIndex > 0)
				{
					int shift = std::min(copyStartIndex, copyStopIndex - static_cast<int>(waveform.size()));
					copyStartIndex -= shift;
					assert(copyStartIndex >= 0);
					copyStopIndex -= shift;
					assert(copyStopIndex >= 0);
				}

				previousWaveStart = copyStartIndex;
				waveOffset = fwfData->size();
			}
			else
			{
				//we will simply increase the current 'wave' of the necessary quantity
				copyStartIndex = previousWaveStop;
				copyStopIndex = std::max(peakIndex + waveMargin, previousWaveStart + static_cast<int>(descriptor.numberOfSamples));

				//virtually shift the current wave
				int thisWaveStart = copyStopIndex - static_cast<int>(descriptor.numberOfSamples);
				assert(thisWaveStart >= previousWaveStart);
				waveOffset += static_cast<uint64_t>(thisWaveStart - previousWaveStart);
				previousWaveStart = thisWaveStart;
			}

			if (copyStopIndex > copyStartIndex)
			{
				//reserve memory for the FWF signal
				fwfData->reserve(fwfData->size() + static_cast<size_t>(copyStopIndex - copyStartIndex) * bytesPerSample);

				//copy the necessary samples
				for (int i = copyStartIndex; i < copyStopIndex; ++i)
				{
					double dValue = (i < static_cast<int>(waveform.size()) ? waveform[i] : descriptor.digitizerOffset); //we may have to pad with zeros at the end
					qint64 iValue = static_cast<qint64>((dValue - descriptor.digitizerOffset) / descriptor.digitizerGain);
					iValue = std::max<qint64>(0, std::min<qint64>(iValue, maxRawValue));

					switch (descriptor.bitsPerSample)
					{
					case 8:
					{
						uint8_t val8 = static_cast<uint8_t>(iValue);
						fwfData->push_back(val8);
						break;
					}
					case 16:
					{
						uint16_t val16 = static_cast<uint16_t>(iValue);
						const uint8_t* _val8 = reinterpret_cast<uint8_t*>(&val16);
						fwfData->push_back(_val8[0]);
						fwfData->push_back(_val8[1]);
						break;
					}
					case 32:
					{
						uint32_t val32 = static_cast<uint32_t>(iValue);
						const uint8_t* _val8 = reinterpret_cast<uint8_t*>(&val32);
						fwfData->push_back(_val8[0]);
						fwfData->push_back(_val8[1]);
						fwfData->push_back(_val8[2]);
						fwfData->push_back(_val8[3]);
						break;
					}
					default:
						assert(false);
						if (app)
							app->dispToConsole("Internal error: wrong 'bps' setting", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
						return nullptr;
					}
				}
				previousWaveStop = copyStopIndex;
			}

			//add the waveform
			ccWaveform w;
			{
				w.setDescriptorID(1);
				w.setDataDescription(waveOffset, descriptor.numberOfSamples * bytesPerSample);
				w.setBeamDir(CCVector3f(0, 0, grid.steps().z / descriptor.samplingRate_ps));
				assert(peak.x >= static_cast<float>(previousWaveStart));
				w.setEchoTime_ps(static_cast<uint32_t>((peak.x - previousWaveStart) * descriptor.samplingRate_ps));
				w.setReturnIndex(peakInfo.returnNumber);
#ifdef _DEBUG
				CCVector3 P0 = w.getSamplePos(0, PP, descriptor);
				CCVector3d Q0 = columnOrigin;
				Q0.z -= previousWaveStart * grid.steps().z;
				double error = (CCVector3::fromArray(Q0.u) - P0).normd();
				assert(error < 1.0e-3);
#endif
			}
			cloud->waveforms().push_back(w);

			if (false)
			{
				//for debug
				ccWaveformProxy p = cloud->waveformProxy(cloud->size() - 1);
				p.toASCII("waveform_extracted.asc");
			}
		}

		cloud->fwfData() = ccPointCloud::SharedFWFDataContainer(fwfData.release());
	}
	catch (const std::bad_alloc&)
	{
		if (app)
			app->dispToConsole("Not enough memory", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		cloud.reset();
	}

	if (cloud)
	{
		cloud->waveforms().shrink_to_fit();
		cloud->shrinkToFit();

		if (cloud->hasScalarFields())
		{
			for (unsigned i = 0; i < cloud->getNumberOfScalarFields(); ++i)
			{
				cloud->getScalarField(static_cast<int>(i))->computeMinAndMax();
			}
			cloud->setCurrentDisplayedScalarField(0);
			cloud->showSF(true);
		}
	}

	return cloud.release();
}

ccBBox OrthoWFTools::ComputeFWFBox(ccPointCloud* cloud, ccProgressDialog* pDlg/*=0*/)
{
	if (!cloud || !cloud->hasFWF())
	{
		assert(false);
		return ccBBox();
	}

	//update the progress dialog
	CCCoreLib::NormalizedProgress nProgress(pDlg, static_cast<unsigned>(cloud->size()));
	if (pDlg)
	{
		pDlg->setWindowTitle("FWF");
		pDlg->setLabelText("Determine FWF bounding-box...");
		pDlg->show();
		QCoreApplication::processEvents();
	}

	ccBBox fwfBBox;
	for (unsigned pi = 0; pi < cloud->size(); ++pi)
	{
		if (pDlg && !nProgress.oneStep())
		{
			//process cancelled by the user
			return ccBBox();
		}

		const ccWaveformProxy& w = cloud->waveformProxy(pi);
		if (!w.isValid())
		{
			//no valid descriptor
			continue;
		}

		const CCVector3* P = cloud->getPoint(pi);
		//we'll use the first and last points to increase the bounding-box
		CCVector3 Q0 = w.getSamplePos(0, *P);
		CCVector3 Q1 = w.getSamplePos(w.numberOfSamples() - 1, *P);
		fwfBBox.add(Q0);
		fwfBBox.add(Q1);
	}

	return fwfBBox;
}
