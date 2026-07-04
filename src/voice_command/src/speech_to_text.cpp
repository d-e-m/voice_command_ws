#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <rclcpp_components/register_node_macro.hpp>
#include <std_msgs/msg/string.hpp>
#include <voice_cmd_messages/action/voice_interpreter.hpp>
#include <ament_index_cpp/get_package_share_directory.hpp>

#include <vosk_api.h>
#include <portaudio.h>

#include <iostream>
#include <cstdlib>
#include <string>
#include <memory>
#include <atomic>

using namespace std::chrono_literals;
using namespace std::placeholders;

namespace voice_command
{
    class SpeechToTextNode : public rclcpp::Node
    {
    public:
        SpeechToTextNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions())
        : Node("speech_to_text_node", options)
        {
            // --- Action Client ---
            action_client_ = rclcpp_action::create_client<voice_cmd_messages::action::VoiceInterpreter>(this, "voice_interpreter");

            // --- Publishers & Subscribers ---
            stt_publisher_ = this->create_publisher<std_msgs::msg::String>("recognized_speech", 10);
            tts_publisher_ = this->create_publisher<std_msgs::msg::String>("tts_commands", 10);
            
            stt_subscriber_ = this->create_subscription<std_msgs::msg::String>(
                "recognized_speech", 10, std::bind(&SpeechToTextNode::stt_callback, this, _1));

            // --- Vosk Model Setup ---
            std::string pkg_share = ament_index_cpp::get_package_share_directory("voice_command");
            std::string model_path = pkg_share + "/vosk_models/vosk-model-small-en-us-0.15";

            RCLCPP_INFO(this->get_logger(), "Loading model from: %s", model_path.c_str());

            model_ = std::shared_ptr<VoskModel>(
                vosk_model_new(model_path.c_str()),
                [](VoskModel* m) { vosk_model_free(m); }
            );

            recognizer_ = std::shared_ptr<VoskRecognizer>(
                vosk_recognizer_new(model_.get(), 16000.0),
                [](VoskRecognizer* r) { vosk_recognizer_free(r); }
            );

            // --- PortAudio Setup ---
            Pa_Initialize();

            PaStream* raw_stream = nullptr;
            Pa_OpenDefaultStream(&raw_stream, 1, 0, paInt16, 16000, 4000, audio_callback_wrapper_, this);
            
            stream_ = std::shared_ptr<PaStream>(
                raw_stream, 
                [](PaStream* s) { 
                    if (s) {
                        Pa_StopStream(s);
                        Pa_CloseStream(s);
                    }
                    Pa_Terminate();
                }
            );

            Pa_StartStream(stream_.get());
            RCLCPP_INFO(this->get_logger(), "Mic active. Awaiting wake word...");
        };

        ~SpeechToTextNode() = default;

        int process_audio(const void *inputBuffer, unsigned long framesPerBuffer) 
        {
            if (vosk_recognizer_accept_waveform(recognizer_.get(), (const char *)inputBuffer, framesPerBuffer * 2)) {
                const char *result = vosk_recognizer_result(recognizer_.get());
                
                // If the robot is busy executing a command, ignore all audio
                if (is_executing_) {
                    return paContinue;
                }

                std::string res_str(result);
                size_t text_pos = res_str.find("\"text\" : \"");
                
                if (text_pos != std::string::npos) {
                    size_t start = text_pos + 10;
                    size_t end = res_str.find("\"", start);
                    std::string spoken_text = res_str.substr(start, end - start);
                    
                    if (!spoken_text.empty()) {
                        
                        if (!is_awake_) {
                            // 1. We are ASLEEP. Check ONLY for the wake word.
                            if (spoken_text.find("wake up") != std::string::npos) { 
                                is_awake_ = true;
                                RCLCPP_INFO(this->get_logger(), "Wake word detected! Listening for commands...");
                                
                                std_msgs::msg::String tts_msg;
                                tts_msg.data = "I am listening";
                                tts_publisher_->publish(tts_msg);
                            }
                        } else {
                            // 2. We are AWAKE. Check for sleep word, otherwise process as command.
                            if (spoken_text.find("go to sleep") != std::string::npos) { 
                                is_awake_ = false;
                                RCLCPP_INFO(this->get_logger(), "Sleep word detected. Muting commands...");
                                
                                std_msgs::msg::String tts_msg;
                                tts_msg.data = "Going to sleep";
                                tts_publisher_->publish(tts_msg);
                            } else {
                                // 3. Valid command received while awake
                                RCLCPP_INFO(this->get_logger(), "Command heard: %s", spoken_text.c_str());
                                
                                std_msgs::msg::String msg;
                                msg.data = spoken_text;
                                stt_publisher_->publish(msg);
                            }
                        }
                    }
                }
            }
            return paContinue;
        }

    private:

        // --- ROS 2 Interfaces ---
        rclcpp_action::Client<voice_cmd_messages::action::VoiceInterpreter>::SharedPtr action_client_;
        rclcpp::Publisher<std_msgs::msg::String>::SharedPtr stt_publisher_;
        rclcpp::Publisher<std_msgs::msg::String>::SharedPtr tts_publisher_;
        rclcpp::Subscription<std_msgs::msg::String>::SharedPtr stt_subscriber_;
        rclcpp::TimerBase::SharedPtr unmute_timer_;

        // --- Audio/Vosk Pointers ---
        std::shared_ptr<VoskModel> model_;
        std::shared_ptr<VoskRecognizer> recognizer_;
        std::shared_ptr<PaStream> stream_;
        
        // --- State Variables ---
        std::atomic<bool> is_awake_{false};     
        std::atomic<bool> is_executing_{false}; 

        // --- Callbacks ---
        static int audio_callback_wrapper_(const void *inputBuffer, void* /* outputBuffer */,
                                        unsigned long framesPerBuffer,
                                        const PaStreamCallbackTimeInfo* /* timeInfo */,
                                        PaStreamCallbackFlags /* statusFlags */,
                                        void *userData)
        {
            SpeechToTextNode *node = static_cast<SpeechToTextNode*>(userData);
            return node->process_audio(inputBuffer, framesPerBuffer);
        }

        void stt_callback(const std_msgs::msg::String::SharedPtr msg)
        {
            if(msg->data.empty()) {
                RCLCPP_INFO(this->get_logger(), "Received empty string, aborting goal.");
                return;
            }
            
            auto goal_msg = voice_cmd_messages::action::VoiceInterpreter::Goal();
            goal_msg.command = msg->data;
            RCLCPP_INFO(this->get_logger(), "Sending the goal message: %s", msg->data.c_str());

            // Lock the audio processor
            is_executing_ = true;

            auto send_goal_options = rclcpp_action::Client<voice_cmd_messages::action::VoiceInterpreter>::SendGoalOptions();
            send_goal_options.goal_response_callback = std::bind(&SpeechToTextNode::goal_response_callback, this, _1);
            send_goal_options.feedback_callback = std::bind(&SpeechToTextNode::feedback_callback, this, _1, _2);
            send_goal_options.result_callback = std::bind(&SpeechToTextNode::result_callback, this, _1);

            action_client_->async_send_goal(goal_msg, send_goal_options);
        };

        void goal_response_callback(const rclcpp_action::ClientGoalHandle<voice_cmd_messages::action::VoiceInterpreter>::SharedPtr &goal_handle)
        {
            std_msgs::msg::String msg;
            
            if(!goal_handle){
                msg.data = "Goal was rejected by server";
                RCLCPP_ERROR(this->get_logger(), "Goal was rejected by server");
                is_executing_ = false; // Reset lock if rejected
            } else {
                msg.data = "Goal accepted by server";
                RCLCPP_INFO(this->get_logger(), "Goal accepted by server");
            }

            tts_publisher_->publish(msg);
        }

        void feedback_callback(rclcpp_action::ClientGoalHandle<voice_cmd_messages::action::VoiceInterpreter>::SharedPtr,
            const std::shared_ptr<const voice_cmd_messages::action::VoiceInterpreter::Feedback> feedback)
        {
            (void)feedback;
            // Optionally log feedback here, kept minimal to avoid terminal spam
            // RCLCPP_INFO(this->get_logger(), "Goal executing..."); 
        }

        void result_callback(const rclcpp_action::ClientGoalHandle<voice_cmd_messages::action::VoiceInterpreter>::WrappedResult &result)
        {
            (void)result;
            RCLCPP_INFO(this->get_logger(), "Action finished. Unlocking microphone in 3 seconds...");
            
            unmute_timer_ = this->create_wall_timer(
                std::chrono::seconds(3),
                [this]() {
                    is_executing_ = false; 
                    RCLCPP_INFO(this->get_logger(), "Microphone unlocked. Awaiting next command or sleep phrase...");
                    unmute_timer_->cancel(); 
                }
            );
        }
    };
}

RCLCPP_COMPONENTS_REGISTER_NODE(voice_command::SpeechToTextNode)