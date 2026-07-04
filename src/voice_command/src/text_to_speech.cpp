#include <rclcpp/rclcpp.hpp>
#include <rclcpp_components/register_node_macro.hpp>
#include <std_msgs/msg/string.hpp>
#include <ament_index_cpp/get_package_share_directory.hpp>

#include <string>
#include <cstdio>
#include <cstdlib>

using namespace std::placeholders;

namespace voice_command
{
    class TextToSpeechNode : public rclcpp::Node 
    {
    public:
        TextToSpeechNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions())
        : Node("text_to_speech_node", options) 
        {
            tts_subscriber_ = this->create_subscription<std_msgs::msg::String>(
                "tts_commands", 10, std::bind(&TextToSpeechNode::tts_callback, this, _1));
        }

        ~TextToSpeechNode() = default;

    private:
        rclcpp::Subscription<std_msgs::msg::String>::SharedPtr tts_subscriber_;

        void tts_callback(const std_msgs::msg::String::SharedPtr msg) 
        {
            RCLCPP_INFO(this->get_logger(), "Speaking: %s", msg->data.c_str());
          
            std::string pkg_share = ament_index_cpp::get_package_share_directory("voice_command");
            std::string piper_bin = pkg_share + "/tts_engine/piper";
            std::string model_path = pkg_share + "/tts_engine/en_US-lessac-medium.onnx";

            std::string command = "echo '" + msg->data + "' | " + piper_bin + 
                                  " --model " + model_path + 
                                  " --output_raw | aplay -r 22050 -f S16_LE -t raw -";

            FILE* pipe = popen(command.c_str(), "r");
            if (pipe) {
                pclose(pipe);
            } else {
                RCLCPP_ERROR(this->get_logger(), "Failed to execute TTS pipeline.");
            }
        }
    };
}

RCLCPP_COMPONENTS_REGISTER_NODE(voice_command::TextToSpeechNode)