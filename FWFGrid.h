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

	//! Clears the grid
	void clear();

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
						static_cast<int>(relativePos.y / m_steps.z + 0.5)
					);
	}

	//! Computes the position of the center of a given cell
	CCVector3d computeCellCenter(const Tuple3i& pos) const
	{
		return m_minCorner + CCVector3d((pos.x + 0.5) * m_steps.x,
										(pos.y + 0.5) * m_steps.y,
										(pos.z + 0.5) * m_steps.z);
	}

	//! Grid cell
	struct Cell
	{
		double valueSum = 0;
		double squareValueSum = 0;
		double weightSum = 0;

		double mean() const { return weightSum > 0 ? valueSum / weightSum : std::numeric_limits<double>::quiet_NaN(); }
		double stdDev() const { return weightSum > 0 ? sqrt(std::abs(squareValueSum / weightSum - (valueSum * valueSum) / (weightSum * weightSum))) : std::numeric_limits<double>::quiet_NaN(); }
	};

protected:

	//! Grid size
	Tuple3ui m_size;

	//! Grid steps
	CCVector3d m_steps;

	//! Grid min corder
	CCVector3d m_minCorner;

	//! Slice (XY)
	typedef std::vector<Cell> Slice;

	//! All cells
	std::vector<Slice> data;

	//! Whether the grid is valid/up-to-date
	bool valid;
};

#endif //FWF_GRID_HEADER
