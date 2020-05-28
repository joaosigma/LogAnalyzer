#include "utils.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <numeric>
#include <ostream>
#include <charconv>

#if defined(_WIN32) || defined(WIN32)
	#include <ws2tcpip.h>
#else
	#include <arpa/inet.h>
#endif

namespace la::utils
{
	namespace
	{
		enum class Protocol { IPV4, IPV6 };

#pragma pack(1)
		struct UDPHeader
		{
			uint16_t src_port;
			uint16_t dst_port;
			uint16_t length;
			uint16_t checksum;
		};
#pragma pack()

		uint32_t ipChecksum(const uint8_t* data, size_t size)
		{
			if (!size)
				return 0;

			bool align = reinterpret_cast<uintptr_t>(data) & 1;

			uint32_t sum = 0;
			if (align)
			{
				sum += (*data << 8);
				data += 1;
				size -= 1;
			}

			if (size >= 2)
			{
				if (reinterpret_cast<uintptr_t>(data) & 2)
				{
					sum += *reinterpret_cast<const uint16_t*>(data);
					data += 2;
					size -= 2;
				}

				for (auto end = data + (size & ~static_cast<size_t>(3)); data < end; data += 4)
				{
					auto value = *reinterpret_cast<const uint32_t*>(data);

					sum += value;
					sum += (sum < value);
				}
				sum = (sum & 0xffff) + (sum >> 16);

				if (size & 2)
				{
					sum += *reinterpret_cast<const uint16_t*>(data);
					data += 2;
				}
			}

			if (size & 1)
				sum += *data;

			if (align)
			{
				sum = (sum & 0xffff) + (sum >> 16);
				sum = (sum & 0xffff) + (sum >> 16);
				sum = ((sum >> 8) & 0xff) | ((sum & 0xff) << 8);
			}

			return sum;
		}

		uint32_t ipChecksumAdd(uint32_t sum, uint32_t value)
		{
			sum += value;
			sum += (sum < value); //add carry
			return sum;
		}

		uint32_t ipChecksumFinal(uint32_t sum)
		{
			sum = (sum & 0xffff) + (sum >> 16);
			sum = (sum & 0xffff) + (sum >> 16);
			return ~sum;
		}

		template<size_t NBuffers>
		void writePacket(std::ostream& streamOut, Protocol protocol, uint64_t timestamp, const std::array<std::tuple<const void*, size_t>, NBuffers>& buffers)
		{
#pragma pack(1)
			struct PacketHeader
			{
				uint32_t ts_sec;
				uint32_t ts_usec;
				uint32_t incl_len;
				uint32_t orig_len;
			};

			struct LinuxCookedHeader
			{
				uint16_t packet_type;
				uint16_t arphrd_type;
				uint16_t address_len;
				uint8_t address[8];
				uint16_t protocol;
			};
#pragma pack()

			//write packet header
			{
				auto sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds{ timestamp });
				auto usec = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::milliseconds{ timestamp } - sec);
				auto totalSize = std::accumulate(buffers.begin(), buffers.end(), static_cast<size_t>(0), [](auto a, const auto& b) { return (a + std::get<1>(b)); });

				PacketHeader header;
				std::memset(&header, 0, sizeof(header));
				header.ts_sec = static_cast<uint32_t>(sec.count());
				header.ts_usec = static_cast<uint32_t>(usec.count());
				header.incl_len = header.orig_len = static_cast<uint32_t>(sizeof(LinuxCookedHeader) + totalSize);

				streamOut.write(reinterpret_cast<const char*>(&header), sizeof(PacketHeader));
			}

			//write Linux "Cooked" header
			{
				LinuxCookedHeader header;
				std::memset(&header, 0, sizeof(header));
				header.packet_type = htons(0);
				header.arphrd_type = htons(1); //ethernet
				header.address_len = htons(6);
				header.address[0] = 0x08; //08:00:08:00:00:00
				header.address[1] = 0x00;
				header.address[2] = 0x08;
				header.protocol = htons(static_cast<uint16_t>((protocol == Protocol::IPV4) ? 0x0800 : 0x86DD));

				streamOut.write(reinterpret_cast<const char*>(&header), sizeof(LinuxCookedHeader));
			}

			//write the buffers
			for (const auto& buffer : buffers)
				streamOut.write(reinterpret_cast<const char*>(std::get<0>(buffer)), std::get<1>(buffer));
		}
	}

	bool Network::writePCAPHeader(std::ostream& streamOut)
	{
#pragma pack(1)
		struct
		{
			uint32_t magic_number;
			uint16_t version_major, version_minor;
			int32_t thiszone;
			uint32_t sigfigs;
			uint32_t snaplen;
			uint32_t network;
		} header;
#pragma pack()

		std::memset(&header, 0, sizeof(header));
		header.magic_number = 0xa1b2c3d4;
		header.version_major = 2;
		header.version_minor = 4;
		header.thiszone = 0;
		header.sigfigs = 0;
		header.snaplen = 65535;
		header.network = 0x71; //Linux "Cooked"

		streamOut.write(reinterpret_cast<const char*>(&header), sizeof(header));

		return true;
	}

	bool Network::writePCAPDataIPV4(std::ostream& streamOut, std::string_view srcAddress, std::string_view dstAddress, int64_t timestamp, std::tuple<const void*, size_t> payload)
	{
#pragma pack(1)
		struct IPV4Header
		{
			uint8_t hdr_len : 4;
			uint8_t version : 4;
			uint8_t tos;
			uint16_t total_length;
			uint16_t id;
			uint16_t flags_fragment;
			uint8_t ttl;
			uint8_t protocol;
			uint16_t checksum;
			uint32_t src;
			uint32_t dst;
		};
#pragma pack()

		UDPHeader headerUDP;
		IPV4Header headerIPV4;
		std::memset(&headerUDP, 0, sizeof(UDPHeader));
		std::memset(&headerIPV4, 0, sizeof(IPV4Header));

		{
			char srcIP[32], dstIP[32];
			uint16_t srcPort, dstPort;
			{
				auto extractAddress = [](std::string_view address, char* const ip, uint16_t& port)
				{
					auto it = address.find(':');
					if (it == std::string_view::npos)
						return false;

					std::memcpy(ip, address.data(), it);
					ip[it] = '\0';

					if (auto [p, ec] = std::from_chars(address.data() + it + 1, address.data() + address.size(), port); ec != std::errc())
						return false;

					return true;
				};

				if (!extractAddress(srcAddress, srcIP, srcPort) || !extractAddress(dstAddress, dstIP, dstPort))
					return false;
			}

			headerIPV4.version = 4;
			headerIPV4.hdr_len = 5;
			headerIPV4.tos = 0;
			headerIPV4.total_length = htons(static_cast<uint16_t>(sizeof(IPV4Header) + sizeof(UDPHeader) + std::get<1>(payload)));
			headerIPV4.id = 0;
			headerIPV4.flags_fragment = htons(0x4000); //don't fragment
			headerIPV4.ttl = 128;
			headerIPV4.protocol = 17; //UDP
			headerIPV4.checksum = 0;
			if (inet_pton(AF_INET, srcIP, &headerIPV4.src) != 1)
				return false;
			if (inet_pton(AF_INET, dstIP, &headerIPV4.dst) != 1)
				return false;

			headerIPV4.checksum = ipChecksumFinal(ipChecksum(reinterpret_cast<uint8_t*>(&headerIPV4), sizeof(IPV4Header)));

			headerUDP.src_port = htons(srcPort);
			headerUDP.dst_port = htons(dstPort);
			headerUDP.length = htons(static_cast<uint16_t>(sizeof(UDPHeader) + std::get<1>(payload)));

			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, ipChecksum(reinterpret_cast<uint8_t*>(&headerIPV4.src), sizeof(uint32_t) * 2));
			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, headerIPV4.protocol << 8);
			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, headerUDP.length);
			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, headerUDP.src_port);
			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, headerUDP.dst_port);
			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, headerUDP.length);
			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, ipChecksum(reinterpret_cast<const uint8_t*>(std::get<0>(payload)), std::get<1>(payload)));
			headerUDP.checksum = ipChecksumFinal(headerUDP.checksum);
		}

		writePacket<3>(streamOut, Protocol::IPV4, timestamp, { {
			{ &headerIPV4, sizeof(IPV4Header) },
			{ &headerUDP, sizeof(UDPHeader) },
			payload
		} });

		return true;
	}

	bool Network::writePCAPDataIPV6(std::ostream& streamOut, std::string_view srcAddress, std::string_view dstAddress, int64_t timestamp, std::tuple<const void*, size_t> payload)
	{
#pragma pack(1)
		struct IPV6Header
		{
			uint8_t priority : 4;
			uint8_t version : 4;
			uint8_t flow_lbl[3];
			uint16_t payload_len;
			uint8_t nexthdr;
			uint8_t hop_limit;
			uint8_t src[16];
			uint8_t dst[16];
		};
#pragma pack()

		UDPHeader headerUDP;
		IPV6Header headerIPV6;
		std::memset(&headerUDP, 0, sizeof(UDPHeader));
		std::memset(&headerIPV6, 0, sizeof(IPV6Header));

		{
			char srcIP[128], dstIP[128];
			uint16_t srcPort, dstPort;
			{
				auto extractAddress = [](std::string_view address, char* const ip, uint16_t& port)
				{
					if (address.empty() || (address.front() != '['))
						return false;

					auto it = address.find("]:");
					if (it == std::string_view::npos)
						return false;

					std::memcpy(ip, address.data() + 1, it - 1);
					ip[it - 1] = '\0';

					if (auto [p, ec] = std::from_chars(address.data() + it + 2, address.data() + address.size(), port); ec != std::errc())
						return false;

					return true;
				};

				if (!extractAddress(srcAddress, srcIP, srcPort) || !extractAddress(dstAddress, dstIP, dstPort))
					return false;
			}

			headerIPV6.version = 6;
			headerIPV6.priority = 0;
			headerIPV6.payload_len = htons(static_cast<uint16_t>(sizeof(UDPHeader) + std::get<1>(payload)));
			headerIPV6.nexthdr = 17; //UDP
			headerIPV6.hop_limit = 0x80;
			if (inet_pton(AF_INET6, srcIP, &headerIPV6.src) != 1)
				return false;
			if (inet_pton(AF_INET6, dstIP, &headerIPV6.dst) != 1)
				return false;

			headerUDP.src_port = htons(srcPort);
			headerUDP.dst_port = htons(dstPort);
			headerUDP.length = htons(static_cast<uint16_t>(sizeof(UDPHeader) + std::get<1>(payload)));

			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, ipChecksum(reinterpret_cast<uint8_t*>(&headerIPV6.src), 32));
			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, headerIPV6.nexthdr << 8);
			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, headerUDP.length);
			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, headerUDP.src_port);
			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, headerUDP.dst_port);
			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, headerUDP.length);
			headerUDP.checksum = ipChecksumAdd(headerUDP.checksum, ipChecksum(reinterpret_cast<const uint8_t*>(std::get<0>(payload)), std::get<1>(payload)));
			headerUDP.checksum = ipChecksumFinal(headerUDP.checksum);
		}

		writePacket<3>(streamOut, Protocol::IPV6, timestamp, { {
			{ &headerIPV6, sizeof(IPV6Header) },
			{ &headerUDP, sizeof(UDPHeader) },
			payload
		} });

		return true;
	}
}
