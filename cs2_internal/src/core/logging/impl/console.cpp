#include <core/common.hpp>
#include "../logging.hpp"

namespace logging::console {

	bool initialize () {
		return true;
	}

	void print_raw (const char* text) {
		(void)text;
	}

} // namespace logging::console
