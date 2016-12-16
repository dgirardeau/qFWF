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

//qCC_db
#include <ccPointCloud.h>
#include <ccScalarField.h>
#include <ccProgressDialog.h>

//Qt
#include <QCoreApplication>
#include <QMainWindow>

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
		ccScalarField* amplitudes = new ccScalarField("Mean amplitude");
		ccScalarField* stdDev = new ccScalarField("Amplitude std. dev.");

		cloud->addScalarField(amplitudes);
		cloud->addScalarField(stdDev);

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
			const FWFGrid::Cell* _cell = &(slice[0]);

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
						stdDev->addElement(_cell->stdDev());
					}
				}
			}
		}

		amplitudes->computeMinAndMax();
		stdDev->computeMinAndMax();

		cloud->setCurrentDisplayedScalarField(0);
		cloud->showSF(true);
	}

	return cloud;
}

ccPointCloud* OrthoWFTools::ConvertGridToFWFCloud(	const FWFGrid& grid,
													FWFPeakDetectorInterface* detector,
													int maxReturnCount,
													const WaveformDescriptor& descriptor,
													double minVal/*=0*/,
													ccMainAppInterface* app/*=0*/)
{
	if (!detector)
	{
		if (app)
			app->dispToConsole(QString("Internal error: invalid input detector").arg(minVal), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		assert(false);
		return 0;
	}
	
	//count the number of valid cells
	unsigned validCellCount = grid.countValidCells(minVal);
	if (validCellCount == 0)
	{
		if (app)
			app->dispToConsole(QString("Grid has no cell with a value above %1?!").arg(minVal), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return 0;
	}

	static const unsigned s_cloudBaseIncrement = 1024;
	QScopedPointer<ccPointCloud> cloud(new ccPointCloud("Grid2FWF"));
	ccScalarField* intensitySF = new ccScalarField("Intensity");
	cloud->addScalarField(intensitySF);
	if (	!cloud->reserve(s_cloudBaseIncrement)
		||	!cloud->reserveTheFWFTable())
	{
		if (app)
			app->dispToConsole("Not enough memory", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return 0;
	}
		
	//now for the conversion process
	try
	{
		//we'll generate synthetic waveforms from the grid 'columns'
		std::vector<double> waveform;
		waveform.resize(grid.size().z);

		//FWF data block
		QScopedPointer<ccPointCloud::FWFDataContainer> fwfData(new ccPointCloud::FWFDataContainer);

		//the descriptor has already been specified by the caller
		cloud->fwfDescriptors().insert(1, descriptor);

		FWFPeakDetectorInterface::BaseParameters detectorParams;
		detectorParams.maxPeakCount = static_cast<unsigned>(std::max(1, maxReturnCount));
		detectorParams.minIntensity = minVal;

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

		unsigned sliceSize = grid.size().x * grid.size().y;

		//progress dialog
		ccProgressDialog pDlg(app ? app->getMainWindow() : 0);
		CCLib::NormalizedProgress nProgress(&pDlg, sliceSize);
		pDlg.setWindowTitle("FWF");
		pDlg.setLabelText("Creating the waveforms...");
		pDlg.show();
		QCoreApplication::processEvents();

//#if defined(_OPENMP)
//		bool error = false;
//#pragma omp parallel for
//		for (int xy = 0; xy < static_cast<int>(sliceSize); ++xy)
//		{
//			if (error)
//			{
//				continue;
//			}
//			unsigned x = (static_cast<unsigned>(xy) % grid.size().x);
//			unsigned y = (static_cast<unsigned>(xy)  - x) / grid.size().x;
//			{
//#else
		for (unsigned y = 0; y < grid.size().y; ++y)
		{
			for (unsigned x = 0; x < grid.size().x; ++x)
			{
//#endif
				if (!nProgress.oneStep())
				{
					//process cancelled by the user
//#if defined(_OPENMP)
//					error = true;
//					continue;
//#else
					return 0;
//#endif
				}
				
				//grab the signal 'values'
				{
					unsigned posInSlice = x + y * grid.size().x;
					for (unsigned z = 0; z < grid.size().z; ++z)
					{
						const FWFGrid::Slice& slice = grid.getSlice(z);
						const FWFGrid::Cell& cell = slice[posInSlice];
						//start from the top!
						waveform[grid.size().z - 1 - z] = cell.weightSum > 0 ? cell.mean() : 0.0;
					}
				}
				
				std::vector<FWFPeakDetectorInterface::Peak> peaks;
				if (detector->detectPeaks(peaks, waveform, detectorParams) && !peaks.empty())
				{
					//if we have detected some peaks
					CCVector3d columnOrigin = grid.computeCellCenter(Tuple3i(	static_cast<int>(x),
																				static_cast<int>(y),
																				static_cast<int>(grid.size().z - 1)
																			)
																	);

					//sort the peaks from the first to the last one
					std::sort(	peaks.begin(),
								peaks.end(),
								[](const FWFPeakDetectorInterface::Peak& a, const FWFPeakDetectorInterface::Peak& b) -> bool { return a.x < b.x; }
					);

					//reserve memory for the next points
					if (cloud->size() + peaks.size() >= cloud->capacity())
					{
						if (!cloud->reserve(cloud->size() + s_cloudBaseIncrement))
						{
							if (app)
								app->dispToConsole("Not enough memory", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
//#if defined(_OPENMP)
//							error = true;
//							continue;
//#else
							return 0;
//#endif
						}
						cloud->waveforms().reserve(cloud->size() + s_cloudBaseIncrement);
					}
					//reserve memory for the FWF signal
					fwfData->reserve(fwfData->size() + waveform.size() * bytesPerSample);

					int previousWaveStart = -1;
					int previousWaveStop = -1;
					size_t waveOffset = fwfData->size();

					//add the corresponding points
					for (const FWFPeakDetectorInterface::Peak& peak : peaks)
					{
						//add the peak point
						CCVector3d P = columnOrigin;
						P.z -= peak.x * grid.steps().z;
						CCVector3 PP = CCVector3::fromArray(P.u);
						cloud->addPoint(PP);

						intensitySF->addElement(peak.y);

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
//#if defined(_OPENMP)
//								error = true;
//								i = copyStopIndex;
//								break;
//#else
								return 0;
//#endif
							}
						}
						previousWaveStop = copyStopIndex;

						//add the waveform
						ccWaveform w;
						{
							w.setDescriptorID(0);
							w.setDataDescription(waveOffset, descriptor.numberOfSamples * bytesPerSample);
							w.setBeamDir(CCVector3f(0, 0, grid.steps().z / descriptor.samplingRate_ps));
							assert(peak.x >= static_cast<float>(previousWaveStart));
							w.setEchoTime_ps(static_cast<uint32_t>((peak.x - previousWaveStart) * descriptor.samplingRate_ps));
#ifdef _DEBUG
							CCVector3 P0 = w.getSamplePos(0, PP, descriptor);
							CCVector3d Q0 = columnOrigin;
							Q0.z -= previousWaveStart * grid.steps().z;
							double error = (CCVector3::fromArray(Q0.u) - P0).normd();
							assert(error < 1.0e-4);
#endif

						}
						cloud->waveforms().push_back(w);
					}
				}
			}
		}

		cloud->fwfData() = ccPointCloud::SharedFWFDataContainer(fwfData.take());
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

		intensitySF->computeMinAndMax();
		cloud->setCurrentDisplayedScalarField(0);
		cloud->showSF(true);
	}

	return cloud.take();
}
