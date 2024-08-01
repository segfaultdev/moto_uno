#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

int main(int argc, const char *argv[]) {
	const char *binary_path = NULL;
	const char *port_path = "/dev/ttyACM0";
	
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--binary")) {
			binary_path = argv[++i];
		}
		
		if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--port")) {
			port_path = argv[++i];
		}
	}
	
	FILE *binary_file = fopen(binary_path, "rb");
	
	if (binary_file == NULL) {
		fprintf(stderr, "[moto] Cannot open binary `%s`.\n", binary_path);
		return 1;
	}
	
	fseek(binary_file, 0, SEEK_END);
	
	int binary_size = (int)(ftell(binary_file));
	rewind(binary_file);
	
	if (binary_size != 1600) {
		fprintf(stderr, "[moto] Invalid binary `%s` (expected 1600 bytes, read %d bytes).\n", binary_path, binary_size);
		return 1;
	}
	
	int port_id = open(port_path, O_RDWR);
	
	if (port_id < 0) {
		fprintf(stderr, "[moto] Cannot open port `%s`.\n", port_path);
		return 1;
	}
	
	struct termios tty;
	tcgetattr(port_id, &tty);
	
	tty.c_cflag &= ~PARENB;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	tty.c_cflag &= ~CRTSCTS;
	tty.c_cflag |= CREAD | CLOCAL;
	
	tty.c_lflag &= ~ICANON;
	tty.c_lflag &= ~ECHO;
	tty.c_lflag &= ~ECHOE;
	tty.c_lflag &= ~ECHONL;
	tty.c_lflag &= ~ISIG;
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
	
	tty.c_oflag &= ~OPOST;
	tty.c_oflag &= ~ONLCR;
	
	tty.c_cc[VTIME] = 0;
	tty.c_cc[VMIN] = 1;
	
	cfsetispeed(&tty, B19200);
	cfsetospeed(&tty, B19200);
	
	tcsetattr(port_id, TCSANOW, &tty);
	
	const char *check_s = "\r\n$ ";
	char c;
	
	for (int i = 0; i < 2; i++) {
		if (read(port_id, &c, 1) < 0) {
			fprintf(stderr, "[moto] Cannot read from port `%s`.\n", port_path);
			return 1;
		}
		
		if (c != check_s[i + 2]) {
			fprintf(stderr, "[moto] Invalid data from port `%s` (0x%02X != 0x%02X).\n", port_path, c, check_s[i + 2]);
			return 1;
		}
	}
	
	fprintf(stderr, "[moto] (1/4) Loading bootstrap into RAM...\n");
	
	if (write(port_id, "b", 1) < 1) {
		fprintf(stderr, "[moto] Cannot write to port `%s`.\n", port_path);
		return 1;
	}
	
	for (int i = 0; i < 4; i++) {
		if (read(port_id, &c, 1) < 0) {
			fprintf(stderr, "[moto] Cannot read from port `%s`.\n", port_path);
			return 1;
		}
		
		if (c != check_s[i]) {
			fprintf(stderr, "[moto] Invalid data from port `%s` (0x%02X != 0x%02X).\n", port_path, c, check_s[i]);
			return 1;
		}
	}
	
	fprintf(stderr, "[moto] (2/4) Executing bootstrap...\n");
	
	if (write(port_id, "x", 1) < 1) {
		fprintf(stderr, "[moto] Cannot write to port `%s`.\n", port_path);
		return 1;
	}
	
	for (int i = 0; i < 4; i++) {
		if (read(port_id, &c, 1) < 0) {
			fprintf(stderr, "[moto] Cannot read from port `%s`.\n", port_path);
			return 1;
		}
		
		if (c != check_s[i]) {
			fprintf(stderr, "[moto] Invalid data from port `%s` (0x%02X != 0x%02X).\n", port_path, c, check_s[i]);
			return 1;
		}
	}
	
	fprintf(stderr, "[moto] (3/4) Setting programmer to transmit mode...\n");
	
	if (write(port_id, "t", 1) < 1) {
		fprintf(stderr, "[moto] Cannot write to port `%s`.\n", port_path);
		return 1;
	}
	
	for (int i = 0; i < 2; i++) {
		if (read(port_id, &c, 1) < 0) {
			fprintf(stderr, "[moto] Cannot read from port `%s`.\n", port_path);
			return 1;
		}
		
		if (c != check_s[i]) {
			fprintf(stderr, "[moto] Invalid data from port `%s` (0x%02X != 0x%02X).\n", port_path, c, check_s[i]);
			return 1;
		}
	}
	
	fprintf(stderr, "[moto] (4/4) Transmitting binary...\n");
	
	for (int i = 0; i < binary_size; i++) {
		uint8_t byte;
		fread(&byte, 1, 1, binary_file);
		
		char hex_s[3];
		sprintf(hex_s, "%02x", byte);
		
		if (write(port_id, hex_s, 2) < 2) {
			fprintf(stderr, "[moto] Cannot write to port `%s`.\n", port_path);
			return 1;
		}
		
		usleep(((30 * 10000) / 192) + (40 * 256));
		fprintf(stderr, "#");
		
		if ((i & 0x1F) == 0x1F) {
			fprintf(stderr, " (%3d%%)\n", ((i + 1) * 100) / binary_size);
		}
	}
	
	fclose(binary_file);
	close(port_id);
	
	fprintf(stderr, "[moto] Done, have a nice day!\n");
	return 0;
}
