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

		std::array<int8_t, 2> pkg_size {};
		uint16_t size;

		while (file.read(reinterpret_cast<char*>(pkg_size.data()), 2))
		{
			++_total_packets_processed;

			if constexpr (std::endian::native == std::endian::big)
			{
				size = static_cast<uint16_t>(
					pkg_size[1] |
					(pkg_size[0] << 8));
			}
			else
			{
				size = static_cast<uint16_t>(
					pkg_size[0] |
					(pkg_size[1] << 8));
			}

			// valid packets size only
			if (size >= 64 && size <= 1522)
			{
				boost::asio::post(_tp, [this, &_path, pos = file.tellg()]()
					{
						std::ifstream file{ _path, std::ios::in | std::ios::binary };

						file.seekg(pos);
						this->worker(std::move(file));
					});
			}

			file.seekg(file.tellg() + static_cast<std::ifstream::pos_type>(size));
		}
	}

	void TrainPacketsParser::worker(std::ifstream pkg_file)
	{
		auto convert = [](const std::array<uint8_t, 2>& field)
			{
				if constexpr (std::endian::native == std::endian::big)
				{
					return static_cast<uint16_t>(
						field[0] |
						(field[1] << 8));
				}
				else
				{
					return static_cast<uint16_t>(
						field[1] |
						(field[0] << 8));
				}
			};
		std::array<uint8_t, 2> ether_type_field{};
		uint16_t type;

		// skip to tag or EtherType value
		pkg_file.seekg(pkg_file.tellg() + static_cast<std::ifstream::pos_type>(12));
		pkg_file.read(reinterpret_cast<char*>(ether_type_field.data()), 2);

		type = convert(ether_type_field);

		// VLAN
		if (type == 0x8100)
		{
			pkg_file.read(reinterpret_cast<char*>(ether_type_field.data()), 2);
			type = convert(ether_type_field);
		}
		else if (type == 0x88A8)
		{
			pkg_file.read(reinterpret_cast<char*>(ether_type_field.data()), 2);
			type = convert(ether_type_field);
		}
		if (type != 0x0800)
		{
			++_non_ipv4_packets;
			return;
		}

		std::array<uint8_t, 4> src{}, dst{};

		pkg_file.seekg(pkg_file.tellg() + static_cast<std::ifstream::pos_type>(12));
		pkg_file.read(reinterpret_cast<char*>(src.data()), 4);
		pkg_file.read(reinterpret_cast<char*>(dst.data()), 4);

		uint32_t src_val = 
			(src[0] << 24) |
			(src[1] << 16) |
			(src[2] << 8) |
			(src[3]);
		uint32_t dst_val =
			(dst[0] << 24) |
			(dst[1] << 16) |
			(dst[2] << 8) |
			(dst[3]);

		{
			std::lock_guard<std::mutex> lock(_mut);

			++_from_to_addrs_count_map[src_val][dst_val];
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

		for (const auto& [from, to_addrs] : obj._from_to_addrs_count_map)
		{
			if (!_addr_to_str.contains(from))
			{
				_addr_to_str[from] = std::move(convert_addr_to_str(from));
			}
			for (const auto& [to, count] : to_addrs)
			{
				if (!_addr_to_str.contains(to))
				{
					_addr_to_str[to] = std::move(convert_addr_to_str(to));
				}

				os << std::setw(15) << std::left << _addr_to_str[from] << "-> "
					<< std::setw(15) << std::left << _addr_to_str[to]
					<< " " << count << '\n';
			}
		}

		return os;
	}
}