#pragma once

#include <chrono>
#include <string>
#include <vector>
#include "Logger.h"

#ifdef _DEBUG
#define PERFORMANCE_SCOPE(name) Timer timer##__LINE__(name)
#else 
#define PERFORMANCE_SCOPE(name) 
#endif

namespace Gibo {

	namespace PERFORMANCE_TEST {
		struct Performance_Results {
			std::string m_name;
			double m_time;
		};

		static std::vector<Performance_Results> results;

		static void Print_and_Clear_Results() {
			for (int i = 0; i < results.size(); i++) {
				Logger::LogPerformance(results[i].m_name + ':' + ' ' + std::to_string(results[i].m_time) + 'm' + 's' + '\n');
			}
			results.clear();
		}
	}

	class Timer {

	public:

		Timer(const char* text, bool logval = false) {
			log = logval;
			stopped = false;
			startpoint = std::chrono::high_resolution_clock::now();
			desc = text;
		}

		~Timer() {
			double a;
			Stop(a);
		}

		float GetTime() {
			auto endTimepoint = std::chrono::high_resolution_clock::now();

			auto start = std::chrono::time_point_cast<std::chrono::microseconds>(startpoint).time_since_epoch().count();
			auto end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch().count();

			auto duration = end - start;
			float ms = duration * 0.001;

			return ms;
		}

		void Sleep() {

		}

		void Stop(double& time) {
			if (stopped) {
				return;
			}
			stopped = true;
			auto endTimepoint = std::chrono::high_resolution_clock::now();

			auto start = std::chrono::time_point_cast<std::chrono::microseconds>(startpoint).time_since_epoch().count();
			auto end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch().count();

			auto duration = end - start;
			double ms = duration * 0.001;
			time = ms;
			if (log) {
				Logger::LogTime("(" + (std::string)desc + ") " + std::to_string(ms) + " milliseconds\n");
			}

			//PERFORMANCE_TEST::results.push_back({ desc, ms });
		}

		void InsertToFile() {
			return;
		}

		void CalculatePercentageofFrame() {

		}

	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> startpoint;
		std::string desc;
		bool stopped;
		bool log;
	};

}