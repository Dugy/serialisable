#include <iostream>
#include <iterator>
#include "serialisable.hpp"
#include "condensed_json.hpp"

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file_name" << std::endl;
		return 1;
	}
	const std::string fileName = argv[1];
	const auto dotLocation = fileName.find_last_of('.');
	std::string name = (dotLocation != std::string::npos) ? fileName.substr(0, dotLocation) : fileName;
	if (dotLocation != std::string::npos && fileName.substr(dotLocation + 1) == "json") {
		Serialisable::JSON json = Serialisable::JSON::load(fileName);
		json.save("readCheck.json");
		std::vector<uint8_t> outVector = json.to<CondensedJSON>();
		std::string outputName = name + ".cjson";
		std::ofstream output(outputName);
		if (!output.good()) {
			std::cerr << "Cannot write file: " << outputName << std::endl;
			return 2;
		}
		for (uint8_t c : outVector) {
			output << c;
		}
	} else {
		std::ifstream input(fileName, std::ios::binary);
		if (!input.good()) {
			std::cerr << "Cannot read file: " << fileName << std::endl;
			return 2;
		}
		std::vector<uint8_t> inputVector(std::istreambuf_iterator<char>(input), {});
		input.close();
		Serialisable::JSON json = Serialisable::JSON::from<CondensedJSON>(inputVector);
		std::string outputName = name + ".json";
		json.save(outputName);
	}
	return 0;
}
