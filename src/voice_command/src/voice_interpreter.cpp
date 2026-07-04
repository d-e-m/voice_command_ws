#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <rclcpp_components/register_node_macro.hpp>
#include <std_msgs/msg/string.hpp>
#include <voice_cmd_messages/action/voice_interpreter.hpp>
#include <memory>
#include <thread>
#include <string>

using namespace std::placeholders;

namespace voice_command
{
    class VoiceInterpreter : public rclcpp::Node 
    {
    public:
        VoiceInterpreter(const rclcpp::NodeOptions &options = rclcpp::NodeOptions())
        : Node("voice_interpreter", options) 
        {
            RCLCPP_INFO(this->get_logger(), "Starting Task Server");

            action_server_ = rclcpp_action::create_server<voice_cmd_messages::action::VoiceInterpreter>(
                this, "voice_interpreter",
                std::bind(&VoiceInterpreter::goal_callback, this, _1, _2),
                std::bind(&VoiceInterpreter::cancel_callback, this, _1),
                std::bind(&VoiceInterpreter::accepted_callback, this, _1)
            );
            
            tts_publisher_ = this->create_publisher<std_msgs::msg::String>("/tts_commands", 10);
        }

        ~VoiceInterpreter() = default;

    private:
        // --- ROS 2 Interfaces ---
        rclcpp_action::Server<voice_cmd_messages::action::VoiceInterpreter>::SharedPtr action_server_;
        rclcpp::Publisher<std_msgs::msg::String>::SharedPtr tts_publisher_;

        // --- Callbacks ---
        rclcpp_action::GoalResponse goal_callback(const rclcpp_action::GoalUUID &uuid,
                std::shared_ptr<const voice_cmd_messages::action::VoiceInterpreter::Goal> goal)
        {
            (void)uuid;

            std::string goal_cmd = "Received goal request with task " + goal->command;
            RCLCPP_INFO(this->get_logger(), "%s", goal_cmd.c_str());
            speak(goal_cmd);
            
            return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
        }

        rclcpp_action::CancelResponse cancel_callback(const std::shared_ptr<rclcpp_action::ServerGoalHandle
            <voice_cmd_messages::action::VoiceInterpreter>> goal_handle) 
        {
            (void)goal_handle;
            
            RCLCPP_INFO(this->get_logger(), "Received request to cancel task");
            
            return rclcpp_action::CancelResponse::ACCEPT;
        }

        void accepted_callback(const std::shared_ptr<rclcpp_action::ServerGoalHandle
                <voice_cmd_messages::action::VoiceInterpreter>> goal_handle)
        {
            std::thread{std::bind(&VoiceInterpreter::execute_cmd, this, _1), goal_handle}.detach();
        }

        void execute_cmd(const std::shared_ptr<rclcpp_action::ServerGoalHandle
            <voice_cmd_messages::action::VoiceInterpreter>> goal_handle)
        {
            RCLCPP_INFO(this->get_logger(), "Executing goal");
            RCLCPP_INFO(this->get_logger(), "Finishing setting goal and result variable");

            auto result = std::make_shared<voice_cmd_messages::action::VoiceInterpreter::Result>();

            RCLCPP_INFO(this->get_logger(), "Setting results");
            result->success = true;
            RCLCPP_INFO(this->get_logger(), "Results set");
            
            goal_handle->succeed(result);
            RCLCPP_INFO(this->get_logger(), "Goal Succeeded");
        }

        void speak(const std::string& text) 
        {
            RCLCPP_INFO(this->get_logger(), "About to speak");
            
            std_msgs::msg::String msg;
            msg.data = text;
            tts_publisher_->publish(msg);

            RCLCPP_INFO(this->get_logger(), "Finished speaking");
        }
    };
}

RCLCPP_COMPONENTS_REGISTER_NODE(voice_command::VoiceInterpreter)