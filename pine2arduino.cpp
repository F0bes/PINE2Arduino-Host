#include "pine/pine.h"

#include <cstdio>
#include <bit>
#include <fcntl.h>
#include <termios.h>

const char *port_name = "/dev/cu.usbmodem11201";

bool set_baud_rate(int fd, speed_t speed)
{
	struct termios tty;
	if (tcgetattr(fd, &tty) != 0)
	{
		std::printf("tcgetattr failed: %s\n", std::strerror(errno));
		return false;
	}

	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	if (tcsetattr(fd, TCSANOW, &tty) != 0)
	{
		std::printf("tcsetattr failed: %s\n", std::strerror(errno));
		return false;
	}
	return true;
}

int main(void)
{
	// Open a connection to PCSX2
	PINE::PCSX2 *ipc = nullptr;
	try
	{
		ipc = new PINE::PCSX2();
		std::printf("PCSX2 Version: %s\n", ipc->Version());
	}
	catch (...)
	{
		std::printf("Failed to create pine instance\n");
		return 1;
	}

	// Open the serial port

	int fd = open(port_name, O_RDWR | O_NOCTTY | O_SYNC);
	FILE *serial = fdopen(fd, "r+");
	if (fd < 0)
	{
		std::printf("Failed to open serial port %s: %s\n",
					port_name, std::strerror(errno));
		delete ipc;
		return 1;
	}

	if(!set_baud_rate(fd, B115200))
	{
		close(fd);
		return 1;
	}

	const uint32_t rpm_address = 0x6060D0;	 // Address of the RPM variable in NASCAR 09 (0-10000)

	// Track the last RPM to avoid sending duplicates
	uint32_t last_rpm = 0;
	while (true)
	{
		// Read the speed from PCSX2
		uint32_t rpm = 0;
		try
		{
			rpm = ipc->Read<uint32_t>(rpm_address);
		}
		catch (...)
		{
			std::printf("Failed to read RPM from PCSX2\n");
			break;
		}

		if(rpm == last_rpm)
		{
			// Could add a small sleep here to avoid busy waiting
			continue;
		}

		last_rpm = rpm;

		// round and convert to int
		float rpm_float = std::bit_cast<float>(rpm);
		uint32_t rpm_int = static_cast<uint32_t>(rpm_float + 0.5f);

		// Send the RPM to the Arduino
		// Format R:XXXX\n
		char buffer[32];
		std::snprintf(buffer, sizeof(buffer), "R:%d\n", rpm_int);
		size_t written = std::fwrite(buffer, 1, std::strlen(buffer), serial);
		if (written != std::strlen(buffer))
		{
			std::printf("Failed to write RPM to Arduino\n");
			break;
		}
		std::printf("Sent buffer: %s\n", buffer);

		std::fflush(serial);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	delete ipc;
	return 0;
}
