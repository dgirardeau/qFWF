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

#include "ComputeOrthoWFDialog.h"

//local
#include "OrthoWFTools.h"
#include "FWFPeakDetectorNaive.h"
#include "FWFPeakDetectorNLSGauss.h"
#include "PeakDetectionDialog.h"

//qCC
#include <ccBoundingBoxEditorDlg.h>
#include <ccMainAppInterface.h>

//qCC_db
#include <ccPointCloud.h>
#include <ccProgressDialog.h>
#include <ccScalarField.h>

//Qt
#include <QSettings>
#include <QMainWindow>

ComputeOrthoWFDialog::ComputeOrthoWFDialog(ccMainAppInterface* app, QWidget* parent/*=0*/)
	: QDialog(parent)
	, Ui::ComputeOrthoWFDialog()
	, m_app(app)
	, m_cloud(0)
	, m_bbEditorDlg(0)
{
	setupUi(this);
	bool showBoxAxes = true;
	bool showRasterGridImage = true;
	m_bbEditorDlg = new ccBoundingBoxEditorDlg(showBoxAxes, showRasterGridImage, this);

	connect(hGridStepDoubleSpinBox,	SIGNAL(valueChanged(double)), this, SLOT(updateGridSize()));
	connect(vGridStepDoubleSpinBox,	SIGNAL(valueChanged(double)), this, SLOT(updateGridSize()));
	connect(editBoxToolButton, SIGNAL(clicked()), this, SLOT(editBox()));
	connect(computePushButton, SIGNAL(clicked()), this, SLOT(computeGrid()));
	connect(buttonBox, &QDialogButtonBox::accepted, [&]() { saveSettings(); accept(); });

	connect(hGridStepDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(invalidateGrid()));
	connect(vGridStepDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(invalidateGrid()));
	connect(radiusDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(invalidateGrid()));
	connect(interpMethodComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(invalidateGrid()));

	connect(exportCloudPushButton, SIGNAL(clicked()), this, SLOT(exportToFWFCloud()));
	connect(exportGridPushButton, SIGNAL(clicked()), this, SLOT(exportToGridCloud()));

	setCurrentGridValidity(false);
}

void ComputeOrthoWFDialog::loadSettings()
{
	QSettings settings;
	settings.beginGroup("FWF_plugin");
	settings.beginGroup("ComputeOrthoWFDialog");
	{
		double hGridStep = settings.value("hGridStep", hGridStepDoubleSpinBox->value()).toDouble();
		double vGridStep = settings.value("vGridStep", vGridStepDoubleSpinBox->value()).toDouble();
		double radius = settings.value("radius", radiusDoubleSpinBox->value()).toDouble();
		int interpMethod = settings.value("interpMethod", interpMethodComboBox->currentIndex()).toInt();

		hGridStepDoubleSpinBox->setValue(hGridStep);
		vGridStepDoubleSpinBox->setValue(vGridStep);
		radiusDoubleSpinBox->setValue(radius);
		interpMethodComboBox->setCurrentIndex(interpMethod);
	}
	settings.endGroup();
	settings.endGroup();
}

void ComputeOrthoWFDialog::saveSettings() const
{
	QSettings settings;
	settings.beginGroup("FWF_plugin");
	settings.beginGroup("ComputeOrthoWFDialog");
	{
		settings.setValue("hGridStep", hGridStepDoubleSpinBox->value());
		settings.setValue("vGridStep", vGridStepDoubleSpinBox->value());
		settings.setValue("radius", radiusDoubleSpinBox->value());
		settings.setValue("interpMethod", interpMethodComboBox->currentIndex());
	}
	settings.endGroup();
	settings.endGroup();
}

bool ComputeOrthoWFDialog::setCloud(ccPointCloud* cloud, ccProgressDialog* pDlg/*=0*/)
{
	m_cloud = cloud;
	m_bbEditorDlg->setBaseBBox(ccBBox());
	buttonFrame->setEnabled(false);

	if (!m_cloud)
	{
		gridGroupBox->setEnabled(false);
		interpGroupBox->setEnabled(false);
		return false;
	}

	if (!m_bbEditorDlg)
	{
		assert(false);
		return false;
	}

	//determine the overall FWF bounding box
	{
		ccBBox fwfBBox = OrthoWFTools::ComputeFWFBox(m_cloud, pDlg);
		if (!fwfBBox.isValid())
		{
			//process cancelled by the user
			m_cloud = 0;
			return false;
		}

		m_bbEditorDlg->setBaseBBox(fwfBBox, false);
	}

	gridGroupBox->setEnabled(true);
	interpGroupBox->setEnabled(true);

	loadSettings();
	updateGridSize();

	return true;
}

void ComputeOrthoWFDialog::editBox()
{
	if (m_bbEditorDlg && m_bbEditorDlg->exec())
	{
		updateGridSize();
		setCurrentGridValidity(false);
	}
}

bool ComputeOrthoWFDialog::computeGridSize(Tuple3ui& size) const
{
	if (!m_bbEditorDlg || !m_bbEditorDlg->getBox().isValid())
	{
		assert(false);
		return false;
	}
	const ccBBox& box = m_bbEditorDlg->getBox();

	double hGridStep = hGridStepDoubleSpinBox->value();
	double vGridStep = vGridStepDoubleSpinBox->value();

	size = OrthoWFTools::ComputeGridSize(box, hGridStep, vGridStep);

	return (size.x + size.y + size.z) != 0;
}

void ComputeOrthoWFDialog::updateGridSize()
{
	Tuple3ui gridSize;
	if (!computeGridSize(gridSize))
	{
		gridWidthLabel->setText("invalid");
		computePushButton->setEnabled(false);
	}
	else
	{
		uint64_t totalGridSize_bytes = static_cast<uint64_t>(gridSize.x) * static_cast<uint64_t>(gridSize.y) * static_cast<uint64_t>(gridSize.z) * sizeof(FWFGrid::CellType);
		gridWidthLabel->setText(QString("%1 x %2 x %3 (%4 Gb)").arg(gridSize.x).arg(gridSize.y).arg(gridSize.z).arg(totalGridSize_bytes / static_cast<double>(1 << 30), 0, 'f', 2));
		computePushButton->setEnabled(true);
	}
}

FWFGrid::InterpolationMethod ComputeOrthoWFDialog::getInterpolationMethod() const
{
	switch (interpMethodComboBox->currentIndex())
	{
	case 0:
		return FWFGrid::LINEAR;
	case 1:
		return FWFGrid::INV_SQUARE;
	case 2:
		return FWFGrid::INV_EXP;
	}

	assert(false);
	return FWFGrid::LINEAR;
}

void ComputeOrthoWFDialog::setCurrentGridValidity(bool state)
{
	if (state)
	{
		assert(m_grid);
	}
	else
	{
		m_grid.clear();
	}

	buttonFrame->setEnabled(state);
}

void ComputeOrthoWFDialog::computeGrid()
{
	if (!m_cloud)
	{
		assert(false);
		return;
	}

	Tuple3ui gridSize;
	if (!computeGridSize(gridSize))
	{
		assert(false);
		return;
	}
	assert(m_bbEditorDlg && m_bbEditorDlg->getBox().isValid());

	gridInfoLabel->setText("Initializing the grid");

	if (m_grid)
	{
		m_grid.clear();
	}
	setCurrentGridValidity(false);

	//create a new grid
	m_grid.reset(new FWFGrid);

	CCVector3d steps(	hGridStepDoubleSpinBox->value(),
						hGridStepDoubleSpinBox->value(),
						vGridStepDoubleSpinBox->value() );

	const ccBBox& box = m_bbEditorDlg->getBox();

	CCVector3d minCorner = CCVector3d::fromArray(box.minCorner().u);

	//init the grid
	if (!m_grid->init(gridSize, steps, minCorner))
	{
		if (m_app)
		{
			m_app->dispToConsole("Failed to initialize the grid", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			gridInfoLabel->setText("Failed to initialize the grid");

		return;
		}
	}

	gridInfoLabel->setText("Computing grid");

	//get parameters
	double radius = radiusDoubleSpinBox->value();
	FWFGrid::InterpolationMethod interpMethod = getInterpolationMethod();

	//now fill it
	if (!m_grid->project(m_cloud, radius, interpMethod, this))
	{
		m_grid.clear();
		return;
	}

	double maxVal = m_grid->computeMaxVal();
	minAmpDoubleSpinBox->setMaximum(maxVal);
	gridInfoLabel->setText(QString("Grid computed (max amplitude: %1)").arg(maxVal));

	setCurrentGridValidity(true);
}

void ComputeOrthoWFDialog::exportToGridCloud()
{
	if (!m_app || !m_grid)
	{
		assert(false);
		return;
	}

	ccPointCloud* cloud = OrthoWFTools::ConvertGridToGridCloud(*m_grid, minAmpDoubleSpinBox->value(), m_app);
	if (cloud)
	{
		cloud->setGlobalShift(m_cloud->getGlobalShift());
		cloud->setGlobalScale(m_cloud->getGlobalScale());
		cloud->setMetaData(m_cloud->metaData(), true);

		cloud->setName(m_cloud->getName() + ".fwf_grid");
		if (m_cloud->getParent())
		{
			m_cloud->getParent()->addChild(cloud);
		}
		m_app->addToDB(cloud);
	}
}

void ComputeOrthoWFDialog::exportToFWFCloud()
{
	if (!m_app || !m_grid)
	{
		assert(false);
		return;
	}

	FWFGrid::ValueType rangeMin = 0, rangeMax = 0;
	if (!m_grid->computeRange(rangeMin, rangeMax))
	{
		if (m_app)
			m_app->dispToConsole("Grid has no valid cell?!", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	//mean peak value
	static double s_minPeakVal = rangeMax + 1.0;
	if (s_minPeakVal > rangeMax)
	{
		s_minPeakVal = rangeMin + (rangeMax - rangeMin) / 5;
	}
	static int s_peakDetectorIndex = 1;
	static int s_maxReturnCount = 2;
	static int s_bpsMode = 1;
	static int s_numberOfSamples = 64;

	PeakDetectionDialog pdDlg(this);

	//load parameters
	pdDlg.minAmplitudeDoubleSpinBox->setRange(rangeMin, rangeMax);
	pdDlg.minAmplitudeDoubleSpinBox->setToolTip(QString("Range: [%1 ; %2]").arg(rangeMin).arg(rangeMax));
	pdDlg.minAmplitudeDoubleSpinBox->setValue(s_minPeakVal);
	pdDlg.echoDetectorComboBox->addItem("Naive");
	pdDlg.echoDetectorComboBox->addItem("NLS_Gauss");
	pdDlg.echoDetectorComboBox->setCurrentIndex(s_peakDetectorIndex);
	pdDlg.maxReturnCountSpinBox->setValue(s_maxReturnCount);
	pdDlg.bpsComboBox->setCurrentIndex(s_bpsMode);
	pdDlg.sampleCountSpinBox->setValue(s_numberOfSamples);
	
	if (!pdDlg.exec())
	{
		//process cancelled by the user
		return;
	}

	//save parameters
	s_minPeakVal = pdDlg.minAmplitudeDoubleSpinBox->value();
	s_peakDetectorIndex = pdDlg.echoDetectorComboBox->currentIndex();
	s_maxReturnCount = pdDlg.maxReturnCountSpinBox->value();
	s_bpsMode = pdDlg.bpsComboBox->currentIndex();
	s_numberOfSamples = pdDlg.sampleCountSpinBox->value();

	//peak detector
	FWFPeakDetectorInterface* detector = 0;
	switch (s_peakDetectorIndex)
	{
	case 0:
		detector = new FWFPeakDetectorNaive;
		break;
	case 1:
		detector = new FWFPeakDetectorNLSGauss;
		break;
	default:
		assert(false);
		if (m_app)
			m_app->dispToConsole("Internal error: invalid peak detector", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	WaveformDescriptor descriptor;
	switch (s_bpsMode)
	{
	case 0:
		descriptor.bitsPerSample = 8;
		break;
	case 1:
		descriptor.bitsPerSample = 16;
		break;
	case 2:
		descriptor.bitsPerSample = 32;
		break;
	default:
		assert(false);
		descriptor.bitsPerSample = 16;
		break;
	}

	descriptor.numberOfSamples = s_numberOfSamples;

	//set the gain and offset automatically
	descriptor.digitizerOffset = rangeMin;
	if (descriptor.digitizerOffset > 0)
	{
		//auto-shift to 0
		descriptor.digitizerOffset = 0;
	}
	descriptor.digitizerGain = (rangeMax - descriptor.digitizerOffset) / (1 << descriptor.bitsPerSample);

	//compute the right sampling rate
	//1) find the first valid waveform
	for (const ccWaveform& w : m_cloud->waveforms())
	{
		if (w.descriptorID() != 0 && m_cloud->fwfDescriptors().contains(w.descriptorID()))
		{
			WaveformDescriptor& d = m_cloud->fwfDescriptors()[w.descriptorID()];

			double sampleStep = w.beamDir().normd() * d.samplingRate_ps;
			double gridStep = m_grid->steps().z;

			descriptor.samplingRate_ps = static_cast<uint32_t>(gridStep * d.samplingRate_ps / sampleStep);
			if (descriptor.samplingRate_ps > 100000 && m_app)
			{
				m_app->dispToConsole("[exportToFWFCloud] High sampling rate detected. Accuracy may be jeopardized (the vertical grid step may be too high).", ccMainAppInterface::WRN_CONSOLE_MESSAGE);
			}
			//descriptor.samplingRate_ps = 1000;
			break;
		}
	}

	ccPointCloud* cloud = OrthoWFTools::ConvertGridToFWFCloud(	*m_grid,
																detector,
																s_maxReturnCount,
																descriptor,
																s_minPeakVal,
																this,
																m_app);

	delete detector;
	detector = 0;

	if (cloud)
	{
		cloud->setGlobalShift(m_cloud->getGlobalShift());
		cloud->setGlobalScale(m_cloud->getGlobalScale());
		cloud->setMetaData(m_cloud->metaData(), true);

		cloud->setName(m_cloud->getName() + ".fwf");
		if (m_cloud->getParent())
		{
			m_cloud->getParent()->addChild(cloud);
		}
		m_app->addToDB(cloud);
	}
}
