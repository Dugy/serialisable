#include <iostream>
#include "serialisable.hpp"

struct Chapter : public Serialisable {
	std::string contents = "";
	std::string author = "Anonymous";

	virtual void serialisation() {
		synch("contents", contents);
		synch("author", author);
	}
};

struct Preferences : public Serialisable {
	std::string lastFolder = "";
	unsigned int lastOpen = 0;
	bool privileged = false;
	Chapter info;
	std::vector<Chapter> chapters;
	std::vector<std::shared_ptr<Chapter>> footnotes;
	std::vector<std::unique_ptr<Chapter>> addenda;
	std::string* editorsNote = nullptr;

	virtual void serialisation() {
		synch("last_folder", lastFolder);
		synch("last_open", lastOpen);
		synch("privileged", privileged);
		synch("info", info);
		synch("chapters", chapters);
		synch("footnotes", footnotes);
		synch("addenda", addenda);
		synch("editors_notes", editorsNote);
	}
};

int main() {
	Serialisable::JSONobject testJson;
	testJson.getObject()["file"] = std::make_shared<Serialisable::JSONstring>("test.json");
	testJson.getObject()["number"] = std::make_shared<Serialisable::JSONdouble>(9);
	testJson.getObject()["makes_sense"] = std::make_shared<Serialisable::JSONbool>(false);
	std::shared_ptr<Serialisable::JSONarray> array = std::make_shared<Serialisable::JSONarray>();
	for (int i = 0; i < 3; i++) {
		std::shared_ptr<Serialisable::JSONobject> obj = std::make_shared<Serialisable::JSONobject>();
		obj->getObject()["index"] = std::make_shared<Serialisable::JSONdouble>(i);
		std::shared_ptr<Serialisable::JSONobject> obj2 = std::make_shared<Serialisable::JSONobject>();
		obj->getObject()["contents"] = obj2;
		obj2->getObject()["empty"] = std::make_shared<Serialisable::JSONobject>();
		array->getVector().push_back(obj);
	}
	testJson.getObject()["data"] = array;
	testJson.writeToFile("test.json");

	std::shared_ptr<Serialisable::JSON> testReadJson = Serialisable::parseJSON("test.json");
	testReadJson->getObject()["makes_sense"]->getBool() = true;
	testReadJson->getObject()["number"]->getDouble() = 42;
	testReadJson->writeToFile("test-reread.json");

	Preferences prefs;
	prefs.load("prefs.json");
	prefs.footnotes.push_back(std::make_shared<Chapter>());
	prefs.footnotes.back()->contents = "There will be a lot of footnotes";
	prefs.footnotes.back()->author = "Dugi";
	prefs.save("prefs.json");

	return 0;
}
