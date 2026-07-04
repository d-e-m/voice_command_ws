cmake_minimum_required(VERSION 3.8)
project(voice_command)

# ============================================================
# Compiler Warnings & IntelliSense
# ============================================================
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# RPATH Configuration (Fixes 'dlopen' runtime errors)
# ============================================================
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# ============================================================
# Find ROS 2 + External Dependencies
# ============================================================
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(std_msgs REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(moveit_ros_planning_interface REQUIRED)
find_package(moveit_msgs REQUIRED)
find_package(voice_cmd_messages REQUIRED)

# ============================================================
# Vosk API Paths
# ============================================================
set(VOSK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/voice_modules/vosk_api/vosk-linux-x86_64-0.3.43)

# ============================================================
# Components (Libraries)
# ============================================================

# 1. Speech-to-Text
add_library(speech_to_text SHARED src/speech_to_text.cpp)
target_include_directories(speech_to_text PRIVATE ${VOSK_DIR})
target_link_libraries(speech_to_text ${VOSK_DIR}/libvosk.so portaudio)
ament_target_dependencies(speech_to_text rclcpp std_msgs ament_index_cpp rclcpp_components rclcpp_action moveit_ros_planning_interface voice_cmd_messages)
rclcpp_components_register_nodes(speech_to_text "voice_command::SpeechToTextNode")

# 2. Text-to-Speech
add_library(text_to_speech SHARED src/text_to_speech.cpp)
ament_target_dependencies(text_to_speech rclcpp std_msgs rclcpp_components)
rclcpp_components_register_nodes(text_to_speech "voice_command::TextToSpeechNode")

# 3. Voice Interpreter
add_library(voice_interpreter SHARED src/voice_interpreter.cpp)
ament_target_dependencies(voice_interpreter rclcpp rclcpp_action std_msgs moveit_ros_planning_interface moveit_msgs voice_cmd_messages rclcpp_components)
rclcpp_components_register_nodes(voice_interpreter "voice_command::VoiceInterpreter")

# ============================================================
# Install Directives
# ============================================================

# Install Models from the correct path
install(DIRECTORY voice_modules/vosk_models
  DESTINATION share/${PROJECT_NAME}
)

# Install Library to the component library path
install(FILES ${VOSK_DIR}/libvosk.so
  DESTINATION lib
)

# Export Plugin Description
pluginlib_export_plugin_description_file(rclcpp_components components.xml)
install(FILES components.xml DESTINATION share/${PROJECT_NAME})

# Install Components
install(TARGETS
  speech_to_text
  text_to_speech
  voice_interpreter
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY tts_engine
  DESTINATION share/${PROJECT_NAME}
  USE_SOURCE_PERMISSIONS
)

ament_package()