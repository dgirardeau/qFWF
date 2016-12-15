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

#ifndef ORTHO_WF_TOOLS_HEADER
#define ORTHO_WF_TOOLS_HEADER

//Local
#include "FWFGrid.h"

//qCC_db
#include <ccWaveform.h>

class ccPointCloud;
class ccMainAppInterface;
class FWFPeakDetectorInterface;

class OrthoWFTools
{
public:

	//! Converts a grid to a point cloud
	static ccPointCloud* ConvertGridToGridCloud(const FWFGrid& grid,
												double minVal = 0,
												ccMainAppInterface* app = 0);

	//! Converts a grid to a point cloud
	static ccPointCloud* ConvertGridToFWFCloud(	const FWFGrid& grid,
												FWFPeakDetectorInterface* detector,
												int maxReturnCount,
												const WaveformDescriptor& descriptor,
												double minVal = 0,
												ccMainAppInterface* app = 0);

};

#endif //ORTHO_WF_TOOLS_HEADER
