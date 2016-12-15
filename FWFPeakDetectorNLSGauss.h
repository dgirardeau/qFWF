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

#ifndef Q_FWF_PEAK_DETECTOR_NLS_GAUSS_HEADER
#define Q_FWF_PEAK_DETECTOR_NLS_GAUSS_HEADER

#include "FWFPeakDetectorInterface.h"

//! Peak detector based on Non-linear Least Squares (Levenberg-Marquardt) fit of a Gaussian mixture
class FWFPeakDetectorNLSGauss : public FWFPeakDetectorInterface
{
public:

	//! Returns the detector type (static)
	static QString Type() { return "NLS_GAUSS"; }

	//inherited from FWFPeakDetectorInterface
	virtual QString type() const { return Type(); }
	virtual bool detectPeaks(std::vector<Peak>& peaks, const std::vector<double>& values, const BaseParameters& baseParameters) override;

	//!¨Parameters
	struct SpecificParameters
	{
		//! Initial smoothing step (optional)
		double smoothWaveformSigma = 0; //0 = deactivated
	};

	//! Sets parameters
	void setSpecificParameters(const SpecificParameters& params) { m_specificParameters = params; }
	//! Returns parameters
	const SpecificParameters& getSpecificParameters() const { return m_specificParameters; }

	//! Generates a synthetic waveform based on the detected peaks
	static bool GenerateSyntheticWaveform(const std::vector<Peak>& peaks, size_t valueCount, std::vector<double>& outputValues);

protected:

	//! Parameters
	SpecificParameters m_specificParameters;
};

#endif //Q_FWF_PEAK_DETECTOR_NLS_GAUSS_HEADER
