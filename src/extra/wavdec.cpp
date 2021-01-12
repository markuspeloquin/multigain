#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>

#include <sox.h>

int
main(int argc, char **argv) {
	if (argc < 2) {
		std::cerr << "Usage: " << *argv << " FILE\n";
		return 1;
	}

	if (sox_init() != SOX_SUCCESS) {
		std::cerr << "sox_init() error\n";
		return 1;
	}

	sox_format_t *fmt;
	if (!(fmt = sox_open_read(argv[1], 0, 0, 0))) {
		std::cerr << "sox_open_read() error\n";
		sox_quit();
		return 1;
	}

	unsigned channels = fmt->signal.channels;
	unsigned rate = fmt->signal.rate;
	size_t num_samples = channels * rate / 75;
	std::unique_ptr<int32_t> sample_buf(new int32_t[num_samples]);

	size_t samples;
	unsigned clips = 0;
	while ((samples = sox_read(fmt, sample_buf.get(), num_samples)) > 0) {
		unsigned chan = 0;
		for (size_t i = 0; i < samples; i++) {
			SOX_SAMPLE_LOCALS;

			std::cout << SOX_SAMPLE_TO_SIGNED_16BIT(
			    sample_buf.get()[i], clips);
			if (++chan == channels) {
				std::cout << '\n';
				chan = 0;
			} else
				std::cout << '\t';
		}
	}

	sox_close(fmt);
	sox_quit();

	return 0;
}
