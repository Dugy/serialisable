# Serialisable
A small header-only library for convenient serialisation of C++ classes without boilerplate. It uses the [JSON format](https://en.wikipedia.org/wiki/JSON). It's designed for minising the amount of code used for saving and loading while keeping the saved file human-readable, it's not optimised for performance.

Normally, persistent preferences tend to have one method for saving that contains code for loading all the content and one method for saving it all. Also, there might be some need for validity checking. This tool allows saving it all in one method that calls overloads of one method to all members to be saved.

It contains a small JSON library to avoid a dependency on a library that is probably much larger than this one. It's not advised to be used for generic JSON usage.

## Usage

Have your classes inherit from the Serialisable class. They have to implement a `serialisation()` method that calls overloads of the `synch()` method that accepts name of the value in the file as first argument and the value (taken as reference) as the second one. If something needs to be processed before saving or after loading, the `saving()` method will return a bool value telling if it's being saved.

Supported types are `std::string`, arithmetic types (converted to `double` because of JSON's specifications), `bool`, any object derived from `Serialisable`, a `std::vector` of serialisable types, a `std::string` indexed `std::unordered_map` of serialisable types, smart pointers to serialisable types (`null` in JSON stands for `nullptr`). If C++17 is available, `std::optinal` is also supported. All of these apply recursively, so it's possible to serialise a `std::shared_ptr<std::unordered_map<std::string, std::vector<int>>>`.

Default values should be set somewhere, because if `load()` does not find the specified file, it does not call the `serialisation()` method.

Missing keys will simply not write the value. Values of wrong types will throw.

```C++
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

	virtual void serialisation() {
		synch("last_folder", lastFolder);
		synch("last_open", lastOpen);
		synch("privileged", privileged);
		synch("info", info);
		synch("chapters", chapters);
		synch("footnotes", footnotes);
		synch("addenda", addenda);
	}
};

//...

// Loading
Preferences prefs;
prefs.load("prefs.json");
  
// Saving
prefs.save("prefs.json");
```

It relies only on standard libraries, so you can use any C++14 compliant compiler to compile it.

## JSON library

The JSON library provided is only to avoid having additional dependencies. It's written to be short, its usage is prone to result in repetitive code. If you need JSON for something else, use a proper JSON library, like [the one written by Niels Lohmann](https://github.com/nlohmann/json), they are much more convenient.

If you really need to use it, for example if you are sure you will not use it much, here is an example:

``` C++
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
```

The structure consists of JSON nodes of various types. They all have the same methods for accessing the contents returning references to the correct types (`getString()`, `getDouble()`, `getBool()`, `getObject()` and `getArray()`), but they are all virtual and only the correct one will not throw an exception. The type can be learned using the `type()` method. The interface class `Serialisable::JSON` is also the _null_ type.

The parser can parse incorrect code in some cases because some of the information in JSON files is redundant.

If necessary, `shared_ptr<JSON>` can also be a serialised member if the `synch` method is used on it.

## A more condensed format

This is a binary markup language designed to use as little space as possible while keeping the same expressive power than JSON (and is thus directly convertible to JSON and back). It's also significantly more space efficient than BSON and Packed JSON. However, it's slow to write. Its only purpose is to take as little space as possible while keeping the versatility of JSON. It's not a compression algorithm, so it can be compressed afterwards to further reduce the size if it contains a lot of strings.

It can be accessed using methods `serialiseCondensed()` and `deserialise()`, using data saved in a `std::vector<uint8_t>`:

```C++
// Assuming 'value' inherits from Serialisable
std::vector<uint8_t> data = value.serialiseCondensed();
//...
std::vector<uint8_t> data = read(source);
value.deserialise(data);
```

It might wrongly guess the number of significant digits of floating point numbers without hints. Use `double` types for lossless encoding of numbers and `float` if the precision isn't important.

In most cases, it uses one byte of markup per value, only long strings and some composite objects need two bytes. Small integers and boolean values are included in the markup bytes and take no additional space themselves. ASCII-only element names in objects are only as long as the strings themselves and the element names are stored only once if there are more objects with the same element names.

### Encoding

It has more types than JSON, but they are selected automatically for better space efficiency and translate to the same JSON. The types are marked with binary prefixes, while the prefixes may contain data themselves:
* **1xxxxxxx** - forms a 15 bit float with the next byte (almost half-precision), 1 bit is sign, 6 are exponent, 8 are mantissa, so the imprecision is about 0.2% and maximal value is in the order of ten power 9 *(total size is 2)*
* **011xxxxx** - string of size below 30, size is stored in the remaining bits *(total size is length + 1)*
* **01111110** - reserved
* **01111111** - long string, zero-terminated *(total size is length + 2)*
* **010xxxxx** - 5 bit signed integer, saved in the type *(total size is 1)*
* **00111xxx** - object whose member names must contain ASCII-symbols and objects with the same layout appear more than once in the JSON, first occurrence comes with a zero-terminated definition of all member names terminated by the most significant bit flipped, last three bits form the identifier *(total size of object is contents + 1 and once element names + 1)*
* **00111101** - reserved
* **00111110** - same, but identifier is the following byte *(total size of object is contents + 2 and once element names + 1)*
* **00111111** - same, but the identifier are the two following bytes *(total size of object is contents + 3 and once element names + 1)*
* **00110xxx** - small object with layout appearing only once, with max size up to 5 written in the type, with ASCII-only element names first and contents after *(total size is contents + element names + 1)*
* **00110110** - large object whose zero-terminated names list must contain ASCII-symbols only *(total size is of object is contents + element names + 2)*
* **00110111** - large, zero-terminated hashtable/object whose zero terminated member names do not contain only ASCII-symbols *(total size is contents + element names + number of elements + 2)*
* **0010xxxx** - array of size up to 14, size is a part of the type, size is stored in the type *(total size 1 + total size of contents)*
* **00101110** - reserved
* **00101111** - large, zero-terminated array of objects *(total size is 2 + total size of contents)*
* **0001xxxx** - forms a 12 bit signed integer with the following byte *(size is 2)*
* **00001111** - double (written in little endian, least significant bit goes first) *(size is 9)*
* **00001110** - float *(size is 5)*
* **00001101** - signed 64 bit integer *(size is 9)*
* **00001100** - unsigned 64 bit integer *(size is 9)*
* **00001011** - signed 32 bit integer *(size is 5)*
* **00001010** - unsigned 32 bit integer *(size is 5)*
* **00001001** - signed 16 bit integer *(size is 3)*
* **00001000** - unsigned 16 bit integer *(size is 3)*
* **000001xx** - reserved
* **00000011** - true *(size is 1)*
* **00000010** - false *(size is 1)*
* **00000001** - null *(size is 1)*
* **00000000** - zero termination, for terminating strings, objects/hashtables and arrays too large to have their sizes in the type byte *(size is counted to types that need it)*

## Optional extensions

Although its extensibility is not as good as it could be, it is possible to extend it. Some extensions are available in this repository.

I would like the condensed format to be also an extension, but I haven't figured out how to do it without making the library less convenient to use.

### SerialisableBrief - write even less code

To write even less code for serialisation, you can use `SerialisableBrief`, a façade above `Serialisable`. It's in its separate header file, `serialisable_brief.hpp`.

It allows serialising with even less code:
```C++
struct Chapter : public SerialisableBrief {
	std::string contents = key("contents");
	std::string author = key("author").init("Anonymous");
};
```
It does the same as the `Chapter` class in the section above. The members must be initialised with the `key()` method, whose argument is the key of the field in JSON. The members can be optionally initialised using the `init()` method that takes any number of arguments that will be fed to the constructor (if not used, it will be default-initialised; brace-enclosed initialisers cannot be used). This can be shortened by adding any number of arguments after the first argument to the `key()` method, the additional arguments will be given to the constructor. This uses `Serialisable` to actually convert the variables to JSON, so it can serialise the same types as `Serialisable` can.

```C++
struct ChapterInfo : public SerialisableBrief {
	std::mutex lock = skip();
	int pages = key("pages").init(100);
	std::string summary = key("summary", "Some interesting stuff");
};
```

**Important:** if a member is *not* to be serialised, it has to be initialised with the `skip()` method (optionally taking constructor arguments; usable also for types that cannot be serialised by `Serialisable`). Otherwise, undefined behaviour is very likely to occur when serialising/deserialising the next member, without any warning. This applies also to any classes that inherit from it unless none of them uses any further serialisation. Therefore, this should be used only for classes that hold data and don't have much other functionality. You have been warned.

The cost of this brevity is proneness to human errors, obscure code and lower performance, especially when constructing the objects. To avoid forgetting the `skip()` method, it's better to use `Serialisable` instead for more complex classes that aren't only for storing data. To reduce overhead, it's recommended to copy or move the objects instead of creating new ones (for example by copying a static object from a factory method).

### SerialisablePolymorphic - different classes in one field

If a field can contain various types of objects, it can be dealt with by keeping it in the JSON format and dealing with it later, but `serialisable_polymorphic.hpp` makes it more convenient. It uses [GenericFactory](https://github.com/Dugy/generic_factory), a small, header-only library providing a generic implementation of the self-registering factory pattern. You will have to add it to your project in order to use this tool.

It allows descendants of a certain parent class that inherits from `SerialisablePolymorphic` to be selected according to a specific key that identifies the type (`_pt`). The parent class can be `SerialisablePolymorphic` itself. Every subclass registers itself as a descendant with some name of the parent class. Then the right subclass can be selected when loading the JSON.

The following example allows the members of the class `Master` to contain the two subclasses of `ContentType` and be serialised and deserialised intact:

```C++
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
	// Registering variant 1 (more readable)
	SERIALISABLE_REGISTER_POLYMORPHIC(ContentType, Content1, "c1");
};

struct Content2 : public ContentType {
	double value;
	void serialisation() override {
		ContentType::serialisation();
		subclass("c2");
		synch("value", value);
	}
	// Registering variant 2 (no macros)
	inline const static polymorphism<ContentType, Content2> = "c2";
};

struct Master : public Serialisable {
	std::vector<std::shared_ptr<ContentType>> contents;
	std::unique_ptr<ContentType> main;
	void serialisation() override {
		synch("contents", contents);
		synch("main", main);
	}
};
```

If you don't have C++17, it's not possible to use `inline static` variables, so you will have to resort to the way provided by `GenericFactory`:

```C++
REGISTER_CHILD_INTO_FACTORY(ContentType, Content1, "c1");
// Must be in the same namespace as the class, but not inside the class
```

### Custom types

It is possible to extend the library to serialise custom types and containers as well. You can do this by specialising `SerialisableInternals::Serialiser` with your type. For example, if you want to serialise also `std::map` indexed with `std::string`, include this file instead of `serialisable.hpp`.

```C++
#include <map>
#include "serialisable.hpp"

namespace SerialisableInternals {

template <typename T>
struct Serialiser<std::map<std::string, T>, std::enable_if_t<Serialiser<T, void>::valid>> {
	constexpr static bool valid = true; // Show that it's not unspecialised
	static_assert (Serialiser<T, void>::valid, "Serialising a map of non-serialisable types");

	static std::shared_ptr<Serialisable::JSONobject> serialise(const std::map<std::string, T>& value) {
		auto made = std::make_shared<Serialisable::JSONobject>();
		for (auto& it : value)
			made->_contents[it.first] = Serialiser<T, void>::serialise(it.second);
		return made;
	}

	static void deserialise(std::map<std::string, T>& result, std::shared_ptr<Serialisable::JSON> value) {
		const std::map<std::string, std::shared_ptr<Serialisable::JSON>>& got = value->getObject();
		for (auto it = result.begin(); it != result.end(); ) {
			if (got.find(it->first) == got.end())
				it = result.erase(it);
			else
				++it;
		}
		for (auto& it : got)
			Serialiser<T, void>::deserialise(result[it.first], it.second);
	}
};

}
```

To implement a custom serialisation for a custom class that does inherit from `Serialisable`, have it inherit from `SerialisableInternals::UnusualSerialisable` as well, which will stop the default serialisation functions from being selected.
