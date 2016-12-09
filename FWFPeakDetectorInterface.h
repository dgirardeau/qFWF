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

#ifndef Q_FWF_PEAK_DETECTOR_INTERFACE_HEADER
#define Q_FWF_PEAK_DETECTOR_INTERFACE_HEADER

//qCC_db
#include <ccWaveform.h>

//Qt
#include <QString>

//System
#include <vector>

//! Peak detector interface
class FWFPeakDetectorInterface
{
public:

	//! Detector type
	virtual QString type() const = 0;

	//! Common parameters (to all detectors)
	struct BaseParameters
	{
		unsigned maxPeakCount = 16;
		double minIntensity = 0.0;
	};

	//! Peak
	struct Peak
	{
		Peak(double _x = 0.0, double _y = 0.0, double _s = 0.0) : x(_x), y(_y), sigma(_s) {}
		double x; //warning: position in terms of 'index'
		double y;
		double sigma;
	};

	//! Detect the peaks
	virtual bool detectPeaks(	std::vector<Peak>& peaks,
								const std::vector<double>& values,
								const BaseParameters& baseParameters) = 0;

	//! Smooth a signal
	/** Gaussian filter (+/- 3*sigma, sigma being expressed in number of samples)
	**/
	static bool SmoothSignal(	std::vector<double>& values,
								double sigma = 1.0);

	//! Naive peak detector (highest local peaks)
	static bool NaiveDetector(	std::vector<int>& peakPositions,
								const std::vector<double>& values,
								const BaseParameters& baseParameters );
};

#endif //Q_FWF_PEAK_DETECTOR_INTERFACE_HEADER
