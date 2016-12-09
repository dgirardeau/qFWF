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

bool FWFPeakDetectorInterface::NaiveDetector(	std::vector<int>& peakPositions,
												const std::vector<double>& values,
												const BaseParameters& baseParameters )
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
					peakPositions.push_back(static_cast<int>(i));
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
				peakPositions.push_back(static_cast<int>((i + plateauStart) / 2));
				plateauStart = 0;
			}
		}

		//now sort the values if there are too many
		if (peakPositions.size() > baseParameters.maxPeakCount)
		{
			std::sort(	peakPositions.begin(),
						peakPositions.end(),
						[&values](const int & a, const int & b) -> bool
						{
							return values[a] > values[b];
						}
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
