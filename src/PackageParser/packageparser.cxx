#include "packageparser.hpp"

#include <fstream>
#include <bit>
#include <format>
#include <iomanip>

#include <boost/asio/basic_file.hpp>

namespace
{
	std::string convert_addr_to_str(uint32_t addr)
	{
		return std::format("{}.{}.{}.{}",
			addr >> 24,
			((addr << 8) >> 24),
			((addr << 16) >> 24),
			((addr << 24) >> 24));
	}

	void endianness(uint8_t* data, size_t size)
	{
		if constexpr (std::endian::native == std::endian::big)
		{
			return;
		}

		--size;

		size_t start = 0;
		
		while (start < size)
		{
			std::swap(data[start++], data[size--]);
		}

		uint32_t tmp = *reinterpret_cast<uint32_t*>(data);

		int a = 1;
	}
}

namespace train_protocol
{
	void TrainPacketsParser::Parse(fs::path& _path)
	{
		std::ifstream file { _path, std::ios::in | std::ios::binary };

		if (!file.is_open())
		{
			throw std::system_error(errno, std::system_category(), "Error: failed to open file.");
		}

		std::array<uint8_t, 2> pkg_size {};
		uint16_t size {};

		while (!file.eof())
		{
			++_total_packets_processed;

			file.read(reinterpret_cast<char*>(pkg_size.data()), 2);
			size = pkg_size[0] | (pkg_size[1] << 8);

			{
				std::vector<uint8_t> payload;

				payload.resize(size);
				file.read(reinterpret_cast<char*>(payload.data()), size);

				boost::asio::post(_tp, [this, data = std::move(payload)]()
					{
						this->worker(std::move(data));
					});
			}
		}

		_tp.join();
	}

	void TrainPacketsParser::worker(std::vector<uint8_t> payload)
	{
		static constexpr size_t ETHERTYPE_START_POS = 12;
		static constexpr size_t ADDRS_START_POS = 26;

		auto convert = [](uint16_t field)
			{
				if constexpr (std::endian::native == std::endian::big)
				{
					return field;
				}

				return (field >> 8) | (field << 8);
			};

		uint16_t type {};
		uint64_t src_dst_addrs {};

		std::memcpy(&type, &payload[ETHERTYPE_START_POS], sizeof(uint16_t));
		type = convert(type);

		// VLAN
		if (type == 0x8100)
		{
			std::memcpy(&type, &payload[ETHERTYPE_START_POS + 2], sizeof(uint16_t));
			std::memcpy(&src_dst_addrs, &payload[ADDRS_START_POS + 2], sizeof(uint64_t));
			type = convert(type);
		}
		else if (type == 0x88A8)
		{
			std::memcpy(&type, &payload[ETHERTYPE_START_POS + 4], sizeof(uint16_t));
			std::memcpy(&src_dst_addrs, &payload[ADDRS_START_POS + 4], sizeof(uint64_t));
			type = convert(type);
		}
		if (type != 0x0800)
		{
			++_non_ipv4_packets;
			return;
		}

		std::memcpy(&src_dst_addrs, &payload[ADDRS_START_POS], sizeof(uint64_t));
		endianness(reinterpret_cast<uint8_t*>(&src_dst_addrs), sizeof(uint64_t));

		{
			std::lock_guard<std::mutex> lock(_mut);

			++_from_to_addrs_count[src_dst_addrs];
		}
	}

	std::ostream& operator<<(std::ostream& os, const TrainPacketsParser& obj)
	{
		os << std::setw(25) << std::left << "Packets processed:"
			<< obj._total_packets_processed << '\n';
		os << std::setw(25) << std::left << "Packets contains IPv4:"
			<< obj._total_packets_processed - obj._non_ipv4_packets << '\n';
		os << std::setw(25) << std::left << "Packets without IPv4:" << obj._non_ipv4_packets << "\n\n";

		std::unordered_map<uint32_t, std::string> _addr_to_str;
		uint32_t from{}, to{};

		for (const auto& [from_to_addrs, count] : obj._from_to_addrs_count)
		{
			from = from_to_addrs >> 32;
			to = (from_to_addrs << 32) >> 32;

			if (!_addr_to_str.contains(from))
			{
				_addr_to_str[from] = std::move(convert_addr_to_str(from));
			}
			if (!_addr_to_str.contains(to))
			{
				_addr_to_str[to] = std::move(convert_addr_to_str(to));
			}

			os << std::setw(15) << std::left << _addr_to_str[from] << "-> "
				<< std::setw(15) << std::left << _addr_to_str[to]
				<< " " << count << '\n';
		}

		return os;
	}
}