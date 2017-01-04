#include "FWFGrid.h"

//qCC_db
#include <ccPointCloud.h>
#include <ccProgressDialog.h>

//Qt
#include <QCoreApplication>

bool FWFGrid::init(	const Tuple3ui& size,
					const CCVector3d& steps,
					const CCVector3d& minCorner)
{
	setValid(false);

	m_size = size;
	m_steps = steps;
	m_minCorner = minCorner;

	if (m_size.x == 0 || m_size.y == 0 || m_size.z == 0)
	{
		//invalid size
		return false;
	}

	try
	{
		m_data.resize(m_size.z);

		size_t sliceSize = m_size.x; sliceSize *= m_size.y;

		for (Slice& slice : m_data)
		{
			slice.resize(sliceSize);
		}
	}
	catch (const std::bad_alloc&)
	{
		//not enough memory
		clear();
		return false;
	}

	setValid(true);
	return true;
}

double FWFGrid::computeMaxVal() const
{
	double maxVal = 0;
	for (const Slice& slice : m_data)
	{
		for (const CellType& cell : slice)
		{
			if (cell.weightSum > 0)
			{
				maxVal = std::max<double>(cell.mean(), maxVal);
			}
		}
	}
	return maxVal;
}

unsigned FWFGrid::countValidCells(double minVal/*=0*/) const
{
	unsigned validCellCount = 0;
	{
		for (unsigned z = 0; z < size().z; ++z)
		{
			const Slice& slice = getSlice(z);
			for (const CellType& cell : slice)
			{
				if (cell.weightSum != 0 && cell.mean() >= minVal)
				{
					++validCellCount;
				}
			}
		}
	}

	return validCellCount;
}

//grab the signal 'values'
bool FWFGrid::getColumnValues(unsigned x, unsigned y, std::vector<double>& values, bool upsideDown/*=false*/) const
{
	if (x >= m_size.x || y >= m_size.y)
	{
		assert(false);
		return false;
	}

	if (values.size() != m_size.z)
	{
		try
		{
			values.resize(m_size.z);
		}
		catch (const std::bad_alloc&)
		{
			//not enough memory
			return false;
		}
	}
	
	unsigned posInSlice = x + y * m_size.x;
	for (unsigned z = 0; z < m_size.z; ++z)
	{
		const Slice& slice = getSlice(z);
		const CellType& cell = slice[posInSlice];
		//start from the top!
		values[upsideDown ? m_size.z - 1 - z : z] = cell.weightSum > 0 ? cell.mean() : 0.0;
	}

	return true;
}

bool FWFGrid::computeRange(ValueType& minVal, ValueType& maxVal) const
{
	minVal = maxVal = 0;
	//count the number of valid cells
	bool firstTest = true;
	{
		for (unsigned z = 0; z < m_size.z; ++z)
		{
			const Slice& slice = getSlice(z);
			for (const CellType& cell : slice)
			{
				if (cell.weightSum != 0)
				{
					double val = cell.mean();
					if (firstTest)
					{
						firstTest = false;
						minVal = maxVal = val;
					}
					else
					{
						if (val < minVal)
							minVal = val;
						else if (val > maxVal)
							maxVal = val;
					}
				}
			}
		}
	}

	return !firstTest;
}

bool FWFGrid::project(ccPointCloud* cloud,
						double radius,
						InterpolationMethod interpMethod,
						QWidget* progressDialogParent/*=0*/)
{
	if (!cloud || !cloud->hasFWF())
	{
		assert(false);
		return false;
	}

	//progress dialog
	QScopedPointer<ccProgressDialog> pDlg(0);
	if (progressDialogParent)
	{
		pDlg.reset(new ccProgressDialog(true, progressDialogParent));
		pDlg->setWindowTitle("FWF");
		pDlg->setLabelText("Projecting the waveforms...");
		pDlg->show();
		QCoreApplication::processEvents();
	}
	CCLib::NormalizedProgress nProgress(pDlg.data(), static_cast<unsigned>(cloud->size()));

	const ccPointCloud::FWFDescriptorSet& descriptors = cloud->fwfDescriptors();

	double squareRadius = radius * radius;
	double sigma = radius / 3.0;
	double expFactor = -1.0 / (2.0 * sigma * sigma); // = - 9/2 * squareRadius

	assert(m_steps.x == m_steps.y);
	int kernelSize = static_cast<int>(ceil(radius / m_steps.x));

	for (unsigned pi = 0; pi < cloud->size(); ++pi)
	{
		if (!nProgress.oneStep())
		{
			//process cancelled by the user
			return false;
		}

		const ccWaveformProxy& w = cloud->waveformProxy(pi);
		if (!w.isValid())
		{
			//no valid descriptor
			continue;
		}

		const CCVector3* P = cloud->getPoint(pi);

		for (uint32_t si = 0; si < w.numberOfSamples(); ++si)
		{
			double amplitude = w.getSample(si);
			if (amplitude == 0)
			{
				continue;
			}

			CCVector3 Q = w.getSamplePos(si, *P);

			//compute the eqiuvalent position in the grid
			Tuple3i voxelPos = computeCellPos(Q);
			if (!inbounds(voxelPos))
			{
				assert(false);
				continue;
			}

			//get the right slice
			FWFGrid::Slice& slice = getSlice(voxelPos.z);

			//now add the contribution of this point to its neighbors
			int minX = std::max(0, voxelPos.x - kernelSize);
			int maxX = std::min(static_cast<int>(m_size.x) - 1, voxelPos.x + kernelSize);
			int minY = std::max(0, voxelPos.y - kernelSize);
			int maxY = std::min(static_cast<int>(m_size.y) - 1, voxelPos.y + kernelSize);
			for (int i = minX; i <= maxX; ++i)
			{
				double dx = Q.x - (m_minCorner.x + (i + 0.5) * m_steps.x);
				double dx2 = dx * dx;
				for (int j = minY; j <= maxY; ++j)
				{
					double dy = Q.y - (m_minCorner.y + (j + 0.5) * m_steps.y);
					double r2 = dx2 + dy*dy;
					if (r2 <= squareRadius)
					{
						CellType& cell = slice[j * m_size.x + i];

						double weight = 1.0;
						switch (interpMethod)
						{
						case LINEAR:
							weight = (radius - sqrt(r2)) / radius; //weight proportional to the distance to the center
							break;
						case INV_SQUARE:
							weight = (squareRadius - r2) / squareRadius; //weight proportional to the squared distance to the center
							break;
						case INV_EXP:
							weight = exp(expFactor * r2); //weight proportional to the exponential of the inversed distance to the center
							break;
						default:
							assert(false);
						}

						cell.add(static_cast<ValueType>(amplitude), static_cast<ValueType>(weight));
					}
				}
			}
		}
	}

	return true;
}