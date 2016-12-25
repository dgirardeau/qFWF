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

#ifndef PEAK_EXTRACTION_TOOLS_HEADER
#define PEAK_EXTRACTION_TOOLS_HEADER

//Local
#include "FWFGrid.h"

//qCC_db
#include <ccWaveform.h>

class ccPointCloud;
class ccMainAppInterface;
class FWFPeakDetectorInterface;

class PeakExtractionTools
{
public:

	//! Extracts new points from a cloud with FWF data
	static ccPointCloud* ExtractNewPeaks(	const ccPointCloud* cloud,
											FWFPeakDetectorInterface* detector,
											int maxReturnCount,
											double minVal = 0,
											ccMainAppInterface* app = 0);
};

#endif //PEAK_EXTRACTION_TOOLS_HEADER
