# Rig calibration
if(OpenMVG_USE_BOOST)

  ###
  # Calibrate a rig based on CCTag localizers
  ###
  add_executable(openMVG_main_rigCalibration main_rigCalibration.cpp)
  install(TARGETS openMVG_main_rigCalibration DESTINATION bin/)
  target_link_libraries(openMVG_main_rigCalibration openMVG_rig openMVG_localization  openMVG_dataio openMVG_image openMVG_features vlsift ${Boost_LIBRARIES}) #todo: delete vlsift
   
  message(STATUS "OpenCV & Boost found: build openMVG_main_cameraCalibration")
  add_executable(openMVG_main_cameraCalibration main_cameraCalibration.cpp)
  target_link_libraries(openMVG_main_cameraCalibration # PUBLIC
    openMVG_dataio
    openMVG_image
    openMVG_calibration
    openMVG_system
    ${OpenCV_LIBRARIES}
    ${Boost_LIBRARIES}
    )
  # target_include_directories(openMVG_main_cameraCalibration PUBLIC ${Boost_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})
  install(TARGETS openMVG_main_cameraCalibration DESTINATION bin/)

  if(OpenMVG_USE_CCTAG)
    target_link_libraries(openMVG_main_rigCalibration
      ${CCTAG_LIBRARIES}
      )
  endif(OpenMVG_USE_CCTAG)

endif(OpenMVG_USE_BOOST)
