#include <iostream>
#include "serialisable_brief.hpp"

enum DocumentType {
	BOOK = 1,
	ESSAY
};

struct Chapter : public SerialisableBrief {
	std::string contents = key("contents");
	std::string author = key("author").init("Anonymous");
};

struct Preferences : public SerialisableBrief {
	std::string lastFolder = key("last_folder").init("");
	unsigned int lastOpen = key("last_open").init(0);
	int daysUntilPublication = key("days_until_publication", -5);
	uint64_t maxFilesAllowed = key("max_files_allowed", UINT64_MAX);
	double relativeValue = key("relative_value").init(0.45);
	bool privileged = key("privileged", false);
	int reusableVariable = skip(3);
	char reusableVariable2 = skip().init('a');
	DocumentType documentType = key("document_type", BOOK);
	Chapter info = key("info");
	std::vector<Chapter> chapters = key("chapters").init(3, Chapter());
	std::vector<std::shared_ptr<Chapter>> footnotes = key("footnotes");
	std::vector<std::unique_ptr<Chapter>> addenda = key("addenda");
//	std::string* editorsNote = key("editors_notes", nullptr);
};

int main() {
	Serialisable::JSONobject testJson;
	testJson.getObject()["file"] = std::make_shared<Serialisable::JSONstring>("test.json");
	testJson.getObject()["number"] = std::make_shared<Serialisable::JSONint>(9);
	testJson.getObject()["float_number"] = std::make_shared<Serialisable::JSONdouble>(9);
	testJson.getObject()["makes_sense"] = std::make_shared<Serialisable::JSONbool>(false);
	std::shared_ptr<Serialisable::JSONarray> array = std::make_shared<Serialisable::JSONarray>();
	for (int i = 0; i < 3; i++) {
		std::shared_ptr<Serialisable::JSONobject> obj = std::make_shared<Serialisable::JSONobject>();
		obj->getObject()["index"] = std::make_shared<Serialisable::JSONint>(i);
		std::shared_ptr<Serialisable::JSONobject> obj2 = std::make_shared<Serialisable::JSONobject>();
		obj->getObject()["contents"] = obj2;
		obj2->getObject()["empty"] = std::make_shared<Serialisable::JSONobject>();
		array->getVector().push_back(obj);
	}
	testJson.getObject()["data"] = array;
	testJson.writeToFile("test.json");

	std::shared_ptr<Serialisable::JSON> testReadJson = Serialisable::parseJSON("test.json");
	testReadJson->getObject()["makes_sense"]->getBool() = true;
	testReadJson->getObject()["number"]->getInt() = 42;
	testReadJson->getObject()["float_number"]->getDouble() = 4.9;
	testReadJson->writeToFile("test-reread.json");

	Preferences prefs;
	prefs.load("prefs.json");
	prefs.footnotes.push_back(std::make_shared<Chapter>());
	prefs.footnotes.back()->contents = "There will be a lot of footnotes";
	prefs.footnotes.back()->author = "Dugi";
	prefs.documentType = ESSAY;
	prefs.save("prefs.json");

	return 0;
}
