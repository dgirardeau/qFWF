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

#include "FWFPeakDetectorInterface.h"

bool FWFPeakDetectorInterface::SmoothSignal(	std::vector<double>& values,
												double sigma/*=1.0*/)
{
	size_t kernelSize = static_cast<size_t>(floor(3 * sigma));
	if (kernelSize == 0)
	{
		//nothing to do
		return true;
	}
	
	std::vector<double> originalValues, weights;

	//compute the kernel weights
	{
		//reserve memory
		try
		{
			originalValues = values;
			weights.resize(1 + kernelSize, 1.0);
		}
		catch (const std::bad_alloc&)
		{
			//not enough memory
			return false;
		}

		//compute weights
		double sigma2_2 = 2 * (sigma * sigma);
		for (size_t i = 1; i <= kernelSize; ++i)
		{
			weights[i] = exp(-(static_cast<double>(i) * i) / sigma2_2);
		}
		
		//just in case
		if (weights.back() < 1.0e-6)
		{
			kernelSize--;
			if (kernelSize == 0)
			{
				//nothing to do
				return true;
			}
			weights.pop_back();
		}
	}

	//apply filter
	{
		for (size_t i = 0; i < values.size(); ++i)
		{
			double weightSum = 0;
			double valueSum = 0;
			size_t dMinus = (i >= kernelSize ? kernelSize : i);
			size_t dPlus = (i + kernelSize < values.size() - 1 ? kernelSize : values.size() - 1 - i);
			for (int j = -static_cast<int>(dMinus); j < static_cast<int>(dPlus); ++j)
			{
				double w = weights[std::abs(j)];
				valueSum += w * originalValues[static_cast<int>(i)+j];
				weightSum += w;
			}

			assert(weightSum != 0); //the central value weight is always 1!
			values[i] = valueSum / weightSum;
		}
	}

	return true;
}
