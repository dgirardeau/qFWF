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

#ifndef Q_FWF_PEAK_DETECTOR_NAIVE_HEADER
#define Q_FWF_PEAK_DETECTOR_NAIVE_HEADER

#include "FWFPeakDetectorInterface.h"

//! Naive peak detector
class FWFPeakDetectorNaive : public FWFPeakDetectorInterface
{
public:

	//! Returns the detector type (static)
	static QString Type() { return "NAIVE"; }

	//inherited from FWFPeakDetectorInterface
	virtual QString type() const { return Type(); }
	virtual bool detectPeaks(std::vector<Peak>& peaks, const std::vector<double>& values, const BaseParameters& baseParameters) override;

};

#endif //Q_FWF_PEAK_DETECTOR_NAIVE_HEADER
