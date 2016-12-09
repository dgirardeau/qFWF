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

#include "FWFPeakDetectorNLSGauss.h"

//Eigen (for LM optimization)
#include <Eigen/Dense>
#include <unsupported/Eigen/NonLinearOptimization>
#include <unsupported/Eigen/NumericalDiff>

//system
#include <assert.h>

static const double s_minSigma = 0.16; //peak width can't be less than one 'pixel', i.e. +/-0.5 => 3 * sigma >= 0.5 => sigma > 0.16

// Generic functor
struct LM_GaussianMixtureFunctor
{
	typedef double Scalar;

	typedef Eigen::VectorXd InputType;
	typedef Eigen::VectorXd ValueType;
	typedef Eigen::MatrixXd JacobianType;
	enum {	InputsAtCompileTime = Eigen::Dynamic,
			ValuesAtCompileTime = Eigen::Dynamic };

	size_t m_peakCount;
	const std::vector<double>& m_values;

	LM_GaussianMixtureFunctor(size_t peakCount, const std::vector<double>& values) : m_peakCount(peakCount), m_values(values) {}

	int inputs() const { return static_cast<int>(m_peakCount * 3); }
	int values() const { return static_cast<int>(m_values.size()); }

	int operator()(const Eigen::VectorXd &x, Eigen::VectorXd &fvec) const
	{
		assert(x.size() == 3 * m_peakCount);
		assert(fvec.size() == m_values.size());

		fvec.setZero();
		
		//sum the gaussians for each sample
		for (size_t pi = 0; pi < m_peakCount; ++pi)
		{
			double mu    = x(3 * pi    );
			double amp   = x(3 * pi + 1);
			double sigma = x(3 * pi + 2);

			mu    = std::max(mu   , 0.0);
			amp   = std::max(amp  , 0.0);
			sigma = std::max(sigma, s_minSigma);

			double sigma2_2 = 2 * sigma * sigma;
			assert(sigma2_2 > 0);
			
			for (int s = 0; s < fvec.size(); ++s)
			{
				double x = s - mu;
				double y = amp * exp(- x * x / sigma2_2);
				fvec(s) += y;
			}
		}

		//now compute the error
		for (int s = 0; s < fvec.size(); ++s)
		{
			fvec(s) = m_values[s] - fvec(s);
		}

		return 0;
	}
};

bool FWFPeakDetectorNLSGauss::GenerateSyntheticWaveform(const std::vector<Peak>& peaks, size_t valueCount, std::vector<double>& values)
{
	try
	{
		values.resize(valueCount, 0);
	}
	catch (const std::bad_alloc&)
	{
		//not enough memory
		return false;
	}

	//sum the gaussians for each sample
	for (const Peak& peak : peaks)
	{
		if (peak.sigma == 0)
		{
			//invalid peak
			assert(false);
			continue;
		}
		
		double sigma2_2 = 2 * peak.sigma * peak.sigma;
		assert(sigma2_2 > 0);

		for (size_t s = 0; s < valueCount; ++s)
		{
			double x = static_cast<double>(s) - peak.x;
			double y = peak.y * exp(-x * x / sigma2_2);
			values[s] += y;
		}
	}

	return true;
}

bool FWFPeakDetectorNLSGauss::detectPeaks(	std::vector<Peak>& peakPositions,
											const std::vector<double>& values,
											const BaseParameters& baseParameters)
{
	//just in case
	peakPositions.clear();

	//check parameters
	if (	baseParameters.maxPeakCount < 1
		||	m_specificParameters.smoothWaveformSigma < 0)
	{
		//invalid parameters
		assert(false);
		return false;
	}

	//apply a gaussian filter first if necessary
	std::vector<double> smoothValues;
	try
	{
		smoothValues = values;
	}
	catch (const std::bad_alloc&)
	{
		//not enough memory
		return false;
	}
	if (m_specificParameters.smoothWaveformSigma > 0)
	{
		if (!SmoothSignal(smoothValues, m_specificParameters.smoothWaveformSigma))
		{
			return false;
		}
	}

	//now detect the peaks in a naive way
	std::vector<int> naivePeakPositions;
	if (!NaiveDetector(naivePeakPositions, smoothValues, baseParameters))
	{
		return false;
	}

	//no candidate peak?!
	if (naivePeakPositions.empty())
	{
		//nothing to do
		return true;
	}

	for (int index : naivePeakPositions)
	{
		Peak peak(static_cast<double>(index), values[index], s_minSigma);
		
		if (peak.y > 0)
		{
			//evaluate the standard deviation on a few samples
			static const int testSize = 5;
			int valueCount = static_cast<int>(values.size());
			int dMinus = (index >= testSize ? testSize : index);
			int dPlus = (index + testSize < valueCount - 1 ? testSize : valueCount - 1 - index);
			int d = std::min(dMinus, dPlus);
			if (d > 0)
			{
				double meanYd = (values[index - d] + values[index + d]) / 2;

				//meanYd = Amp * exp(-(x-mu)^2 / (2*sigma^2))
				//meanYd / Amp = exp(-(x-mu)^2 / (2*sigma^2))
				double u = meanYd / peak.y;
				//ln(meanYd / Amp) = -(x-mu)^2 / (2*sigma^2)
				double ln_u = log(u);
				//(2*sigma^2) = -(x-mu)^2 / ln(meanYd / Amp)
				//sigma = sqrt(-(x-mu)^2 / (2 *ln(meanYd / Amp))
				peak.sigma = sqrt(-d*d / (2*ln_u));
			}
		}
		
		peakPositions.push_back(peak);
	}

	//eventually use Levenberg-Marquardt to refine the peaks
	{
		LM_GaussianMixtureFunctor functor(peakPositions.size(), values);
		Eigen::NumericalDiff<LM_GaussianMixtureFunctor> numDiff(functor);
		Eigen::LevenbergMarquardt<Eigen::NumericalDiff<LM_GaussianMixtureFunctor>, double> lm(numDiff);
		lm.parameters.maxfev = 2000;
		lm.parameters.xtol = 1.0e-10;

		Eigen::VectorXd x(peakPositions.size() * 3);
		for (size_t i = 0; i < peakPositions.size(); ++i)
		{
			x(3 * i    ) = peakPositions[i].x;
			x(3 * i + 1) = peakPositions[i].y;
			x(3 * i + 2) = peakPositions[i].sigma;
		}

		int ret = lm.minimize(x);

		//update the Gaussians
		for (size_t i = 0; i < peakPositions.size(); ++i)
		{
			peakPositions[i].x     = x(3 * i    );
			peakPositions[i].y     = x(3 * i + 1);
			peakPositions[i].sigma = x(3 * i + 2);
		}
	}

	return true;
}
