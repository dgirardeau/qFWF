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

//Local
#include "FWFPeakDetectorNaive.h"

//Eigen (for LM optimization)
#include <Eigen/Dense>
#include <unsupported/Eigen/NonLinearOptimization>
#include <unsupported/Eigen/NumericalDiff>

//system
#include <assert.h>

static const double s_penaltyK = 1000.0;

static double Penalty(double x)
{
	return s_penaltyK * x * x * x; //k * x^3
}

static double PenaltyDeriv(double x)
{
	return 3 * s_penaltyK * x * x; // 3 * k * x^2
}


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
	double m_muMin, m_muMax;

	LM_GaussianMixtureFunctor(size_t peakCount, const std::vector<double>& values) : m_peakCount(peakCount), m_values(values)
	{
		m_muMin = 1.0;
		m_muMax = static_cast<double>(m_values.size() - 1);
	}

	int inputs() const { return static_cast<int>(m_peakCount * 3); }
	int values() const { return static_cast<int>(m_values.size()); }

	void filterInputValues(double& mu, double& amp, double& sigma) const
	{
		//if (mu < m_muMin)
		//{
		//	mu = exp(mu - m_muMin);
		//}
		//else if (mu >= m_muMax)
		//{
		//	mu = m_muMax + exp(m_muMax - mu);
		//}

		////amp = std::max(amp, 0.0);
		//if (!isfinite(sigma))
		//{
		//	sigma = FWFPeakDetectorInterface::Peak::MinSigma();
		//}
		//else if (sigma < 2 * FWFPeakDetectorInterface::Peak::MinSigma())
		//{
		//	sigma = FWFPeakDetectorInterface::Peak::MinSigma() + exp(sigma / (2 * FWFPeakDetectorInterface::Peak::MinSigma()) - 1.0);
		//}
	}

	double mu_penalty(double mu) const
	{
		if (mu < m_muMin)
		{
			return Penalty(m_muMin - mu);
		}
		else if (mu > m_muMax)
		{
			return Penalty(mu - m_muMax);
		}
		else
		{
			return  0.0;
		}
	}

	double mu_penaltyDeriv(double mu) const
	{
		if (mu < m_muMin)
		{
			return PenaltyDeriv(mu - m_muMin);
		}
		else if (mu > m_muMax)
		{
			return PenaltyDeriv(mu - m_muMax);
		}
		else
		{
			return  0.0;
		}
	}

	double amp_penalty(double amp) const
	{
		if (amp < 0)
		{
			return Penalty(-amp);
		}
		else
		{
			return  0.0;
		}
	}

	double amp_penaltyDeriv(double amp) const
	{
		if (amp < 0)
		{
			return PenaltyDeriv(-amp);
		}
		else
		{
			return  0.0;
		}
	}

	double sigma_penalty(double sigma) const
	{
		if (sigma < FWFPeakDetectorInterface::Peak::MinSigma())
		{
			return Penalty(FWFPeakDetectorInterface::Peak::MinSigma() - sigma);
		}
		else
		{
			return  0.0;
		}
	}

	double sigma_penaltyDeriv(double sigma) const
	{
		if (sigma < FWFPeakDetectorInterface::Peak::MinSigma())
		{
			return PenaltyDeriv(FWFPeakDetectorInterface::Peak::MinSigma() - sigma);
		}
		else
		{
			return  0.0;
		}
	}

	//jacobian
	int df(const Eigen::VectorXd &x, Eigen::MatrixXd &fjac) const
	{
		assert(x.size() == 3 * m_peakCount);

		for (size_t pi = 0; pi < m_peakCount; ++pi)
		{
			double mu    = x(3 * pi    );
			double amp   = x(3 * pi + 1);
			double sigma = x(3 * pi + 2);

			filterInputValues(mu, amp, sigma);

			double sigma2 = sigma * sigma;
			assert(sigma2 > 0);

			for (size_t s = 0; s < m_values.size(); ++s)
			{
				double xc = s - mu;
				double xc2 = xc*xc;
				double exp_u = exp(-xc2 / (2 * sigma2));

				//don't forget the 'minus' sign because we compute 'y - f(x)'
				fjac(s, 3 * pi    ) = - amp * (xc / sigma2) * exp_u; //A * ((x - mu) / sigma^2) * exp(...)
				fjac(s, 3 * pi + 1) = - exp_u; //exp(...)
				fjac(s, 3 * pi + 2) = - (xc2 / (sigma * sigma2)) * amp * exp_u; //A * ((x - mu)^2 / sigma^3) * exp(...)

				fjac(s, 3 * pi    ) -= mu_penaltyDeriv(mu);
				fjac(s, 3 * pi + 1) -= amp_penaltyDeriv(amp);
				fjac(s, 3 * pi + 2) -= sigma_penaltyDeriv(sigma);
			}
		}
		return 0;
	}

	//error evaluation at each step
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

			filterInputValues(mu, amp, sigma);

			double sigma2_2 = 2 * sigma * sigma;
			//assert(sigma2_2 > 0);
			
			for (int s = 0; s < fvec.size(); ++s)
			{
				double x = s - mu;
				double y = amp * exp(- x * x / sigma2_2);
				fvec(s) += y;
				fvec(s) += mu_penalty(mu) + amp_penalty(amp) + sigma_penalty(sigma);
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
	std::vector<Peak> naivePeakPositions;
	if (!FWFPeakDetectorNaive().detectPeaks(naivePeakPositions, smoothValues, baseParameters))
	{
		return false;
	}

	//no candidate peak?!
	if (naivePeakPositions.empty())
	{
		//nothing to do
		return true;
	}

	for (size_t pi = 0; pi < naivePeakPositions.size(); ++pi)
	{
		const Peak& naivePeak = naivePeakPositions[pi];
		Peak peak = naivePeak;

		//(approximate) position of the peak
		int valueCount = static_cast<int>(values.size());
		int index = std::min(static_cast<int>(peak.x), valueCount - 1);
		
		//use the real amplitude
		peak.y = values[index];
		if (peak.y > 0)
		{
			//evaluate the standard deviation on a few samples
			static const int testSize = 5;
			int dMinus = (index >= testSize ? testSize : index);
			if (pi != 0 && index - dMinus <= naivePeakPositions[pi - 1].x)
			{
				//too close from the previous peak
				dMinus /= 2;
			}
			int dPlus = (index + testSize < valueCount - 1 ? testSize : valueCount - 1 - index);
			if (pi + 1 != naivePeakPositions.size() && index + dPlus >= naivePeakPositions[pi + 1].x)
			{
				//too close from the next peak
				dPlus /= 2;
			}
			int d = std::min(dMinus, dPlus);
			if (d > 0)
			{
				double meanYd = (values[index - d] + values[index + d]) / 2;

				//meanYd = Amp * exp(-(x-mu)^2 / (2*sigma^2))
				//meanYd / Amp = exp(-(x-mu)^2 / (2*sigma^2))
				double u = meanYd / peak.y;
				//ln(meanYd / Amp) = -(x-mu)^2 / (2*sigma^2)
				if (u > 0 && u < 1.0)
				{
					double ln_u = log(u);
					//(2*sigma^2) = -(x-mu)^2 / ln(meanYd / Amp)
					//sigma = sqrt(-(x-mu)^2 / (2 *ln(meanYd / Amp))
					peak.sigma = sqrt(-d*d / (2 * ln_u));
				}
				else
				{
					peak.sigma = d / 3.0;
				}
				peak.sigma = std::max(peak.sigma, Peak::MinSigma());
			}
		}
		
		assert(peak.x >= 0);
		peakPositions.push_back(peak);
	}

	//eventually use Levenberg-Marquardt to refine the peaks
	{
		LM_GaussianMixtureFunctor functor(peakPositions.size(), values);
#define USE_JACOBIAN //much faster
#ifdef USE_JACOBIAN
		Eigen::LevenbergMarquardt<LM_GaussianMixtureFunctor> lm(functor);
#else
		typedef Eigen::NumericalDiff<LM_GaussianMixtureFunctor> LM_GaussianMixtureFunctorDiff;
		LM_GaussianMixtureFunctorDiff functorDiff(functor);
		Eigen::LevenbergMarquardt<LM_GaussianMixtureFunctorDiff, double> lm(functorDiff);
#endif
		lm.parameters.maxfev = 100;
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
			double mu    = x(3 * i    );
			double amp   = x(3 * i + 1);
			double sigma = x(3 * i + 2);

			functor.filterInputValues(mu, amp, sigma);
			assert(mu <= static_cast<double>(values.size()));

			peakPositions[i].x =     mu;
			peakPositions[i].y =     amp;
			peakPositions[i].sigma = sigma;
		}
	}

	return true;
}

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
		//assert(sigma2_2 > 0);

		for (size_t s = 0; s < valueCount; ++s)
		{
			double x = static_cast<double>(s)-peak.x;
			double y = peak.y * exp(-x * x / sigma2_2);
			values[s] += y;
		}
	}

	return true;
}
