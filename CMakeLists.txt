cmake_minimum_required(VERSION 2.8)

option( INSTALL_QFWF_PLUGIN "Check to install the qFWF plugin" OFF )

if (INSTALL_QFWF_PLUGIN)

#CloudCompare 'FWF' plugin
project( QFWF_PLUGIN_DLL )

	#As we are going to re-use several components of the main qCC project
	#we define its sources path relatively to the plugin's dir
	set(CloudCompare_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../qCC)

	#include the whole CC directory (easier)
	include_directories( ${CloudCompare_SOURCE_DIR} )
	
	#we need the ccBoundingBoxEditorDlg dialog
	file( GLOB CC_PLUGIN_CUSTOM_HEADER_LIST ${CloudCompare_SOURCE_DIR}/ccBoundingBoxEditorDlg*.h )
	file( GLOB CC_PLUGIN_CUSTOM_SOURCE_LIST ${CloudCompare_SOURCE_DIR}/ccBoundingBoxEditorDlg*.cpp )

	#don't forget associated widgets(.ui)
	set( CC_PLUGIN_CUSTOM_UI_LIST ${CloudCompare_SOURCE_DIR}/ui_templates/boundingBoxEditorDlg.ui )

	include( ../CMakePluginTpl.cmake )

endif()
