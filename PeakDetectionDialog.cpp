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

#include "PeakDetectionDialog.h"

//Qt
#include <QSettings>

void PeakDetectionDialog::loadSettings()
{
	QSettings settings;
	settings.beginGroup("FWF_plugin");
	settings.beginGroup("PeakDetectionDialog");
	{
		settings.setValue("Detector",       echoDetectorComboBox->currentIndex());
		settings.setValue("MinAmplitude",   minAmplitudeDoubleSpinBox->value());
		settings.setValue("MaxReturnCount", maxReturnCountSpinBox->value());
		settings.setValue("BitsPerSample",  bpsComboBox->currentIndex());
		settings.setValue("SampleCount",    sampleCountSpinBox->value());
	}
	settings.endGroup();
	settings.endGroup();
}

void PeakDetectionDialog::saveSettings()
{
	QSettings settings;
	settings.beginGroup("FWF_plugin");
	settings.beginGroup("PeakDetectionDialog");
	{
		int detector        = settings.value("Detector",       echoDetectorComboBox->currentIndex()).toInt();
		double minAmplitude = settings.value("MinAmplitude",   minAmplitudeDoubleSpinBox->value()).toDouble();
		int maxReturnCount  = settings.value("MaxReturnCount", maxReturnCountSpinBox->value()).toInt();
		int bps             = settings.value("BitsPerSample",  bpsComboBox->currentIndex()).toInt();
		int sampleCount     = settings.value("SampleCount",    sampleCountSpinBox->value()).toInt();

		echoDetectorComboBox->setCurrentIndex(detector);
		minAmplitudeDoubleSpinBox->setValue(minAmplitude);
		maxReturnCountSpinBox->setValue(maxReturnCount);
		bpsComboBox->setCurrentIndex(bps);
		sampleCountSpinBox->setValue(sampleCount);
	}
	settings.endGroup();
	settings.endGroup();
}
