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
	/** \warning The 'x', coordinate and 'sigma' (width) of the peak are expressed in terms of sample 'index' (not in picoseconds, etc.)
	**/
	struct Peak
	{
		//! Default constructor
		Peak(double _x = 0.0, double _y = 0.0, double _s = MinSigma()) : x(_x), y(_y), sigma(_s) {}
		
		//! Peak position (in terms of index)
		double x;
		//! Peak height (amplitude)
		double y;
		//! Peak 'sigma' (e.g. 1/3rd of the 'width' for Gaussian pulses)
		double sigma;

		//! Returns the minimum theoretical peak 'sigma' (width)
		static double MinSigma()
		{
			//peak width can't be less than one 'pixel', i.e. +/-0.5 => 3 * sigma >= 0.5 => sigma > 0.16
			return 0.16;
		}
	};

	//! Detect the peaks
	virtual bool detectPeaks(	std::vector<Peak>& peaks,
								const std::vector<double>& values,
								const BaseParameters& baseParameters) = 0;

public: //helpers

	//! Smooth a signal
	/** Gaussian filter (+/- 3*sigma, sigma being expressed in number of samples)
	**/
	static bool SmoothSignal(	std::vector<double>& values,
								double sigma = 1.0);

};

#endif //Q_FWF_PEAK_DETECTOR_INTERFACE_HEADER
