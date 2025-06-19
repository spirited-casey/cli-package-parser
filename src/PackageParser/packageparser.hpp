#pragma once

// stl
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

// boost
#include <boost/asio/thread_pool.hpp>

namespace train_protocol
{
	namespace fs = std::filesystem;

	class TrainPacketsParser final
	{
	public:
		void Parse(fs::path&);

		friend std::ostream& operator<<(std::ostream&, const TrainPacketsParser&);
	private:
		void worker(std::ifstream);

		uint32_t _total_packets_processed = 0;
		std::atomic<uint32_t> _non_ipv4_packets = 0;

		std::mutex _mut;
		boost::asio::thread_pool _tp;

		std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> _from_to_addrs_count_map;
	};
}