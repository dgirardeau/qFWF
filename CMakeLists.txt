cmake_minimum_required(VERSION 2.8)

option( INSTALL_QFWF_PLUGIN "Check to install the qFWF plugin" OFF )

if (INSTALL_QFWF_PLUGIN)

#CloudCompare 'FWF' plugin
project( QFWF_PLUGIN_DLL )

include( ../CMakePluginTpl.cmake )

endif()
