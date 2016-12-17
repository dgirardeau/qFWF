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

#ifndef FWF_GRID_HEADER
#define FWF_GRID_HEADER

//CCLib
#include <CCGeom.h>

//system
#include <limits>
#include <vector>
#include <algorithm>

class ccPointCloud;
class QWidget;

//! Raster grid type
struct FWFGrid
{
	//! Default constructor
	FWFGrid()
		: m_steps(1.0, 1.0, 1.0)
		, m_minCorner(0, 0, 0)
		, valid(false)
	{}
	
	//! Destructor
	virtual ~FWFGrid() { clear(); }

	//! Initializes / resets the grid
	bool init(	const Tuple3ui& size,
				const CCVector3d& steps,
				const CCVector3d& minCorner);

	//! Interpolation method
	enum InterpolationMethod
	{
		LINEAR,
		INV_SQUARE,
		INV_EXP
	};

	//! Projects a cloud inside the grid
	bool project(	ccPointCloud* cloud,
					double radius,
					InterpolationMethod interpMethod,
					QWidget* progressDialogParent = 0);

	//! Computes the max (mean) value
	double computeMaxVal() const;
	
	//! Computes the range of (mean) values inside the grid
	bool computeRange(double& minVal, double& maxVal) const;

	//! Clears the grid
	void clear()
	{
		//reset
		m_size = Tuple3ui(0, 0, 0);
		m_data.clear();
		setValid(false);
	}

	//! Sets valid
	inline void setValid(bool state) { valid = state; }
	//! Returns whether the grid is 'valid' or not
	inline bool isValid() const { return valid; }

	//! Computes the position of the cell that includes a given point
	Tuple3i computeCellPos(const CCVector3& P) const
	{
		CCVector3d relativePos = CCVector3d::fromArray(P.u) - m_minCorner;

		//DGM: we use the 'PixelIsArea' convention
		return Tuple3i(	static_cast<int>(relativePos.x / m_steps.x + 0.5),
						static_cast<int>(relativePos.y / m_steps.y + 0.5),
						static_cast<int>(relativePos.z / m_steps.z + 0.5)
					);
	}

	//! Computes the position of the center of a given cell
	CCVector3d computeCellCenter(const Tuple3i& pos) const
	{
		return m_minCorner + CCVector3d((pos.x + 0.5) * m_steps.x,
										(pos.y + 0.5) * m_steps.y,
										(pos.z + 0.5) * m_steps.z);
	}

	//! Returns whether the given voxel position is inside the grid or not
	bool inbounds(const Tuple3i& pos) const
	{
		return	pos.x >= 0
			&&	pos.y >= 0
			&&	pos.z >= 0
			&&	static_cast<unsigned>(pos.x) < m_size.x
			&&	static_cast<unsigned>(pos.y) < m_size.y
			&&	static_cast<unsigned>(pos.z) < m_size.z;
	}

	//! Counts the number of valid cells above a given value
	unsigned countValidCells(double minVal/*=0*/) const;

	//! Returns the values inside a column
	bool getColumnValues(unsigned x, unsigned y, std::vector<double>& values, bool upsideDown = false) const;

	//! Grid cell
	struct Cell
	{
		void add(double value, double weight = 1.0)
		{
			value *= weight;
			valueSum += value;
			squareValueSum += value * value;
			weightSum += weight;
		}
		
		double valueSum = 0;
		double squareValueSum = 0;
		double weightSum = 0;

		double mean() const { return weightSum > 0 ? valueSum / weightSum : std::numeric_limits<double>::quiet_NaN(); }
		double stdDev() const { return weightSum > 0 ? sqrt(std::abs(squareValueSum / weightSum - (valueSum * valueSum) / (weightSum * weightSum))) : std::numeric_limits<double>::quiet_NaN(); }
	};

	//! Grid size
	inline const Tuple3ui& size() const { return m_size; }

	//! Grid steps
	inline const CCVector3d& steps() const { return m_steps; }

	//! Slice (XY)
	typedef std::vector<Cell> Slice;

	//! Returns a given slice
	inline Slice& getSlice(size_t z) { return m_data[z]; }
	//! Returns a given slice (const version)
	inline const Slice& getSlice(size_t z) const { return m_data[z]; }

	//! Grid min corder
	inline const CCVector3d& minCorner() const { return m_minCorner; }

protected:

	//! Grid size
	Tuple3ui m_size;

	//! Grid steps
	CCVector3d m_steps;

	//! Grid min corder
	CCVector3d m_minCorner;

	//! All cells
	std::vector<Slice> m_data;

	//! Whether the grid is valid/up-to-date
	bool valid;
};

#endif //FWF_GRID_HEADER
