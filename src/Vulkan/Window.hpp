#pragma once

#include "WindowConfig.hpp"
#include "Vulkan.hpp"
#include <functional>
#include <vector>
#include <iostream>
#include <chrono>

namespace Vulkan
{

	class Window final
	{
	public:

		VULKAN_NON_COPIABLE(Window)

		explicit Window(const WindowConfig& config);
		~Window();

		// Window instance properties.
		const WindowConfig& Config() const { return config_; }
		GLFWwindow* Handle() const { return window_; }
		float ContentScale() const;
		VkExtent2D FramebufferSize() const;
		VkExtent2D WindowSize() const;

		// GLFW instance properties (i.e. not bound to a window handler).
		const char* GetKeyName(int key, int scancode) const;
		std::vector<const char*> GetRequiredInstanceExtensions() const;
		double GetTime() const;

		// Callbacks
		std::function<void()> DrawFrame;
		std::function<void(int key, int scancode, int action, int mods)> OnKey;
		std::function<void(double xpos, double ypos)> OnCursorPosition;
		std::function<void(int button, int action, int mods)> OnMouseButton;
		std::function<void(double xoffset, double yoffset)> OnScroll;

		// Methods
		void Close();
		bool IsMinimized() const;
		void Run();
		void WaitForEvents() const;

	private:

		const WindowConfig config_;
		GLFWwindow* window_{};
	};


	class FrameTimer {
	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> lastTime;
		std::vector<double> frameDurations;

	public:
		FrameTimer() {
			lastTime = std::chrono::high_resolution_clock::now();
		}

		void markFrame() {
			auto now = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double, std::milli> frameDuration = now - lastTime;

			frameDurations.push_back(frameDuration.count());
			lastTime = now;
		}

		void printFrameDurations() {
			double total = 0;
			for (double duration : frameDurations) {
				total += duration;
			}
			std::cout << "Average frame duration over 1000 frames: " << total / 1000 << std::endl;
		}
	};



}
