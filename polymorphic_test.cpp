#include <iostream>
#include <iterator>
#include "serialisable_polymorphic.hpp"

struct ContentType : public SerialisablePolymorphic {
	bool fullscreen = false;
	void serialisation() override {
		synch("fullscreen", fullscreen);
	}
};

struct Content1 : public ContentType {
	std::string value;
	void serialisation() override {
		ContentType::serialisation();
		subclass("c1");
		synch("value", value);
	}
	SERIALISABLE_REGISTER_POLYMORPHIC(ContentType, Content1, "c1");
};

struct Content2 : public ContentType {
	double value;
	void serialisation() override {
		ContentType::serialisation();
		subclass("c2");
		synch("value", value);
	}
	SERIALISABLE_REGISTER_POLYMORPHIC(ContentType, Content2, "c2");
};

struct Parent : public Serialisable {
	std::vector<std::shared_ptr<ContentType>> contents;
	std::unique_ptr<ContentType> main;
	void serialisation() override {
		synch("contents", contents);
		synch("main", main);
	}
};

int main(int argc, char** argv) {
	Parent parent;
	parent.load("polymorphs.json");
	parent.contents.push_back(std::make_shared<Content1>());
	parent.contents.push_back(std::make_shared<Content2>());
	parent.save("polymorphs.json");
	return 0;
}
