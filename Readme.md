# Implementing Voice Command for ROS2

### **The Architecture (The "What")**

You will build a ROS 2 C++ package containing three distinct nodes:

1. **STT Node:** Captures mic audio via PortAudio, processes it via the Vosk C API, and publishes a `std_msgs::String` to `/voice_commands`.
2. **Interpreter Node:** Subscribes to `/voice_commands`, maps text to a MoveIt 2 `MoveGroupInterface` command, executes the physical motion, and publishes status to `/tts_commands`.
3. **TTS Node:** Subscribes to `/tts_commands` and triggers the system's `espeak` synthesizer.

---

### **1. Speech-to-Text (STT) Node**

**The "What":** The Vosk C API and PortAudio.
**The "Why":** Vosk is natively built in C. Bypassing Python wrappers saves RAM. PortAudio is the standard C library for cross-platform audio capture.
**The "How":** You must manually download the Vosk dynamic library and the language model for the Pi's architecture (AARCH64/ARM64).

**Setup Instructions:**

1. Install PortAudio:
```bash
sudo apt-get update
sudo apt-get install portaudio19-dev

```


2. Download the Vosk library for Raspberry Pi (64-bit) and extract it:
```bash
wget https://github.com/alphacep/vosk-api/releases/download/v0.3.43/vosk-linux-aarch64-0.3.43.zip
unzip vosk-linux-aarch64-0.3.43.zip -d ~/vosk_api

```


3. Download the lightweight English model:
```bash
wget https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip
unzip vosk-model-small-en-us-0.15.zip -d ~/vosk_models

```
---

### ** Tying it together: CMakeLists.txt**

**The "How":** To compile this, your `CMakeLists.txt` must explicitly link the downloaded Vosk `.so` file, the PortAudio system library, and the MoveIt interface.

```cmake
cmake_minimum_required(VERSION 3.8)
project(voice_robot_control)

# Find ROS 2 dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(moveit_ros_planning_interface REQUIRED)

# Find PortAudio (Installed via apt)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

# Set Vosk Library Path manually (pointing to where you unzipped it)
set(VOSK_INCLUDE_DIR "/home/ubuntu/vosk_api")
set(VOSK_LIBRARY "/home/ubuntu/vosk_api/libvosk.so")

# 1. STT Node
add_executable(stt_node src/stt_node.cpp)
target_include_directories(stt_node PUBLIC ${VOSK_INCLUDE_DIR} ${PORTAUDIO_INCLUDE_DIRS})
target_link_libraries(stt_node ${VOSK_LIBRARY} ${PORTAUDIO_LIBRARIES})
ament_target_dependencies(stt_node rclcpp std_msgs)

# 2. Interpreter Node
add_executable(voice_interpreter src/voice_interpreter.cpp)
ament_target_dependencies(voice_interpreter rclcpp std_msgs moveit_ros_planning_interface)

# 3. TTS Node
add_executable(tts_node src/tts_node.cpp)
ament_target_dependencies(tts_node rclcpp std_msgs)

install(TARGETS stt_node voice_interpreter tts_node
  DESTINATION lib/${PROJECT_NAME})

ament_package()

```

Run `colcon build --packages-select voice_robot_control` in your ROS 2 workspace, and source your setup file. You now have a hyper-efficient, fully offline, C++ voice-control pipeline.


---

### Note: For SBC running Ubuntu 22.04 or 24.04, you will need the **x86_64** (often called AMD64) version of the Vosk library.

Here is the exact breakdown of how to get it and why it differs from the Pi version.

---

### **The "What"**

You need the pre-compiled Vosk C/C++ dynamic library (`.so`) specifically packaged for **`linux-x86_64`**.

### **The "Why"**

Standard Ubuntu desktop and server installations run on Intel or AMD processors, which use the **x86_64** instruction set. The Raspberry Pi uses an **ARM** processor (AARCH64). If you try to compile or run the ARM library we used previously on your standard Ubuntu machine, your compiler will throw an "Exec format error" or fail to link the library because the architectures are fundamentally incompatible. Vosk provides pre-compiled binaries for both.

### **The "How"**

The implementation and your C++ code remain **exactly the same**. The only thing that changes is the `.zip` file you download and where you point your `CMakeLists.txt`.

**Setup Instructions for Ubuntu 22.04 / 24.04 (x86_64):**

1. **Install PortAudio** (same as before):
```bash
sudo apt-get update
sudo apt install libportaudio2 portaudio19-dev

```


2. **Download the x86_64 Vosk library:**
Notice the URL specifically targets `x86_64` instead of `aarch64`.
```bash
wget https://github.com/alphacep/vosk-api/releases/download/v0.3.43/vosk-linux-x86_64-0.3.43.zip
unzip vosk-linux-x86_64-0.3.43.zip -d ~/vosk_api

```


3. **Download the Language Model** (same as before):
Models are architecture-agnostic, so you can use the exact same model.
```bash
wget https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip
unzip vosk-model-small-en-us-0.15.zip -d ~/vosk_models

```



### **Updating your CMakeLists.txt**

Because the folder name changed when you unzipped it, you just need to update the path in your `CMakeLists.txt` to point to the new x86_64 directory.

```cmake
# Set Vosk Library Path to the newly extracted x86_64 folder
set(VOSK_INCLUDE_DIR "/home/your_username/vosk_api/vosk-linux-x86_64-0.3.43")
set(VOSK_LIBRARY "/home/your_username/vosk_api/vosk-linux-x86_64-0.3.43/libvosk.so")

```

Everything else in the ROS 2 pipeline—the STT node, the Interpreter, and the TTS node—will compile and run flawlessly on your generic Ubuntu machine.