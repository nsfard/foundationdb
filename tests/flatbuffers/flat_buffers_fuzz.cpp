#include <iostream>
#include <fstream>
#include <vector>

#include "flow/flat_buffers.h"
#include "flat_buffers_boost_variant.h"

using namespace flat_buffers;
#include <table.h>

using namespace std;

namespace {
struct Arena {
	std::vector<std::pair<uint8_t*, size_t>> allocated;

	uint8_t* operator()(size_t sz) {
		auto res = new uint8_t[sz];
		allocated.emplace_back(res, sz);
		return res;
	}

	size_t get_size(uint8_t* ptr) {
		for (auto p : allocated) {
			if (p.first == ptr) {
				return p.second;
			}
		}
		return -1;
	}

	~Arena() {
		for (auto p : allocated) {
			delete[] p.first;
		}
	}
};
} // namespace

int main(int argc, char** argv) {
	streampos size;
	std::unique_ptr<uint8_t[]> memblock;

	ifstream file(argv[1], ios::in | ios::binary | ios::ate);
	if (file.is_open()) {
		size = file.tellg();
		memblock.reset(new uint8_t[size]);
		file.seekg(0, ios::beg);
		file.read(reinterpret_cast<char*>(memblock.get()), size);
		file.close();

	} else {
		cerr << "Unable to open file";
		return 1;
	}
	Table0 t;
	auto context = []() {};
	flat_buffers::detail::load(t, memblock.get(), context);
	Arena arena;
	auto out = flat_buffers::detail::save(arena, t, flat_buffers::FileIdentifier{});
	std::ofstream outfile;
	outfile.open(argv[2]);
	outfile.write(reinterpret_cast<char*>(out), arena.get_size(out));
	outfile.close();
	return 0;
}
