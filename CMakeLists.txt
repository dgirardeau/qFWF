cmake_minimum_required(VERSION 2.8)

option( INSTALL_QFWF_PLUGIN "Check to install the qFWF plugin" OFF )

if (INSTALL_QFWF_PLUGIN)

	#CloudCompare 'FWF' plugin
	project( QFWF_PLUGIN_DLL )

	AddPlugin( NAME ${PROJECT_NAME} )
	
	list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
	find_package(LASzip MODULE REQUIRED)
	target_link_libraries(${PROJECT_NAME} LASzip::LASzip)

	target_sources( ${PROJECT_NAME} PRIVATE
		${CMAKE_CURRENT_LIST_DIR}/../../core/IO/qLASIO/src/LasVlr.cpp
		${CMAKE_CURRENT_LIST_DIR}/../../core/IO/qLASIO/src/LasDetails.cpp
		${CMAKE_CURRENT_LIST_DIR}/../../core/IO/qLASIO/src/LasScalarField.cpp
		${CMAKE_CURRENT_LIST_DIR}/../../core/IO/qLASIO/src/CopcLoader.cpp
		${CMAKE_CURRENT_LIST_DIR}/../../core/IO/qLASIO/include/LasVlr.h
	)
	target_include_directories( ${PROJECT_NAME} PRIVATE
		${CMAKE_CURRENT_LIST_DIR}/../../core/IO/qLASIO/include
	)

	include( CMakePolicies NO_POLICY_SCOPE )

	#As we are going to re-use several components of the main qCC project
	#we define its sources path relatively to the plugin's dir
	set(CloudCompare_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../qCC)

	#include the whole CC directory (easier)
	include_directories( ${CloudCompare_SOURCE_DIR} )
	
	file( GLOB PLUGIN_HDR_LIST ${CMAKE_CURRENT_SOURCE_DIR}/*.h )
	file( GLOB PLUGIN_SRC_LIST ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp )
	file( GLOB PLUGIN_UI_LIST ${CMAKE_CURRENT_SOURCE_DIR}/*.ui )
	
	#we need the ccBoundingBoxEditorDlg dialog
	file( GLOB CC_HDR_LIST ${CloudCompare_SOURCE_DIR}/ccBoundingBoxEditorDlg*.h )
	file( GLOB CC_SRC_LIST ${CloudCompare_SOURCE_DIR}/ccBoundingBoxEditorDlg*.cpp )
	set( CC_UI_LIST	${CloudCompare_SOURCE_DIR}/ui_templates/boundingBoxEditorDlg.ui )

	# add source files
	target_sources( ${PROJECT_NAME}
		PRIVATE
			${PLUGIN_HDR_LIST}
			${PLUGIN_SRC_LIST}
			${PLUGIN_UI_LIST}
			${CC_HDR_LIST}
			${CC_SRC_LIST}
			${CC_UI_LIST}
	)
	
	#we also need Eigen (for Levenberg-Marquardt optimization, etc.)
	set( EIGEN_ROOT_DIR "" CACHE PATH "Eigen root (contains the Eigen directory)" )
	if ( NOT EIGEN_ROOT_DIR )
		message( SEND_ERROR "No Eigen root directory specified (EIGEN_ROOT_DIR)" )
	endif()

    #include( CMakePluginTemplate )
	#include( ../../CMakePluginTpl.cmake )

    set_target_properties( ${PROJECT_NAME} PROPERTIES
        # FIXME Remove after everything has moved to targets
        AUTOUIC_SEARCH_PATHS "${CloudCompare_SOURCE_DIR}/ui_templates;${CMAKE_CURRENT_SOURCE_DIR}/ui"
    )

    # FIXME These are required becaue we have dependencies outside of the api
    target_include_directories( ${PROJECT_NAME}
        PRIVATE
            ${CloudCompare_SOURCE_DIR}
            ${CloudCompare_SOURCE_DIR}/../common
    )

	include_directories( ${EIGEN_ROOT_DIR} )

endif()
