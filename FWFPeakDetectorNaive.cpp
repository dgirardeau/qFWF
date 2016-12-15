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

#include "FWFPeakDetectorNaive.h"

//system
#include <assert.h>

bool FWFPeakDetectorNaive::detectPeaks(	std::vector<Peak>& peakPositions,
										const std::vector<double>& values,
										const BaseParameters& baseParameters)
{
	//simply look for the highest peaks
	try
	{
		peakPositions.clear();

		size_t plateauStart = 0;
		for (size_t i = 1; i + 1 < values.size(); ++i)
		{
			if (values[i] < baseParameters.minIntensity)
			{
				continue;
			}

			if (values[i] > values[i - 1])
			{
				if (values[i] > values[i + 1])
				{
					//peak
					peakPositions.push_back(Peak(static_cast<double>(i), values[i]));
					plateauStart = 0;
				}
				else if (values[i] == values[i + 1])
				{
					//start of a (potential) plateau
					plateauStart = i;
				}
				else if (plateauStart != 0)
				{
					//in the end, it was not a plateau
					plateauStart = 0;
				}
			}
			else if (plateauStart != 0 && values[i] < values[i - 1])
			{
				//end of the plateau: we take the middle
				peakPositions.push_back(Peak(static_cast<double>((i + plateauStart) / 2), values[i - 1]));
				plateauStart = 0;
			}
		}

		//now sort the values if there are too many
		if (peakPositions.size() > baseParameters.maxPeakCount)
		{
			std::sort(	peakPositions.begin(),
						peakPositions.end(),
						[](const Peak& a, const Peak& b) -> bool { return a.y > b.y; }
			);

			peakPositions.resize(baseParameters.maxPeakCount);
		}
	}
	catch (const std::bad_alloc&)
	{
		//not enough memory
		return false;
	}

	return true;
}
